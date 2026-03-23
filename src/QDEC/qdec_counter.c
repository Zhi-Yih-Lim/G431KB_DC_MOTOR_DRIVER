#include <zephyr/kernel.h>
#include <zephyr/driver/counter.h> // To use Zephyr's counter interface
#include "qdec.h"

// Delay between qdec angle readout.
static qdec_readout_delay_us 500000; // 500 msec

// Counter callback (ISR), stops execution of all other threads
// - including main - and executes ISR.
// The signature of the ISR follows the "counter_top_callback_t"
// signature defined under "Zephyr API Documentation: Counter".
void counter_isr(const struct device *dev,
            void *user_data){

    // When the ISR is invoked, we want to add a work item 
    // to the work queue to readout the angular displacement
    // from the qdec.
                
}