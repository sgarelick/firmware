#include "app_display.h"
#include "drv_i2c.h"
#include "app_inputs.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sam.h"
#include <stdbool.h>
#include <string.h>

const uint8_t SevenSegmentASCII[96] = {
	0x00, /* (space) */
	0x86, /* ! */
	0x22, /* " */
	0x7E, /* # */
	0x6D, /* $ */
	0xD2, /* % */
	0x46, /* & */
	0x20, /* ' */
	0x29, /* ( */
	0x0B, /* ) */
	0x21, /* * */
	0x70, /* + */
	0x10, /* , */
	0x40, /* - */
	0x80, /* . */
	0x52, /* / */
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
	0x09, /* : */
	0x0D, /* ; */
	0x61, /* < */
	0x48, /* = */
	0x43, /* > */
	0xD3, /* ? */
	0x5F, /* @ */
	0x77, /* A */
	0x7C, /* B */
	0x39, /* C */
	0x5E, /* D */
	0x79, /* E */
	0x71, /* F */
	0x3D, /* G */
	0x76, /* H */
	0x30, /* I */
	0x1E, /* J */
	0x75, /* K */
	0x38, /* L */
	0x15, /* M */
	0x37, /* N */
	0x3F, /* O */
	0x73, /* P */
	0x6B, /* Q */
	0x33, /* R */
	0x6D, /* S */
	0x78, /* T */
	0x3E, /* U */
	0x3E, /* V */
	0x2A, /* W */
	0x76, /* X */
	0x6E, /* Y */
	0x5B, /* Z */
	0x39, /* [ */
	0x64, /* \ */
	0x0F, /* ] */
	0x23, /* ^ */
	0x08, /* _ */
	0x02, /* ` */
	0x5F, /* a */
	0x7C, /* b */
	0x58, /* c */
	0x5E, /* d */
	0x7B, /* e */
	0x71, /* f */
	0x6F, /* g */
	0x74, /* h */
	0x10, /* i */
	0x0C, /* j */
	0x75, /* k */
	0x30, /* l */
	0x14, /* m */
	0x54, /* n */
	0x5C, /* o */
	0x73, /* p */
	0x67, /* q */
	0x50, /* r */
	0x6D, /* s */
	0x78, /* t */
	0x1C, /* u */
	0x1C, /* v */
	0x14, /* w */
	0x76, /* x */
	0x6E, /* y */
	0x5B, /* z */
	0x46, /* { */
	0x30, /* | */
	0x70, /* } */
	0x01, /* ~ */
	0xFF, /* (del) */
};

enum app_display_channel
{
	APP_DISPLAY_CHANNEL_RPM_1 = 0x40,
	APP_DISPLAY_CHANNEL_RPM_2,
	APP_DISPLAY_CHANNEL_RPM_3,
	APP_DISPLAY_CHANNEL_RPM_4,
	APP_DISPLAY_CHANNEL_GEAR,
	APP_DISPLAY_CHANNEL_SHIFT_1,
	APP_DISPLAY_CHANNEL_SHIFT_2,
	APP_DISPLAY_CHANNEL_WARNING,
};

static uint16_t make_code(uint8_t code) {
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

static void tlc59108_set_output_state(uint8_t address, uint8_t state)
{
	uint16_t tx = make_code(state);
	drv_i2c_write_register(DRV_I2C_CHANNEL_LED, address, 0x80 | 0xC, &tx, 2);
}

static void display_gear(char c)
{
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_GEAR, SevenSegmentASCII[c - 0x20]);
}

static void display_rpm(const char *str)
{
	int n = strlen(str);
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_RPM_1, n > 0 ? SevenSegmentASCII[str[0] - 0x20] : 0);
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_RPM_2, n > 1 ? SevenSegmentASCII[str[1] - 0x20] : 0);
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_RPM_3, n > 2 ? SevenSegmentASCII[str[2] - 0x20] : 0);
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_RPM_4, n > 3 ? SevenSegmentASCII[str[3] - 0x20] : 0);
}

static void display_shift(unsigned bitmask)
{
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_SHIFT_1, bitmask & 0xFF);
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_SHIFT_2, (bitmask >> 8) & 0x7F);
}

static void display_warning(unsigned bitmask)
{
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_WARNING, bitmask & 0x1F);
}

static void set_digit(uint8_t display_index, uint8_t digit, bool dp) {
	if(digit > 9) {
		// Invalid digit
		return; 
	}
	
	uint8_t code = SevenSegmentASCII[digit + 0x10];
	uint8_t address = APP_DISPLAY_CHANNEL_RPM_1 + display_index;
	uint16_t tx = make_code(code);
	tx |= (dp << 14);
	
	drv_i2c_write_register(DRV_I2C_CHANNEL_LED, address, 0x80 | 0xC, &tx, 2);
}

static void set_shift_lights(int n)
{
	if (n < 0 || n > 15)
	{
		return;
	}
	unsigned bitmask = 0;
	for (int i = n; i > 0; --i)
	{
		bitmask |= (1 << (i-1));
	}
	display_shift(bitmask);
}

static void set_rpm(int rpm) {
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


#define INIT_DELAY 200

static xTaskHandle StatusTaskID;

static struct
{
	struct {
		int previousPosition, currentPosition;
	} dials[NUM_DIALS];
} app_display_data = {0};

static void StatusTask()
{
	// Toggle DISPLAY_RESET_L
	PORT_REGS->GROUP[0].PORT_DIRSET = PORT_PA21;
	PORT_REGS->GROUP[0].PORT_OUTCLR = PORT_PA21;
	vTaskDelay(1);
	PORT_REGS->GROUP[0].PORT_OUTSET = PORT_PA21;
	vTaskDelay(1);

	// set MODE1 for all TLC ICs
	uint8_t data[8];
	data[0] = 0x01;
	for (int i = 0; i < 8; ++i)
	{
		drv_i2c_write_register(DRV_I2C_CHANNEL_LED, 0x40 | i, 0, data, 1);
	}
	vTaskDelay(1);
	
	// Initialization visual / power on self test
	display_rpm("InIt");
	vTaskDelay(INIT_DELAY);
	display_shift(0x1F);
	vTaskDelay(INIT_DELAY);	
	display_shift(0x1F << 5);
	vTaskDelay(INIT_DELAY);	
	display_shift(0x1F << 10);
	vTaskDelay(INIT_DELAY);	
	display_shift(0);
	display_warning(0x1F);
	vTaskDelay(INIT_DELAY);
	display_warning(0);
	display_rpm("\x7F\x7F\x7F\x7F");
	display_gear('\x7F');
	vTaskDelay(INIT_DELAY);
	
	
	int rpm = 0;
	unsigned warning = 0;
	while (1)
	{
		// Check for changes in dial position and display value if so
		for (int i = 0; i < NUM_DIALS; ++i)
		{
			app_display_data.dials[i].currentPosition = app_inputs_get_dial(i);
			if (app_display_data.dials[i].currentPosition != app_display_data.dials[i].previousPosition
					&& app_display_data.dials[i].previousPosition != 0)
			{
				char s[5] = {0};
				snprintf(s, 5, "%d", app_display_data.dials[i].currentPosition);
				display_gear(i + '1');
				display_rpm(s);
				display_shift(0);
				display_warning(0);
				vTaskDelay(1000);
			}
			app_display_data.dials[i].previousPosition = app_display_data.dials[i].currentPosition;
		}
		
		set_rpm(rpm);
		display_gear('1');
		display_warning(warning);
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
