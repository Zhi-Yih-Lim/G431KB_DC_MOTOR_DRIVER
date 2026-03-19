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
static const struct device *const qdec3_dev = DEVICE_DT_GET(QDEC3_NODE_ID);
static struct sensor_value angle; // 'sensor_value' struct to store angle

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
        printk("Cannot find QDEC3 device!\n");
        return;
    }
    else{
        printk("QDEC device found\n");
    }
}