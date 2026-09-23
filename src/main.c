#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "types.h"
#include "PWM/pwm.h"
#include "QDEC/qdec.h"
#include "DRIVER_CONFIG/can_config.h"
#include "CAN/fdcan.h"
#include "TIMER/main_timer.h"
#include "STATE_MACHINE/state_machine.h"
#include "STAT_RGB/status_rgb.h"
//#include "UART/uart.h"
#include "err_msgq.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>// For "sys_be32_to_cpu"

LOG_MODULE_REGISTER(main, 3); // Info level

// ============================================================================
// MACROS
// ============================================================================
#define ERR_MSGQ_SIZE 10 // Number of data elements that can be held by msgq.

// ============================================================================
// Local variables
// ============================================================================
static const uint32_t main_thread_sleep_ms = 100000;
static struct sm t_arr[1]; // Statically allocating memory for 'state_machine_p'
static struct sm* state_machine_p = t_arr;

// ============================================================================
// Forward Declarations
// ============================================================================
int _init_components();
int _start_components();
void _can_rx_process_entry_func(void *, void *, void *);

// ============================================================================
// Thread that waits on new CAN messages on "can_rx_msgq"
// ============================================================================
#define CAN_RX_PROCESS_THREAD_STACK_SIZE 2048
#define CAN_RX_PROCESS_THREAD_PRIORITY 4
K_THREAD_DEFINE(can_rx_process_thread,
                CAN_RX_PROCESS_THREAD_STACK_SIZE,
                _can_rx_process_entry_func,
                NULL, NULL, NULL,
                CAN_RX_PROCESS_THREAD_PRIORITY,
                0,
                0);

// ============================================================================
// Error message queue related
// ============================================================================
// Message queue variable
struct k_msgq err_msgq;

// Initialize error message queue
K_MSGQ_DEFINE(err_msgq, sizeof(struct err_msgq_data), ERR_MSGQ_SIZE, 1);

int main (void)
{
    LOG_INF("In main.");
    // MAIN LOOP SHOULD ONLY BE RESPONSIBLE FOR THREAD INIT AND PERIOD ERROR 
    // CHECKING !

    struct err_msgq_data err_data;
    int ret = 0;
    
    LOG_INF("Initializaing components.");

    ret = _init_components();

    if(!ret){
        LOG_ERR("Failed to initialize one or more components.");
        return 0;
    }

    ret = _start_components();

    if (!ret){
        LOG_ERR("Failed to start one or more components.");
        return 0;
    }

    while(1){
        LOG_INF("Main loop.\n");


        // Check to see if there are any errors in the error message queue
        ret = k_msgq_get(&err_msgq, &err_data, K_NO_WAIT);


        if(!ret){
            switch(err_data.thread){
                case PWM:
                    printk("PWM thread error %d.\n", err_data.err_no);
                    // TODO: Send data to main mcu via CAN.
                    // TODO: Re-initialize the PWM thread.
                    break;
                default:
                    printk("Unknown thread id of %d with err no of %d.\n",
                           err_data.thread,
                           err_data.err_no);
                    // TODO: Send data to main mcu via CAN.
                    break; 
            }
        }
       
        k_msleep(main_thread_sleep_ms);

    }
       
    return 0;
}


// ============================================================================
// Internal helper functions
// ============================================================================
int _init_components()
{
    int ret = 1;

    ret &= qdec_init(); // Quadrature Encoder
    if(!ret){
        LOG_ERR("Failed to initialize QDEC.");
        return ret;
    }
    ret &= fdcan_init(); // FDCAN 
    if(!ret){
        LOG_ERR("Failed to initialize FDCAN.");
        return ret;
    }
    ret &= core_counter_init(); // Core timer
    if(!ret){
        LOG_ERR("Failed to initialize Counter.");
        return ret;
    }
    ret &= pwm_init(); // PWM
    if(!ret){
        LOG_ERR("Failed to initialize PWM.");
        return ret;
    }

    //ret &= uart_init();
    //if(!ret){
    //    LOG_ERR("Failed to initialize USART.");
    //    return ret;
    //}
    
    ret &= sm_init(state_machine_p); // State machine
    if(!ret){
        LOG_ERR("Failed to initialize state machine.");
        return ret;
    }

    ret &= status_rgb_init();
    if(!ret){
        LOG_ERR("Failed to initialize status rgb.");
        return ret;
    }
    return ret;
}

int _start_components()
{
    int ret = 1;
    // Nothing to start for QDEC.
    ret &= set_status_rgb(SM_STATE_IDLE); 
    ret &= fd_can_start();
    ret &= start_core_counter();
    return ret;
}

void _can_rx_process_entry_func(void *p1, void *p2, void *p3)
{
    int ret;
    static struct can_frame t_arr[1];// Assigning static space for pointer 
                                     // variable below
    struct can_frame *can_data_struct_p = t_arr;
    static struct event_struct event_s = {0};

    uint8_t target_id;
    char action_arr[CAN_ACTION_SIZE] = {0}; // Triggers C's zero filling rule
                                            // by providing an initialzer that
                                            // sets the first byte.
    char tick_arr[CAN_TICK_DATA_SIZE] = {0};

    uint32_t float_byteswap_in, float_byteswap_out; 
                                  // Used for temporarily storing byte-swapped 
                                  // float for angular velocity
    
    // Wait for messages on the can_rx's message queue.
    // not busy waiting.
    while(1){
        ret = k_msgq_get(&can_rx_msgq, can_data_struct_p, K_FOREVER);

        if(ret < 0){
            LOG_ERR("Error %d getting data from can_rx_msgq.", ret);
            event_s.event = SM_EVENT_ERROR;
            sm_post_event(state_machine_p, event_s);
        }
        else{
            if(can_dlc_to_bytes(can_data_struct_p->dlc) != CAN_DATA_SIZE){
                LOG_ERR("Incorrect data length received.");
                continue;
            }

            // CAN data packet format:
            // [Target ID (1-byte), Action (2-bytes), Actual Data (4-bytes),
            //  Counter-ticks (4-bytes)]
            target_id = can_data_struct_p->data[0];

            LOG_INF("Target ID is %d", target_id);

            // All motor drivers will receive messages from the main controller
            // 0x64. Check to see if message is intended for this specific 
            // driver. If "target_id" == 0xFF, payload is directed at all motor
            // drivers connected on the bus.
            if(target_id == 0xFF || target_id == LOCAL_CAN_ID){

                LOG_INF("Target ID within acceptance field.");

                // TODO: Analyze message and post events to 
                //       sm_post_event();
                memcpy(action_arr, can_data_struct_p->data+CAN_TID_SIZE, 
                       (size_t)CAN_ACTION_SIZE);

                LOG_INF("Action[0] is %c", action_arr[0]);
                LOG_INF("Action[1] is %c", action_arr[1]);

                if(!action_arr[0]){
                    LOG_ERR("Data in action array unrecognized.");
                    // Reset the action_arr
                    action_arr[0] = 0;
                    action_arr[1] = 0;
                    continue;
                }

                if(state_machine_p){
                    switch(action_arr[0]){
                        case 'S': // Stop command
                            event_s.event = SM_EVENT_STOP_MOTOR;
                            ret = sm_post_event(state_machine_p,
                                                event_s);
                                                
                            if(!ret){
                                LOG_ERR("Failed to post stop motor event \
                                        on to event queue.");

                                state_machine_p = NULL;
                            }
                            break;
                        case 'R': // Reset counter command 
                            event_s.event = SM_EVENT_COUNTER_RESET;
                            ret = sm_post_event(state_machine_p, 
                                                event_s);

                            if(!ret){
                                LOG_ERR("Failed to post counter reset event \
                                        on to event queue.");

                                state_machine_p = NULL;
                            }
                            break;
                        case 'M': // Move command
                            static motor_data_t m_data ={
                                .trgt_ticks = 0,
                                .direction = 0,
                                .angular_vel = 0.0f
                            };

                            // Calclate the zero-indexed position of the first 
                            // byte of the actual data in 
                            // "can_data_struct_p->data"
                            static int actual_data_offset = CAN_TID_SIZE + 
                                                             CAN_ACTION_SIZE; 
                           
                            // First store the big-endianed incoming data as a 
                            // uint32_t variable, byte-swap to match the processors
                            // endianess and re-interpret the correctly swapped bits
                            // as float.
                            memcpy(&float_byteswap_in,
                                   (void *)(can_data_struct_p->data + 
                                            actual_data_offset),
                                   CAN_ACT_DATA_SIZE);

                            // Endianess byte-swap
                            float_byteswap_out = sys_be32_to_cpu(float_byteswap_in);

                            // The four-bytes of the actual data section of the
                            // incoming array represents float data.
                            m_data.angular_vel = 
                                UNALIGNED_GET((float *)&float_byteswap_out);

                            LOG_INF("The angular data is set to %f.", (double)m_data.angular_vel);
                            
                            // Calculate the zero-indexed position of the first
                            // byte of the tick data in 
                            // "can_data_struct_p->data" 
                            static int tick_offset = CAN_TID_SIZE + 
                                                     CAN_ACTION_SIZE + 
                                                     CAN_ACT_DATA_SIZE;
                                                 
                            // Copy over the target ticks from the incoming
                            // data array to tick_arr.
                            memcpy(tick_arr,
                                   (void *)(can_data_struct_p->data + 
                                            tick_offset),
                                   CAN_TICK_DATA_SIZE);

                            // Convert 4, 1-byte elements into a single variable 
                            // of size 32-bit.
                            m_data.trgt_ticks = 
                                sys_be32_to_cpu(UNALIGNED_GET((uint32_t*)tick_arr));
                            
                            LOG_INF("The target ticks is set to %u.", m_data.trgt_ticks);

                            switch(action_arr[1]){
                                case 'F': // Clockwise
                                    LOG_INF("Motor moving clockwise.");
                                    m_data.direction = 0;
                                    event_s.event = SM_EVENT_SCHED_MOVE;
                                    // (!) "m_data" passed by ref
                                    event_s.user_data = (void *)&m_data;
                                    ret = sm_post_event(state_machine_p, 
                                                        event_s);
                                    break;
                                case 'B': // Counter-clockwise
                                    LOG_INF("Motor moving counter-clockwise.");
                                    m_data.direction = 1;
                                    event_s.event = SM_EVENT_SCHED_MOVE;
                                    // (!) "m_data" passed by ref
                                    event_s.user_data = (void *)&m_data;
                                    ret = sm_post_event(state_machine_p, event_s);
                                    break;
                            }
                            break;
                        case 'G': // Get data from driver
                            switch(action_arr[1]){
                                case 'T': // Get counter ticks
                                    event_s.event = SM_EVENT_COUNTER_RESET;
                                    ret = sm_post_event(state_machine_p, event_s);
                                    
                                    if(!ret){
                                        LOG_ERR("Failed to post counter reset event on to \
                                                event queue.");

                                        state_machine_p = NULL;
                                    }
                                    break;
                            }
                            break;
                        case 'T': // Transmit data from driver
                            break;
                        default:
                            LOG_WRN("Unrecognized action command.");
                            break;
                    }
                }
                else{
                    LOG_ERR("No state machine instance.");
                }
            }
        }
    }    
}