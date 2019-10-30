/*
 * pca9685.h
 *
 * Copyright 2013 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */
#ifndef _PCA9685_H
#define _PCA9685_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
*  Macros
*/

//Register defines from data sheet
#define PCA9685_PWM_CHANNELS 16
#define PCA9685_MODE1 0x00 // location for Mode1 register address
#define PCA9685_MODE2 0x01 // location for Mode2 reigster address
#define PCA9685_LED0  0x06 // location for start of LED0 registers
#define ALL_LED_ON_L  0xFA
#define ALL_LED_ON_H  0xFB
#define ALL_LED_OFF_L 0xFC
#define ALL_LED_OFF_H 0xFC
#define PRE_SCALE     0xFE

#define FRQ_1526HZ 0x03
#define FRQ_1221HZ 0x04
#define FRQ_1017Hz 0x05
#define FRQ_872HZ  0x06
#define FRQ_763Hz  0x07
#define FRQ_678Hz  0x08
#define FRQ_610Hz  0x09
#define FRQ_555Hz  0x1A
#define FRQ_509Hz  0x1B
#define FRQ_470Hz  0x1C
#define FRQ_436Hz  0x1D
#define FRQ_407Hz  0x1E
#define FRQ_381Hz  0x1F
#define FRQ_359Hz  0x20
#define FRQ_339Hz  0x21
#define FRQ_321Hz  0x22

#define DevPCA9685PW  0x80      // device address, see datasheet 0x40 << Lese/Schreibe bit

    void pca9685_init(void);
	void setPWMOn(uint8_t number);
	void setAllPWMOn(void);
	void setAllPWMOff(void);	
	void setPWMOff(uint8_t number);
	void setPWMDimmed(uint8_t number, uint16_t amount);
	void writePWM(uint8_t number, uint16_t pwmON, uint16_t pwmOFF);

	void writeRegister(uint8_t regAddress, uint8_t val);
	uint16_t readRegister(uint8_t  regAddress);

#ifdef __cplusplus
}
#endif
#endif
