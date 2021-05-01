/* 
 * File:   app_data.h
 * Author: Connor
 *
 * Created on May 1, 2021, 1:35 PM
 */

#ifndef APP_DATA_H
#define	APP_DATA_H

#include <stdbool.h>
#include <stdint.h>
#include "drv_can.h"
#include "FreeRTOS.h"

struct app_data_message {
	uint32_t id;
	TickType_t timestamp_ms;
	uint8_t data[8];
};

void app_data_init(void);

bool app_data_is_missing(enum drv_can_rx_buffer_table id);
bool app_data_read_buffer(enum drv_can_rx_buffer_table id, struct app_data_message * output);
bool app_data_read_from_queue(struct app_data_message * output);


#endif	/* APP_DATA_H */

