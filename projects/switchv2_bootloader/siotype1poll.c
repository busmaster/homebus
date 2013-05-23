/*
 * siotype1poll.c for ATmega88
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

#if (F_CPU == 1000000UL)
#define BRREG    12 /* 9600 @ 1MHz  */
#define BRX2        /* U2X = 1      */   
#else
#error adjust baud rate settings for your CPU clock frequency
#endif

/*-----------------------------------------------------------------------------
*  typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Sio Initialisierung
*/
void SioInit(void) {
     
   unsigned int baud = BRREG; 

   /* Set baud rate UART0: 9600 */
   UBRR0H = (unsigned char)(baud >> 8);
   UBRR0L = (unsigned char)baud;
#ifdef BRX2
   /* double baud rate */
   UCSR0A = 1 << U2X0;
#endif
   /* Set frame format: 8 data, 1 stop bit */
   /* Enable Receiver und Transmitter */
   UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); 
   UCSR0B = (1 << RXEN0) | (1 << TXEN0);
   UCSR0B &= ~(1 << UCSZ02);
}

/*-----------------------------------------------------------------------------
*  Sio Sendepuffer schreiben
*/
void SioWrite(uint8_t ch) {
   
   /* Wait for empty transmit buffer */
   while ((UCSR0A & (1 << UDRE0)) == 0);

   cli();
   UDR0 = ch;
   /* quit transmit complete interrupt (by writing a 1)      */   
   if ((UCSR0A & (1 << TXC0)) != 0) {
      UCSR0A |= (1 << TXC0);
   }   
   sei();
}

/*-----------------------------------------------------------------------------
*  Sio Empfangspuffer lesen
*/
uint8_t SioRead(uint8_t *pCh) {

   if ((UCSR0A & (1 << RXC0)) != 0) {
      *pCh = UDR0;
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
   while ((UCSR0A & (1 << TXC0)) == 0);
}

/*-----------------------------------------------------------------------------
*  dump sio rx buf
*/
void SioReadFlush(void) {

   while (UCSR0A & (1 << RXC0)) {
      UDR0;
   }
}

