#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h> // To use Zephyr's counter interface
#include <zephyr/sys/printk.h>
#include "qdec.h"

// Flag to see if the qdec counter device has been properly set up.
// Failure prevents counter from starting.
static int qdec_counter_ready = 0;

#define QDEC_COUNTER_NODE_ID DT_NODELABEL(qdec_counter) // Node identifer for 
                                                        // 'qdec_counter' node


// Get device pointer from node identifier
static const struct device *qdec_cntr_dev = DEVICE_DT_GET(
                                                QDEC_COUNTER_NODE_ID);

// Counter callback (ISR), stops execution of all other threads
// - including main - and executes ISR.
// The signature of the ISR follows the "counter_top_callback_t"
// signature defined under "Zephyr API Documentation: Counter".
static void counter_isr(const struct device *dev,
                        void *user_data){
    // When the ISR is invoked, we want to add a work item 
    // to the work queue to readout the angular displacement
    // from the qdec.
    printk("counter_isr -> Add angular displacement acquisition"
           " to work queue.\n");                
}

void qdec_counter_init(uint32_t readout_period_us){

    int ret;

    // Check to see if timer device is ready
    if(!device_is_ready(qdec_cntr_dev))
    {
        printk("Cannot find QDEC Counter device! \n");
        return;
    }
    else{
        printk("QDEC Counter device found. \n");
    }

    // Initialize a 'struct counter_top_cfg'.
    struct counter_top_cfg counter_cfg = {
        .ticks = counter_us_to_ticks(qdec_cntr_dev, readout_period_us),
        .callback = counter_isr,
        .user_data = NULL, // TODO: Pass in reference to work queue ?
        .flags = 0 
    };

    // Initialize a top counter
    ret = counter_set_top_value(qdec_cntr_dev, &counter_cfg);

    if(ret < 0){
        printk("qdec_counter_init -> Error (%d): Failed to start counter\r\n", 
               ret);
        return;
    }

    qdec_counter_ready = 1;

    printk("qdec_counter_init -> "
           "Set counter top value of %d micro-seconds.\r\n",
           readout_period_us);
}

void start_qdec_counter(){
    int ret;

    if(qdec_counter_ready){
        ret = counter_start(qdec_cntr_dev);
        if(ret < 0){
            printk("start_counter -> Error (%d): Failed to start counter\r\n", 
                ret);
            return;
        }

        printk("start_counter -> Counter started\r\n");
    }    
    else{
        printk("start_counter -> Failed to start counter as "
               "QDEC Counter is not ready\r\n");
        return;
    }
}

void stop_qdec_counter(){
    counter_stop(qdec_cntr_dev);

    qdec_counter_ready = 0;
}