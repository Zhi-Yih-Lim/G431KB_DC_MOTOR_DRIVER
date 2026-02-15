#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <math.h>

// Setting node identifier for PWM
#define PWM3_NODE_ID DT_NODELABEL(pwm3)

// Get device pointer from node identifier
static const struct device *const pwm3_dev = DEVICE_DT_GET(PWM3_NODE_ID);

// ============================================================================
// Macros
// ============================================================================
static const uint32_t CLK_FREQ = 170000000; //Clock frequency of timer 3
static const uint32_t DRV8871_FREQ = 90000000; // Desired frequency to drive the DRV8871.

/* Internal helper functions */
static uint32_t _calc_pwm_period_clk_cycles(uint32_t clk_freq, 
                                            uint32_t trgt_freq);
static uint16_t _calc_pwm_duty_clk_cycles(uint32_t period_clk_cycle,
                                          uint8_t on_percent);
static void _init_pwm(void);

/* Locally global variables */
static uint32_t PWM_PERIOD_CLK_CYCLES;
static uint16_t PWM_DUTY_CLK_CYCLES;


int main (void)
{
    // Check to see if PWM device is ready.
    if(!device_is_ready(pwm3_dev))
    {
        printk("Cannot find PWM3 device!\n");
        return 0;
    }
    else{
        printk("PWM device found\n");
    }

    // Initialize PWM static variables
    _init_pwm();

    // Set PWM parameters
    if(!pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD_CLK_CYCLES, 
                       PWM_DUTY_CLK_CYCLES, PWM_POLARITY_NORMAL)){
        printk("Set IN1 pwm without error.\n");
    }
    else{
        printk("Failed to set PWM cycles for IN1\n");
        return 0;
    }

    if(!pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD_CLK_CYCLES,
                       PWM_DUTY_CLK_CYCLES, PWM_POLARITY_INVERTED)){
        printk("Set IN2 without error.\n");
    }
    else{
        printk("Failed to set PWM cycles for IN2\n");
        return 0;
    }

    return 0;
}

// ============================================================================
// Function Definitions
// ============================================================================
/*
    Brief: Assign the static global variables their appropriate values.
*/
static void _init_pwm(void){

    PWM_PERIOD_CLK_CYCLES = _calc_pwm_period_clk_cycles(CLK_FREQ, 
                                                        DRV8871_FREQ);
    PWM_DUTY_CLK_CYCLES = _calc_pwm_duty_clk_cycles(PWM_PERIOD_CLK_CYCLES, 
                                                    50); 

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
    float timer_clk_cycle_sec = 1.0/clk_freq;

    // Calculate the time for one period of the deisred PWM output frequency.
    float pwm_period_sec = 1.0/trgt_freq;

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
