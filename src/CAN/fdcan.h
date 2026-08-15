#ifndef FDCAN_H
#define FDCAN_H

#include <zephyr/drivers/can.h> // Main needs to use "struct can_frame"

extern struct k_msgq can_rx_msgq;

int fdcan_init();
int fd_can_start();
int fd_can_send(const uint8_t *data_2_send, 
                char *data_type);

#endif