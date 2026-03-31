#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include "qdec.h"

// ============================================================================
// MACROS
// ============================================================================
#define QDEC3_NODE_ID DT_NODELABEL(qdec3) // Node identifer for 'qdec3' node

// ============================================================================
// Locally global variables
// ============================================================================
// Get device pointer from node identifier
static const struct device * const qdec3_dev = DEVICE_DT_GET(QDEC3_NODE_ID);
static struct sensor_value angle; // 'sensor_value' struct to store angle
static int qdec_ready = 0;

// ============================================================================
// Function definitions
// ============================================================================

/*
    Brief: To be invoked 
*/
void qdec_init(){
    // Check to see if PWM device is ready.
    if(!device_is_ready(qdec3_dev))
    {
        printk("Cannot find QDEC3 device! \r\n");
        return;
    }
    else{
        printk("QDEC device found \r\n");
        qdec_ready = 1;
    }
}

/*
    Brief: To be called by qdec counter isr to read angle
*/
void qdec_read_angle(){
    if(qdec_ready){
       int ret;
       
       ret = sensor_sample_fetch(qdec3_dev);

       if(ret != 0){
            printk("qdec_read_angle -> Failed to fetch sample (%d) \r\n", ret);
       }

       ret = sensor_channel_get(qdec3_dev, SENSOR_CHAN_ROTATION, &angle);
       
       if(ret != 0){
            printk("qdec_read_angle -> Failed to get data (%d) \r\n", ret);
       }

       printk("qdec_read_angle -> The motor's current angular displacement "
              "is %d degrees. \r\n", angle.val1);
    }
    else{
        printk("qdec_read_angle -> QDEC device is not ready. \r\n");
    }

}