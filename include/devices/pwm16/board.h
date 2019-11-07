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
#define LED1_ON     PORTD &= ~0x40
#define LED1_OFF    PORTD |= 0x40
#define LED1_STATE  ((PORTD & 0x40) == 0)

/* LED auf JP2 (nicht interruptsicher)*/
#define LED2_ON     PORTD &= ~0x80
#define LED2_OFF    PORTD |= 0x80
#define LED2_STATE  ((PORTD & 0x80) == 0)

#define LED_ON      1
#define LED_OFF     0

#define NUM_LED     2

#define POWER_GOOD                   ((PIND & 0b00100000) != 0)

#define BUS_TRANSCEIVER_POWER_DOWN   (PORTD |= 0b00010000)
#define BUS_TRANSCEIVER_POWER_UP     (PORTD &= ~0b00010000)

#define PWM_ALLOUT_ENABLE       (PORTC &= ~0b00000100)
#define PWM_ALLOUT_DISABLE      (PORTC |= 0b00000100)

#endif
