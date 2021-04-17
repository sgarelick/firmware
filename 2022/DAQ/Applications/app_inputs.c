#include "app_inputs.h"
#include "drv_adc.h"
#include "FreeRTOS.h"
#include "task.h"

static xTaskHandle InputsTaskID;

static struct app_inputs_data
{
	struct drv_adc_results adc_results;
	struct {
		int position, last;
	} dials[NUM_DIALS];
} app_inputs_data = {0};

int app_inputs_get_dial(unsigned dial)
{
	if (dial < NUM_DIALS)
	{
		return app_inputs_data.dials[dial].position;
	}
	else
	{
		return 0;
	}
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
		vTaskDelay(50);
	}
	vTaskDelete(NULL);
}

void app_inputs_init(void)
{
	xTaskCreate(InputsTask, "INPUT", configMINIMAL_STACK_SIZE + 1000, NULL, 1, &InputsTaskID);
}
