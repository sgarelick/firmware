/*
 * app_sensorRead.h
 *
 * Created: 7/26/2020 7:38:48 PM
 *  Author: connor
 */ 


#ifndef APP_SENSORREAD_H_
#define APP_SENSORREAD_H_

#include <stdint.h>
#include "drv_adc.h"

void app_sensorRead_init(void);
void app_sensorRead_periodic(void);

extern uint16_t sensorResults[DRV_ADC_SEQUENCE_COUNT];

#endif /* APP_SENSORREAD_H_ */