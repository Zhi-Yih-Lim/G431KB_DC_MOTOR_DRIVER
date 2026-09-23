#ifndef UART_H
#define UART_H

#include <stddef.h> // For "size_t" type

int uart_init();
int uart_send(const char* data, size_t data_len);

#endif