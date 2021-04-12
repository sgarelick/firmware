#include <drv_clock.h>
#include <drv_adc.h>
#include <drv_can.h>

// Warning: this is not running in an RTOS task
void drv_init(void)
{
	drv_clock_init();
	drv_adc_init();
	drv_can_init();
}