#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

// First, get the node id from the node label "pwm2",
// then get the node's full name from the node id of "pwm2".
#define PWM_Test DEVICE_DT_NAME(DT_NODELABEL(pwm2))

/* Macros for PWM control*/
// Period 


int main (void)
{
    const struct device *pwm_dev = NULL;
    pwm_dev = device_get_binding(PWM_Test);

    if(!pwm_dev)
    {
        printk("Cannot find PWM device!\n");
        return 0;
    }
    else{
        printk("PWM device found\n");
    }

    return 0;
}