#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <math.h>
#include "pwm.h"

// ============================================================================
// MACROS
// ============================================================================
#define PWM_THREAD_STACK_SIZE 1024 // Size of PWM thread on stack
#define PWM_THREAD_PRIORITY 7 // PWM thread priority level
#define PWM3_NODE_ID DT_NODELABEL(pwm3) // Node identifer for 'pwm3' node


// ============================================================================
// Locally global variables
// ============================================================================
// Get device pointer from node identifier
static const struct device *const pwm3_dev = DEVICE_DT_GET(PWM3_NODE_ID);

static const uint32_t CLK_FREQ = 170000000; // Clock frequency of timer 3
static const uint32_t DRV8871_FREQ = 90000; // Desired frequency to drive the 
                                            // DRV8871.
static uint32_t PWM_PERIOD_CLK_CYCLES;// Number of clock cycles per pwm period
static uint16_t PWM_DUTY_CLK_CYCLES;// Number of clock cycles for the specified
                                    // duty cycle.
static uint8_t PA6_S; // Indicates whether pin PA6 is being PWMed (ACTIVE LOW)
static uint8_t PA7_S; // Indicates whether pin PA7 is being PWMed (ACTIVE LOW)

static const uint32_t pwm_thread_sleep_ms = 500; // Sleep period for the PWM
                                                 // thread.
static const uint32_t main_thread_sleep_ms = 500;


// ============================================================================
// Local helper methods
// ============================================================================
static uint32_t _calc_pwm_period_clk_cycles(uint32_t clk_freq, 
                                            uint32_t trgt_freq);
static uint16_t _calc_pwm_duty_clk_cycles(uint32_t period_clk_cycle,
                                          uint8_t on_percent);
static void _init_pwm(void);

static void _set_pwm(Dir direction, uint8_t power);


// ============================================================================
// PWM thread related
// ============================================================================
// PWM thread id to be used in 'K_THREAD_DEFINE' below.
extern const k_tid_t pwm_tid;

// Statically defining and initializing a thread.
K_THREAD_DEFINE(pwm_tid,                // Name of the thread
                PWM_THREAD_STACK_SIZE,  // Stack size of thread in bytes 
                pwm_thread_start,       // Thread entry function
                NULL, NULL, NULL,       // arg_1, arg_2, and arg_3
                PWM_THREAD_PRIORITY,    // Thread priority 
                0,                      // Thread options
                0);                     // Scheduling delay

// ============================================================================
// Function definitions
// ============================================================================
/*
    Brief: PWM thread entry point intended to be invoked in main.

    @param arg_1 -> 3: Optional arguments to be passed into the thread.
*/
void pwm_thread_start(void *arg_1, void *arg_2, void *arg_3){
    
    int ret = 0;
    Dir pwm_dir = STAT;

    // Start off the PWM in braking mode
    _set_pwm(pwm_dir, 0);        

    while(1){
        // Check the message queue for any changes to motor's direction

        // Set the the PWM 

        // Sleep the thread to relinquish resource for other threads
        //k_msleep(pwm_thread_sleep_ms);

        _set_pwm(CLKW, 50);

        k_msleep(pwm_thread_sleep_ms);

        //_set_pwm(CCLKW, 20);

        //k_msleep(pwm_thread_sleep_ms);

        _set_pwm(STAT, 30);

        k_msleep(pwm_thread_sleep_ms);
 
    }
}
// ============================================================================
// Internal functions definitions
// ============================================================================
/*
    Brief: Assign the static global variables their appropriate values.
*/
static void _init_pwm(void){

    PWM_PERIOD_CLK_CYCLES = _calc_pwm_period_clk_cycles(CLK_FREQ, 
                                                        DRV8871_FREQ);

    printk("_init_pwm :: The number of clock cycles per PWM period is %d\n", PWM_PERIOD_CLK_CYCLES);

    PA6_S = PA7_S = 0;

}

/*
    Brief: Calculates the number of clock cycles for one period of a 
           specified PWM frequency. 

    @param clk_freq: The clock frequency.
    @param trgt_freq: The desired output PWM frequency.

    @return The number of clock cycles for one PWM period.

*/
static uint32_t _calc_pwm_period_clk_cycles(uint32_t clk_freq, 
                                            uint32_t trgt_freq){

    // Calculate the time for one cycle of the timer's frequency.
    float timer_clk_cycle_sec = 1.0f/clk_freq;

    // Calculate the time for one period of the deisred PWM output frequency.
    float pwm_period_sec = 1.0f/trgt_freq;

    // Calculate the number of clock cycles for one PWM period, rounded up.
    // (!) Personal choice for rounding up as it is okay for one PWM period to
    //     be slightly slower rather than going too fast, hitting the lower
    //     limit of DRV8871's acceptable frequency.
    return ceil(pwm_period_sec/timer_clk_cycle_sec);

}

/*
    Brief: Calculates the number of clock cycles for a specified PWM duty 
           cycle.

    @param period_clk_cycle: Number of clock cycles for a single PWM period.
    @param on_percent: Desired percentage (in integers) of PWM high signal. 

    @return The number of clock cycles for the desired duty cycle.

*/
static uint16_t _calc_pwm_duty_clk_cycles(uint32_t period_clk_cycle,
                                          uint8_t on_percent){
    return floor(on_percent/100.0*period_clk_cycle);

}

/*
    Brief: Controls the PWM signals of PA6 and PA7 depending on the input
           arguments.

    @param direction: The intended direction of rotation of the motor 
                      (when viewed from the front side exposed shaft).
    @param power: The power of the motor (0-100).
*/
static void _set_pwm(Dir direction, uint8_t power){

    uint8_t _pwr = 0;
    Dir _dir = 0;
    
    // Set a cap on the maximum power
    if(power > 100){
        printk("_set_pwm :: Power set to be above 100, capping power to 100.\n");
        _pwr = 100;
    }
    else{
        _pwr = power;
    }

    // Set the motor to be stationary in the event of an undocumented
    // 'Dir' input.
    if(direction < 0 || direction > 2){
        printk("_set_pwm :: Direction set undefined, stopping the motor.\n");
        _dir = 0;
    }
    else{
        _dir = direction;
    }
    
    // Calculate he number of signal high clock cycles based.
    PWM_DUTY_CLK_CYCLES = _calc_pwm_duty_clk_cycles(PWM_PERIOD_CLK_CYCLES, 
                                                   _pwr);

    switch(direction){
        case 0: // Motors are stationary
            // Set PA6 and PA7 to both be ACTIVE LOW at 0% duty-cycle 
            // (inverted polarity) to enter braking mode.
            if(pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES, 
                              0, PWM_POLARITY_INVERTED)){
                printk("_set_pwm :: Case 0 failed to set PA6.\n");
                return;
            }

            PA6_S = 1;

            if(pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                              0, PWM_POLARITY_INVERTED)){
                printk("_set_pwm :: Case 0 failed to set PA7.\n");
                return;
            }

            PA7_S = 1;
            break;

        case 1: // Motors rotating clockwise
            // Set IN1 to be high at desired duty cycle and keep IN2 LOW.
            if(pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES, 
                              0, PWM_POLARITY_INVERTED)){
                printk("_set_pwm :: Case 1 failed to set PA6.\n");
                //// Check to see if IN2 is set to high. If so, set it LOW.
                //if(IN2_S){
                //    pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                //                   0, PWM_POLARITY_NORMAL);
                //    IN2_S = 0;
                //}
                //return;
            }

            IN1_S = 1;

            if(pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                              PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                printk("_set_pwm :: Case 1 failed to set PA7.\n");
                //// Reset the output of channel 1 to be LOW.
                //pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES, 
                //              0, PWM_POLARITY_NORMAL);
                //IN1_S = 0;
                //return;
            }

            IN2_S = 0;
            break;
        case 2: // Motors rotating counter clockwise
            // Set IN1 to be LOW and PWM IN2.
            if(pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES, 
                              PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
                printk("_set_pwm :: Case 2 failed to set PA6.\n");
                //// Check to see if IN2 is set to high. If so, set it LOW.
                //if(IN2_S){
                //    pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                //                   0, PWM_POLARITY_NORMAL);
                //    IN2_S = 0;
                //}
                //return;
            }

            IN1_S = 0;

            if(pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                              0, PWM_POLARITY_INVERTED)){
                printk("_set_pwm :: Case 2 failed to set PA7.\n");
                return;
            }

            IN2_S = 1;
            break;
        default:
            printk("_set_pwm :: Case \'default\',"
                   "setting PA6 and PA7 to HIGH");
            // Set both IN1 and IN2 channels to be HIGH.
            pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES, 
                           0, PWM_POLARITY_INVERTED);
            pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                           0, PWM_POLARITY_INVERTED);
            IN1_S = 1;
            IN2_S = 1;
            break;
    }

}