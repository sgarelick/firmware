#include "drv_main.h"
#include "drv_uart.h"
#include "drv_spi.h"
#include "drv_i2c.h"

void drv_init(void)
{
	drv_uart_init();
	drv_spi_init();
	drv_i2c_init();
}

void drv_periodic(void)
{
	
}
