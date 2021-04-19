#include "app_display.h"
#include "app_inputs.h"
#include "app_datalogger.h"

void app_init(void)
{
	app_display_init();
	app_inputs_init();
	app_datalogger_init();
}
