#include "state_machine.h"
#include "../STAT_RGB/status_rgb.h"
#include <zephyr/logging/log.h>
#include "../TIMER/main_timer.h"

LOG_MODULE_REGISTER(state_machine, 3); // Info level

// ============================================================================
// Configuration
// ============================================================================
#define SM_EVENT_QUEUE_LENGTH 10

#define SM_THREAD_STACK_SIZE 2048
#define SM_THREAD_PRIORITY 5

// Statically allocate stack for threads.
K_THREAD_STACK_DEFINE(sm_thread_stack, SM_THREAD_STACK_SIZE);

// Buffer for the event queue
static char event_q_buffer[SM_EVENT_QUEUE_LENGTH * sizeof(sm_event_t)];


// ============================================================================
// Forward Declarations
// ============================================================================
static void _sm_thread_task(void *p1, void *p2, void *p3);
static void _sm_thread_dispatch(sm_t *sm, sm_event_t event);
static void _sm_error_entry(sm_t *sm);

// ============================================================================
// Public APIs
// ============================================================================
int sm_init(sm_t *sm)
{
    if(sm == NULL){
        LOG_ERR("No state machine instance provided.");
        return 0;
    }

    // Initialize the state machine's state to be IDLE
    sm->state = SM_STATE_IDLE;
    sm->sm_tid = 0;

    // Initialize the event message queue.
    k_msgq_init(&sm->event_queue, event_q_buffer, sizeof(sm_event_t), 
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

int sm_post_event(sm_t *sm, sm_event_t event)
{
    if(sm == NULL){
        LOG_ERR("No state machine instance provided.");
        return 0;
    }

    int ret;
    
    // Add event to event queue immediately.
    ret = k_msgq_put(&sm->event_queue, &event, K_NO_WAIT);

    if(ret < 0){
        LOG_ERR("Failed to put event on event queue, Error (%d)", ret);
        return 0;
    }

    return 1;
}

sm_state_t sm_get_state(sm_t *sm)
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
static void _sm_timer_reset_exit(sm_t *sm)
{
    set_status_rgb(SM_STATE_IDLE);
    sm->state = SM_STATE_IDLE;
    LOG_INF("Swithced to IDLE state from reset exit function.");
}

static void _sm_timer_reset_entry(sm_t *sm)
{
    set_status_rgb(SM_STATE_COUNTER_RESET);
    sm->state = SM_STATE_COUNTER_RESET;
    LOG_INF("Swithced to COUNTER_RESET state from reset entry function.");
    // Reset the counter and set the machine's state back to IDLE
    if(!reset_core_counter()){
        LOG_ERR("Failed to reset counter.");
        _sm_error_entry(sm);
        return;
    }
    k_msleep(3000); // DELETE ME WHEN CONFIRMED WORKING !
    _sm_timer_reset_exit(sm);
}

static void _sm_move_motor_entry(sm_t *sm)
{
    set_status_rgb(SM_STATE_MOVE);
    sm->state = SM_STATE_MOVE;
    // TODO: Start thread to move motors under PD controller
}

static void _sm_move_motor_exit(sm_t *sm)
{
    // TODO: Terminate move motor thread.
    // TODO: Set state RGB to indicate we will be in IDLE state.
    set_status_rgb(SM_STATE_IDLE);
    sm->state = SM_STATE_IDLE;
}

static void _sm_error_entry(sm_t *sm)
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
    @Brief: Thread task that keeps a lookout and consumes events posted on the
            event queue. Invokes "_sm_thread_dispatch" when new event detected.
*/
static void _sm_thread_task(void *p1, void *p2, void *p3)
{
    LOG_INF("State machine task");

    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    sm_t *sm = (sm_t *)p1;
    sm_event_t event;

    while(1){
        if(k_msgq_get(&sm->event_queue, &event, K_FOREVER) == 0){
            _sm_thread_dispatch(sm, event);
        }
    }
    
}

/*
    @Brief: Helper method that handles the logic for state machine transitions
*/
static void _sm_thread_dispatch(sm_t *sm, sm_event_t event)
{
    switch(sm->state){

        case SM_STATE_IDLE:
            switch(event){
                case SM_EVENT_COUNTER_RESET:
                    _sm_timer_reset_entry(sm);
                    break;
                case SM_EVENT_MOVE:
                    _sm_move_motor_entry(sm);
                    break;
                case SM_EVENT_ERROR:
                    _sm_error_entry(sm);
                    break;
                default:
                    LOG_ERR("Undefined event in SM_STATE_IDLE.");
                    _sm_error_entry(sm);
                    break;
            }
            break;

        case SM_STATE_MOVE:
            break;

        case SM_STATE_COUNTER_RESET:
            switch(event){
                case SM_EVENT_COUNTER_RESET:
                    // Already resetting, do nothing
                    break;
                case SM_EVENT_MOVE:
                    LOG_ERR("Attempting to move motors while counter \
                            is resetting.");
                    _sm_error_entry(sm);
                    break;
                case SM_EVENT_ERROR:
                    _sm_error_entry(sm);
                    break;
                default:
                    LOG_ERR("Undefined event in SM_STATE_COUNTER_RESET.");
                    _sm_error_entry(sm);
                    break;
            }
            break;

        case SM_STATE_ERROR:
            switch(event){
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
