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
#define CLK_FREQ 170000000 // Clock frequency of timer 3
#define DRV8871_FREQ 90000000 // Desired frequency to drive the DRV8871.

#define PWM_PERIOD 1890 // In clock cycles at 5.88 nanoseconds per cycle.
#define PWM_DUTY_CYCLE 945 // In clock cycles at 5.88 nanoseconds per cycle.

/* Internal helper functions */
static uint32_t _calc_pwm_period_clk_cycles(uint32_t clk_freq, 
                                            uint32_t trgt_freq);
static uint16_t _calc_pwm_duty_clk_cycles(uint32_t period_clk_cycle,
                                          uint8_t on_percent);

/* Locally global variables */
static const uint32_t PWM_PERIOD_CLK_CYCLES = _calc_pwm_period_clk_cycles(
                                                CLK_FREQ, 
                                                DRV8871_FREQ);
static const uint16_t PWM_DUTY_CLK_CYCLES = _calc_pwm_period_clk_cycles(
                                                CLK_FREQ, 
                                                DRV8871_FREQ);


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
    Brief: Calculates the number of clock cycles for a PWM period.

    @param clk_freq: Frequency of timer.
    @param trgt_freq: Desired frequency of PWM output.

    @return The number of clock cycles for one PWM period. 
*/
static uint32_t _calc_pwm_period_clk_cycles(uint32_t clk_freq, 
                                            uint32_t trgt_freq){
    // Calculate the time of one clock cycle of the timer's frequency.
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
