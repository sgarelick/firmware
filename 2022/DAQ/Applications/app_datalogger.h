/* 
 * File:   app_datalogger.h
 * Author: Connor
 *
 * Created on April 18, 2021, 5:35 PM
 */

#ifndef APP_DATALOGGER_H
#define	APP_DATALOGGER_H

#include <stdbool.h>
#include <stdint.h>
#include "drv_can.h"

void app_datalogger_init(void);

bool app_datalogger_is_missing(enum drv_can_rx_buffer_table id);
const uint8_t * app_datalogger_read_double_buffer(enum drv_can_rx_buffer_table id);


#endif	/* APP_DATALOGGER_H */

