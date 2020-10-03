#include "app_main.h"
#include "drv_uart.h"
#include "drv_spi.h"
#include "drv_i2c.h"

void app_init(void)
{
	
}

static uint8_t last_result;
static volatile uint8_t calenread[7];

void app_periodic(void)
{
//	drv_uart_send_message(DRV_UART_CHANNEL_TEST, "weed\r\n");
	last_result = drv_spi_transfer(DRV_SPI_CHANNEL_TEST, last_result);
	
	// start, write mode
	SERCOM3_REGS->I2CM.SERCOM_ADDR = SERCOM_I2CM_ADDR_ADDR((0x68 << 1) | 0);
	// wait
	while (!(SERCOM3_REGS->I2CM.SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_MB_Msk)) {}
	// can we talk?
	if (SERCOM3_REGS->I2CM.SERCOM_STATUS & (SERCOM_I2CM_STATUS_ARBLOST_Msk | SERCOM_I2CM_STATUS_RXNACK_Msk))
	{
		SERCOM3_REGS->I2CM.SERCOM_CTRLB = SERCOM_I2CM_CTRLB_CMD(3);
		return;
	}
	// write
	SERCOM3_REGS->I2CM.SERCOM_DATA = SERCOM_I2CM_DATA_DATA(0);
	// wait
	while (!(SERCOM3_REGS->I2CM.SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_MB_Msk)) {}
	// can we talk?
	if (SERCOM3_REGS->I2CM.SERCOM_STATUS & (SERCOM_I2CM_STATUS_ARBLOST_Msk | SERCOM_I2CM_STATUS_RXNACK_Msk))
	{
		SERCOM3_REGS->I2CM.SERCOM_CTRLB = SERCOM_I2CM_CTRLB_CMD(3);
		return;
	}
	// repeated start, read mode
	SERCOM3_REGS->I2CM.SERCOM_ADDR = SERCOM_I2CM_ADDR_ADDR((0x68 << 1) | 1);
	
	for (int i = 0; i < 7; ++i)
	{
		//wait
		while (!(SERCOM3_REGS->I2CM.SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_SB_Msk)) {}
		//read
		calenread[i] = SERCOM3_REGS->I2CM.SERCOM_DATA;
		if (i < 6)
		{
			// ask for more
			SERCOM3_REGS->I2CM.SERCOM_CTRLB = SERCOM_I2CM_CTRLB_CMD(2);
		}
		else
		{
			// no more
			SERCOM3_REGS->I2CM.SERCOM_CTRLB = SERCOM_I2CM_CTRLB_CMD(3) | SERCOM_I2CM_CTRLB_ACKACT(1);
		}
	}
	
	last_result = drv_spi_transfer(DRV_SPI_CHANNEL_TEST, last_result);
}