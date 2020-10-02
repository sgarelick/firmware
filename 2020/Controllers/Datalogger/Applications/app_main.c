#include "app_main.h"
#include "drv_uart.h"
#include "drv_spi.h"

void app_init(void)
{
	
}

static uint8_t last_result;

void app_periodic(void)
{
//	drv_uart_send_message(DRV_UART_CHANNEL_TEST, "weed\r\n");
	last_result = drv_spi_transfer(DRV_SPI_CHANNEL_TEST, last_result);
}