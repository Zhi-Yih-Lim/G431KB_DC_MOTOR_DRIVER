#include "status_rgb.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>

#define STRIP_NUM_PIXELS 1
#define LED_BRIGHTNESS 20
#define RGB(_r,_g,_b) {.r =(_r), .g =(_g), .b =(_b)}

// ============================================================================
// Predefined colors for each state
// ============================================================================

static const struct led_rgb state_colors[] = {
    RGB(LED_BRIGHTNESS, LED_BRIGHTNESS, LED_BRIGHTNESS),// IDLE -> White
    RGB(0x00, 0x00, LED_BRIGHTNESS),// Counter reset -> Blue
    RGB(0x00, LED_BRIGHTNESS, 0x00),// Moving -> Green
    RGB(LED_BRIGHTNESS, 0x00, 0x00),// Error -> Red
};

LOG_MODULE_REGISTER(STATE_RGB, 3); // Info level
static const struct device *const state_rgb_dev = DEVICE_DT_GET(DT_NODELABEL(state_rgb));


// 'struct led_rgb' pointer for controling the single RGB
static struct led_rgb pixel;


int status_rgb_init()
{
    LOG_INF("status_rgb init");

    // Check to see if led_strip device is ready.
    if(!device_is_ready(state_rgb_dev))
    {
        LOG_ERR("Cannot find LED_STRIP device!\n");
        return 0;
    }
    else{
        LOG_INF("LED_STRIP device found\n");
    }

    return 1;

}


int set_status_rgb(sm_state_t state)
{
    if(device_is_ready(state_rgb_dev)){
        
        int ret;

        memcpy(&pixel, &state_colors[(int)state], sizeof(struct led_rgb));
        
        ret = led_strip_update_rgb(state_rgb_dev, &pixel, STRIP_NUM_PIXELS);

        if(ret){
            LOG_ERR("Failed to set state_rgb");
            return 0;
        }

        return 1;
    }
    else{
        LOG_ERR("State RGB device not ready.");
        return 0;
    }
}