#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include "PWM/pwm.h"
#include "err_msgq.h"

// ============================================================================
// MACROS
// ============================================================================
#define ERR_MSGQ_SIZE 10 // Number of data elements that can be held by msgq.

// ============================================================================
// Local variables
// ============================================================================
static const uint32_t main_thread_sleep_ms = 500;


// ============================================================================
// Error message queue related
// ============================================================================
// Message queue variable
struct k_msgq err_msgq;

// Initialize error message queue
K_MSGQ_DEFINE(err_msgq, sizeof(struct err_msgq_data), ERR_MSGQ_SIZE, 1);

int main (void)
{
    // MAIN LOOP SHOULD ONLY BE RESPONSIBLE FOR THREAD INIT AND PERIOD ERROR 
    // CHECKING !

    struct err_msgq_data err_data;
    int ret = 0;
    
    pwm_init();

    //// Start the PWM thread
    //pwm_tid = k_thread_create(&pwm_thread,         // Thread struct
    //                          pwm_thread_stack,    // Pointer to stack space 
    //                          K_THREAD_STACK_SIZEOF(pwm_thread_stack),
    //                          pwm_thread_start,    // Thread entry point func                          
    //                          NULL,                // arg_1
    //                          NULL,                // arg_2
    //                          NULL,                // arg_3
    //                          PWM_THREAD_PRIORITY, // Thread priority level
    //                          0,                   // Thread options
    //                          K_NO_WAIT            // Delay b4 starting thread
    //                         );

    while(1){
        printk("Main loop.\n");

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




