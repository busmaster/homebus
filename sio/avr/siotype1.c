/*-----------------------------------------------------------------------------
*  siotype1.c 
*  Devices: ATmega88
*/

/*-----------------------------------------------------------------------------
*  Includes
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
#define SIO_RX_BUF_SIZE 16  /* must be power of 2 */
#endif
#ifndef SIO_TX_BUF_SIZE  
#define SIO_TX_BUF_SIZE 32 /* must be power of 2 */
#endif      

/*-----------------------------------------------------------------------------
*  typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/                                
static TIdleStateFunc               sIdleFunc = 0;

static uint8_t                        sRxBuffer[SIO_RX_BUF_SIZE];
static uint8_t                        sRxBufWrIdx = 0;
static uint8_t                        sRxBufRdIdx = 0;
static uint8_t                        sTxBuffer[SIO_TX_BUF_SIZE];
static uint8_t                        sTxBufWrIdx = 0;
static uint8_t                        sTxBufRdIdx = 0;
static TBusTransceiverPowerDownFunc   sBusTransceiverPowerDownFunc = 0;
static TSioMode                       sMode = eSioModeHalfDuplex;

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
*  init sio module
*/
void SioInit(void) {

}

/*-----------------------------------------------------------------------------
*  open sio channel
*/
int SioOpen(const char *pPortName,   /* is ignored */
            TSioBaud baud, 
            TSioDataBits dataBits, 
            TSioParity parity, 
            TSioStopBits stopBits,
            TSioMode mode
   ) {
         
   uint16_t  ubrr; 
   bool      error = true;
   bool      doubleBr = false;
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
         ucsrc |= (1 << UCSZ01) | (1 << UCSZ00);
         ucsrb &= ~(1 << UCSZ02);
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
         ucsrc &= ~(1 << UPM01) & ~(1 << UPM00);
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
         ucsrc &= ~(1 << USBS0);
         break;
      default:
         error = true;
         break;
   }
   if (error == true) {
      return -1;
   }

   /* set baud rate */
   UBRR0H = (unsigned char)(ubrr >> 8);
   UBRR0L = (unsigned char)ubrr;
   if (doubleBr == true) {
      /* double baud rate */
      UCSR0A = 1 << U2X0;
   }

   UCSR0C = ucsrc;

   /* Enable Receiver und Transmitter */
   ucsrb |= (1 << RXEN0) | (1 << RXCIE0) | (1 << TXEN0);
   ucsrb &= ~(1 << UDRIE0);
   UCSR0B = ucsrb;
  
   sMode = mode;
  
   return 0;             
}

/*-----------------------------------------------------------------------------
*  set system idle mode enable callback
*/
void SioSetIdleFunc(int handle, TIdleStateFunc idleFunc) {
   sIdleFunc = idleFunc;
}

/*-----------------------------------------------------------------------------
*  set transceiver power down callback function
*/
void SioSetTransceiverPowerDownFunc(int handle, TBusTransceiverPowerDownFunc btpdFunc) {
   sBusTransceiverPowerDownFunc = btpdFunc;
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
   if ((UCSR0B & (1 << UDRIE0)) == 0) {
      UCSR0B |= 1 << UDRIE0;
   }
   /* disable transmit complete interrupt */
   if ((UCSR0B & (1 << TXCIE0)) != 0) {
      UCSR0B &= ~(1 << TXCIE0);
   }
   if (sBusTransceiverPowerDownFunc != 0) { 
      sBusTransceiverPowerDownFunc(false); /* enable bus transmitter */
   }   
   RESTORE_INT(flag);
   
   return bufSize;           
}

/*-----------------------------------------------------------------------------
*  free space in tx buffer
*/
static uint8_t SioGetNumTxFreeChar(int handle) {

   uint8_t rdIdx;
   uint8_t wrIdx;
                                
   rdIdx = sTxBufRdIdx;      
   wrIdx = sTxBufWrIdx;
   
   return (rdIdx - wrIdx - 1) & (SIO_TX_BUF_SIZE - 1);
}

/*-----------------------------------------------------------------------------
*  number of characters in rx buffer
*/
uint8_t SioGetNumRxChar(int handle) {

   uint8_t rdIdx;
   uint8_t wrIdx;
                                
   rdIdx = sRxBufRdIdx;      
   wrIdx = sRxBufWrIdx;
   
   return (wrIdx - rdIdx) & (SIO_RX_BUF_SIZE - 1);
}

/*-----------------------------------------------------------------------------
*  read from sio rx buffer
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
*  undo read from rx buffer
*/
uint8_t SioUnRead(int handle, uint8_t *pBuf, uint8_t bufSize) {

   uint8_t rxFreeLen;
   uint8_t i;  
   uint8_t rdIdx;                    
   bool  flag;
            
   bufSize = min(bufSize, SIO_RX_BUF_SIZE);
   
   /* set back read index. so rx interrupt cannot write to undo buffer */
   rdIdx = sRxBufRdIdx;       
   rdIdx -= bufSize;
   rdIdx &= (SIO_RX_BUF_SIZE - 1);      

   flag = DISABLE_INT;
   /* aet back write index if necessary */
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
*  USART tx interrupt (data register empty)
*/
ISR(USART_UDRE_vect) {

   uint8_t rdIdx;
   rdIdx = sTxBufRdIdx;
        
   if (sTxBufWrIdx != rdIdx) { 
      if (sMode == eSioModeHalfDuplex) {
         /* disable receiver */
         if ((UCSR0B & (1 << RXEN0)) != 0) {
            UCSR0B &= ~(1 << RXEN0);
         }
      }
      /* get next character from tx buffer */
      rdIdx++;                    
      rdIdx &= (SIO_TX_BUF_SIZE - 1);
      UDR0 = sTxBuffer[rdIdx];
      sTxBufRdIdx = rdIdx;

      /* quit transmit complete interrupt (by writing a 1)      */   
      if ((UCSR0A & (1 << TXC0)) != 0) {
         UCSR0A |= (1 << TXC0);
      }   
   } else {
      if (sMode == eSioModeHalfDuplex) {
         /* enable transmit complete interrupt (to reenable receiver) */
         UCSR0B |= (1 << TXCIE0);
      }
      /* no character in tx buffer -> disable UART UDRE Interrupt */
      UCSR0B &= ~(1 << UDRIE0);
   }
}

/*-----------------------------------------------------------------------------
*  USART tx complete interrupt
*  used for half duplex mode
*/
ISR(USART_TX_vect) {

   if (sBusTransceiverPowerDownFunc != 0) { 
      sBusTransceiverPowerDownFunc(true); /* disable bus transmitter */
   } 
   /* reenable receiver */
   UCSR0B |= (1 << RXEN0);
   /* disable transmit complete interrupt */
   UCSR0B &= ~(1 << TXCIE0);
}

/*-----------------------------------------------------------------------------
*  USART rx complete interrupt
*/
ISR(USART_RX_vect) {

   uint8_t wrIdx;

   wrIdx = sRxBufWrIdx;
   wrIdx++;                    
   wrIdx &= (SIO_RX_BUF_SIZE - 1);
   if (wrIdx != sRxBufRdIdx) {
      sRxBuffer[wrIdx] = UDR0;
   } else {
      /* buffer voll */
      wrIdx--;
      wrIdx &= (SIO_RX_BUF_SIZE - 1);
      /* Zeichen muss gelesen werden, sonst bleibt Interrupt stehen */
      UDR0;
   }  
   sRxBufWrIdx = wrIdx;
   /* do not go idle when characters are in rxbuffer */
   if (sIdleFunc != 0) {
      sIdleFunc(false);
   }
}


