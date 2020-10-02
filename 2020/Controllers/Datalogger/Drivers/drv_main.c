#include "drv_main.h"
#include "drv_uart.h"
#include "drv_spi.h"

void drv_init(void)
{
	drv_uart_init();
	drv_spi_init();
}

void drv_periodic(void)
{
	
}
