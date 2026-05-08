#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include "PWM/pwm.h"
#include "QDEC/qdec.h"
#include "CAN/fdcan.h"
#include "err_msgq.h"

// ============================================================================
// MACROS
// ============================================================================
#define ERR_MSGQ_SIZE 10 // Number of data elements that can be held by msgq.

// ============================================================================
// Local variables
// ============================================================================
static const uint32_t main_thread_sleep_ms = 100000;


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
    
    struct pwm_msgq_data pwm_data;

    printk("Invoking pwm_init().\n");
    pwm_init();
    printk("Invoking fdcan_init().\n");
    fdcan_init();
    printk("Invoking qdec_counter_init(). \n");
    qdec_counter_init(1000000);


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

    // Start the qdec counter
    start_qdec_counter();

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

        printk("main -> Setting CLKW direction at 20%% power. \n");

        pwm_data.direction = CLKW;
        pwm_data.power = 100;

        k_msgq_put(&pwm_msgq, &pwm_data, K_NO_WAIT);

        k_msleep(main_thread_sleep_ms);
        
        //printk("main -> Setting CLKW direction at 40%% power. \n");

        //pwm_data.direction = CLKW;
        //pwm_data.power = 40;
        //pwm_data.angle = 0;

        //k_msgq_put(&pwm_msgq, &pwm_data, K_NO_WAIT);

        //k_msleep(main_thread_sleep_ms);

    }
       
    return 0;
}




