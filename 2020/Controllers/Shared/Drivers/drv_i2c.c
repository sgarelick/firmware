#include "drv_i2c.h"

static void rx_handler(int sercom, sercom_registers_t * module);

void drv_i2c_init(void)
{
	for (enum drv_i2c_channel channel = (enum drv_i2c_channel)0U; channel < DRV_I2C_CHANNEL_COUNT; ++channel)
	{
		const struct drv_i2c_channelConfig * config = &drv_i2c_config.channelConfig[channel];
		
//		memset(&channelData[channel], 0, sizeof(struct drv_uart_channelData));
//		sercomToChannelMap[config->sercom_id] = channel;
//		
		// Enable the bus clock, peripheral clock, and interrupts for the chosen SERCOM#
		drv_serial_enable_sercom(config->sercom_id);
		
		// Register the interrupt handler, for interoperability with drv_spi and drv_i2c.
		drv_serial_register_handler(config->sercom_id, rx_handler);
		
		// Set up SDA, SCL pins.
		drv_serial_set_pinmux(config->sda_pinmux);
		drv_serial_set_pinmux(config->scl_pinmux);
		
		// Disable SERCOM
		config->module->SERCOM_CTRLA = SERCOM_I2CM_CTRLA_ENABLE(0);
		// Wait for synchronization
		while (config->module->SERCOM_SYNCBUSY) {}

		config->module->SERCOM_CTRLA =
				SERCOM_I2CM_CTRLA_MODE_I2C_MASTER | SERCOM_I2CM_CTRLA_INACTOUT_55US;
				
		config->module->SERCOM_BAUD = config->baud;
		
		config->module->SERCOM_CTRLA |= SERCOM_I2CM_CTRLA_ENABLE(1);
		while (config->module->SERCOM_SYNCBUSY) {}
	}
}

static void rx_handler(int sercom, sercom_registers_t * module)
{
}
