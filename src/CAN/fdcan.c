#include "fdcan.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/can.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fdcan, 3); // Info level

// Const variables
static const uint32_t can_id = 0x101;
static const struct device *const fdcan_dev = DEVICE_DT_GET(DT_NODELABEL(fdcan1));
const struct can_filter rx_filter = {
    .flags = 0U, // Matches frames with 11-bit IDs.
    .id = 0x100, // Accepts data from CAN ID 0x100
    .mask = CAN_STD_ID_MASK // Bit mask for standard 11-bit id.
    //.mask = 0U
};

const struct can_frame frame = {
    .flags = CAN_FRAME_FDF|CAN_FRAME_BRS,
    .id = 0x123,
    .dlc = 5,
    .data = {1,7,3,1,4}
};

// Function forward declarations
void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data);
void tx_callback(const struct device *dev, int error, void *user_data);


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

    // Set up receiving filter and callback
    int filter_id;

    filter_id = can_add_rx_filter(fdcan_dev, can_rx_callback, NULL, &rx_filter);

    if(filter_id < 0){
        printk("fdcan_init -> Unable to add rx_filter.\r\n");
    }
    else{
        printk("fdcan_init -> rx_filter successfully added.\r\n");
    }
    
}

void fd_can_start(){
    int ret;

    struct can_timing data_timing = {
        .sjw = 1,
        .prop_seg = 0,
        .phase_seg1 = 6,
        .phase_seg2 = 3,
        .prescaler = 17
    };

    ret = can_set_mode(fdcan_dev, CAN_MODE_FD);

    if (ret != 0) {
		LOG_ERR("Error setting CAN mode [%d]", ret);
		return;
	}

    //ret = can_set_timing_data(fdcan_dev, &data_timing);

    //if (ret != 0){
    //    LOG_ERR("Error setting timing parameters for data [%d]", ret);
    //    return;
    //}

    ret = can_start(fdcan_dev);

    if(ret != 0){
       LOG_ERR("Error starting CAN Controller [%d]. \r\n", ret);
       return;
    }
    else{
        LOG_INF("Successfully started CAN controller.\r\n");
    }
}
/* Function definitions */

// Callback for receiving messages
void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data){
    printk("The sender's id is %d.\r\n", frame->id);
    printk("The data length code (DLC) is %d bytes.\r\n", frame->dlc);
    uint8_t data_length_bytes = frame->dlc;

    for(uint8_t c = 0; c < data_length_bytes; c++){
        LOG_INF("The received message at index %d is %d.", c, frame->data[c]);
    }
}

void fd_can_send(){
    int ret;
    
    ret = can_send(fdcan_dev, &frame, K_FOREVER, tx_callback, "Test Sender");

    if (ret != 0){
        printk("Cand message sending failed [%d].\r\n", ret);
    }
    else{
        printk("CAN successfully sent message\r\n");
    }

}


void tx_callback(const struct device *dev, int error, void *user_data){
    char *sender = (char *)user_data;

    if (error != 0){
        LOG_ERR("fdcan_txcallback -> Sending failed [%d]. Sender: %s\r\n", error, sender);
    }
}