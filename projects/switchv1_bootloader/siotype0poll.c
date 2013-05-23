/*
 * siotype0poll.c
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
   UBRRH = (unsigned char)(baud >> 8);
   UBRRL = (unsigned char)baud;
#ifdef BRX2
   /* double baud rate */
   UCSRA = 1 << U2X;
#endif
   /* Enable Receiver und Transmitter */
   UCSRB = (1 << RXEN) | (1 << TXEN);
   /* Set frame format: 8 data, 1 stop bit */
   UCSRC = (1 << URSEL) | (3 << UCSZ0); 
}

/*-----------------------------------------------------------------------------
*  Sio Sendepuffer schreiben
*/
void SioWrite(uint8_t ch) {
   
   /* Wait for empty transmit buffer */
   while ((UCSRA & (1 << UDRE)) == 0);
   UDR = ch;
}

/*-----------------------------------------------------------------------------
*  Sio Empfangspuffer lesen
*/
uint8_t SioRead(uint8_t *pCh) {

   if ((UCSRA & (1<<RXC)) != 0) {
      *pCh = UDR;
      return 1;
   } else {
      return 0;
   }
}

/*-----------------------------------------------------------------------------
*  zurückschreiben
*/
void SioReadFlush(void) {
   
   while (UCSRA & (1<<RXC)) {
      UDR;
   }
}

