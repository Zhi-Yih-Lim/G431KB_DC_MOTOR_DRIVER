#include "uart.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uart, 3); // Info level

// Macros
#define TX_BUF_SIZE 16

// Local variables
static const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(usart2));
static uint8_t tx_buff[TX_BUF_SIZE];

int uart_init(){
    LOG_INF("uart_init()");

    if(!device_is_ready(uart_dev))
    {
        LOG_ERR("Cannot find USART2 device!\n");
        return 0;
    }
    else{
        LOG_INF("UART2 device found\n");
        return 1;
    }
}


int uart_send(const char* data, size_t data_len)
{
    if(data && data_len){
        // Send message out via DMA without allocating a timeout.
        int ret = uart_tx(uart_dev, tx_buff, data_len, SYS_FOREVER_US);

        if(ret != 0){
            LOG_ERR("uart_tx failed: %d", ret);
            return 0;
        }

        return 1;
    }
}