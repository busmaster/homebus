/*
 * sio.c for ATmega32u4
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

#include <stdint.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "sysdef.h"
#include "sio.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
#undef BRX2

#if (F_CPU == 1843200UL)
#define BRREG    11 /* 9600 @ 1.8432MHz  */
#else
#error adjust baud rate settings for your CPU clock frequency
#endif

/*-----------------------------------------------------------------------------
*  init
*/
void SioInit(void) {

    unsigned int baud = BRREG;

    /* Set baud rate UART1: 9600 */
    UBRR1H = (unsigned char)(baud >> 8);
    UBRR1L = (unsigned char)baud;

    /* Set frame format: 8 data, 1 stop bit */
    /* Enable Receiver und Transmitter */
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
    UCSR1B = (1 << RXEN1) | (1 << TXEN1);
    UCSR1B &= ~(1 << UCSZ12);
}

/*-----------------------------------------------------------------------------
*  exit
*/
void SioExit(void) {

    /* set reset values, first turn off the uart*/
    UCSR1B = 0;
    UCSR1C = 0b00000110;
    UCSR1A = 1 << TXC1;

    UBRR1H = 0;
    UBRR1L = 0;
}

/*-----------------------------------------------------------------------------
*  Sio Sendepuffer schreiben
*/
void SioWrite(uint8_t ch) {

    /* Wait for empty transmit buffer */
    while ((UCSR1A & (1 << UDRE1)) == 0);

    UDR1 = ch;
}

/*-----------------------------------------------------------------------------
*  Sio Empfangspuffer lesen
*/
uint8_t SioRead(uint8_t *pCh) {

    if ((UCSR1A & (1 << RXC1)) != 0) {
        *pCh = UDR1;
        return 1;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------------
*  wait for transmit register empty
*/
void SioWriteReady(void) {

    /* Wait for transmit complete */
    while ((UCSR1A & (1 << TXC1)) == 0);
    UCSR1A |= (1 << TXC1);
}

/*-----------------------------------------------------------------------------
*  sio rx buf
*/
void SioReadFlush(void) {

    while (UCSR1A & (1 << RXC1)) {
        UDR1;
    }
}
