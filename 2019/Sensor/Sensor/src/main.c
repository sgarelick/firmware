/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# Minimal main function that starts with a call to system_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */
#include <asf.h>
#include "CANBus19.h"

static struct can_module can_instance;
struct adc_module adc_instance;
#define ADC_SAMPLES 1
uint16_t adc_result_buffer[ADC_SAMPLES];

static void configure_can(void)
{
	/* Set up the CAN TX/RX pins */
	struct system_pinmux_config pin_config;
	system_pinmux_get_config_defaults(&pin_config);
	pin_config.mux_position = CAN_TX_MUX_SETTING;
	system_pinmux_pin_set_config(CAN_TX_PIN, &pin_config);
	pin_config.mux_position = CAN_RX_MUX_SETTING;
	system_pinmux_pin_set_config(CAN_RX_PIN, &pin_config);
	
	/* Initialize the module. */
	struct can_config config_can;
	can_get_config_defaults(&config_can);
	can_init(&can_instance, CAN_MODULE, &config_can);
	can_start(&can_instance);
}

int buffer;
static void can_send_standard_message(uint32_t id_value, uint8_t *data, uint32_t data_length)
{
	uint32_t i;
	struct can_tx_element tx_element;
	can_get_tx_buffer_element_defaults(&tx_element);
	tx_element.T0.reg |= CAN_TX_ELEMENT_T0_STANDARD_ID(id_value);
	tx_element.T1.bit.DLC = data_length;
	for (i = 0; i < data_length; i++) {
		tx_element.data[i] = *data;
		data++;
	}
	if (++buffer >= CONF_CAN0_TX_BUFFER_NUM) buffer = 0;
	can_set_tx_buffer_element(&can_instance, &tx_element, buffer);
	can_tx_transfer_request(&can_instance, 1 << buffer);
}

volatile int sensor_id;
volatile int sensor_ready;
volatile uint16_t sensor_data[6];
void adc_complete_callback(struct adc_module *const module)
{
	uint16_t result;
	int res = adc_read(adc_module, &result);
	if (res == STATUS_OK) {
		sensor_data[sensor_id] = result;
	} else {
		sensor_data[sensor_id] = 0;
	}
	if (++sensor_id >= 6) {
		sensor_id = 0;
		sensor_ready = 1;
	}
}

void configure_adc(void)
{
	struct adc_config config_adc;
	adc_get_config_defaults(&config_adc);
	config_adc.clock_prescaler = ADC_CLOCK_PRESCALER_DIV8;
	config_adc.reference       = ADC_REFERENCE_INTVCC2;
	config_adc.positive_input  = ADC_POSITIVE_INPUT_PIN0;
	config_adc.resolution      = ADC_RESOLUTION_10BIT;
	adc_init(&adc_instance, ADC0, &config_adc);
	adc_enable(&adc_instance);
}

void configure_adc_callbacks(void)
{
	adc_register_callback(&adc_instance, adc_complete_callback, ADC_CALLBACK_READ_BUFFER);
	adc_enable_callback(&adc_instance, ADC_CALLBACK_READ_BUFFER);
}


int main (void)
{
	uint8_t data[8];
	system_init();
	
	configure_can();
	configure_adc();
	configure_adc_callbacks();

	/* Insert application code here, after the board has been initialized. */
	sensor_id = 0;
	sensor_data[0] = sensor_data[1] = sensor_data[2] = sensor_data[3] =	sensor_data[4] = sensor_data[5] = 0;
	sensor_ready = 0;
	adc_enable_positive_input_sequence(adc_module, 0x000000FE); // enable sequence of pins 0,1,4,5,6,7
	
	while(1) {
		while(sensor_ready) {
			INIT_DataLogger3(data);
			SET_DataLogger3_Analog1(data, sensor_data[0]);
			SET_DataLogger3_Analog2(data, sensor_data[1]);
			SET_DataLogger3_Analog3(data, sensor_data[2]);
			SET_DataLogger3_Analog4(data, sensor_data[3]);
			SET_DataLogger3_Analog5(data, sensor_data[4]);
			SET_DataLogger3_Analog6(data, sensor_data[5]);
			can_send_standard_message(ID_DataLogger3, data, 8);
			sensor_ready = 0;
		}
	}

}
