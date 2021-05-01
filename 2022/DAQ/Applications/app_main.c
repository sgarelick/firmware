#include "app_display.h"
#include "app_inputs.h"
#include "app_datalogger.h"
#include "app_data.h"

void app_init(void)
{
	app_data_init();
	app_display_init();
	app_inputs_init();
	app_datalogger_init();
}
