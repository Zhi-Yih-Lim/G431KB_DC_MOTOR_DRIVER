#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

#define CENTRAL_CAN_ID 0x64 // 100 decimal
#define LOCAL_CAN_ID 0x65 // 101 decimal

#define CAN_TID_SIZE 1 // Number of bytes for the 'Target ID' field in 'data'
#define CAN_ACTION_SIZE 2 // Number of bytes for the 'Action' command in 'data'
#define CAN_ACT_DATA_SIZE 4 // Number of bytes for the actual data field in 'data'
#define CAN_TICK_DATA_SIZE 4 // Number of bytes required to store the counter's ticks
#define CAN_DATA_SIZE 12 // Next largest DLC after 11 - Total data lenght so far.

// Macros for the TX and RX message queues
#define CAN_RX_MSGQ_LEN 10
#define CAN_TX_MSGQ_LEN 10

#define CAN_TX_FLAGS CAN_FRAME_FDF|CAN_FRAME_BRS

// Macros for the RX thread accepting CAN messages
#define CAN_RX_THREAD_STACK_SIZE 1024 // Static area allocated for thread that 
                                              // processes incoming CAN data.
#define CAN_RX_THREAD_PRIORITY 4 // Priority of thread processing incoming CAN
                                         // data.

// Macros for the TX thread sending out CAN messages
#define CAN_TX_THREAD_STACK_SIZE 1024 // Area to be allocated for thread that 
                                              // handles outgoing data.
#define CAN_TX_THREAD_PRIORITY 4 // Priority of thread handling outgoing data.

#endif