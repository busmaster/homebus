/*
 * siotype2.c for ATmega128
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
#include <string.h>

#include "sysdef.h"
#include "sio.h"

/*-----------------------------------------------------------------------------
*  Macros
*/    

#if (F_CPU == 1000000UL)
#define UBRR_9600   12     /* 9600 @ 1MHz  */
#define BRX2_9600   true   /* double baud rate */       
#define ERR_9600    false  /* baud rate is possible */ 
#elif (F_CPU == 3686400UL)
#define UBRR_9600   23     /* 9600 @ 3.6864MHz  */
#define BRX2_9600   false  /* double baud rate off */       
#define ERR_9600    false  /* baud rate is possible */ 
#else
#error adjust baud rate settings for your CPU clock frequency
#endif

                 
#define SIO_RX0_BUF_SIZE 4  /* power of 2 */
#define SIO_TX0_BUF_SIZE 16 /* power of 2 */      

#define SIO_RX1_BUF_SIZE 64  /* power of 2 */
#define SIO_TX1_BUF_SIZE 128 /* power of 2 */    

#define NUM_CHANNELS  2

#define CHECK_HANDLE

#ifdef CHECK_HANDLE
#define RETURN_ON_INVALID_HDL(hdl)          \
   if (((hdl) < 0) ||                       \
       ((hdl) > 1) ||                       \
       (sChan[hdl].valid == false))         \
      return
#define RETURN_0_ON_INVALID_HDL(hdl)        \
   if (((hdl) < 0) ||                       \
       ((hdl) > 1) ||                       \
       (sChan[hdl].valid == false))         \
      return 0
      
#else
#define RETURN_ON_INVALID_HDL(hdl)
#define RETURN_0_ON_INVALID_HDL(hdl)
#endif      

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef struct {
   uint8_t                      *pRxBuf;
   uint8_t                      *pTxBuf;
   uint8_t                      *pTxBufBuffered;
   uint8_t                      rxBufWrIdx;
   uint8_t                      rxBufRdIdx;
   uint8_t                      rxBufSize;
   uint8_t                      txBufWrIdx;
   uint8_t                      txBufRdIdx;
   uint8_t                      txBufSize;
   uint8_t                      txBufBufferedPos;
   uint8_t                      txBufBufferedSize;
   volatile uint8_t             *ubrrh;
   volatile uint8_t             *ubrrl;
   volatile uint8_t             *ucsra;
   volatile uint8_t             *ucsrb;
   volatile uint8_t             *ucsrc;
   volatile uint8_t             *udr;
   TSioMode                     mode;
   TIdleStateFunc               idleFunc;
   TBusTransceiverPowerDownFunc busTransceiverPowerDownFunc;
   bool                         valid;
} TChanDesc;

/*-----------------------------------------------------------------------------
*  Variables
*/             
static uint8_t sRx0Buffer[SIO_RX0_BUF_SIZE];
static uint8_t sTx0Buffer[SIO_TX0_BUF_SIZE];
static uint8_t sTx0BufferBuffered[SIO_TX0_BUF_SIZE];

static uint8_t sRx1Buffer[SIO_RX1_BUF_SIZE];
static uint8_t sTx1Buffer[SIO_TX1_BUF_SIZE];
static uint8_t sTx1BufferBuffered[SIO_TX1_BUF_SIZE];

static TChanDesc sChan[NUM_CHANNELS];

/*-----------------------------------------------------------------------------
*  Functions
*/
static uint8_t SioGetNumTxFreeChar(int handle);
static void UdreInt(int handle);
static void TxcInt(int handle);
static void RxcInt(int handle);

/*-----------------------------------------------------------------------------
*  init sio module
*/
void SioInit(void) {

   int        i;
   TChanDesc  *pChan = &sChan[0];

   for (i = 0; i < NUM_CHANNELS; i++) {
      pChan->valid = false;
      pChan++;
   }
}

/*-----------------------------------------------------------------------------
*  open channel
*/
int SioOpen(const char *pPortName,   /* is ignored */
            TSioBaud baud, 
            TSioDataBits dataBits, 
            TSioParity parity, 
            TSioStopBits stopBits,
            TSioMode mode
   ) {

   uint16_t     ubrr; 
   bool       error = true;
   bool       doubleBr = false;
   uint8_t      ucsrb = 0;
   uint8_t      ucsrc = 0;
   TChanDesc  *pChan;
   int        hdl = -1;

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
   
   if (strcmp(pPortName, "USART0") == 0) {
      hdl = 0;
      pChan = &sChan[hdl];
      pChan->pRxBuf = sRx0Buffer;
      pChan->rxBufSize = sizeof(sRx0Buffer);   
      pChan->pTxBuf = sTx0Buffer;
      pChan->txBufSize = sizeof(sTx0Buffer);  
      pChan->pTxBufBuffered = sTx0BufferBuffered;
      pChan->txBufBufferedSize = sizeof(sTx0BufferBuffered);
      pChan->ubrrh = (volatile uint8_t *)0x90; /* UBRR0H */
      pChan->ubrrl = (volatile uint8_t *)0x29; /* UBRR0L */
      pChan->ucsra = (volatile uint8_t *)0x2b; /* UCSR0A */
      pChan->ucsrb = (volatile uint8_t *)0x2a; /* UCSR0B */
      pChan->ucsrc = (volatile uint8_t *)0x95; /* UCSR0C */
      pChan->udr =   (volatile uint8_t *)0x2c; /* UDR0   */
   } else if (strcmp(pPortName, "USART1") == 0) {
      hdl = 1;
      pChan = &sChan[hdl];
      pChan->pRxBuf = sRx1Buffer;      
      pChan->rxBufSize = sizeof(sRx1Buffer);   
      pChan->pTxBuf = sTx1Buffer;
      pChan->txBufSize = sizeof(sTx1Buffer);
      pChan->pTxBufBuffered = sTx1BufferBuffered;    
      pChan->txBufBufferedSize = sizeof(sTx1BufferBuffered);
      pChan->ubrrh = (volatile uint8_t *)0x98; /* UBRR1H */
      pChan->ubrrl = (volatile uint8_t *)0x99; /* UBRR1L */
      pChan->ucsra = (volatile uint8_t *)0x9b; /* UCSR1A */
      pChan->ucsrb = (volatile uint8_t *)0x9a; /* UCSR1B */
      pChan->ucsrc = (volatile uint8_t *)0x9d; /* UCSR1C */
      pChan->udr =   (volatile uint8_t *)0x9c; /* UDR1   */  
   } else {
      error = true;
   }
   
   if (error == true) {
      return -1;
   }
   
   *pChan->ubrrh = (unsigned char)(ubrr >> 8);
   *pChan->ubrrl = (unsigned char)ubrr;
   if (doubleBr == true) {
      /* double baud rate */
      *pChan->ucsra = 1 << U2X;
   }
   *pChan->ucsrc = ucsrc;
   /* Enable Receiver und Transmitter */
   ucsrb |= (1 << RXEN) | (1 << RXCIE) | (1 << TXEN);
   ucsrb &= ~(1 << UDRIE);
   *pChan->ucsrb = ucsrb;

   pChan->valid = true;
   pChan->rxBufWrIdx = 0;
   pChan->rxBufRdIdx = 0;
   pChan->txBufWrIdx = 0;
   pChan->txBufBufferedPos = 0;
   pChan->txBufRdIdx = 0;
   pChan->idleFunc = 0;
   pChan->busTransceiverPowerDownFunc = 0;
   pChan->mode = mode;    
   
   return hdl;
}

/*-----------------------------------------------------------------------------
*  set system idle mode enable callback
*/
void SioSetIdleFunc(int handle, TIdleStateFunc idleFunc) {

   TChanDesc  *pChan;
   
   RETURN_ON_INVALID_HDL(handle);
   pChan = &sChan[handle];
   pChan->idleFunc = idleFunc;
}

/*-----------------------------------------------------------------------------
*  set transceiver power down callback function
*/
void SioSetTransceiverPowerDownFunc(int handle, TBusTransceiverPowerDownFunc btpdFunc) {

   TChanDesc  *pChan;
   
   RETURN_ON_INVALID_HDL(handle);
   pChan = &sChan[handle];   
   pChan->busTransceiverPowerDownFunc = btpdFunc;   
}

/*-----------------------------------------------------------------------------
*  write to sio channel tx buffer
*/
uint8_t SioWrite(int handle, uint8_t *pBuf, uint8_t bufSize) {
                        
   uint8_t *pStart;   
   uint8_t i;
   uint8_t txFree;
   uint8_t wrIdx;
   bool  flag;
   uint8_t txBufSize;
   TChanDesc  *pChan;
   
   RETURN_0_ON_INVALID_HDL(handle);
   pChan = &sChan[handle]; 
   pStart = pChan->pTxBuf;
   txBufSize = pChan->txBufSize;
   wrIdx = pChan->txBufWrIdx;
   txFree = SioGetNumTxFreeChar(handle);
   if (bufSize > txFree) {
      bufSize = txFree;
   }
   for (i = 0; i < bufSize; i++) {      
      wrIdx++;                    
      wrIdx &= (txBufSize - 1);
      *(pStart + wrIdx) =  *(pBuf + i);
   }    

   flag = DISABLE_INT;
   pChan->txBufWrIdx = wrIdx;
   /* enable UART UDRE Interrupt */
   if ((*pChan->ucsrb & (1 << UDRIE)) == 0) {
      *pChan->ucsrb |= 1 << UDRIE;
   }
   /* disable transmit complete interrupt */
   if ((*pChan->ucsrb & (1 << TXCIE)) != 0) {
      *pChan->ucsrb &= ~(1 << TXCIE);
   }
   
   if (pChan->busTransceiverPowerDownFunc != 0) {
      pChan->busTransceiverPowerDownFunc(false); /* enable bus transmitter */
   } 
   RESTORE_INT(flag);
   
   return bufSize;           
}

/*-----------------------------------------------------------------------------
*  write data to tx buffer - do not yet start with tx
*/
uint8_t SioWriteBuffered(int handle, uint8_t *pBuf, uint8_t bufSize) {

   uint8_t   len;
   TChanDesc  *pChan;
   
   RETURN_0_ON_INVALID_HDL(handle);
   pChan = &sChan[handle]; 

   len = pChan->txBufBufferedSize - pChan->txBufBufferedPos;
   len = min(len, bufSize);
   memcpy(&pChan->pTxBufBuffered[pChan->txBufBufferedPos], pBuf, len);
   pChan->txBufBufferedPos += len;
   
   return bufSize;           
}

/*-----------------------------------------------------------------------------
*  trigger tx of buffer
*/
bool SioSendBuffer(int handle) {

   unsigned long bytesWritten;    
   bool          rc;
   TChanDesc     *pChan;

   RETURN_0_ON_INVALID_HDL(handle);
   pChan = &sChan[handle]; 
      
   bytesWritten = SioWrite(handle, pChan->pTxBufBuffered, pChan->txBufBufferedPos);
   if (bytesWritten == pChan->txBufBufferedPos) {
       rc = true;
   } else {
       rc = false;
   }
   pChan->txBufBufferedPos = 0;
   
   return rc;
}

/*-----------------------------------------------------------------------------
*  free space in tx buffer
*/
static uint8_t SioGetNumTxFreeChar(int handle) {

   uint8_t rdIdx;
   uint8_t wrIdx;
   uint8_t txBufSize;
   TChanDesc  *pChan;
   
   pChan = &sChan[handle]; 
   rdIdx = pChan->txBufRdIdx;      
   wrIdx = pChan->txBufWrIdx;
   txBufSize = pChan->txBufSize;
   
   return (rdIdx - wrIdx - 1) & (txBufSize - 1);
}

/*-----------------------------------------------------------------------------
*  number of characters in rx buffer
*/
uint8_t SioGetNumRxChar(int handle) {

   uint8_t rdIdx;
   uint8_t wrIdx;
   uint8_t rxBufSize;
   TChanDesc  *pChan;
   
   RETURN_0_ON_INVALID_HDL(handle);
   pChan = &sChan[handle]; 
   rdIdx = pChan->rxBufRdIdx;      
   wrIdx = pChan->rxBufWrIdx;
   rxBufSize = pChan->rxBufSize;
   
   return (wrIdx - rdIdx) & (rxBufSize - 1);
}

/*-----------------------------------------------------------------------------
*  read from sio rx buffer
*/
uint8_t SioRead(int handle, uint8_t *pBuf, uint8_t bufSize) {

   uint8_t rxLen;
   uint8_t i;  
   uint8_t rdIdx;
   uint8_t rxBufSize;   
   TChanDesc  *pChan;
   bool  flag;
   
   RETURN_0_ON_INVALID_HDL(handle);
   pChan = &sChan[handle]; 
   rdIdx = pChan->rxBufRdIdx;
   rxBufSize = pChan->rxBufSize;
   rxLen = SioGetNumRxChar(handle);

   if (rxLen < bufSize) {
      bufSize = rxLen;
   }
              
   for (i = 0; i < bufSize; i++) {
      rdIdx++;                    
      rdIdx &= (rxBufSize - 1);
      *(pBuf + i) = pChan->pRxBuf[rdIdx];
   }    
   
   pChan->rxBufRdIdx = rdIdx;              

   flag = DISABLE_INT;
   if (SioGetNumRxChar(handle) == 0) {
      if (pChan->idleFunc != 0) {
         pChan->idleFunc(true);
      }
   }
   RESTORE_INT(flag);

   return bufSize;
}

/*-----------------------------------------------------------------------------
*  undo read from rx buffer
*/
uint8_t SioUnRead(int handle, uint8_t *pBuf, uint8_t bufSize) {

   uint8_t rxFreeLen;
   uint8_t i;  
   uint8_t rdIdx;                    
   bool  flag;
   uint8_t rxBufSize; 
   TChanDesc  *pChan;
   
   RETURN_0_ON_INVALID_HDL(handle);
   pChan = &sChan[handle]; 
   rxBufSize = pChan->rxBufSize;
   bufSize = min(bufSize, rxBufSize);
   
   /* set back read index. so rx interrupt cannot write to undo buffer */
   rdIdx = pChan->rxBufRdIdx;       
   rdIdx -= bufSize;
   rdIdx &= (rxBufSize - 1);      

   flag = DISABLE_INT;
   /* aet back write index if necessary */
   rxFreeLen = rxBufSize - 1 - SioGetNumRxChar(handle); 
   if (bufSize > rxFreeLen) {
      pChan->rxBufWrIdx -= (bufSize - rxFreeLen);
   }    
   pChan->rxBufRdIdx = rdIdx;
   RESTORE_INT(flag);

   rdIdx++;
   for (i = 0; i < bufSize; i++) {
      pChan->pRxBuf[rdIdx] = *(pBuf + i); 
      rdIdx++;                    
      rdIdx &= (rxBufSize - 1);
   }    

   if ((pChan->idleFunc != 0) &&
       (bufSize > 0)) {
      pChan->idleFunc(false);
   }

   return bufSize;
}

/*-----------------------------------------------------------------------------
*  USART0 tx interrupt (data register empty)
*/
ISR(USART0_UDRE_vect) {

   UdreInt(0);
}

/*-----------------------------------------------------------------------------
*  USART1 tx interrupt (data register empty)
*/
ISR(USART1_UDRE_vect) {
   
   UdreInt(1);
}

static void UdreInt(int handle) {

   uint8_t rdIdx;
   TChanDesc  *pChan = &sChan[handle];
   rdIdx = pChan->txBufRdIdx;
        
   if (pChan->txBufWrIdx != rdIdx) { 
      if (pChan->mode == eSioModeHalfDuplex) {
         /* disable receiver */
         if ((*pChan->ucsrb & (1 << RXEN)) != 0) {
            *pChan->ucsrb &= ~(1 << RXEN);
         }
      }
      /* n\E4chstes Zeichen aus Sendepuffer holen */
      rdIdx++;                    
      rdIdx &= (pChan->txBufSize - 1);
      *pChan->udr = pChan->pTxBuf[rdIdx];
      pChan->txBufRdIdx = rdIdx;

      /* quit transmit complete interrupt (by writing a 1)      */   
      if ((*pChan->ucsra & (1 << TXC)) != 0) {
         *pChan->ucsra |= (1 << TXC);
      }   
   } else {
      if (pChan->mode == eSioModeHalfDuplex) {
         /* enable tramsit complete interrupt (to reenable receiver) */
         *pChan->ucsrb |= (1 << TXCIE);
      }
      /* kein Zeichen im Sendepuffer -> disable UART UDRE Interrupt */
      *pChan->ucsrb &= ~(1 << UDRIE);
   }
}

/*-----------------------------------------------------------------------------
*  USART0 tx complete interrupt
*  used for half duplex mode
*/
ISR(USART0_TX_vect) {

   TxcInt(0);
}

/*-----------------------------------------------------------------------------
*  USART1 tx complete interrupt
*  used for half duplex mode
*/
ISR(USART1_TX_vect) {

   TxcInt(1);
}

static void TxcInt(int handle) {

   TChanDesc  *pChan = &sChan[handle];

   if (pChan->busTransceiverPowerDownFunc != 0) { 
      pChan->busTransceiverPowerDownFunc(true); /* disable bus transmitter */
   }
   /* reenable receiver */
   *pChan->ucsrb |= (1 << RXEN);
   /* disable transmit complete interrupt */
   *pChan->ucsrb &= ~(1 << TXCIE);
}

/*-----------------------------------------------------------------------------
*  USART0 rx complete interrupt
*/
ISR(USART0_RX_vect) {
   
   RxcInt(0);
}

/*-----------------------------------------------------------------------------
*  USART1 tx interrupt (data register empty)
*/
ISR(USART1_RX_vect) {

   RxcInt(1);
}

static void RxcInt(int handle) {
 
   uint8_t wrIdx;
   uint8_t rxBufSize;
   TChanDesc  *pChan = &sChan[handle];

   wrIdx = pChan->rxBufWrIdx;
   rxBufSize = pChan->rxBufSize;
   wrIdx++;                    
   wrIdx &= (rxBufSize - 1);
   if (wrIdx != pChan->rxBufRdIdx) {
      pChan->pRxBuf[wrIdx] = *pChan->udr;
   } else {
      /* buffer full */
      wrIdx--;
      wrIdx &= (rxBufSize - 1);
      /* must read character to quit interrupt */
      *pChan->udr;
   }  
   pChan->rxBufWrIdx = wrIdx;
   /* do not go idle when characters are in rxbuffer */
   if (pChan->idleFunc != 0) {
      pChan->idleFunc(false);
   }
}
