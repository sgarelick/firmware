#include "drv_uart_private.h"
#include "drv_uart.h"

static const struct drv_uart_channelConfig channels[DRV_UART_CHANNEL_COUNT] =
{
	[DRV_UART_CHANNEL_LTE] = {
		.sercom_id = 0,
		.module = &SERCOM0_REGS->USART_INT,
		.tx_pin = PIN_PA08,
		.tx_port = PORT_PA08,
		.tx_mux = MUX_PA08C_SERCOM0_PAD0,
		.tx_pad = SERCOM_USART_INT_CTRLA_TXPO_PAD0,
		.rx_pin = PIN_PA09,
		.rx_port = PORT_PA09,
		.rx_mux = MUX_PA09C_SERCOM0_PAD1,
		.rx_pad = SERCOM_USART_INT_CTRLA_RXPO_PAD1,
		.baud = UART_BAUD(115200)
	}
};

const struct drv_uart_config drv_uart_config = {
	.channelConfig = channels,
};