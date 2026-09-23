#include "state_machine.h"
#include "../STAT_RGB/status_rgb.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h> // For access to "atomic_t" & "atomic_val_t"
#include <zephyr/drivers/sensor.h> // For internal use of "struct sensor_value"
#include <math.h> // For usage of "llround()"
#include "../TIMER/main_timer.h" // To access counter related methods.
#include "../CAN/fdcan.h" // For "drivers/can.h" and "fd_can_send()"
#include "../DRIVER_CONFIG/can_config.h" // For "CENTRAL_CAN_ID"
#include "../DRIVER_CONFIG/pid_config.h" // To access "PD_REFR_US"
#include "../PID/pid.h"// For pid related functionalities.
#include "../PWM/pwm.h" // For PWM related functionalities.
#include "../QDEC/qdec.h" // For QDEC related functionalities.
#include "../types.h"

LOG_MODULE_REGISTER(state_machine, 3); // Info level

// ============================================================================
// Mutexes and Semaphores
// ============================================================================
static struct k_spinlock glob_ang_vel_slock; // Angular velocity value populated
                                            // by new target coming in from CAN
                                            // BUS while the motor is spinning.

// ============================================================================
// Configuration
// ============================================================================
#define SM_EVENT_QUEUE_LENGTH 10

#define SM_THREAD_STACK_SIZE 2048
#define SM_THREAD_PRIORITY 5

#define PD_THREAD_STACK_SIZE 1024
#define PD_THREAD_PRIORITY 3 // Highest priority under main.

// ============================================================================
// Static memory allocation for threads
// ============================================================================
// Statically allocate stack for event processing thread.
K_THREAD_STACK_DEFINE(sm_thread_stack, SM_THREAD_STACK_SIZE);

// Statically allocate stack for PD motor control thread.
K_THREAD_STACK_DEFINE(pd_thread_stack, PD_THREAD_STACK_SIZE);


// ============================================================================
// Local variables
// ============================================================================

// Thread struct for PD thread
static struct k_thread pd_motor_thread;
static pid_t pid; // A PID instance
static atomic_t pd_stop_flag = ATOMIC_INIT(0); // Flag to stop PD thread

// Buffer for the event queue
static char event_q_buffer[SM_EVENT_QUEUE_LENGTH * sizeof(struct event_struct)];

uint8_t can_out_data_arr[CAN_DATA_SIZE] = {0}; // Static array used to curate 
                                               // outgoing CAN data

static char state_str[11] = {0}; // To store the string of the thread's state.

static int64_t glob_angular_vel_scaled = 0; // Access controlled by 
                                            // "ang_vel_mutex"

// ============================================================================
// Forward Declarations
// ============================================================================
static void _sm_thread_task(void *p1, void *p2, void *p3);
static void _pid_thread_task(void *p1, void *p2, void *p3);
static void _sm_thread_dispatch(struct sm *sm, struct event_struct event_s);
static void _sm_error_entry(struct sm *sm);
static int64_t _convert_rad_to_scaled_deg_ps(float radps);

// ============================================================================
// Inline functions
// ============================================================================
static inline int64_t angvel_to_scaled(struct sensor_value r)
{
    int64_t scaled = (int64_t)r.val1 * ANG_VEL_SCALE;

    scaled += r.val2;

    return scaled;
}

// ============================================================================
// Public APIs
// ============================================================================
int sm_init(struct sm *sm)
{
    if(sm == NULL){
        LOG_ERR("No state machine instance provided.");
        return 0;
    }

    // Initialize static PID instance
    pid_init(&pid);

    // Initialize the state machine's state to be IDLE
    sm->state = SM_STATE_IDLE;
    sm->sm_tid = 0;

    // Initialize the event message queue.
    k_msgq_init(&sm->event_queue, event_q_buffer, sizeof(struct event_struct), 
                SM_EVENT_QUEUE_LENGTH);

    // Configure and start the thread that consumes and process events
    // asycnchronously added to "sm->event_queue".
    sm->sm_tid = k_thread_create(
        &sm->sm_thread,
        sm_thread_stack,
        K_THREAD_STACK_SIZEOF(sm_thread_stack),
        _sm_thread_task,
        sm, NULL, NULL,
        SM_THREAD_PRIORITY,
        0,
        K_NO_WAIT);

    if(!sm->sm_tid){
        LOG_ERR("Unable to start the thread for event queue consumption");
        return 0;
    }

    return 1;
}

int sm_post_event(struct sm *sm, struct event_struct event_s)
{
    if(sm == NULL){
        LOG_ERR("No state machine instance provided.");
        return 0;
    }

    int ret;
    
    // Add event to event queue immediately.
    ret = k_msgq_put(&sm->event_queue, &event_s, K_NO_WAIT);

    if(ret < 0){
        LOG_ERR("Failed to put event on event queue, Error (%d)", ret);
        return 0;
    }

    return 1;
}

sm_state_t sm_get_state(struct sm *sm)
{
    if(sm == NULL){
        LOG_ERR("No state machine instance provided.");
        return SM_STATE_ERROR;
    }

    return sm->state;
}

// ============================================================================
// State entry and exit functions
// ============================================================================
static void _sm_idle_entry(struct sm *sm)
{
    set_status_rgb(SM_STATE_IDLE);
    sm->state = SM_STATE_IDLE;
}

static void _sm_timer_reset_exit(struct sm *sm)
{
    LOG_INF("Switched to IDLE state from reset exit function.");
    _sm_idle_entry(sm);
}

static void _sm_timer_reset_entry(struct sm *sm)
{
    set_status_rgb(SM_STATE_COUNTER_RESET);
    sm->state = SM_STATE_COUNTER_RESET;
    LOG_INF("Switched to COUNTER_RESET state from reset entry function.");
    // Reset the counter and set the machine's state back to IDLE
    if(!reset_core_counter()){
        LOG_ERR("Failed to reset counter.");
        _sm_error_entry(sm);
        return;
    }
    _sm_timer_reset_exit(sm);
}

void _sm_move_motor_entry(struct sm *sm, void *usr_data)
{
    LOG_INF("In motor state entry function.");

    // Temporary placeholder used for checking difference in received
    // angular velocity to previous target angular velocity.
    static motor_data_t motor_data = {0};
    static int64_t scaled_ang_vel = 0;

    // Get the pid's thread state.
    k_thread_state_str(sm->pid_tid, state_str, sizeof(state_str));
    
    // If thread is not running.
    if(strcmp(state_str, "dead") == 0){
        set_status_rgb(SM_STATE_MOVE);

        // The user data is of 'motor_data_t' type.
        sm->pid_tid = k_thread_create(&pd_motor_thread,
                                    pd_thread_stack,
                                    K_THREAD_STACK_SIZEOF(pd_thread_stack),
                                    _pid_thread_task,
                                    sm, usr_data, &pid,
                                    PD_THREAD_PRIORITY,
                                    0,
                                    K_NO_WAIT); 
        
        if(!sm->pid_tid){
            LOG_ERR("Unable to start PD motor controller thread.");
            _sm_error_entry(sm);
        }

        LOG_INF("Started PD motor control thread.");
    }
    else{
        // Thread is currently running.

        // Upack the angular velocity value in 'usr_data'
        memcpy((void *)&motor_data, usr_data, sizeof(motor_data_t));
        scaled_ang_vel = _convert_rad_to_scaled_deg_ps(motor_data.angular_vel);

        // Check to see if it is different from "glob_angular_vel_scaled"
        if(scaled_ang_vel != glob_angular_vel_scaled){
            // Lock the spinlock and update the global variable so that the PID
            // loop sees this updated value on its next loop
            K_SPINLOCK(&glob_ang_vel_slock){
                glob_angular_vel_scaled = scaled_ang_vel;
            }
        }
    }
    
}

static void _sm_sched_pid_thread_alarm(struct sm *sm, void *usr_data){

    // Switch the state to  SM_STATE_MOVE so that when an SM_EVENT_MOVE
    // is triggered during this state, the PD thread will be fired.
    if(sm->state != SM_STATE_MOVE){
        sm->state = SM_STATE_MOVE;
    }

    int ret;
    struct event_struct event_s = {.event = SM_EVENT_MOVE,
                                   .user_data = usr_data};



    // Schedule an alarm that puts an SM_EVENT_MOVE event on the 
    // event queue.

    // Need to send function pointer to "sm_post_event"
    // Need to provide the state machine's instance,
    // Need to provide the user data to create an "struct event_struct"
    // to be passed into the "sm_post_event" method.

    ret = set_move_alarm(sm_post_event,
                         sm,
                         event_s);

    if(ret){
        LOG_INF("Successfully set move alarm.");
    }

}

static void _sm_move_motor_exit(struct sm *sm)
{
    // As recommended by Zephyr's documentation, running PD thread will 
    // be terminated by signalling the PD thread to terminate itself as 
    // opposed to abruptly aborting it.
    atomic_set(&pd_stop_flag, 1);
}

static void _sm_get_ticks_entry(struct sm *sm)
{
    int ret;
    set_status_rgb(SM_STATE_GET_TICKS);
    sm->state = SM_STATE_GET_TICKS;
    uint8_t unpacked_counter_ticks[4] = {0};

    // Unpack a 32-bit counter value into 4, 8-bit values (Big Endian) 
    get_current_ticks_unpacked(unpacked_counter_ticks);

    // Outgoing data frame
    can_out_data_arr[0] = CENTRAL_CAN_ID;
    can_out_data_arr[1] = 0x54; //"T"
    can_out_data_arr[2] = 0x54; //"T"
    can_out_data_arr[3] = 0x00;
    can_out_data_arr[4] = 0x00;
    can_out_data_arr[5] = 0x00;
    can_out_data_arr[6] = 0x00;
    can_out_data_arr[7] = unpacked_counter_ticks[0];
    can_out_data_arr[8] = unpacked_counter_ticks[1];
    can_out_data_arr[9] = unpacked_counter_ticks[2];
    can_out_data_arr[10] = unpacked_counter_ticks[3];

    ret = fd_can_send(can_out_data_arr, "Counter Ticks");

    if(ret){
        LOG_ERR("Error [%d] sending out CAN message.", ret);
        _sm_error_entry(sm);
    }

    _sm_idle_entry(sm);
}
static void _sm_error_entry(struct sm *sm)
{
    if(sm->state != SM_STATE_ERROR){
        // TODO: Set state RGB to indicate we will be in Error state.
        set_status_rgb(SM_STATE_ERROR);
        sm->state = SM_STATE_ERROR;
        // TODO: Kill all ongoing processes
        // TODO: Kill off event queue waiting thread ?
        // TODO: Disable event queue ?
        // TODO: Send out error on CAN BUS to notify all other drivers.
        // TODO: Keep coming back to this state indefinitely.
    }
    LOG_ERR("State machine in error state.");
    k_msleep(1000);
}

// ============================================================================
// Internal functions
// ============================================================================
/* 
    @Brief: Functions that converts a float radians per second to scaled 
            degrees per second in integer format.
*/
static int64_t _convert_rad_to_scaled_deg_ps(float radps)
{
    LOG_INF("The angular velocity in radians per second is %f.", (double)radps);
    float deg_per_sec = (double)radps * (double)(180.0/3.141592653589793);
    return (int64_t)llround(deg_per_sec * (float)ANG_VEL_SCALE);
}
/*
    @Brief: Thread task that keeps a lookout and consumes events posted on the
            event queue. Invokes "_sm_thread_dispatch" when new event detected.
*/
static void _sm_thread_task(void *p1, void *p2, void *p3)
{
    LOG_INF("State machine task");

    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct sm *sm = (struct sm *)p1;
    struct event_struct event_s;

    while(1){
        if(k_msgq_get(&sm->event_queue, &event_s, K_FOREVER) == 0){
            _sm_thread_dispatch(sm, event_s);
        }
    }
    
}

/*
    @param p1: Pointer to "struct sm" instance.

    @param p2: Pointer to "motor_data_t" struct containing information
               on motor actuation.

    @param p3: Pointer to "pid_t" type of a pid instance.
*/

static void _pid_thread_task(void *p1, void *p2, void *p3)
{
    //(!) Thread to be stopped by external command from CAN
    LOG_INF("PD motor control thread task.");

    motor_data_t motor_data = {0};
    struct sensor_value prev_encoder = {0}; 
    struct sensor_value current_encoder = {0};
    int64_t trgt_ang_vel_scaled, meas_ang_vel_scaled;
    int64_t current_ticks = 0, prev_ticks = 0;
    int64_t pid_update_output;
    int32_t delta_us;
    pid_t pid;
    int ret;
    k_spinlock_key_t slock_key;

    memcpy((void *)&motor_data, p2, sizeof(motor_data_t));

    memcpy((void *)&pid, p3, sizeof(pid_t));

    trgt_ang_vel_scaled = _convert_rad_to_scaled_deg_ps(motor_data.angular_vel);

    // Initialize "glob_angular_vel_scaled"
    glob_angular_vel_scaled = trgt_ang_vel_scaled; 

    LOG_INF("Thee scaled target degrees per second is %lld", trgt_ang_vel_scaled);

    LOG_INF("The target ticks to start moving the motors is %u.",
            motor_data.trgt_ticks);

    LOG_INF("The direction of rotation is %s.", 
            (motor_data.direction?"C.C.W":"C.W."));

    // Main task
    // 'pd_stop_flag' set by _sm_move_motor_exit 
    while(!atomic_get(&pd_stop_flag)){
        LOG_INF("Motor is running.");

        // Poll quadrature encoder
        ret = qdec_read_angle(&current_encoder);

        if(!ret){
            LOG_ERR("Unable to fetch current encoder value.");
            break;
        }

        // Check to see if the target angular velocity has been changed
        if(trgt_ang_vel_scaled != glob_angular_vel_scaled){
            // Attempt to acquire the spinlock guarding
            // "glob_angular_vel_scaled" in a non-blocking fashion.
            if(k_spin_trylock(&glob_ang_vel_slock, &slock_key) == 0){
                trgt_ang_vel_scaled = glob_angular_vel_scaled;
                k_spin_unlock(&glob_ang_vel_slock, slock_key);
            }

            // If failed to acquire lock, retry acquiring lock next loop.
        }
        

        // TODO: A function that uses the current and previous encoder
        //       values to calculate the measured angular velocity

        // Scale measured sensor value up 
        meas_ang_vel_scaled = angvel_to_scaled(current_encoder);
        // Get current ms
        current_ticks = get_current_ticks();
        // Calculate elapsed ms
        delta_us = get_ticks_from_us(current_ticks) - get_ticks_from_us(prev_ticks);
        // Calculate current/measured angular velocity.
        // Update "prev_ticks"
        prev_ticks = current_ticks;
        // Update "prev_encoder"
        prev_encoder = current_encoder;
        // With scaled sensor value and elapsed ms, calculate output
        pid_update_output = pid_update(&pid, trgt_ang_vel_scaled,
                                       meas_ang_vel_scaled, delta_us);
        // Actuate PWM
        ret = pwm_actuate(pid_update_output);

        if(!ret){
            LOG_ERR("Failed to set pwm.");
            break;
        }

        k_usleep(PD_REFR_US);
    }

    // If ever broken out of while loop, that means that error has occured.
    // Instead of invoking _sm_error_entry(), put an error event on the event
    // queue so that this thread is able to return/ terminate execution.
    ret = sm_post_event((struct sm *)p1, 
                        (struct event_struct){.event = SM_EVENT_ERROR});

    // Reset the flag.
    atomic_set(&pd_stop_flag, 0);
}

/*
    @Brief: Helper method that handles the logic for state machine transitions
*/
static void _sm_thread_dispatch(struct sm *sm, struct event_struct event_s)
{
    switch(sm->state){
        case SM_STATE_IDLE:
            switch(event_s.event){
                case SM_EVENT_COUNTER_RESET:
                    _sm_timer_reset_entry(sm);
                    break;
                case SM_EVENT_MOVE:
                    // Transition in "SM_EVENT_MOVE" should only happen when 
                    // the state machine is in "SM_STATE_MOVE".
                    LOG_ERR("SM_EVENT_MOVE triggered in SM_STATE_IDLE.");
                    _sm_error_entry(sm);
                    break;
                case SM_EVENT_SCHED_MOVE:
                    // Schedule an alarm to put a move event on the event queue
                    _sm_sched_pid_thread_alarm(sm, event_s.user_data);
                    break;
                case SM_EVENT_SEND_TICKS:
                    _sm_get_ticks_entry(sm);
                    break;
                case SM_EVENT_SEND_ENCODER:
                    // Encoder already reset when in IDLE state. Ignore.
                    break;
                case SM_EVENT_STOP_MOTOR:
                    // Motor is not moving to being with. Ignore.
                    break;
                case SM_EVENT_ERROR:
                    LOG_ERR("SM_EVENT_ERROR received durin SM_STATE_IDLE.");
                    _sm_error_entry(sm);
                    break;
                default:
                    LOG_ERR("Undefined event in SM_STATE_IDLE.");
                    _sm_error_entry(sm);
                    break;
            }
            break;

        case SM_STATE_MOVE:
            switch(event_s.event){
                case SM_EVENT_MOVE:
                    // Only triggered by move alarm timing out.
                    // Motor could either be currently stopped or moving.
                    // Decision to handle motor moving or stopped delegated
                    // over to "_sm_move_motor_entry"
                    _sm_move_motor_entry(sm, event_s.user_data);
                    break;
                case SM_EVENT_SCHED_MOVE:
                    // New angular velocity setting sent in from CAN BUS.
                    // Schedule an alarm to put a move event on the event queue.
                    _sm_sched_pid_thread_alarm(sm, event_s.user_data);
                    break;
                case SM_EVENT_SEND_TICKS:
                    _sm_get_ticks_entry(sm);
                    break;
                case SM_EVENT_SEND_ENCODER:
                    // TODO: Entry function for getting encoder value.
                    // Should track how many TX mailboxes are in use before 
                    // attempting to send data out to prevent blocking.
                    break;
                case SM_EVENT_STOP_MOTOR:
                    _sm_move_motor_exit(sm);
                    // TODO: Include check for motors stopped.
                    // Check some signal on PWM side ?
                    _sm_idle_entry(sm);
                    break;
                case SM_EVENT_ERROR:
                    LOG_ERR("Error event encountered in move state.");
                    _sm_error_entry(sm);
                    break;
                default:
                    LOG_ERR("Undefined event in SM_STATE_MOVE.");
                    _sm_error_entry(sm);
                    break;
            }
            break;

        case SM_STATE_COUNTER_RESET:
            switch(event_s.event){
                case SM_EVENT_COUNTER_RESET:
                    // Already resetting, do nothing
                    break;
                case SM_EVENT_MOVE:
                    LOG_WRN("Attempting to move motors while counter \
                            is resetting, move command ignored.");
                    break;
                case SM_EVENT_SEND_TICKS:
                    LOG_WRN("Attempting to get ticks while counter \
                            is resetting, get ticks command ignored.");
                case SM_EVENT_ERROR:
                    _sm_error_entry(sm);
                    break;
                default:
                    LOG_ERR("Undefined event in SM_STATE_COUNTER_RESET.");
                    _sm_error_entry(sm);
                    break;
            }
            break;

        case SM_STATE_GET_TICKS:
            switch(event_s.event){
                case SM_EVENT_SEND_TICKS:
                    // Already getting ticks, do nothing.
                    break;
                case SM_EVENT_SEND_ENCODER:
                    // Only one "can_send" operation at a time. Ignore.
                    break;
                default:
                    LOG_ERR("Undefined event in SM_STATE_GET_TICKS.");
                    _sm_error_entry(sm);
                    break;
            }

        case SM_STATE_ERROR:
            switch(event_s.event){
                case SM_EVENT_COUNTER_RESET:
                    _sm_error_entry(sm);
                    break;
                case SM_EVENT_MOVE:
                    _sm_error_entry(sm);
                    break;
                default:
                    LOG_ERR("Undefined event in SM_STATE_ERROR.");
                    _sm_error_entry(sm);
                    break;
            }
            break;
        
        default:
            // Should not end up here
            LOG_ERR("Undefined state in switch.");
            _sm_error_entry(sm);
            break;
    }
}
