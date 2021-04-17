#include "app_inputs.h"
#include "drv_adc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sam.h"

static xTaskHandle InputsTaskID;

static const struct {
	struct app_inputs_config_digital {
		uint8_t group;
		uint8_t pin;
		uint32_t port;
		uint8_t pincfg;
	} digital[APP_INPUTS_DIGITAL_COUNT];
} app_inputs_config = {
	.digital = {
		[APP_INPUTS_DRS_L]		= { 0, PIN_PA00, PORT_PA00, PORT_PINCFG_INEN(1) | PORT_PINCFG_PULLEN(1) },
		[APP_INPUTS_DRS_R]		= { 0, PIN_PA01, PORT_PA01, PORT_PINCFG_INEN(1) | PORT_PINCFG_PULLEN(1) },
		[APP_INPUTS_FUSE]		= { 0, PIN_PA04, PORT_PA04, PORT_PINCFG_INEN(1) },
		[APP_INPUTS_MISC_L]		= { 0, PIN_PA27, PORT_PA27, PORT_PINCFG_INEN(1) | PORT_PINCFG_PULLEN(1) },
		[APP_INPUTS_MISC_R]		= { 0, PIN_PA28, PORT_PA28, PORT_PINCFG_INEN(1) | PORT_PINCFG_PULLEN(1) },
		[APP_INPUTS_SHIFT_DOWN]	= { 1, PIN_PB02, PORT_PB02, PORT_PINCFG_INEN(1) | PORT_PINCFG_PULLEN(1) },
		[APP_INPUTS_SHIFT_UP]	= { 1, PIN_PB03, PORT_PB03, PORT_PINCFG_INEN(1) | PORT_PINCFG_PULLEN(1) },
	},
};

static struct app_inputs_data
{
	struct drv_adc_results adc_results;
	struct {
		int position, last;
	} dials[NUM_DIALS];
	struct app_inputs_data_digital {
		bool active, last;
	} digital[APP_INPUTS_DIGITAL_COUNT];
} app_inputs_data = {0};

int app_inputs_get_dial(enum app_inputs_analog dial)
{
	return app_inputs_data.dials[dial].position;
}

bool app_inputs_get_button(enum app_inputs_digital button)
{
	return app_inputs_data.digital[button].active;
}

static int rotaryPosition(uint16_t counts)
{
	if (counts > 3909) return 1;
	if (counts > 3537) return 2;
	if (counts > 3164) return 3;
	if (counts > 2792) return 4;
	if (counts > 2419) return 5;
	if (counts > 2048) return 6;
	if (counts > 1675) return 7;
	if (counts > 1303) return 8;
	if (counts > 931) return 9;
	if (counts > 558) return 10;
	if (counts > 186) return 11;
	return 12;
}


static void InputsTask()
{
	// Initialize digital inputs
	for (enum app_inputs_digital channel = (enum app_inputs_digital)0U; channel < APP_INPUTS_DIGITAL_COUNT; ++channel)
	{
		const struct app_inputs_config_digital * config = &app_inputs_config.digital[channel];
		PORT_REGS->GROUP[config->group].PORT_PINCFG[config->pin & 0x1F] = config->pincfg;
		PORT_REGS->GROUP[config->group].PORT_OUTSET = config->port;
	}
	
	while (1)
	{
		// Reading of dials
		drv_adc_read_sequence_sync(&app_inputs_data.adc_results);
		for (int i = 0; i < NUM_DIALS; ++i)
		{
			int reading = rotaryPosition(app_inputs_data.adc_results.results[i]);
			if (reading != app_inputs_data.dials[i].position)
			{
				// Debouncing
				if (reading == app_inputs_data.dials[i].last)
				{
					app_inputs_data.dials[i].position = reading;
				}
			}
			app_inputs_data.dials[i].last = reading;
		}
		
		// Reading of digital inputs
		uint32_t din[2] = { // hit registers only once
			PORT_REGS->GROUP[0].PORT_IN,
			PORT_REGS->GROUP[1].PORT_IN,
		};
		for (enum app_inputs_digital channel = (enum app_inputs_digital)0U; channel < APP_INPUTS_DIGITAL_COUNT; ++channel)
		{
			const struct app_inputs_config_digital * config = &app_inputs_config.digital[channel];
			struct app_inputs_data_digital * data = &app_inputs_data.digital[channel];
			bool reading = !(din[config->group] & config->port);
			if (reading != data->active)
			{
				// Debouncing
				if (reading == data->last)
				{
					data->active = reading;
				}
				data->last = reading;
			}
		}
		vTaskDelay(50);
	}
	vTaskDelete(NULL);
}

void app_inputs_init(void)
{
	xTaskCreate(InputsTask, "INPUT", configMINIMAL_STACK_SIZE + 1000, NULL, 1, &InputsTaskID);
}
