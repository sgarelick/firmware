#include "app_display.h"
#include "drv_i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sam.h"
#include <stdbool.h>

const uint8_t SevenSegmentDigits[10] = {
	0x3F, /* 0 */
	0x06, /* 1 */
	0x5B, /* 2 */
	0x4F, /* 3 */
	0x66, /* 4 */
	0x6D, /* 5 */
	0x7D, /* 6 */
	0x07, /* 7 */
	0x7F, /* 8 */
	0x6F, /* 9 */
};

const uint8_t DisplayAddresses[5] = {
	0x40,
	0x41,
	0x42,
	0x43,
	0x44
};

uint16_t make_code(uint8_t code) {
	uint16_t output = 0;
	// each LED takes 2 bits -- set to 01 if on, and 00 if off
	output |= code & 1;
	output |= (code & 0b10) << 1;
	output |= (code & 0b100) << 2;
	output |= (code & 0b1000) << 3;
	output |= (code & 0b10000) << 4;
	output |= (code & 0b100000) << 5;
	output |= (code & 0b1000000) << 6;
	output |= (code & 0b10000000) << 7;
	return output;
}

void set_digit(uint8_t display_index, uint8_t digit, bool dp) {
	if(digit > 9) {
		// Invalid digit
		return; 
	}
	
	uint8_t code = SevenSegmentDigits[digit];
	uint8_t address = DisplayAddresses[display_index];
	uint16_t tx = make_code(code);
	tx |= (dp << 14);
	
	drv_i2c_write_register(DRV_I2C_CHANNEL_LED, address, 0x80 | 0xC, &tx, 2);
}

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void set_shift_lights(int n)
{
	if (n < 0 || n > 15)
	{
		return;
	}
	
	uint16_t tx1 = 0;
	uint16_t tx2 = 0;
	for (int i = n; i > 0; --i)
	{
		if (i > 8)
		{
			tx2 |= (1 << 2*(i-9));
		}
		else
		{
			tx1 |= (1 << 2*(i-1));
		}
	}
	drv_i2c_write_register(DRV_I2C_CHANNEL_LED, 0x45, 0x80 | 0xC, &tx1, 2);
	drv_i2c_write_register(DRV_I2C_CHANNEL_LED, 0x46, 0x80 | 0xC, &tx2, 2);
}

void set_rpm(int rpm) {
	// Write the last four displays
	bool over10k = (rpm >= 10000);
	int rem = rpm;
	if (over10k)
	{
		rem /= 10;
	}
	for(int i = 0; i < 4; i++) {
		int digit = rem % 10;
		rem /= 10;
		bool dp = false;
		if ((over10k && i == 2) || (!over10k && i == 3))
		{
			dp = true;
		}
		set_digit(3-i, digit, dp);
	}
	
	// Write shift lights
	// <6400 -> 0
	// 6400 - 9000 -> 1 - 10
	// >9000 -> 15
	if (rpm < 6400)
	{
		// all off
		set_shift_lights(0);
	}
	else if (rpm >= 9000)
	{
		// all on
		set_shift_lights(15);
	}
	else
	{
		int n = (rpm - 6400 + 260) / 260;
		set_shift_lights(n);
	}
}

void set_gear(uint8_t gear) {
	set_digit(4, gear, false);
}

void set_warning(unsigned mask)
{
	uint16_t tx = make_code(mask);
	drv_i2c_write_register(DRV_I2C_CHANNEL_LED, 0x47, 0x80 | 0xC, &tx, 2);
}



static volatile int result1, result2;

xTaskHandle StatusTaskID;

static void StatusTask()
{

	PORT_REGS->GROUP[0].PORT_DIRSET = PORT_PA21;
	PORT_REGS->GROUP[0].PORT_OUTCLR = PORT_PA21;
	vTaskDelay(1);
	PORT_REGS->GROUP[0].PORT_OUTSET = PORT_PA21;
	vTaskDelay(1);

	uint8_t data[8];

	data[0] = 0x01;

	for (int i = 0; i < 8; ++i)
	{
		drv_i2c_write_register(DRV_I2C_CHANNEL_LED, 0x40 | i, 0, data, 1);
	}
	vTaskDelay(1);
	int rpm = 0;
	unsigned warning = 0;
	while (1)
	{
		set_rpm(rpm);
		set_gear(1);
		set_warning(warning);
		rpm += 10;
		if (rpm > 11000)
		{
			rpm = 0;
		}
		warning += 1;
		if (warning > 31)
		{
			warning = 0;
		}
		vTaskDelay(50);
	}
	vTaskDelete(NULL);
}


void app_display_init(void)
{
	xTaskCreate(StatusTask, "STATUS", configMINIMAL_STACK_SIZE + 1000, NULL, 1, &StatusTaskID);
}
