/* 
 * File:   sevenseg.h
 * Author: bsiderowf
 *
 * Created on October 31, 2020, 12:56 PM
 */

#ifndef SEVENSEG_H
#define	SEVENSEG_H

void sevenseg_init(void);
void set_digit(uint8_t display_index, uint8_t digit);
void set_rpm(uint16_t rpm);
void set_gear(uint8_t gear);

#endif	/* SEVENSEG_H */

