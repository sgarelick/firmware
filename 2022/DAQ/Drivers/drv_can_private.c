#include "drv_can.h"


static const struct drv_can_standard_filter standardFiltersVEHICLE[CAN0_STANDARD_FILTERS_NUM] =
{
	DRV_CAN_STD_FILTER(VEHICLE, SB_FRONT1_SIGNALS1),
	DRV_CAN_STD_FILTER(VEHICLE, SB_FRONT1_SIGNALS2),
};
static const struct drv_can_extended_filter extendedFiltersVEHICLE[CAN0_EXTENDED_FILTERS_NUM] =
{
	DRV_CAN_XTD_FILTER(VEHICLE, SB_FRONT1_SIGNALS1),
	DRV_CAN_XTD_FILTER(VEHICLE, SB_FRONT1_SIGNALS2),
};
static const struct drv_can_standard_filter standardFiltersPE3[CAN1_STANDARD_FILTERS_NUM];
static const struct drv_can_extended_filter extendedFiltersPE3[CAN1_EXTENDED_FILTERS_NUM] = 
{
	DRV_CAN_XTD_FILTER(PE3, PE01),
};


// Definitions for each transmit buffer
static const struct drv_can_tx_buffer_config transmitConfig[DRV_CAN_TX_BUFFER_COUNT] = {
};

const struct drv_can_config drv_can_config = {

	.standard_filters_can0 = standardFiltersVEHICLE,
	.extended_filters_can0 = extendedFiltersVEHICLE,
	.standard_filters_can1 = standardFiltersPE3,
	.extended_filters_can1 = extendedFiltersPE3,	.transmit_config = transmitConfig,
};
