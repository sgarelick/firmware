#pragma once
#include "drv_uart_private.h"
#include "drv_serial.h"
#include "sam.h"

void drv_uart_init(void);
void drv_uart_periodic(void);

void drv_uart_clear_response(enum drv_uart_channel channel);
const char * drv_uart_get_response_buffer(enum drv_uart_channel channel);

void drv_uart_send_message(enum drv_uart_channel channel, const char * msg);
void drv_uart_send_data(enum drv_uart_channel channel, const uint8_t * msg, unsigned length);


struct drv_uart_channelConfig {
	int sercom_id;
	sercom_usart_int_registers_t * module;
	unsigned rx_pinmux, tx_pinmux, rx_pad, tx_pad;
	unsigned baud;
};

struct drv_uart_config {
	const struct drv_uart_channelConfig * channelConfig;
};
extern const struct drv_uart_config drv_uart_config;