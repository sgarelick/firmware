#include "app_display.h"
#include "drv_i2c.h"
#include "app_inputs.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sam.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "drv_rtc.h"
#include "drv_lte.h"
#include "drv_can.h"
#include "pe3.h"
#include "app_data.h"
#include "app_datalogger.h"

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
	drv_i2c_write_register(DRV_I2C_CHANNEL_LED, address, 0x80 | 0xC, (const uint8_t *)&tx, 2);
}

static void display_gear(char c)
{
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_GEAR, SevenSegmentASCII[c - 0x20]);
}

static void display_rpm(const char *str, unsigned dpmask)
{
	int i, n;
	char c[4];
	n = strlen(str);
	for (i = 0; i < 4; ++i)
	{
		c[i] = n > i ? SevenSegmentASCII[str[i] - 0x20] : 0;
		c[i] |= (dpmask & (1 << i)) << (7-i);
	}
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_RPM_1, c[0]);
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_RPM_2, c[1]);
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_RPM_3, c[2]);
	tlc59108_set_output_state(APP_DISPLAY_CHANNEL_RPM_4, c[3]);
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
	
	drv_i2c_write_register(DRV_I2C_CHANNEL_LED, address, 0x80 | 0xC, (const uint8_t *)&tx, 2);
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

static void scroll_text(const char *s)
{
	int i, n;
	n = strlen(s);
	display_gear(' ');
	display_shift(0);
	display_warning(0);
	for (i = 0; i < n-3; ++i)
	{
		display_rpm(s + i, 0);
		vTaskDelay(200);
	}
	vTaskDelay(500);
}


#define INIT_DELAY 200

static xTaskHandle StatusTaskID;

enum display_mode
{
	DM_RPM,
	DM_MPH,
	DM_TIME,
	DM_DEBUG,
	
	DM_COUNT
};

#define DEBUG_LEN 128

static struct
{
	int eARBFront, eARBFrontPrevious;
	int eARBRear, eARBRearPrevious;
	int daqMode, daqModePrevious;
	int displayMode, displayModePrevious;
	bool eARBFrontChanged, eARBRearChanged, daqModeChanged, displayModeChanged;
	bool drs, drsPrevious, drsChanged;
	bool shiftUp, shiftUpPrevious, shiftUpChanged;
	bool shiftDown, shiftDownPrevious, shiftDownChanged;
	bool misc, miscPrevious, miscChanged;
	int rpm, temp;
	bool rpmValid, tempValid;
	union {
		struct {
			uint8_t daq:1;
			uint8_t telemetry:1;
			uint8_t datalogger:1;
			uint8_t fuse:1;
			uint8_t coolant:1;
		} bit;
		uint8_t reg;
	} warnings;
	char debug[DEBUG_LEN];
} app_display_data = {0};

static void read_inputs()
{
	strcpy(app_display_data.debug, " ");
	app_display_data.eARBFrontPrevious = app_display_data.eARBFront;
	app_display_data.eARBRearPrevious = app_display_data.eARBRear;
	app_display_data.daqModePrevious = app_display_data.daqMode;
	app_display_data.displayModePrevious = app_display_data.displayMode;
	app_display_data.drsPrevious = app_display_data.drs;
	app_display_data.shiftUpPrevious = app_display_data.shiftUp;
	app_display_data.shiftDownPrevious = app_display_data.shiftDown;
	app_display_data.miscPrevious = app_display_data.misc;
	
	app_display_data.eARBFront = app_inputs_get_dial(APP_INPUTS_SW1);
	app_display_data.eARBRear = app_inputs_get_dial(APP_INPUTS_SW2);
	app_display_data.daqMode = app_inputs_get_dial(APP_INPUTS_SW3);
	app_display_data.displayMode = app_inputs_get_dial(APP_INPUTS_SW4)-1;
	app_display_data.drs = (app_inputs_get_button(APP_INPUTS_DRS_L) || app_inputs_get_button(APP_INPUTS_DRS_R));
	app_display_data.shiftUp = app_inputs_get_button(APP_INPUTS_SHIFT_UP);
	app_display_data.shiftDown = app_inputs_get_button(APP_INPUTS_SHIFT_DOWN);
	app_display_data.misc = (app_inputs_get_button(APP_INPUTS_MISC_L) || app_inputs_get_button(APP_INPUTS_MISC_R));
	
	app_display_data.eARBFrontChanged = (app_display_data.eARBFront != app_display_data.eARBFrontPrevious);
	app_display_data.eARBRearChanged = (app_display_data.eARBRear != app_display_data.eARBRearPrevious);
	app_display_data.daqModeChanged = (app_display_data.daqMode != app_display_data.daqModePrevious);
	app_display_data.displayModeChanged = (app_display_data.displayMode != app_display_data.displayModePrevious);
	app_display_data.drsChanged = (app_display_data.drs != app_display_data.drsPrevious && app_display_data.drs);
	app_display_data.shiftUpChanged = (app_display_data.shiftUp != app_display_data.shiftUpPrevious && app_display_data.shiftUp);
	app_display_data.shiftDownChanged = (app_display_data.shiftDown != app_display_data.shiftDownPrevious && app_display_data.shiftDown);
	app_display_data.miscChanged = (app_display_data.misc != app_display_data.miscPrevious && app_display_data.misc);
	
	// Read engine speed
	struct app_data_message message;
	if (app_data_read_buffer(DRV_CAN_RX_BUFFER_PE3_PE01, &message))
	{
		struct pe3_pe01_t pe01;
		pe3_pe01_unpack(&pe01, message.data, 8);
		if (pe3_pe01_engine_speed_is_in_range(pe01.engine_speed))
		{
			app_display_data.rpm = pe01.engine_speed;
			app_display_data.rpmValid = true;
		}
		else
		{
			app_display_data.rpmValid = false;
		}
	}
	else
	{
		app_display_data.rpmValid = false;
		strcat(app_display_data.debug, "nO PE01 - ");
	}
	
	bool sb_front1_missing = app_data_is_missing(DRV_CAN_RX_BUFFER_VEHICLE_SB_FRONT1_SIGNALS1);
	if (sb_front1_missing)
	{
		strcat(app_display_data.debug, "nO SbF1 - ");
	}
	bool lte_missing = !drv_lte_is_logged_in();
	if (lte_missing)
	{
		strcat(app_display_data.debug, "LtE nr - ");
	}
#if 0 /* fuse indicator broken on PDM Rev0 */
	bool fuse_blown = app_inputs_get_button(APP_INPUTS_FUSE);
	if (fuse_blown)
	{
		strcat(app_display_data.debug, "FuSE bLOn - ");
	}
#endif
	bool datalogger_bad = !app_datalogger_okay();
	
	app_display_data.warnings.bit.daq = sb_front1_missing;
	app_display_data.warnings.bit.telemetry = lte_missing;
	app_display_data.warnings.bit.datalogger = datalogger_bad;
	app_display_data.warnings.bit.fuse = false;
	app_display_data.warnings.bit.coolant = false;
	
	strcat(app_display_data.debug, ".");
}

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
	display_rpm("InIt", 0);
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
	display_rpm("\x7F\x7F\x7F\x7F", 0xF);
	display_gear('\x7F');
	vTaskDelay(INIT_DELAY);
	
	app_display_data.warnings.reg = 0;
	int gear = 0;
	app_display_data.displayMode = DM_TIME;
	read_inputs();
	while (1)
	{
		read_inputs();
		// Check for changes in dial position and display value if so
		if (app_display_data.eARBFrontChanged)
		{
			char s[5] = {0};
			snprintf(s, 5, "%d", app_display_data.eARBFront);
			display_gear('F');
			display_rpm(s, 0);
			display_shift(0);
			display_warning(0);
			vTaskDelay(1000);
		}
		if (app_display_data.eARBRearChanged)
		{
			char s[5] = {0};
			snprintf(s, 5, "%d", app_display_data.eARBRear);
			display_gear('R');
			display_rpm(s, 0);
			display_shift(0);
			display_warning(0);
			vTaskDelay(1000);
		}
		if (app_display_data.daqModeChanged)
		{
			const char *s;
			switch (app_display_data.daqMode)
			{
			case 1:
				s = "ACCELErAtIOn";
				break;
			case 2:
				s = "AUtOCrOSS";
				break;
			case 3:
				s = "brAkES";
				break;
			}
			scroll_text(s);
		}
		if (app_display_data.displayModeChanged)
		{
			const char *s;
			switch (app_display_data.displayMode)
			{
			case DM_RPM:
				s = "rpMM";
				break;
			case DM_MPH:
				s = "MMPH";
				break;
			case DM_TIME:
				s = "TIME";
				break;
			case DM_DEBUG:
				s = "ErrS";
				break;
			}
			scroll_text(s);
		}
		if (app_display_data.drsChanged)
		{
			scroll_text("DrS");
		}
		if (app_display_data.miscChanged)
		{
			scroll_text("MISC");
		}
		if (app_display_data.shiftDownChanged)
		{
			scroll_text	("DOWN");
		}
		if (app_display_data.shiftUpChanged)
		{
			scroll_text	("UP");
		}
		// Update warnings
		if (app_display_data.displayMode == DM_RPM)
		{
			if (app_display_data.rpmValid)
			{
				set_rpm(app_display_data.rpm);
			}
			else
			{
				display_rpm("Err", 0);
				display_shift(0);
			}
			
			display_gear(gear > 0 ? (gear + '0') : 'n');
		}
		if (app_display_data.displayMode == DM_TIME)
		{
			struct tm * time = localtime(NULL);
			char timestr[5];
			if (time->tm_sec % 15 < 2)
			{
				strftime(timestr, 5, "%Y", time);
				display_rpm(timestr, 0);
				display_gear('y');
			}
			else if (time->tm_sec % 15 < 5)
			{
				strftime(timestr, 5, "%m%d", time);
				display_rpm(timestr, 0b0010);
				display_gear('d');
			}
			else if (time->tm_sec % 15 < 10)
			{
				strftime(timestr, 5, "%H%M", time);
				display_rpm(timestr, (time->tm_sec % 2 == 0) << 1);
				display_gear('t');
			}
			else
			{
				strftime(timestr, 5, "%S", time);
				snprintf(timestr + 2, 3, "%02d", drv_rtc_get_ms() / 10);
				display_rpm(timestr, 0b0010);
				display_gear('s');
			}
		}
		if (app_display_data.displayMode == DM_DEBUG)
		{
			scroll_text(app_display_data.debug);
		}
		
		
		
		display_warning(app_display_data.warnings.reg);
		vTaskDelay(50);
	}
	vTaskDelete(NULL);
}


void app_display_init(void)
{
	xTaskCreate(StatusTask, "STATUS", configMINIMAL_STACK_SIZE + 1000, NULL, 1, &StatusTaskID);
}
