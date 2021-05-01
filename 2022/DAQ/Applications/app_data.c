#include "app_data.h"
#include "FreeRTOS.h"
#include "task.h"
#include "drv_can.h"
#include "semphr.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

#define MISSING_FOR 1000
#define SYNC_INTERVAL 1000
#define DELAY_PERIOD 4



#define WRITE_QUEUE_SIZE 32

// Raw CAN data
static struct {
	struct app_data_message buffer[DRV_CAN_RX_BUFFER_COUNT];
	struct app_data_message write_queue[WRITE_QUEUE_SIZE];
	volatile int write_queue_wp, write_queue_rp;
	SemaphoreHandle_t mutex;
} app_data_data = {0};

static void copy_buffer(can_registers_t * bus, enum drv_can_rx_buffer_table id)
{
	if (drv_can_check_rx_buffer(bus, id))
	{
		const struct drv_can_rx_buffer_element * buf = drv_can_get_rx_buffer(id);
		app_data_data.buffer[id].id = buf->RXBE_0.bit.ID;
		app_data_data.buffer[id].timestamp_ms = xTaskGetTickCount();
		memcpy(app_data_data.buffer[id].data, (const uint8_t *)buf->DB, 8); // not really volatile, protected by NDAT1
		
		// add to write queue
		if ((app_data_data.write_queue_rp + 1) % WRITE_QUEUE_SIZE != app_data_data.write_queue_wp)
		{
			app_data_data.write_queue[app_data_data.write_queue_wp].id = buf->RXBE_0.bit.ID;
			app_data_data.write_queue[app_data_data.write_queue_wp].timestamp_ms = xTaskGetTickCount();
			memcpy(app_data_data.write_queue[app_data_data.write_queue_wp].data, (const uint8_t *)buf->DB, 8);
			
			app_data_data.write_queue_wp = (app_data_data.write_queue_wp + 1) % WRITE_QUEUE_SIZE;
		}
		
		drv_can_clear_rx_buffer(bus, id);
	}
}

bool app_data_is_missing(enum drv_can_rx_buffer_table id)
{
	bool result = false;
	if (xSemaphoreTake(app_data_data.mutex, 1))
	{
		result = (xTaskGetTickCount() - app_data_data.buffer[id].timestamp_ms) > MISSING_FOR;
		
		xSemaphoreGive(app_data_data.mutex);
	}
	return result;
}

bool app_data_read_buffer(enum drv_can_rx_buffer_table id, struct app_data_message * output)
{
	bool result = false;
	if (xSemaphoreTake(app_data_data.mutex, 1))
	{
		memcpy(output, &app_data_data.buffer[id], sizeof(struct app_data_message));
		result = (xTaskGetTickCount() - app_data_data.buffer[id].timestamp_ms) <= MISSING_FOR;
		
		xSemaphoreGive(app_data_data.mutex);
	}
	
	return result;
}

bool app_data_read_from_queue(struct app_data_message * output)
{
	bool result = false;
	//if (xSemaphoreTake(app_data_data.mutex, 1))
	{
		if (app_data_data.write_queue_rp != app_data_data.write_queue_wp)
		{
			// Copy output and advance RP
			memcpy(output, &app_data_data.write_queue[app_data_data.write_queue_rp], sizeof(struct app_data_message));
			app_data_data.write_queue_rp = (app_data_data.write_queue_rp + 1) % WRITE_QUEUE_SIZE;
			result = true;
		}
		//xSemaphoreGive(app_data_data.mutex);
	}
	
	return result;
}

static xTaskHandle DataTaskID;
static void DataTask(void* n)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	app_data_data.mutex = xSemaphoreCreateMutex();
	while (1)
	{
		if (xSemaphoreTake(app_data_data.mutex, DELAY_PERIOD))
		{
			copy_buffer(CAN0_REGS, DRV_CAN_RX_BUFFER_VEHICLE_SB_FRONT1_SIGNALS1);
			copy_buffer(CAN0_REGS, DRV_CAN_RX_BUFFER_VEHICLE_SB_FRONT1_SIGNALS2);
			
			xSemaphoreGive(app_data_data.mutex);
		}
		
		vTaskDelayUntil(&xLastWakeTime, DELAY_PERIOD);
	}
	vTaskDelete(NULL);
}

void app_data_init(void)
{
	xTaskCreate(DataTask, "DATA", configMINIMAL_STACK_SIZE + 128, NULL, 3, &DataTaskID);
}