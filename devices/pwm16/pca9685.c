/*
 * main.c
 *
 * Copyright 2019 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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

#include <avr/io.h>
#include <util/delay.h>

#include "sysdef.h" 
#include "pca9685.h"
#include "i2cmaster.h"
#include "led.h"

void pca9685_init(void) {

    i2c_start(0x00);				// software reset
    i2c_write(0x06);				// software reset
    i2c_stop();
	writeRegister(PCA9685_MODE1, 0x00);	// reset the device
	writeRegister(PCA9685_MODE1, 0xA0);// set up for auto increment 0b1010 0000
	writeRegister(PCA9685_MODE2, 0x04);	// set to output
	writeRegister(PRE_SCALE, FRQ_872HZ);
}

void setPWMOn(uint8_t number) {
	if (number < PCA9685_PWM_CHANNELS)	{	
	   i2c_start_wait(DevPCA9685PW+I2C_WRITE);
	   i2c_write(PCA9685_LED0 + 4*number);	   
	   i2c_write(0x00);
	   i2c_write(0x10);
	   i2c_write(0x00);
	   i2c_write(0x00);
	   i2c_stop(); 
	}
}

void setPWMOff(uint8_t number) {
	if (number < PCA9685_PWM_CHANNELS)	{	
       i2c_start_wait(DevPCA9685PW+I2C_WRITE);
	   i2c_write(PCA9685_LED0 + 4*number);	   
	   i2c_write(0x00);
	   i2c_write(0x00);
	   i2c_write(0x00);
	   i2c_write(0x10);
	   i2c_stop(); 	
	}
}

void setAllPWMOn(void) {
	i2c_start_wait(DevPCA9685PW+I2C_WRITE);
	i2c_write(ALL_LED_ON_L);	   
	i2c_write(0x00);
	i2c_write(0x10);
	i2c_write(0x00);
	i2c_write(0x00);
	i2c_stop();   
}

void setAllPWMOff(void) {
	i2c_start_wait(DevPCA9685PW+I2C_WRITE);
	i2c_write(ALL_LED_ON_L);	   
	i2c_write(0x00);
	i2c_write(0x00);
	i2c_write(0x00);
	i2c_write(0x10);
	i2c_stop(); 		
}

void setPWMDimmed(uint8_t number, uint16_t amount) {		// Amount from 0-4095 (off-on)
	if (amount == 0)	{
		setPWMOff(number);
	} else if (amount >= 4095) {
		setPWMOn(number);
	} else {
		writePWM(number, 0, (amount & 0xfff));
	}
}

void setAllPWMDimmed(uint16_t amount) {		// Amount from 0-4095 (off-on)
	if (amount == 0)	{
		setAllPWMOff();
	} else if (amount >= 4095) {
		setAllPWMOn();
	} else {
	   i2c_start_wait(DevPCA9685PW+I2C_WRITE);
	   i2c_write(ALL_LED_ON_L);	   
	   i2c_write(0x00);
	   i2c_write(0x00);
	   i2c_write((uint8_t)(amount & 0xFF));
	   i2c_write((uint8_t)((amount >> 8) & 0x0F));
	   i2c_stop();		
	}
}

void writePWM(uint8_t number, uint16_t pwmON, uint16_t pwmOFF) {	// LED_ON and LED_OFF are 12bit values (0-4095); ledNumber is 0-15
	if (number < PCA9685_PWM_CHANNELS)	{
	   i2c_start_wait(DevPCA9685PW+I2C_WRITE);
	   i2c_write(PCA9685_LED0 + 4*number);	   
	   i2c_write((uint8_t)(pwmON & 0xFF));
	   i2c_write((uint8_t)((pwmON >> 8) & 0x1F));
	   i2c_write((uint8_t)(pwmOFF & 0xFF));
	   i2c_write((uint8_t)((pwmOFF >> 8) & 0x1F));
	   i2c_stop();   
	}
}

void writeRegister(uint8_t regAddress, uint8_t data) {
	i2c_start_wait(DevPCA9685PW+I2C_WRITE);
	i2c_write(regAddress);
	i2c_write(data);
	i2c_stop();
}

uint16_t readRegister(uint8_t regAddress) {
	uint8_t ret;
	i2c_start_wait(DevPCA9685PW+I2C_WRITE);
	i2c_write(regAddress);
	i2c_rep_start(DevPCA9685PW+I2C_READ);
    ret = i2c_readNak();
    i2c_stop();
	return ret;
}