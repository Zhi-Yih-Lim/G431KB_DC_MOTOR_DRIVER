#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include "PWM/pwm.h"

static const uint32_t main_thread_sleep_ms = 500;

int main (void)
{

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
        printk("Entered main loop.\n");
        k_msleep(main_thread_sleep_ms);
    }
       
    return 0;
}




