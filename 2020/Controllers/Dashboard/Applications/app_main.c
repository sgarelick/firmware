#include "app_main.h"
#include "drv_i2c.h"
#include "sevenseg.h"
#include "FreeRTOS.h"
#include "task.h"

volatile int GearPosition, EngineSpeed;

void app_init(void)
{
	sevenseg_init();
	
	GearPosition = 0;
	EngineSpeed = 1000;
}

void app_periodic(void)
{
	//const uint8_t tx[] = {0x00, 0x66};
	//drv_i2c_write_register(DRV_I2C_CHANNEL_EXPANDERS, 0x25, 0x12, tx, 2);
	
    set_gear(GearPosition);
    set_rpm(EngineSpeed);
    set_rgb_one_digit(6, RGB_MAGENTA);
    
	unsigned time = xTaskGetTickCount();
	if (time % 1000 == 0)
	{
	if (++GearPosition > 4)
		GearPosition = 0;
	if (++EngineSpeed > 12000)
		EngineSpeed = 0;
	}
	
    set_gear(GearPosition);
    //set_rpm(EngineSpeed);
    
}
