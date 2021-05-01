#include <string.h>

#include "app_datalogger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "drv_can.h"
#include <time.h>
#include <stdio.h>
#include "ff.h"

#define MISSING_FOR 1000
#define SYNC_INTERVAL 1000

struct app_datalogger_message {
	uint32_t id;
	TickType_t timestamp_ms;
	char data[8];
};

#define WRITE_QUEUE_SIZE 32

// Raw CAN data
static struct {
	uint8_t raw_data[DRV_CAN_RX_BUFFER_COUNT][8];
	bool valid[DRV_CAN_RX_BUFFER_COUNT];
	TickType_t last_updated[DRV_CAN_RX_BUFFER_COUNT];
	struct app_datalogger_message write_queue[WRITE_QUEUE_SIZE];
	int write_queue_wp, write_queue_rp;
	FATFS fs;
	FIL fp;
	bool file_opened;
	TickType_t last_sync;
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
		
		// add to write queue
		if ((app_datalogger_data.write_queue_rp + 1) % WRITE_QUEUE_SIZE != app_datalogger_data.write_queue_wp)
		{
			app_datalogger_data.write_queue[app_datalogger_data.write_queue_wp].id = buf->RXBE_0.bit.ID;
			app_datalogger_data.write_queue[app_datalogger_data.write_queue_wp].timestamp_ms = xTaskGetTickCount();
			memcpy(app_datalogger_data.write_queue[app_datalogger_data.write_queue_wp].data, (const uint8_t *)buf->DB, 8);
			
			app_datalogger_data.write_queue_wp = (app_datalogger_data.write_queue_wp + 1) % WRITE_QUEUE_SIZE;
		}
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

static bool open_file(void)
{
	int i;
	FRESULT fresult;
	char name[15];
	app_datalogger_data.file_opened = false;
	for (i = 1; i < 99999; ++i)
	{
		snprintf(name, 15, "log%05d.csv", i);
		fresult = f_open(&app_datalogger_data.fp, name, FA_WRITE | FA_CREATE_NEW);
		if (fresult == FR_OK)
		{
			app_datalogger_data.file_opened = true;
			return true;
		}
		else if (fresult != FR_EXIST)
		{
			break;
		}
	}
	return false;
}

static xTaskHandle DataloggerTaskID;
static void DataloggerTask(void* n)
{
	f_mount(&app_datalogger_data.fs, "", 0);
	while (1)
	{
		copy_buffer(CAN0_REGS, DRV_CAN_RX_BUFFER_VEHICLE_SB_FRONT1_SIGNALS1);
		
		if (app_datalogger_data.file_opened)
		{
			TickType_t now = xTaskGetTickCount();
			if ((now - app_datalogger_data.last_sync) > SYNC_INTERVAL)
			{
				app_datalogger_data.last_sync = now;
				if (f_sync(&app_datalogger_data.fp) != FR_OK) goto handle_error;
			}
			
			int fres = 0;
			while (app_datalogger_data.write_queue_rp != app_datalogger_data.write_queue_wp)
			{
				// write a message
				fres = f_printf(&app_datalogger_data.fp, "%08d,%08x,%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].timestamp_ms,
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].id,
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].data[0],
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].data[1],
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].data[2],
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].data[3],
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].data[4],
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].data[5],
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].data[6],
							 app_datalogger_data.write_queue[app_datalogger_data.write_queue_rp].data[7]
							 );
				
				app_datalogger_data.write_queue_rp = (app_datalogger_data.write_queue_rp + 1) % WRITE_QUEUE_SIZE;
			}
			if (fres < 0) goto handle_error;
		}
		else
		{
			if (open_file())
			{
				f_puts("time,id,data\r\n", &app_datalogger_data.fp);
			}
		}
		
		vTaskDelay(4);
		continue;
handle_error:
		app_datalogger_data.file_opened = false;
		f_close(&app_datalogger_data.fp);
	}
	vTaskDelete(NULL);
}

void app_datalogger_init(void)
{
	xTaskCreate(DataloggerTask, "DL", configMINIMAL_STACK_SIZE + 128, NULL, 2, &DataloggerTaskID);
}