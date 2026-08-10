#ifndef FDCAN_H
#define FDCAN_H

#include "../STATE_MACHINE/state_machine.h"
#include <zephyr/drivers/can.h> // Main needs to use "struct can_frame"

#define CAN_DATA_SIZE 8 // Number of bytes in a CAN data frame.
#define CAN_ACTION_SIZE 2 // Number of bytes for the 'Action' command in 'data'
#define CAN_TID_SIZE 1 // Number of bytes for the 'Target ID' field in 'data'
#define CAN_RAW_DATA_SIZE 5 // Number of bytes for the raw data field in 'data'

extern struct k_msgq can_rx_msgq;

int fdcan_init();
int fd_can_start();
void fd_can_send();

#endif