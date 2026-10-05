#ifndef FDCAN_H
#define FDCAN_H

#include <zephyr/drivers/can.h> // Main needs to use "struct can_frame"

struct sm; // Forward declaration.

int fdcan_init();
int fd_can_start();
int fd_can_begin_rx_processor(struct sm *sm_p);
int fd_can_send(const uint8_t *data_2_send, 
                char *data_type);
void fd_can_error_halt(); // TODO: Clear out all data in 'tx_data_arr', release all semaphores, clear out message tx and rx message queues, stop both threads.

#endif