#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

// Setting node identifier for PWM
#define PWM3_NODE_ID DT_NODELABEL(pwm3)

// Get device pointer from node identifier
static const struct device *const pwm3_dev = DEVICE_DT_GET(PWM3_NODE_ID); // Static to restrict visiblity locally.

/* Macros for PWM control*/
#define PWM_PERIOD 1890 // In clock cycles at 5.88 nanoseconds per cycle.
#define PWM_DUTY_CYCLE 945 // In clock cycles at 5.88 nanoseconds per cycle.


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
    if(!pwm_set_cycles(pwm3_dev, 1, PWM_PERIOD, PWM_DUTY_CYCLE, PWM_POLARITY_NORMAL)){
        printk("Set IN1 pwm without error.\n");
    }
    else{
        printk("Failed to set PWM cycles for IN1\n");
        return 0;
    }

    if(!pwm_set_cycles(pwm3_dev, 2, PWM_PERIOD, PWM_DUTY_CYCLE, PWM_POLARITY_INVERTED)){
        printk("Set IN2 without error.\n");
    }
    else{
        printk("Failed to set PWM cycles for IN2\n");
        return 0;
    }

    return 0;
}