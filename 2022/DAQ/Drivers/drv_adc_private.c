/*
 * drv_adc_private.c
 *
 * Created: 7/26/2020 7:08:29 PM
 *  Author: connor
 */ 
#include "drv_adc.h"
#include "sam.h"

static const struct drv_adc_channelConfig channelConfig[DRV_ADC_CHANNEL_COUNT] = {
};

struct drv_adc_config drv_adc_config = {
	.channelConfig = channelConfig,
};