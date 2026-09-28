#include "fdcan.h"
#include "../DRIVER_CONFIG/can_config.h"
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fdcan, 3); // Info level


// Initialize the tx & rx message queue
K_MSGQ_DEFINE(can_rx_msgq, sizeof(struct can_frame), CAN_RX_MSGQ_LEN, 1);
K_MSGQ_DEFINE(can_tx_msgq, sizeof(struct can_frame), CAN_TX_MSGQ_LEN, 1);

// Const variables
static const struct device *const fdcan_dev = DEVICE_DT_GET(DT_NODELABEL(fdcan1));
const struct can_filter rx_filter = {
    .flags = 0U, // Matches frames with 11-bit IDs.
    .id = CENTRAL_CAN_ID, // Accepts data from CAN ID 0x100
    .mask = CAN_STD_ID_MASK // Bit mask for standard 11-bit id.
    //.mask = 0U
};

// A statically global 'can_frame' to store outgoing data.
static struct can_frame out_data = {
        .flags = CAN_TX_FLAGS,
        .id = LOCAL_CAN_ID
};

// TX and RX threads related
K_THREAD_STACK_DEFINE(rx_stack_area, CAN_RX_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(tx_stack_area, CAN_TX_THREAD_STACK_SIZE);
static struct k_thread rx_thread, tx_thread;
static k_tid_t rx_tid, tx_tid;

// Function forward declarations
void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data);
void tx_callback(const struct device *dev, int error, void *user_data);
void _can_rx_process_entry_func(void *p1, void *p2, void *p3);

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

// Function that spawns the CAN RX processing thread after the state
// machine has been initialized.
int fd_can_begin_rx_processor(struct sm *sm_p)
{
    if(!sm_p){
        LOG_ERR("No state machine instance provided.");
        return 0;
    }

    rx_tid = k_thread_create(&rx_thread, rx_stack_area,
                             K_THREAD_STACK_SIZEOF(rx_stack_area),
                             _can_rx_process_entry_func,
                             sm_p, NULL, NULL,
                             CAN_RX_THREAD_PRIORITY,
                             0, K_NO_WAIT);

    LOG_INF("CAN RX thread started.");

    return 1;
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


// Internal functions
void _can_rx_process_entry_func(void *p1, void *p2, void *p3)
{
    struct sm *sm_p = (struct sm*)p1;

    // TODO: Change all references to state machine into sm_p

    int ret;
    static struct can_frame t_arr[1];// Assigning static space for pointer 
                                     // variable below
    struct can_frame *can_data_struct_p = t_arr;
    static struct event_struct event_s = {0};

    uint8_t target_id;
    char action_arr[CAN_ACTION_SIZE] = {0}; // Triggers C's zero filling rule
                                            // by providing an initialzer that
                                            // sets the first byte.
    char tick_arr[CAN_TICK_DATA_SIZE] = {0};

    uint32_t float_byteswap_in, float_byteswap_out; 
                                  // Used for temporarily storing byte-swapped 
                                  // float for angular velocity
    
    // Wait for messages on the can_rx's message queue.
    // not busy waiting.
    while(1){
        ret = k_msgq_get(&can_rx_msgq, can_data_struct_p, K_FOREVER);

        if(ret < 0){
            LOG_ERR("Error %d getting data from can_rx_msgq.", ret);
            event_s.event = SM_EVENT_ERROR;
            sm_post_event(state_machine_p, event_s);
        }
        else{
            if(can_dlc_to_bytes(can_data_struct_p->dlc) != CAN_DATA_SIZE){
                LOG_ERR("Incorrect data length received.");
                continue;
            }

            // CAN data packet format:
            // [Target ID (1-byte), Action (2-bytes), Actual Data (4-bytes),
            //  Counter-ticks (4-bytes)]
            target_id = can_data_struct_p->data[0];

            LOG_INF("Target ID is %d", target_id);

            // All motor drivers will receive messages from the main controller
            // 0x64. Check to see if message is intended for this specific 
            // driver. If "target_id" == 0xFF, payload is directed at all motor
            // drivers connected on the bus.
            if(target_id == 0xFF || target_id == LOCAL_CAN_ID){

                LOG_INF("Target ID within acceptance field.");

                // TODO: Analyze message and post events to 
                //       sm_post_event();
                memcpy(action_arr, can_data_struct_p->data+CAN_TID_SIZE, 
                       (size_t)CAN_ACTION_SIZE);

                LOG_INF("Action[0] is %c", action_arr[0]);
                LOG_INF("Action[1] is %c", action_arr[1]);

                if(!action_arr[0]){
                    LOG_ERR("Data in action array unrecognized.");
                    // Reset the action_arr
                    action_arr[0] = 0;
                    action_arr[1] = 0;
                    continue;
                }

                if(state_machine_p){
                    switch(action_arr[0]){
                        case 'S': // Stop command
                            event_s.event = SM_EVENT_STOP_MOTOR;
                            ret = sm_post_event(state_machine_p,
                                                event_s);
                                                
                            if(!ret){
                                LOG_ERR("Failed to post stop motor event \
                                        on to event queue.");

                                state_machine_p = NULL;
                            }
                            break;
                        case 'R': // Reset counter command 
                            event_s.event = SM_EVENT_COUNTER_RESET;
                            ret = sm_post_event(state_machine_p, 
                                                event_s);

                            if(!ret){
                                LOG_ERR("Failed to post counter reset event \
                                        on to event queue.");

                                state_machine_p = NULL;
                            }
                            break;
                        case 'M': // Move command
                            // TODO: Do not use static here. Pass-by-value instead.
                            static motor_data_t m_data ={
                                .trgt_ticks = 0,
                                .direction = 0,
                                .angular_vel = 0.0f
                            };

                            // Calclate the zero-indexed position of the first 
                            // byte of the actual data in 
                            // "can_data_struct_p->data"
                            static int actual_data_offset = CAN_TID_SIZE + 
                                                             CAN_ACTION_SIZE; 
                           
                            // First store the big-endianed incoming data as a 
                            // uint32_t variable, byte-swap to match the processors
                            // endianess and re-interpret the correctly swapped bits
                            // as float.
                            memcpy(&float_byteswap_in,
                                   (void *)(can_data_struct_p->data + 
                                            actual_data_offset),
                                   CAN_ACT_DATA_SIZE);

                            // Endianess byte-swap
                            float_byteswap_out = sys_be32_to_cpu(float_byteswap_in);

                            // The four-bytes of the actual data section of the
                            // incoming array represents float data.
                            m_data.angular_vel = 
                                UNALIGNED_GET((float *)&float_byteswap_out);

                            LOG_INF("The angular data is set to %f.", (double)m_data.angular_vel);
                            
                            // Calculate the zero-indexed position of the first
                            // byte of the tick data in 
                            // "can_data_struct_p->data" 
                            static int tick_offset = CAN_TID_SIZE + 
                                                     CAN_ACTION_SIZE + 
                                                     CAN_ACT_DATA_SIZE;
                                                 
                            // Copy over the target ticks from the incoming
                            // data array to tick_arr.
                            memcpy(tick_arr,
                                   (void *)(can_data_struct_p->data + 
                                            tick_offset),
                                   CAN_TICK_DATA_SIZE);

                            // Convert 4, 1-byte elements into a single variable 
                            // of size 32-bit.
                            m_data.trgt_ticks = 
                                sys_be32_to_cpu(UNALIGNED_GET((uint32_t*)tick_arr));
                            
                            LOG_INF("The target ticks is set to %u.", m_data.trgt_ticks);

                            switch(action_arr[1]){
                                case 'F': // Clockwise
                                    LOG_INF("Motor moving clockwise.");
                                    m_data.direction = 0;
                                    event_s.event = SM_EVENT_SCHED_MOVE;
                                    // (!) "m_data" passed by ref
                                    event_s.user_data = (void *)&m_data;
                                    ret = sm_post_event(state_machine_p, 
                                                        event_s);
                                    break;
                                case 'B': // Counter-clockwise
                                    LOG_INF("Motor moving counter-clockwise.");
                                    m_data.direction = 1;
                                    event_s.event = SM_EVENT_SCHED_MOVE;
                                    // (!) "m_data" passed by ref
                                    event_s.user_data = (void *)&m_data;
                                    ret = sm_post_event(state_machine_p, event_s);
                                    break;
                            }
                            break;
                        case 'G': // Get data from driver
                            switch(action_arr[1]){
                                case 'T': // Get counter ticks
                                    event_s.event = SM_EVENT_COUNTER_RESET;
                                    ret = sm_post_event(state_machine_p, event_s);
                                    
                                    if(!ret){
                                        LOG_ERR("Failed to post counter reset event on to \
                                                event queue.");

                                        state_machine_p = NULL;
                                    }
                                    break;
                            }
                            break;
                        case 'T': // Transmit data from driver
                            break;
                        default:
                            LOG_WRN("Unrecognized action command.");
                            break;
                    }
                }
                else{
                    LOG_ERR("No state machine instance.");
                }
            }
        }
    }    
}