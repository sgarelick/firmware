#include "app_main.h"
#include "drv_uart.h"

void app_init(void)
{
	
}

void app_periodic(void)
{
	drv_uart_send_message(DRV_UART_CHANNEL_TEST, "weed\r\n");
}