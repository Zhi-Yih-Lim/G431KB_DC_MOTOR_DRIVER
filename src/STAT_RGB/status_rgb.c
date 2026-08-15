#include "status_rgb.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>

#define STRIP_NUM_PIXELS 1
#define LED_BRIGHTNESS 10
#define RGB(_r,_g,_b) {.r =(_r), .g =(_g), .b =(_b)}
#define INTENSITY(_cval) ((int)(_cval*(LED_BRIGHTNESS/100.0f)))

// ============================================================================
// Predefined colors for each state
// ============================================================================

static const struct led_rgb state_colors[] = {
    RGB(INTENSITY(255), INTENSITY(255), INTENSITY(255)),// IDLE : White
    RGB(INTENSITY(0), INTENSITY(0), INTENSITY(255)),// Counter Reset : Blue
    RGB(INTENSITY(0), INTENSITY(255), INTENSITY(0)),// Moving : Green
    RGB(INTENSITY(127), INTENSITY(0), INTENSITY(255)),// Get Ticks : Purple
    RGB(INTENSITY(255), INTENSITY(0), INTENSITY(0)),// Error : Red
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