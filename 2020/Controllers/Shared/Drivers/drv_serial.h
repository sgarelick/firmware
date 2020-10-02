/* 
 * File:   drv_serial.h
 * Author: Connor
 *
 * Created on October 2, 2020, 10:59 AM
 */

#ifndef DRV_SERIAL_H
#define	DRV_SERIAL_H

#include <sam.h>

static inline void drv_serial_enable_sercom(int id)
{
	switch (id)
	{
	case 0:
		MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM0(1);
		GCLK_REGS->GCLK_PCHCTRL[19] = GCLK_PCHCTRL_CHEN(1) | GCLK_PCHCTRL_GEN_GCLK0;
		NVIC_EnableIRQ(SERCOM0_IRQn);
		break;
	case 1:
		MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM1(1);
		GCLK_REGS->GCLK_PCHCTRL[20] = GCLK_PCHCTRL_CHEN(1) | GCLK_PCHCTRL_GEN_GCLK0;
		NVIC_EnableIRQ(SERCOM1_IRQn);
		break;
	case 2:
		MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM2(1);
		GCLK_REGS->GCLK_PCHCTRL[21] = GCLK_PCHCTRL_CHEN(1) | GCLK_PCHCTRL_GEN_GCLK0;
		NVIC_EnableIRQ(SERCOM2_IRQn);
		break;
	case 3:
		MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM3(1);
		GCLK_REGS->GCLK_PCHCTRL[22] = GCLK_PCHCTRL_CHEN(1) | GCLK_PCHCTRL_GEN_GCLK0;
		NVIC_EnableIRQ(SERCOM3_IRQn);
		break;
	case 4:
		MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM4(1);
		GCLK_REGS->GCLK_PCHCTRL[23] = GCLK_PCHCTRL_CHEN(1) | GCLK_PCHCTRL_GEN_GCLK0;
		NVIC_EnableIRQ(SERCOM4_IRQn);
		break;
	case 5:
		MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM5(1);
		GCLK_REGS->GCLK_PCHCTRL[25] = GCLK_PCHCTRL_CHEN(1) | GCLK_PCHCTRL_GEN_GCLK0;
		NVIC_EnableIRQ(SERCOM5_IRQn);
		break;
	default:
		asm("bkpt #69");// halt
		break;
	}
}

typedef void (*drv_serial_handler_t)(int, sercom_usart_int_registers_t *);

void drv_serial_register_handler(int sercom, drv_serial_handler_t handler);

#endif	/* DRV_SERIAL_H */

