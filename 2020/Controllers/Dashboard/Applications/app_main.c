#include "app_main.h"
#include "drv_i2c.h"

volatile int GearPosition, EngineSpeed;

void app_init(void)
{
	const uint8_t tx[] = {0x00, 0x00};
	drv_i2c_write_register(DRV_I2C_CHANNEL_EXPANDERS, 0x25, 0x00, tx, 2);
	
	GearPosition = 0;
	EngineSpeed = 1000;
}

void app_periodic(void)
{
	const uint8_t tx[] = {0x00, 0x66};
	drv_i2c_write_register(DRV_I2C_CHANNEL_EXPANDERS, 0x25, 0x12, tx, 2);
	
	if (++GearPosition > 4)
		GearPosition = 0;
	if (++EngineSpeed > 12000)
		EngineSpeed = 0;
}
