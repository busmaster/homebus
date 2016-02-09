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

/* LED auf JP1 (nicht interruptsicher)*/
#define LED1_ON     PORTG &= ~0x10
#define LED1_OFF    PORTG |= 0x10
#define LED1_STATE  ((PORTG & 0x10) == 0)

/* LED auf JP2 (nicht interruptsicher)*/
#define LED2_ON     PORTG &= ~0x08
#define LED2_OFF    PORTG |= 0x08
#define LED2_STATE  ((PORTG & 0x08) == 0)

#define LED_ON      1
#define LED_OFF     0

#define NUM_LED     2

#define POWER_GOOD   ((PIND & 0b00000001) != 0)

#define BUS_TRANSCEIVER_POWER_DOWN   (PORTE |= 0b10000000)
#define BUS_TRANSCEIVER_POWER_UP     (PORTE &= ~0b10000000)
                       
/* Anzahl der digitalen Ausgänge */                       
#define NUM_DIGOUT   31


/* Ausgänge */

/* Port A-D wären Bit-adressierbar */

#define DIGOUT_0_ON       (PORTA |=  0b00000001)
#define DIGOUT_0_OFF      (PORTA &= ~0b00000001)

#define DIGOUT_1_ON       (PORTA |=  0b00000010)
#define DIGOUT_1_OFF      (PORTA &= ~0b00000010)

#define DIGOUT_2_ON       (PORTA |=  0b00000100)
#define DIGOUT_2_OFF      (PORTA &= ~0b00000100)

#define DIGOUT_3_ON       (PORTA |=  0b00001000)
#define DIGOUT_3_OFF      (PORTA &= ~0b00001000)

#define DIGOUT_4_ON       (PORTA |=  0b00010000)
#define DIGOUT_4_OFF      (PORTA &= ~0b00010000)

#define DIGOUT_5_ON       (PORTA |=  0b00100000)
#define DIGOUT_5_OFF      (PORTA &= ~0b00100000)

#define DIGOUT_6_ON       (PORTA |=  0b01000000)
#define DIGOUT_6_OFF      (PORTA &= ~0b01000000)

#define DIGOUT_7_ON       (PORTA |=  0b10000000)
#define DIGOUT_7_OFF      (PORTA &= ~0b10000000)

#define DIGOUT_8_ON       (PORTC |=  0b00000001)
#define DIGOUT_8_OFF      (PORTC &= ~0b00000001)

#define DIGOUT_9_ON       (PORTC |=  0b00000010)
#define DIGOUT_9_OFF      (PORTC &= ~0b00000010)

#define DIGOUT_10_ON       (PORTC |=  0b00000100)
#define DIGOUT_10_OFF      (PORTC &= ~0b00000100)

#define DIGOUT_11_ON       (PORTC |=  0b00001000)
#define DIGOUT_11_OFF      (PORTC &= ~0b00001000)

#define DIGOUT_12_ON       (PORTC |=  0b00010000)
#define DIGOUT_12_OFF      (PORTC &= ~0b00010000)

#define DIGOUT_13_ON       (PORTC |=  0b00100000)
#define DIGOUT_13_OFF      (PORTC &= ~0b00100000)

#define DIGOUT_14_ON       (PORTC |=  0b01000000)
#define DIGOUT_14_OFF      (PORTC &= ~0b01000000)

#define DIGOUT_15_ON       (PORTC |=  0b10000000)
#define DIGOUT_15_OFF      (PORTC &= ~0b10000000)

#define DIGOUT_16_ON       (PORTB |=  0b00010000)
#define DIGOUT_16_OFF      (PORTB &= ~0b00010000)

#define DIGOUT_17_ON       (PORTB |=  0b00100000)
#define DIGOUT_17_OFF      (PORTB &= ~0b00100000)

#define DIGOUT_18_ON       (PORTB |=  0b01000000)
#define DIGOUT_18_OFF      (PORTB &= ~0b01000000)

#define DIGOUT_19_ON       (PORTB |=  0b10000000)
#define DIGOUT_19_OFF      (PORTB &= ~0b10000000)

#define DIGOUT_20_ON       (PORTD |=  0b00010000)
#define DIGOUT_20_OFF      (PORTD &= ~0b00010000)

#define DIGOUT_21_ON       (PORTD |=  0b00100000)
#define DIGOUT_21_OFF      (PORTD &= ~0b00100000)

#define DIGOUT_22_ON       (PORTD |=  0b01000000)
#define DIGOUT_22_OFF      (PORTD &= ~0b01000000)

#define DIGOUT_23_ON       (PORTD |=  0b10000000)
#define DIGOUT_23_OFF      (PORTD &= ~0b10000000)

/* Port F ist nicht Bit-adressierbar */
#define DIGOUT_24_ON       (PORTF |=  0b00000001)
#define DIGOUT_24_OFF      (PORTF &= ~0b00000001)

#define DIGOUT_25_ON       (PORTF |=  0b00000010)
#define DIGOUT_25_OFF      (PORTF &= ~0b00000010)

#define DIGOUT_26_ON       (PORTF |=  0b00000100)
#define DIGOUT_26_OFF      (PORTF &= ~0b00000100)

#define DIGOUT_27_ON       (PORTF |=  0b00001000)
#define DIGOUT_27_OFF      (PORTF &= ~0b00001000)

#define DIGOUT_28_ON       (PORTF |=  0b00010000)
#define DIGOUT_28_OFF      (PORTF &= ~0b00010000)

#define DIGOUT_29_ON       (PORTF |=  0b00100000)
#define DIGOUT_29_OFF      (PORTF &= ~0b00100000)

#define DIGOUT_30_ON       (PORTF |=  0b01000000)
#define DIGOUT_30_OFF      (PORTF &= ~0b01000000)

/*-----------------------------------------------------------------------------
*  typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/

#endif
