/*
 * main.c
 * Initializes the RTOS and manages periodic tasks.
 *
 * Created: 7/11/2020 4:59:30 PM
 * Author : connor
 */


#include "sam.h"
#include "FreeRTOS.h"
#include "task.h"
#include "system_samc21.h"

#define DELAY_PERIOD 1U

void drv_init(void);
void app_init(void);


int main(void)
{
	SystemInit();

	// increase flash wait states from 0 to 1. if this is not done, the
	// CPU has a seizure trying to run at 48MHz
	NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_MANW(1) | NVMCTRL_CTRLB_RWS(1);
	// up speed to 48MHz
	OSCCTRL_REGS->OSCCTRL_OSC48MDIV = OSCCTRL_OSC48MDIV_DIV_DIV1;
	// wait for synchronization
	while (OSCCTRL_REGS->OSCCTRL_OSC48MSYNCBUSY)
	{
	}
	SystemCoreClock = 48000000;

	drv_init();
	app_init();

	vTaskStartScheduler();
	while (1);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask,
								   signed char *pcTaskName)
{
	(void)xTask;
	(void)pcTaskName;
	configASSERT(0);
}

__attribute__((naked))
void HardFault_Handler(void)
{
	configASSERT(0);
}