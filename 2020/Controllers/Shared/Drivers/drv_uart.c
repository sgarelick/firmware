#include "drv_uart.h"
#include "drv_serial.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"

#define TXBUFLEN 100
#define RXBUFLEN 200
#define LINEBUFLEN 50
#define BAUD 115200

struct drv_uart_channelData {
	volatile char rxbuf[RXBUFLEN];
	volatile int rxbufi;
	char linebuf[LINEBUFLEN];
	char txbuf[TXBUFLEN];
	volatile char *txbufstart;
	char *txbufend;
	volatile TaskHandle_t txnotifytask;
};

static struct drv_uart_channelData channelData[DRV_UART_CHANNEL_COUNT];
static enum drv_uart_channel sercomToChannelMap[6];

static void rx_handler(int sercom, sercom_registers_t * module);

void drv_uart_init(void)
{
	for (enum drv_uart_channel channel = (enum drv_uart_channel)0U; channel < DRV_UART_CHANNEL_COUNT; ++channel)
	{
		const struct drv_uart_channelConfig * config = &drv_uart_config.channelConfig[channel];
		
		memset(&channelData[channel], 0, sizeof(struct drv_uart_channelData));
		sercomToChannelMap[config->sercom_id] = channel;
		
		// Enable the bus clock, peripheral clock, and interrupts for the chosen SERCOM#
		drv_serial_enable_sercom(config->sercom_id);
		
		// Register the interrupt handler, for interoperability with drv_spi and drv_i2c.
		drv_serial_register_handler(config->sercom_id, rx_handler);
		
		// Set up TX and RX pins.
		drv_serial_set_pinmux(config->tx_pinmux);
		drv_serial_set_pinmux(config->rx_pinmux);
		
		// Disable SERCOM
		config->module->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_ENABLE(0);
		// Wait for synchronization
		while (config->module->SERCOM_SYNCBUSY) {}

		// Default UART settings (async, no LIN, LSB first, no parity)
		config->module->SERCOM_CTRLA =
				SERCOM_USART_INT_CTRLA_DORD_LSB | SERCOM_USART_INT_CTRLA_CMODE_ASYNC |
				SERCOM_USART_INT_CTRLA_FORM_USART_FRAME_NO_PARITY | SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK |
				config->tx_pad | config->rx_pad;

		// Default UART settings (enable sending/receiving, 1 stop bit, 8 data bits)
		config->module->SERCOM_CTRLB =
				SERCOM_USART_INT_CTRLB_RXEN(1) | SERCOM_USART_INT_CTRLB_TXEN(1) |
				SERCOM_USART_INT_CTRLB_ENC(0) | SERCOM_USART_INT_CTRLB_SFDE(0) |
				SERCOM_USART_INT_CTRLB_SBMODE_1_BIT | SERCOM_USART_INT_CTRLB_CHSIZE_8_BIT;

		// Set baud rate
		config->module->SERCOM_BAUD = config->baud;  

		// Enable RX interrupt
		config->module->SERCOM_INTENSET = SERCOM_USART_INT_INTENSET_RXC(1) | SERCOM_USART_INT_INTENSET_TXC(1);

		// Start!
		config->module->SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE(1);
		while (config->module->SERCOM_SYNCBUSY) {}

	}
}

enum drv_uart_statusCode drv_uart_send_message(enum drv_uart_channel channel, const char * msg)
{
	return drv_uart_send_data(channel, (const uint8_t *)msg, strlen(msg));
}

enum drv_uart_statusCode drv_uart_send_data(enum drv_uart_channel channel, const uint8_t * msg, unsigned length)
{
	if (channel < DRV_UART_CHANNEL_COUNT)
	{
		struct drv_uart_channelData * data = &channelData[channel];
		sercom_usart_int_registers_t * module = drv_uart_config.channelConfig[channel].module;

		// Load data to be sent by interrupt routines
		memcpy(data->txbuf, msg, length);
		data->txbufstart = data->txbuf + 1;
		data->txbufend = data->txbuf + length;
		data->txnotifytask = xTaskGetCurrentTaskHandle();
		
		// Send the first byte to kick off the chain
		module->SERCOM_INTFLAG = 0xF;
		while (!(module->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk)) {};
		module->SERCOM_DATA = *msg;
		
		// Wait for chain to finish
		uint32_t retval = ulTaskNotifyTake(pdTRUE, 10);
		if (retval == 1)
		{
			return DRV_UART_SUCCESS;
		}
		else
		{
			return DRV_UART_TIMEOUT;
		}
	}
	else
	{
		return DRV_UART_ERROR;
	}
}


void drv_uart_clear_response(enum drv_uart_channel channel)
{
	if (channel < DRV_UART_CHANNEL_COUNT)
	{
		__disable_irq();
		channelData[channel].rxbufi = 0;
		memset((char *)channelData[channel].rxbuf, 0, RXBUFLEN);
		__enable_irq();
	}
}

const char * drv_uart_get_response_buffer(enum drv_uart_channel channel)
{
	if (channel < DRV_UART_CHANNEL_COUNT)
	{
		// locally, it's not volatile
		return (const char *)channelData[channel].rxbuf;
	}
	else
	{
		return NULL;
	}
}

const char * drv_uart_read_line(enum drv_uart_channel channel, int timeout)
{
	if (channel < DRV_UART_CHANNEL_COUNT)
	{
		struct drv_uart_channelData * data = &channelData[channel];
		const char * lend;
		
		while (!(lend = strstr((const char *)data->rxbuf, "\r")) && timeout > 0)
		{
			vTaskDelay(1);
			timeout--;
		}
		
		if (lend)
		{
			int length = lend - data->rxbuf;
			if (length > LINEBUFLEN)
				length = LINEBUFLEN;
			strncpy(data->linebuf, (const char *)data->rxbuf, length);
			return data->linebuf;
		}
	}
	return NULL;
}

static void rx_handler(int sercom, sercom_registers_t * module)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	struct drv_uart_channelData * data = &channelData[sercomToChannelMap[sercom]];
	
	// Check for incoming data
	if (module->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_RXC_Msk)
	{
		uint8_t incoming = module->USART_INT.SERCOM_DATA;
		if (data->rxbufi < RXBUFLEN)
		{
			data->rxbuf[data->rxbufi++] = incoming;
		}
	}
	
	// Check if we can send more data
	if (module->USART_INT.SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_TXC_Msk)
	{
		if (data->txbufstart < data->txbufend)
		{
			module->USART_INT.SERCOM_DATA = *(data->txbufstart++);
		}
		else if (data->txbufstart == data->txbufend)
		{
			module->USART_INT.SERCOM_INTFLAG = SERCOM_USART_INT_INTFLAG_TXC(1);
			vTaskNotifyGiveFromISR(data->txnotifytask, &xHigherPriorityTaskWoken);
			data->txnotifytask = NULL;
		}
	}
	
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

