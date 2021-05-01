#include "app_telemetry.h"
#include "app_data.h"
#include "FreeRTOS.h"
#include "task.h"
#include "drv_can.h"
#include "drv_lte.h"
#include <time.h>
#include <stdio.h>

#define DELAY_PERIOD 500
static struct app_data_message message;
#define STR_SIZE (DRV_CAN_RX_BUFFER_COUNT * 27 + 1)
static char str[STR_SIZE];

static xTaskHandle TelemetryTaskID;
static void TelemetryTask(void* n)
{
	while (!drv_lte_configure());
	
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while (1)
	{	
		if (drv_lte_is_logged_in())
		{
			// send data
			int strpos = 0;
			for (enum drv_can_rx_buffer_table id = (enum drv_can_rx_buffer_table)0U; id < DRV_CAN_RX_BUFFER_COUNT; ++id)
			{
				if (app_data_read_buffer(id, &message))
				{
					strpos += snprintf(str+strpos, STR_SIZE-strpos, "%08lx=%02x%02x%02x%02x%02x%02x%02x%02x ",
								 message.id,
								 message.data[0],
								 message.data[1],
								 message.data[2],
								 message.data[3],
								 message.data[4],
								 message.data[5],
								 message.data[6],
								 message.data[7]);
					//if (!drv_lte_mqtt_publish("CAN", str)) break;
					
				}
			}
			if (strpos>0) drv_lte_mqtt_publish("CAN", str);
		}
		else
		{
			if (drv_lte_is_network_registered())
			{
				drv_lte_mqtt_login();
			}
		}
		vTaskDelayUntil(&xLastWakeTime, DELAY_PERIOD);
	}
	vTaskDelete(NULL);
}

void app_telemetry_init(void)
{
	xTaskCreate(TelemetryTask, "TEL", configMINIMAL_STACK_SIZE + 256, NULL, 2, &TelemetryTaskID);
}
