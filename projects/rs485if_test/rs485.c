/*
 * siotype1.c for ATmega88
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

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "sysdef.h"
#include "rs485.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
#define SIO_TX_BUF_SIZE 50

#define BR_1ST_CHAR     22 /* 100 kBaud for 1st char */
#define BR_250K         8  /* 256 kBaud              */

#define TX_ENABLE       (PORTC |= 0b00001000)
#define TX_DISABLE      (PORTC &= ~0b00001000)

/*-----------------------------------------------------------------------------
*  Variables
*/
static uint8_t                        sTxBuffer[SIO_TX_BUF_SIZE];
static uint8_t                        sTxBufWrIdx;
static uint8_t                        sTxBufRdIdx;

/*-----------------------------------------------------------------------------
*  open sio channel
*/
void Rs485Init(void) {

    UBRR1H = 0; 

    UCSR1A = (1 << TXC1) | (1 << U2X1);
    UDR1;
 
    /* 8 bit, no par, 2 stop */
    UCSR1C = (1 << USBS1) | (1 << UCSZ11) | (1 << UCSZ10);
    UCSR1B = (1 << TXEN1);

    sTxBufWrIdx = 0;
    sTxBufRdIdx = 0;
}

/*-----------------------------------------------------------------------------
*  write to sio channel tx buffer
*/
uint8_t Rs485Write(uint8_t *pBuf, uint8_t bufSize) {

    if ((bufSize == 0) || (bufSize > sizeof(sTxBuffer))) {
        return 0;
    }
    if (sTxBufRdIdx != 0) {
        /* write in progress */
        return 0;
    }

    sTxBufRdIdx = 0;
    sTxBufWrIdx = bufSize;
    memcpy(sTxBuffer, pBuf, bufSize);
 
    TX_ENABLE;
    UBRR1L = BR_1ST_CHAR;
    UCSR1B |= (1 << UDRIE1);

    return bufSize;
}

/*-----------------------------------------------------------------------------
*  USART tx interrupt (data register empty)
*/
ISR(USART1_UDRE_vect) {

    if (sTxBufRdIdx == 0) {
        UCSR1B |= (1 << TXCIE1);
        UCSR1B &= ~(1 << UDRIE1);
    } else {
        if (sTxBufRdIdx == sTxBufWrIdx) {
            UCSR1B |= (1 << TXCIE1);
            UCSR1B &= ~(1 << UDRIE1);
            return;
        }
    }
    UDR1 = sTxBuffer[sTxBufRdIdx];
    sTxBufRdIdx++;
}

/*-----------------------------------------------------------------------------
*  USART tx complete interrupt
*/
ISR(USART1_TX_vect) {
    
    if (sTxBufRdIdx == 1) {
        UCSR1B |= (1 << UDRIE1);
        UCSR1B &= ~(1 << TXCIE1);
        UBRR1L = BR_250K;
    } else {
        sTxBufRdIdx = 0;
        sTxBufWrIdx = 0;
        UCSR1B &= ~(1 << TXCIE1);
        TX_DISABLE;
    }
}
