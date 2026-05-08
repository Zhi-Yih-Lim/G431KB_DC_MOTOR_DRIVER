#include "fdcan.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/can.h>

// Const variables
static const uint32_t can_id = 0x101;
static const struct device *const fdcan_dev = DEVICE_DT_GET(DT_NODELABEL(fdcan1));

void fdcan_init(){
    // Check to see if CAN device is ready.
    if(!device_is_ready(fdcan_dev))
    {
        printk("Cannot find FDCAN device!\n");
        return;
    }
    else{
        printk("FDCAN device found\n");
    }
}