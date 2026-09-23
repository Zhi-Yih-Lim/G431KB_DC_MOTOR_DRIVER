#include "fdcan.h"
#include "../DRIVER_CONFIG/can_config.h"
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fdcan, 3); // Info level

// ============================================================================
// Macros
// ============================================================================
#define CAN_RX_MSGQ_LEN 10
#define CAN_TX_FLAGS CAN_FRAME_FDF|CAN_FRAME_BRS

// Initialize the message queue
K_MSGQ_DEFINE(can_rx_msgq, sizeof(struct can_frame), CAN_RX_MSGQ_LEN, 1);

// Const variables
static const struct device *const fdcan_dev = DEVICE_DT_GET(DT_NODELABEL(fdcan1));
const struct can_filter rx_filter = {
    .flags = 0U, // Matches frames with 11-bit IDs.
    .id = CENTRAL_CAN_ID, // Accepts data from CAN ID 0x100
    .mask = CAN_STD_ID_MASK // Bit mask for standard 11-bit id.
    //.mask = 0U
};

static struct can_frame out_data = {
        .flags = CAN_TX_FLAGS,
        .id = LOCAL_CAN_ID,
};

// Function forward declarations
void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data);
void tx_callback(const struct device *dev, int error, void *user_data);

/*
    @Brief: Checks that the FDCAN device is ready and assigns an rx filter

    @param: None

    @return: 0 -> Failed ; 1 -> Success
*/

int fdcan_init(){
    LOG_INF("fdcan_init");
    // Check to see if CAN device is ready.
    if(!device_is_ready(fdcan_dev))
    {
        LOG_ERR("Cannot find FDCAN device!\n");
        return 0;
    }
    else{
        LOG_INF("FDCAN device found\n");
    }

    // Set up receiving filter and callback
    int filter_id;

    filter_id = can_add_rx_filter(fdcan_dev, can_rx_callback, NULL, &rx_filter);

    if(filter_id < 0){
        LOG_ERR("fdcan_init -> Unable to add rx_filter.\r\n");
        return 0;
    }
    else{
        LOG_INF("fdcan_init -> rx_filter successfully added.\r\n");
    }

    return 1;
    
}

int fd_can_start(){
    int ret;

    ret = can_set_mode(fdcan_dev, CAN_MODE_FD);

    if (ret != 0) {
		LOG_ERR("Error setting CAN mode [%d]", ret);
		return 0;
	}

    ret = can_start(fdcan_dev);

    if(ret != 0){
       LOG_ERR("Error starting CAN Controller [%d]. \r\n", ret);
       return 0;
    }
    else{
        LOG_INF("Successfully started CAN controller.\r\n");
    }

    return 1;
}

/* Function definitions */

// Callback for receiving messages (ISR context)
void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data){

    ARG_UNUSED(user_data);
    
    int ret = k_msgq_put(&can_rx_msgq, (void *)frame, K_NO_WAIT);

    LOG_INF("CAN MSG received");

    if(ret < 0){
        k_msgq_put(&can_rx_msgq, NULL, K_NO_WAIT); // Error condition.
    }
}

int fd_can_send(const uint8_t *data_2_send, 
                char *data_type){
    int ret;

    // Clear out memory contents between sends
    memset(out_data.data, 0, CAN_MAX_DLEN);

    // Set the data length
    out_data.dlc = can_bytes_to_dlc(CAN_DATA_SIZE),

    // Set contents to send out
    memcpy(out_data.data, data_2_send, CAN_DATA_SIZE);
    
    // Blocks until TX mailbox is assigned or error occured. Does not 
    // wait for acknowledgement of reception of outgoing message.
    ret = can_send(fdcan_dev, &out_data, K_FOREVER, tx_callback, data_type);

    if (ret != 0){
        LOG_ERR("Failed to send CAN message, Error [%d].", ret);
        return 0;
    }

    LOG_INF("CAN successfully sent message");

    return 1;
}


void tx_callback(const struct device *dev, int error, void *user_data){
    char *sender = (char *)user_data;

    if (error != 0){
        LOG_ERR("fdcan_txcallback -> Sending failed [%d]. Sender: %s\r\n", error, sender);
    }
}