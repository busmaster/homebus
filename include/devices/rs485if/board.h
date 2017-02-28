/*
 * board.h
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
#ifndef _BOARD_H
#define _BOARD_H

#include <avr/io.h>

/*-----------------------------------------------------------------------------
*  Macros
*/                     

/* LED at JP1 */
#define LED1_ON     PORTA |= 0b10000000
#define LED1_OFF    PORTA &= ~0b10000000
#define LED1_STATE  ((PORTA & 0b10000000) != 0)

/* LED at JP2 */
#define LED2_ON     PORTA |= 0b01000000
#define LED2_OFF    PORTA &= ~0b01000000
#define LED2_STATE  ((PORTA & 0b01000000) != 0)

#define LED_ON      1
#define LED_OFF     0

#define NUM_LED     2

#define POWER_GOOD   ((PINC & 0b01000000) != 0)

#define BUS_TRANSCEIVER_POWER_DOWN   (PORTB |= 0b00000001)
#define BUS_TRANSCEIVER_POWER_UP     (PORTB &= ~0b00000001)
                       
/* number of outputs */
#define NUM_DIGOUT   1

/* outputs */
#define DIGOUT_0_ON       (PORTD |=  0b00010000)
#define DIGOUT_0_OFF      (PORTD &= ~0b00010000)

#endif
