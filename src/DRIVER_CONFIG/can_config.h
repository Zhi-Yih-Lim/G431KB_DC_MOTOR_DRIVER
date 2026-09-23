#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

#define CENTRAL_CAN_ID 0x64 // 100 decimal
#define LOCAL_CAN_ID 0x65 // 101 decimal

#define CAN_TID_SIZE 1 // Number of bytes for the 'Target ID' field in 'data'
#define CAN_ACTION_SIZE 2 // Number of bytes for the 'Action' command in 'data'
#define CAN_ACT_DATA_SIZE 4 // Number of bytes for the actual data field in 'data'
#define CAN_TICK_DATA_SIZE 4 // Number of bytes required to store the counter's ticks
#define CAN_DATA_SIZE 12 // Next largest DLC after 11 - Total data lenght so far.

#endif