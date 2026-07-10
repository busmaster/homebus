/*
 * board.h
 * 
 * Copyright 2026 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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
#define POWER_GOOD        ((PIND & 0b00000001) != 0)

#define BUS_TRANSCEIVER_POWER_DOWN   (PORTD |=  0b00100000)
#define BUS_TRANSCEIVER_POWER_UP     (PORTD &= ~0b00100000)
                       
/* Number of digital outputs */
#define NUM_DIGOUT   8

/* Number of shaders */
#define NUM_SHADER   4


/* Outputs */

#define DIGOUT_0_ON       (PORTB |=  0b01000000)
#define DIGOUT_0_OFF      (PORTB &= ~0b01000000)

#define DIGOUT_1_ON       (PORTD |=  0b10000000)
#define DIGOUT_1_OFF      (PORTD &= ~0b10000000)

#define DIGOUT_2_ON       (PORTF |=  0b00000001)
#define DIGOUT_2_OFF      (PORTF &= ~0b00000001)

#define DIGOUT_3_ON       (PORTE |=  0b01000000)
#define DIGOUT_3_OFF      (PORTE &= ~0b01000000)

#define DIGOUT_4_ON       (PORTF |=  0b00000010)
#define DIGOUT_4_OFF      (PORTF &= ~0b00000010)

#define DIGOUT_5_ON       (PORTC |=  0b01000000)
#define DIGOUT_5_OFF      (PORTC &= ~0b01000000)

#define DIGOUT_6_ON       (PORTF |=  0b01000000)
#define DIGOUT_6_OFF      (PORTF &= ~0b01000000)

#define DIGOUT_7_ON       (PORTC |=  0b10000000)
#define DIGOUT_7_OFF      (PORTC &= ~0b10000000)

#endif
