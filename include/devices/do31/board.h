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
 *****************************************************************************
 * ACHTUNG! Board-Designeaendert!
 *****************************************************************************
 */

#ifndef _BOARD_H
#define _BOARD_H

/*-----------------------------------------------------------------------------
*  Includes
*/
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
#define DIGOUT_0_STATE    ((PORTA &  0b00000001) != 0)
#define DIGOUT_0_TOGGLE   (DIGOUT_0_STATE ? DIGOUT_0_OFF : DIGOUT_0_ON)

#define DIGOUT_1_ON       (PORTA |=  0b00000010)
#define DIGOUT_1_OFF      (PORTA &= ~0b00000010)
#define DIGOUT_1_STATE    ((PORTA &  0b00000010) != 0)
#define DIGOUT_1_TOGGLE   (DIGOUT_1_STATE ? DIGOUT_1_OFF : DIGOUT_1_ON)

#define DIGOUT_2_ON       (PORTA |=  0b00000100)
#define DIGOUT_2_OFF      (PORTA &= ~0b00000100)
#define DIGOUT_2_STATE    ((PORTA &  0b00000100) != 0)
#define DIGOUT_2_TOGGLE   (DIGOUT_2_STATE ? DIGOUT_2_OFF : DIGOUT_2_ON)

#define DIGOUT_3_ON       (PORTA |=  0b00001000)
#define DIGOUT_3_OFF      (PORTA &= ~0b00001000)
#define DIGOUT_3_STATE    ((PORTA &  0b00001000) != 0)
#define DIGOUT_3_TOGGLE   (DIGOUT_3_STATE ? DIGOUT_3_OFF : DIGOUT_3_ON)

#define DIGOUT_4_ON       (PORTA |=  0b00010000)
#define DIGOUT_4_OFF      (PORTA &= ~0b00010000)
#define DIGOUT_4_STATE    ((PORTA &  0b00010000) != 0)
#define DIGOUT_4_TOGGLE   (DIGOUT_4_STATE ? DIGOUT_4_OFF : DIGOUT_4_ON)

#define DIGOUT_5_ON       (PORTA |=  0b00100000)
#define DIGOUT_5_OFF      (PORTA &= ~0b00100000)
#define DIGOUT_5_STATE    ((PORTA &  0b00100000) != 0)
#define DIGOUT_5_TOGGLE   (DIGOUT_5_STATE ? DIGOUT_5_OFF : DIGOUT_5_ON)

#define DIGOUT_6_ON       (PORTA |=  0b01000000)
#define DIGOUT_6_OFF      (PORTA &= ~0b01000000)
#define DIGOUT_6_STATE    ((PORTA &  0b01000000) != 0)
#define DIGOUT_6_TOGGLE   (DIGOUT_6_STATE ? DIGOUT_6_OFF : DIGOUT_6_ON)

#define DIGOUT_7_ON       (PORTA |=  0b10000000)
#define DIGOUT_7_OFF      (PORTA &= ~0b10000000)
#define DIGOUT_7_STATE    ((PORTA &  0b10000000) != 0)
#define DIGOUT_7_TOGGLE   (DIGOUT_7_STATE ? DIGOUT_7_OFF : DIGOUT_7_ON)

#define DIGOUT_8_ON       (PORTC |=  0b00100000)
#define DIGOUT_8_OFF      (PORTC &= ~0b00100000)
#define DIGOUT_8_STATE    ((PORTC &  0b00100000) != 0)
#define DIGOUT_8_TOGGLE   (DIGOUT_8_STATE ? DIGOUT_8_OFF : DIGOUT_8_ON)

#define DIGOUT_9_ON       (PORTC |=  0b00010000)
#define DIGOUT_9_OFF      (PORTC &= ~0b00010000)
#define DIGOUT_9_STATE    ((PORTC &  0b00010000) != 0)
#define DIGOUT_9_TOGGLE   (DIGOUT_9_STATE ? DIGOUT_9_OFF : DIGOUT_9_ON)

#define DIGOUT_10_ON       (PORTC |=  0b00001000)
#define DIGOUT_10_OFF      (PORTC &= ~0b00001000)
#define DIGOUT_10_STATE    ((PORTC &  0b00001000) != 0)
#define DIGOUT_10_TOGGLE   (DIGOUT_10_STATE ? DIGOUT_10_OFF : DIGOUT_10_ON)

#define DIGOUT_11_ON       (PORTC |=  0b00000100)
#define DIGOUT_11_OFF      (PORTC &= ~0b00000100)
#define DIGOUT_11_STATE    ((PORTC &  0b00000100) != 0)
#define DIGOUT_11_TOGGLE   (DIGOUT_11_STATE ? DIGOUT_11_OFF : DIGOUT_11_ON)

#define DIGOUT_12_ON       (PORTC |=  0b00000010)
#define DIGOUT_12_OFF      (PORTC &= ~0b00000010)
#define DIGOUT_12_STATE    ((PORTC &  0b00000010) != 0)
#define DIGOUT_12_TOGGLE   (DIGOUT_12_STATE ? DIGOUT_12_OFF : DIGOUT_12_ON)

#define DIGOUT_13_ON       (PORTC |=  0b00000001)
#define DIGOUT_13_OFF      (PORTC &= ~0b00000001)
#define DIGOUT_13_STATE    ((PORTC &  0b00000001) != 0)
#define DIGOUT_13_TOGGLE   (DIGOUT_13_STATE ? DIGOUT_13_OFF : DIGOUT_13_ON)

#define DIGOUT_14_ON       (PORTB |=  0b10000000)
#define DIGOUT_14_OFF      (PORTB &= ~0b10000000)
#define DIGOUT_14_STATE    ((PORTB &  0b10000000) != 0)
#define DIGOUT_14_TOGGLE   (DIGOUT_14_STATE ? DIGOUT_14_OFF : DIGOUT_14_ON)

#define DIGOUT_15_ON       (PORTB |=  0b01000000)
#define DIGOUT_15_OFF      (PORTB &= ~0b01000000)
#define DIGOUT_15_STATE    ((PORTB &  0b01000000) != 0)
#define DIGOUT_15_TOGGLE   (DIGOUT_15_STATE ? DIGOUT_15_OFF : DIGOUT_15_ON)

#define DIGOUT_16_ON       (PORTB |=  0b00100000)
#define DIGOUT_16_OFF      (PORTB &= ~0b00100000)
#define DIGOUT_16_STATE    ((PORTB &  0b00100000) != 0)
#define DIGOUT_16_TOGGLE   (DIGOUT_16_STATE ? DIGOUT_16_OFF : DIGOUT_16_ON)

#define DIGOUT_17_ON       (PORTB |=  0b00010000)
#define DIGOUT_17_OFF      (PORTB &= ~0b00010000)
#define DIGOUT_17_STATE    ((PORTB &  0b00010000) != 0)
#define DIGOUT_17_TOGGLE   (DIGOUT_17_STATE ? DIGOUT_17_OFF : DIGOUT_17_ON)

#define DIGOUT_18_ON       (PORTC |=  0b01000000)
#define DIGOUT_18_OFF      (PORTC &= ~0b01000000)
#define DIGOUT_18_STATE    ((PORTC &  0b01000000) != 0)
#define DIGOUT_18_TOGGLE   (DIGOUT_18_STATE ? DIGOUT_18_OFF : DIGOUT_18_ON)

#define DIGOUT_19_ON       (PORTC |=  0b10000000)
#define DIGOUT_19_OFF      (PORTC &= ~0b10000000)
#define DIGOUT_19_STATE    ((PORTC &  0b10000000) != 0)
#define DIGOUT_19_TOGGLE   (DIGOUT_19_STATE ? DIGOUT_19_OFF : DIGOUT_19_ON)

#define DIGOUT_20_ON       (PORTD |=  0b00010000)
#define DIGOUT_20_OFF      (PORTD &= ~0b00010000)
#define DIGOUT_20_STATE    ((PORTD &  0b00010000) != 0)
#define DIGOUT_20_TOGGLE   (DIGOUT_20_STATE ? DIGOUT_20_OFF : DIGOUT_20_ON)

#define DIGOUT_21_ON       (PORTD |=  0b00100000)
#define DIGOUT_21_OFF      (PORTD &= ~0b00100000)
#define DIGOUT_21_STATE    ((PORTD &  0b00100000) != 0)
#define DIGOUT_21_TOGGLE   (DIGOUT_21_STATE ? DIGOUT_21_OFF : DIGOUT_21_ON)

#define DIGOUT_22_ON       (PORTD |=  0b01000000)
#define DIGOUT_22_OFF      (PORTD &= ~0b01000000)
#define DIGOUT_22_STATE    ((PORTD &  0b01000000) != 0)
#define DIGOUT_22_TOGGLE   (DIGOUT_22_STATE ? DIGOUT_22_OFF : DIGOUT_22_ON)

#define DIGOUT_23_ON       (PORTD |=  0b10000000)
#define DIGOUT_23_OFF      (PORTD &= ~0b10000000)
#define DIGOUT_23_STATE    ((PORTD &  0b10000000) != 0)
#define DIGOUT_23_TOGGLE   (DIGOUT_23_STATE ? DIGOUT_23_OFF : DIGOUT_23_ON)

/* Port F ist nicht Bit-adressierbar */
#define DIGOUT_24_ON       (PORTF |=  0b00000001)
#define DIGOUT_24_OFF      (PORTF &= ~0b00000001)
#define DIGOUT_24_STATE    ((PORTF &  0b00000001) != 0)
#define DIGOUT_24_TOGGLE   (DIGOUT_24_STATE ? DIGOUT_24_OFF : DIGOUT_24_ON)  

#define DIGOUT_25_ON       (PORTF |=  0b00000010)
#define DIGOUT_25_OFF      (PORTF &= ~0b00000010)
#define DIGOUT_25_STATE    ((PORTF &  0b00000010) != 0)
#define DIGOUT_25_TOGGLE   (DIGOUT_25_STATE ? DIGOUT_25_OFF : DIGOUT_25_ON)  

#define DIGOUT_26_ON       (PORTF |=  0b00000100)
#define DIGOUT_26_OFF      (PORTF &= ~0b00000100)
#define DIGOUT_26_STATE    ((PORTF &  0b00000100) != 0)
#define DIGOUT_26_TOGGLE   (DIGOUT_26_STATE ? DIGOUT_26_OFF : DIGOUT_26_ON)  

#define DIGOUT_27_ON       (PORTF |=  0b00001000)
#define DIGOUT_27_OFF      (PORTF &= ~0b00001000)
#define DIGOUT_27_STATE    ((PORTF &  0b00001000) != 0)
#define DIGOUT_27_TOGGLE   (DIGOUT_27_STATE ? DIGOUT_27_OFF : DIGOUT_27_ON)  

#define DIGOUT_28_ON       (PORTF |=  0b00010000)
#define DIGOUT_28_OFF      (PORTF &= ~0b00010000)
#define DIGOUT_28_STATE    ((PORTF &  0b00010000) != 0)
#define DIGOUT_28_TOGGLE   (DIGOUT_28_STATE ? DIGOUT_28_OFF : DIGOUT_28_ON)  

#define DIGOUT_29_ON       (PORTF |=  0b00100000)
#define DIGOUT_29_OFF      (PORTF &= ~0b00100000)
#define DIGOUT_29_STATE    ((PORTF &  0b00100000) != 0)
#define DIGOUT_29_TOGGLE   (DIGOUT_29_STATE ? DIGOUT_29_OFF : DIGOUT_29_ON)  

#define DIGOUT_30_ON       (PORTF |=  0b01000000)
#define DIGOUT_30_OFF      (PORTF &= ~0b01000000)
#define DIGOUT_30_STATE    ((PORTF &  0b01000000) != 0)
#define DIGOUT_30_TOGGLE   (DIGOUT_30_STATE ? DIGOUT_30_OFF : DIGOUT_30_ON)  

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