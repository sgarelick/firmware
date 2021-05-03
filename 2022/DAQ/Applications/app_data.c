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
#define MESSAGE_BUFFER_SIZE 32

// Raw CAN data
static struct {
	struct app_data_message buffer[MESSAGE_BUFFER_SIZE];
	struct drv_can_rx_fifo_0_element fifo_tmp;
	TickType_t tscv_epoch;
} app_data_data = {0};


static inline bool is_missing(int i)
{
	return (xTaskGetTickCount() - app_data_data.buffer[i].timestamp_ms) > MISSING_FOR;
}

bool app_data_is_missing(int frame_id)
{
	for (int i = 0; i < MESSAGE_BUFFER_SIZE && app_data_data.buffer[i].id != 0; ++i)
	{
		if (app_data_data.buffer[i].id == frame_id)
		{
			return is_missing(i);
		}
	}
	return true;
}

bool app_data_read_message(int frame_id, struct app_data_message * output)
{
	for (int i = 0; i < MESSAGE_BUFFER_SIZE && app_data_data.buffer[i].id != 0; ++i)
	{
		if (app_data_data.buffer[i].id == frame_id)
		{
			if (is_missing(i))
			{
				return false;
			}
			else
			{
				memcpy(output, &app_data_data.buffer[i], sizeof(struct app_data_message));
				return true;
			}
		}
	}
	return false;
}

bool app_data_read_buffer(int i, struct app_data_message * output)
{
	if (i < MESSAGE_BUFFER_SIZE && app_data_data.buffer[i].id != 0)
	{
		memcpy(output, &app_data_data.buffer[i], sizeof(struct app_data_message));
		return true;
	}
	return false;
}

static inline const struct app_data_message * insert_into_buffer()
{
	uint32_t id = app_data_data.fifo_tmp.RXF0E_0.bit.ID;
	if (!app_data_data.fifo_tmp.RXF0E_0.bit.XTD)
		id = (id >> 18) & 0x7FF;
	for (int i = 0; i < MESSAGE_BUFFER_SIZE; ++i)
	{
		if (app_data_data.buffer[i].id == id || app_data_data.buffer[i].id == 0)
		{
			app_data_data.buffer[i].id = id;
			if ((xTaskGetTickCount() - app_data_data.tscv_epoch) < 2000)
			{
				// Get highly accurate tick count
				app_data_data.buffer[i].timestamp_ms = app_data_data.tscv_epoch + (app_data_data.fifo_tmp.RXF0E_1.bit.RXTS * 4 / 125);
			}
			else
			{
				// No hope
				app_data_data.buffer[i].timestamp_ms = xTaskGetTickCount();
			}
			memcpy(app_data_data.buffer[i].data, (const uint8_t *)app_data_data.fifo_tmp.DB, 8);
			return &app_data_data.buffer[i];
		}
	}
	return NULL;
}

static inline void reset_tscv_epoch(void)
{
	if (xTaskGetTickCount() - app_data_data.tscv_epoch > 1000)
	{
		drv_can_reset_timestamp(CAN0_REGS);
		drv_can_reset_timestamp(CAN1_REGS);
		app_data_data.tscv_epoch = xTaskGetTickCount();
	}
}
const struct app_data_message * app_data_pop_fifo(void)
{
	const struct app_data_message * result;
	if (drv_can_pop_fifo_0(CAN0_REGS, &app_data_data.fifo_tmp))
	{
		if ((result = insert_into_buffer()) != NULL)
			return result;
	}
	if (drv_can_pop_fifo_0(CAN1_REGS, &app_data_data.fifo_tmp))
	{
		if ((result = insert_into_buffer()) != NULL)
			return result;
	}
	// No messages to read, might as well clean up
	reset_tscv_epoch();
	return NULL;
}