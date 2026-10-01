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

// ============================================================================
// Thread that waits on new CAN messages on "can_rx_msgq"
// ============================================================================

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

    ret &= fd_can_begin_rx_processor(state_machine_p);
    if(!ret){
        LOG_ERR("Failed to start can rx processing thread.");
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

