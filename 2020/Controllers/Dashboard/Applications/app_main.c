#include "app_main.h"
#include "drv_i2c.h"

void app_init(void)
{
	const uint8_t tx[] = {0x00, 0x00};
	drv_i2c_write_register(DRV_I2C_CHANNEL_EXPANDERS, 0x25, 0x00, tx, 2);
}

void app_periodic(void)
{
	const uint8_t tx[] = {0x00, 0x66};
	drv_i2c_write_register(DRV_I2C_CHANNEL_EXPANDERS, 0x25, 0x12, tx, 2);
}
