#include "app_telemetry.h"
#include "app_data.h"
#include "FreeRTOS.h"
#include "task.h"
#include "drv_can.h"
#include "drv_lte.h"
#include <time.h>
#include <stdio.h>

#define DELAY_PERIOD 500
#define STR_SIZE (DRV_CAN_RX_BUFFER_COUNT * 27 + 1)
#define STACK_SIZE 512

static struct {
	struct app_data_message message;
	char str[STR_SIZE];
	StaticTask_t rtos_task_id;
	StackType_t  rtos_stack[STACK_SIZE];
} app_telemetry_data;

static void app_telemetry_task()
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
				if (app_data_read_buffer(id, &app_telemetry_data.message))
				{
					strpos += snprintf(app_telemetry_data.str+strpos, STR_SIZE-strpos, "%08lx=%02x%02x%02x%02x%02x%02x%02x%02x ",
								 app_telemetry_data.message.id,
								 app_telemetry_data.message.data[0],
								 app_telemetry_data.message.data[1],
								 app_telemetry_data.message.data[2],
								 app_telemetry_data.message.data[3],
								 app_telemetry_data.message.data[4],
								 app_telemetry_data.message.data[5],
								 app_telemetry_data.message.data[6],
								 app_telemetry_data.message.data[7]);
					//if (!drv_lte_mqtt_publish("CAN", str)) break;
					
				}
			}
			if (strpos>0) drv_lte_mqtt_publish("CAN", app_telemetry_data.str);
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
	xTaskCreateStatic(app_telemetry_task, "TEL", STACK_SIZE, NULL, 2, app_telemetry_data.rtos_stack, &app_telemetry_data.rtos_task_id);
}
