#include <string.h>

#include "app_datalogger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "drv_can.h"

#define MISSING_FOR 1000

// Raw CAN data
static struct {
	uint8_t raw_data[DRV_CAN_RX_BUFFER_COUNT][8];
	bool valid[DRV_CAN_RX_BUFFER_COUNT];
	TickType_t last_updated[DRV_CAN_RX_BUFFER_COUNT];
} app_datalogger_data = {0};

static void copy_buffer(can_registers_t * bus, enum drv_can_rx_buffer_table id)
{
	if (drv_can_check_rx_buffer(bus, id))
	{
		const struct drv_can_rx_buffer_element * buf = drv_can_get_rx_buffer(id);
		memcpy(app_datalogger_data.raw_data[id], (const uint8_t *)buf->DB, 8); // not really volatile, protected by NDAT1
		drv_can_clear_rx_buffer(bus, id);
		app_datalogger_data.valid[id] = true;
		app_datalogger_data.last_updated[id] = xTaskGetTickCount();
	}
}

bool app_datalogger_is_missing(enum drv_can_rx_buffer_table id)
{
	return (xTaskGetTickCount() - app_datalogger_data.last_updated[id]) > MISSING_FOR;
}

const uint8_t * app_datalogger_read_double_buffer(enum drv_can_rx_buffer_table id)
{
	if (app_datalogger_is_missing(xTaskGetTickCount()))
	{
		return NULL;
	}
	return app_datalogger_data.raw_data[id];
}

static xTaskHandle DataloggerTaskID;
static void DataloggerTask(void* n)
{
	while (1)
	{
		copy_buffer(CAN0_REGS, DRV_CAN_RX_BUFFER_VEHICLE_SB_FRONT1_SIGNALS1);
		vTaskDelay(4);
	}
	vTaskDelete(NULL);
}

void app_datalogger_init(void)
{
	xTaskCreate(DataloggerTask, "DL", configMINIMAL_STACK_SIZE + 128, NULL, 2, &DataloggerTaskID);
}