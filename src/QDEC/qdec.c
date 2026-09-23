#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include "qdec.h"

LOG_MODULE_REGISTER(quadrature_encoder, 3); // Info level

// ============================================================================
// MACROS
// ============================================================================
#define QDEC3_NODE_ID DT_NODELABEL(qdec3) // Node identifer for 'qdec3' node

// ============================================================================
// Locally global variables
// ============================================================================
// Get device pointer from node identifier
static const struct device * const qdec3_dev = DEVICE_DT_GET(QDEC3_NODE_ID);
static int qdec_ready = 0;

// ============================================================================
// Function definitions
// ============================================================================

/*
    Brief: To be invoked 
*/
int qdec_init(){
    // Check to see if PWM device is ready.
    if(!device_is_ready(qdec3_dev))
    {
        LOG_ERR("Cannot find QDEC3 device.");
        return 0;
    }
    else{
        LOG_INF("QDEC device found.");
        qdec_ready = 1;
        return 1;
    }
}

/*
    Brief: Perform a simple angle readout.
*/
int qdec_read_angle(struct sensor_value * deg){
    if(qdec_ready){
       int ret;
       
       // Blocks until data from the requested sensor channel has been obtained
       // and stored into the driver instance's private data.
       ret = sensor_sample_fetch(qdec3_dev);

       if(ret != 0){
            LOG_ERR("Failed to sensor_sample_fetch. Error [%d].", ret);
            return 0;
       }

       // To obtain the most recently fetched channel data.
       // Supports either "SENSOR_CAN_ROTATION" to return the angular rotation 
       // in degrees.
       // Or "SENSOR_CHAN_ENCODER_COUNT" to return the raw quadrature decoder
       // counts, in raw counts.
       ret = sensor_channel_get(qdec3_dev, SENSOR_CHAN_ROTATION, deg);
       
       if(ret != 0){
            LOG_ERR("Failed to get data. Error [%d].", ret);
            return 0;
       }

       return 1;
    }
    else{
        LOG_ERR("QDEC device is not ready.");
        return 0;
    }
}