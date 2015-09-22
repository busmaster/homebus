/*
 * siotype0.c for ATmega8
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

#ifndef SIO_RX_BUF_SIZE
#define SIO_RX_BUF_SIZE 16  /* 2er-Potenz!!*/
#endif
#ifndef SIO_TX_BUF_SIZE
#define SIO_TX_BUF_SIZE 32 /* 2er-Potenz!!*/
#endif

/*-----------------------------------------------------------------------------
*  typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/
static uint8_t          sRxBuffer[SIO_RX_BUF_SIZE];
static uint8_t          sRxBufWrIdx = 0;
static uint8_t          sRxBufRdIdx = 0;
static TIdleStateFunc sIdleFunc = 0;

static uint8_t sTxBuffer[SIO_TX_BUF_SIZE];
static uint8_t sTxBufWrIdx = 0;
static uint8_t sTxBufRdIdx = 0;

#if (F_CPU == 1000000UL)
#define UBRR_9600   12     /* 9600 @ 1MHz  */
#define BRX2_9600   true   /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */
#else
#error adjust baud rate settings for your CPU clock frequency
#endif

/*-----------------------------------------------------------------------------
*  Functions
*/
static uint8_t SioGetNumTxFreeChar(int handle);

/*-----------------------------------------------------------------------------
*  Sio Initialisierung
*/
void SioInit(TIdleStateFunc idleFunc) {

   sIdleFunc = idleFunc;
}

/*-----------------------------------------------------------------------------
*  Sio Initialisierung
*/
int SioOpen(const char *pPortName,   /* is ignored */
            TSioBaud baud,
            TSioDataBits dataBits,
            TSioParity parity,
            TSioStopBits stopBits,
            TSioMode mode
   ) {

   uint16_t  ubrr;
   bool    error = true;
   bool    doubleBr = false;
   uint8_t   ucsrb = 0;
   uint8_t   ucsrc = 0;

   switch (baud) {
      case eSioBaud9600:
         ubrr = UBRR_9600;
         error = ERR_9600;
         doubleBr = BRX2_9600;
         break;
      default:
         error = true;
         break;
   }
   if (error == true) {
      return -1;
   }

   switch (dataBits) {
      case eSioDataBits8:
         ucsrc |= (1 << UCSZ1) | (1 << UCSZ0);
         ucsrb &= ~(1 << UCSZ2);
         break;
      default:
         error = true;
         break;
   }
   if (error == true) {
      return -1;
   }

   switch (parity) {
      case eSioParityNo:
         ucsrc &= ~(1 << UPM1) & ~(1 << UPM0);
         break;
      default:
         error = true;
         break;
   }
   if (error == true) {
      return -1;
   }

   switch (stopBits) {
      case eSioStopBits1:
         ucsrc &= ~(1 << USBS);
         break;
      default:
         error = true;
         break;
   }
   if (error == true) {
      return -1;
   }

   /* set baud rate */
   UBRRH = (unsigned char)(ubrr >> 8);
   UBRRL = (unsigned char)ubrr;
   if (doubleBr == true) {
      /* double baud rate */
      UCSRA = 1 << U2X;
   }

   UCSRC = ucsrc | (1 << URSEL);

   /* Enable Receiver und Transmitter */
   ucsrb |= (1 << RXEN) | (1 << RXCIE) | (1 << TXEN) | (0 << UDRIE);;
   UCSRB = ucsrb;

   return true;
}

/*-----------------------------------------------------------------------------
*  Sio Sendepuffer schreiben
*/
uint8_t SioWrite(int handle, uint8_t *pBuf, uint8_t bufSize) {

   uint8_t *pStart;
   uint8_t i;
   uint8_t txFree;
   uint8_t wrIdx;
   bool  flag;

   pStart = &sTxBuffer[0];
   txFree = SioGetNumTxFreeChar(handle);
   if (bufSize > txFree) {
      bufSize = txFree;
   }

   wrIdx = sTxBufWrIdx;
   for (i = 0; i < bufSize; i++) {
      wrIdx++;
      wrIdx &= (SIO_TX_BUF_SIZE - 1);
      *(pStart + wrIdx) =  *(pBuf + i);
   }

   flag = DISABLE_INT;
   sTxBufWrIdx = wrIdx;
   /* enable UART UDRE Interrupt */
   UCSRB |= 1 << UDRIE;
   RESTORE_INT(flag);

   return bufSize;
}

/*-----------------------------------------------------------------------------
*  Tx-Interrupt (USART1 data register empty)
*/
ISR(USART_UDRE_vect) {

   uint8_t rdIdx;
   rdIdx = sTxBufRdIdx;

   if (sTxBufWrIdx != rdIdx) {
      /* disable receiver (damit die eigenen gesendeten Daten nicht empfangen werden */
      if ((UCSRB & (1 << RXEN)) != 0) {
         UCSRB &= ~(1 << RXEN);
      }

      /* nächstes Zeichen aus Sendepuffer holen */
      rdIdx++;
      rdIdx &= (SIO_TX_BUF_SIZE - 1);
      UDR = sTxBuffer[rdIdx];
      sTxBufRdIdx = rdIdx;

      /* transmit complete interrupt quittieren (durch Schreiben von 1)      */
      if ((UCSRA & (1 << TXC)) != 0) {
         UCSRA |= (1 << TXC);
      }

   } else {
      /* enable tramsit complete interrupt (zum Wiederschalten des Empfangs) */
      UCSRB |= (1 << TXCIE);

      /* kein Zeichen im Sendepuffer -> disable UART UDRE Interrupt */
      UCSRB &= ~(1 << UDRIE);
   }
}

/*-----------------------------------------------------------------------------
*  USART1 Tx-complete-Interrupt
*/
ISR(USART_TXC_vect) {

   /* receiver wieder einschalten */
   UCSRB |= (1 << RXEN);

   /* disable transmit complete interrupt */
   UCSRB &= ~(1 << TXCIE);

}


/*-----------------------------------------------------------------------------
*  Freier Platz im Sendepuffer
*/
static uint8_t SioGetNumTxFreeChar(int handle) {

   uint8_t rdIdx;
   uint8_t wrIdx;

   rdIdx = sTxBufRdIdx;
   wrIdx = sTxBufWrIdx;

   return (rdIdx - wrIdx - 1) & (SIO_TX_BUF_SIZE - 1);
}

/*-----------------------------------------------------------------------------
*  Anzahl der Zeichen im Empfangspuffer
*/
uint8_t SioGetNumRxChar(int handle) {

   uint8_t rdIdx;
   uint8_t wrIdx;

   rdIdx = sRxBufRdIdx;
   wrIdx = sRxBufWrIdx;

   return (wrIdx - rdIdx) & (SIO_RX_BUF_SIZE - 1);
}

/*-----------------------------------------------------------------------------
*  Sio Empfangspuffer lesen
*/
uint8_t SioRead(int handle, uint8_t *pBuf, uint8_t bufSize) {

   uint8_t rxLen;
   uint8_t i;
   uint8_t rdIdx;
   bool  flag;

   rdIdx = sRxBufRdIdx;
   rxLen = SioGetNumRxChar(handle);

   if (rxLen < bufSize) {
      bufSize = rxLen;
   }

   for (i = 0; i < bufSize; i++) {
      rdIdx++;
      rdIdx &= (SIO_RX_BUF_SIZE - 1);
      *(pBuf + i) = sRxBuffer[rdIdx];
   }

   sRxBufRdIdx = rdIdx;

   flag = DISABLE_INT;
   if (SioGetNumRxChar(handle) == 0) {
      if (sIdleFunc != 0) {
         sIdleFunc(true);
      }
   }
   RESTORE_INT(flag);

   return bufSize;
}

/*-----------------------------------------------------------------------------
*  Sio in Empfangspuffer zurückschreiben
*/
uint8_t SioUnRead(int handle, uint8_t *pBuf, uint8_t bufSize) {

   uint8_t rxFreeLen;
   uint8_t i;
   uint8_t rdIdx;
   bool  flag;

   bufSize = min(bufSize, SIO_RX_BUF_SIZE);

   /* Readzeiger zurücksetzen damit Rx-Interrupt nicht weiterschreiben kann */
   rdIdx = sRxBufRdIdx;
   rdIdx -= bufSize;
   rdIdx &= (SIO_RX_BUF_SIZE - 1);

   flag = DISABLE_INT;
   /* Schreibzeiger falls erforderlich zurücksetzen */
   rxFreeLen = SIO_RX_BUF_SIZE - 1 - SioGetNumRxChar(handle);
   if (bufSize > rxFreeLen) {
       sRxBufWrIdx -= (bufSize - rxFreeLen);
   }
   sRxBufRdIdx = rdIdx;
   RESTORE_INT(flag);

   rdIdx++;
   for (i = 0; i < bufSize; i++) {
      sRxBuffer[rdIdx] = *(pBuf + i);
      rdIdx++;
      rdIdx &= (SIO_RX_BUF_SIZE - 1);
   }

   if ((sIdleFunc != 0) &&
       (bufSize > 0)) {
      sIdleFunc(false);
   }

   return bufSize;
}

/*-----------------------------------------------------------------------------
*  check handle
*/
bool SioHandleValid(int handle) {
    return true;
}

/*-----------------------------------------------------------------------------
*  USART1 Rx-Complete Interrupt
*/
ISR(USART_RXC_vect) {

   uint8_t wrIdx;
   uint8_t dummy;

   wrIdx = sRxBufWrIdx;
   wrIdx++;
   wrIdx &= (SIO_RX_BUF_SIZE - 1);
   if (wrIdx != sRxBufRdIdx) {
      sRxBuffer[wrIdx] = UDR;
   } else {
      /* buffer voll */
      wrIdx--;
      wrIdx &= (SIO_RX_BUF_SIZE - 1);
      /* Zeichen muss gelesen werden, sonst bleibt Interrupt stehen */
      dummy = UDR;
   }
   sRxBufWrIdx = wrIdx;
   /* do not go idle when characters are in rxbuffer */
   if (sIdleFunc != 0) {
      sIdleFunc(false);
   }
}

