#include <string.h>

#include "app_datalogger.h"
#include "app_data.h"
#include "FreeRTOS.h"
#include "task.h"
#include "drv_can.h"
#include <time.h>
#include <stdio.h>
#include "ff.h"

#define SYNC_INTERVAL 1000
#define DELAY_PERIOD 10

static struct {
	FATFS fs;
	FIL fp;
	bool file_opened;
	TickType_t last_sync;
	TickType_t last_write;
} app_datalogger_data = {0};


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
	TickType_t xLastWakeTime = xTaskGetTickCount();
	f_mount(&app_datalogger_data.fs, "", 0);
	while (1)
	{	
		if (app_datalogger_data.file_opened)
		{
			if ((xLastWakeTime - app_datalogger_data.last_sync) > SYNC_INTERVAL)
			{
				app_datalogger_data.last_sync = xLastWakeTime;
				if (f_sync(&app_datalogger_data.fp) != FR_OK) goto handle_error;
			}
			
			int fres = 0;
			struct app_data_message message;
			while (app_data_read_from_queue(&message))
			{
				// write a message
				fres = f_printf(&app_datalogger_data.fp, "%08d,%08x,%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
							 message.timestamp_ms,
							 message.id,
							 message.data[0],
							 message.data[1],
							 message.data[2],
							 message.data[3],
							 message.data[4],
							 message.data[5],
							 message.data[6],
							 message.data[7]);
				if (fres < 0) goto handle_error;
				app_datalogger_data.last_write = xLastWakeTime;
			}
		}
		else
		{
			if (open_file())
			{
				f_puts("time,id,data\r\n", &app_datalogger_data.fp);
			}
		}
		
		vTaskDelayUntil(&xLastWakeTime, DELAY_PERIOD);
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

bool app_datalogger_okay(void)
{
	return app_datalogger_data.file_opened && (xTaskGetTickCount() - app_datalogger_data.last_write) < 1000;
}