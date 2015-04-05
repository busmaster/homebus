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

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "sysdef.h"
#include "sio.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

#if (F_CPU == 1000000UL)
#define UBRR_9600   12     /* 9600 @ 1MHz  */
#define BRX2_9600   true   /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER1_PRESCALER (0b100 << CS10) // prescaler clk/256
#define TIMER1_MS1       4               // 1 ms
#define TIMER1_MS2       8               // 2 ms


#elif (F_CPU == 3686400UL)
#define UBRR_9600   23     /* 9600 @ 3.6864MHz  */
#define BRX2_9600   false  /* double baud rate off */
#define ERR_9600    false  /* baud rate is possible */

/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER1_PRESCALER (0b101 << CS10) // prescaler clk/1024
#define TIMER1_MS1       4               // 4 * 1024 / 3686400 = 1.1
#define TIMER1_MS2       8               // 8 * 1024 / 3686400 = 2.2

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

#define MAX_TX_RETRY  16
#define INTERCHAR_TIMEOUT  TIMER1_MS2

#define MAX_JAM_CNT   8

// default: use INT1 for USART1
//          no external INT for USART0
#ifndef USART1_EXTINT
#define USART1_EXTINT 1
#endif

#if USART1_EXTINT == 1
#define USART1_EICR      (volatile uint8_t *)0x6a /* EICRA */
#define USART1_EICR_MSK  ((1 << ISC11) | (1 << ISC10))
#define USART1_EICR_VAL  (1 << ISC11)  /* falling edge */
#define USART1_EIMSK     (volatile uint8_t *)0x59 /* EIMSK */
#define USART1_EIMSK_VAL (1 << INT1)
#define USART1_EIFR      (volatile uint8_t *)0x58 /* EIFR */
#define USART1_EIFR_VAL  (1 << INTF1)
#define USART1_EXTVECT   INT1_vect
#endif

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef struct {
    bool     txStartup;
    uint8_t  txRetryCnt;
    uint8_t  txRxComparePos;
    uint8_t  txJamCnt;
    uint16_t txDelayTicks;
    enum {
        eIdle,          // no activity
        eRxing,         // rx is in progress (no data in tx)
        eTxing,         // tx is in progress (is read back)
        eTxStopped,     // tx is terminated after jamming
        eTxPending,     // tx buffer is ready to be sent, but rx is in progress
        eTxJamming      // tx jam
    } state;
} TCommState;

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
    volatile uint16_t            *timerOcr;
    uint8_t                      timerIntMsk;
    uint8_t                      timerIntFlg;
    volatile uint8_t             *extIntEIMSK;
    uint8_t                      extIntEIMSKVal;
    volatile uint8_t             *extIntEIFR;
    uint8_t                      extIntEIFRVal;
    TCommState                   comm;
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

static uint16_t sRand;

/*-----------------------------------------------------------------------------
*  Functions
*/
static uint8_t GetNumTxFreeChar(TChanDesc *pChan);
static void UdreInt(int handle);
static void TxcInt(int handle);
static void RxcInt(int handle);
static void TimerInit(void);
static void TimerStart(TChanDesc *pChan, uint16_t delayTicks);
static void TimerStop(TChanDesc *pChan);
static void RxStartDetectionInit(void);

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
    TimerInit();
    RxStartDetectionInit();
}

/*-----------------------------------------------------------------------------
*  set seed of random num generator
*/
void SioRandSeed(uint8_t seed) {
    sRand = seed;
}

/*-----------------------------------------------------------------------------
*  very fast rand function
*/
static uint8_t MyRand(void) {
    sRand = (sRand << 7) - sRand + 251;
    return (uint8_t)(sRand + (sRand >> 8));
}

/*-----------------------------------------------------------------------------
*  open sio channel
*/
int SioOpen(const char *pPortName,   /* is ignored */
            TSioBaud baud,
            TSioDataBits dataBits,
            TSioParity parity,
            TSioStopBits stopBits,
            TSioMode mode            /* is ignored, always half duplex */
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
        pChan->timerOcr = (volatile uint16_t *)0x4a ; /* OCR1AL */
        pChan->timerIntMsk = 1 << OCIE1A;
        pChan->timerIntFlg = 1 << OCF1A;
#ifdef USART0_EXTINT
        pChan->extIntEIMSK    = USART0_EIMSK;
        pChan->extIntEIMSKVal = USART0_EIMSK_VAL;
        pChan->extIntEIFR     = USART0_EIFR;
        pChan->extIntEIFRVal  = USART0_EIFR_VAL;
#else
        pChan->extIntEIMSK    = 0;
#endif        
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
        pChan->timerOcr = (volatile uint16_t *)0x48; /* OCR1BL */
        pChan->timerIntMsk = 1 << OCIE1B;
        pChan->timerIntFlg = 1 << OCF1B;
#ifdef USART1_EXTINT
        pChan->extIntEIMSK    = USART1_EIMSK;
        pChan->extIntEIMSKVal = USART1_EIMSK_VAL;
        pChan->extIntEIFR     = USART1_EIFR;
        pChan->extIntEIFRVal  = USART1_EIFR_VAL;
#else
        pChan->extIntEIMSK    = 0;
#endif      
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

    pChan->comm.state = eIdle;
    pChan->comm.txRetryCnt = 0;
    pChan->comm.txRxComparePos = 0;
    pChan->comm.txDelayTicks = 0;
    pChan->comm.txStartup = false;

    pChan->valid = true;
    pChan->rxBufWrIdx = 0;
    pChan->rxBufRdIdx = pChan->rxBufSize - 1; /* last index read */
    pChan->txBufWrIdx = 0;
    pChan->txBufBufferedPos = 0;
    pChan->txBufRdIdx = 0;
    pChan->idleFunc = 0;
    pChan->busTransceiverPowerDownFunc = 0;
    pChan->mode = mode;

    /* enable the receiver und transmitter */
    ucsrb |= (1 << RXEN) | (1 << RXCIE) | (1 << TXEN);
    ucsrb &= ~(1 << UDRIE);
    *pChan->ucsrb = ucsrb;

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

    if (bufSize == 0) {
        return 0;
    } 

    RETURN_0_ON_INVALID_HDL(handle);
    pChan = &sChan[handle];
    pStart = pChan->pTxBuf;
    txBufSize = pChan->txBufSize;
    wrIdx = pChan->txBufWrIdx;
    txFree = GetNumTxFreeChar(pChan);
    if (bufSize > txFree) {
        bufSize = txFree;
    }
    for (i = 0; i < bufSize; i++) {
        wrIdx++;
        wrIdx &= (txBufSize - 1);
        *(pStart + wrIdx) =  *(pBuf + i);
    }

    flag = DISABLE_INT;
    if (pChan->txBufWrIdx == pChan->txBufRdIdx) {
        pChan->comm.txStartup = true;
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
    }
    pChan->txBufWrIdx = wrIdx;

    RESTORE_INT(flag);

    return bufSize;
}

/*-----------------------------------------------------------------------------
*  stop tx and clear tx buffer
*/
static void StopTx(TChanDesc  *pChan) {
    pChan->txBufRdIdx = pChan->txBufWrIdx;
}

/*-----------------------------------------------------------------------------
*  write data to tx buffer - do not yet start with tx
*/
uint8_t SioWriteBuffered(int handle, uint8_t *pBuf, uint8_t bufSize) {
    uint8_t   len;
    TChanDesc  *pChan;

    RETURN_0_ON_INVALID_HDL(handle);
    pChan = &sChan[handle];
    if ((pChan->comm.state == eIdle) ||
        (pChan->comm.state == eRxing)) {
        len = pChan->txBufBufferedSize - pChan->txBufBufferedPos;
        len = min(len, bufSize);
        memcpy(&pChan->pTxBufBuffered[pChan->txBufBufferedPos], pBuf, len);
        pChan->txBufBufferedPos += len;
    } else {
        return 0;
    }

    return len;
}

/*-----------------------------------------------------------------------------
*  trigger tx of buffer
*/
bool SioSendBuffer(int handle) {
    unsigned long bytesWritten;
    bool          rc;
    TChanDesc     *pChan;
    bool          flag;

    RETURN_0_ON_INVALID_HDL(handle);
    pChan = &sChan[handle];

    flag = DISABLE_INT;

    if (pChan->comm.state == eIdle) {
        pChan->comm.txRxComparePos = 0;
        bytesWritten = SioWrite(handle, pChan->pTxBufBuffered, pChan->txBufBufferedPos);
        if (bytesWritten == pChan->txBufBufferedPos) {
            rc = true;
        } else {
            rc = false;
        }
    } else if (pChan->comm.state == eRxing) {
        pChan->comm.txDelayTicks = INTERCHAR_TIMEOUT;
        pChan->comm.state = eTxPending;
        rc = true;
    } else {
        rc = false;
    }

    RESTORE_INT(flag);

    return rc;
}

/*-----------------------------------------------------------------------------
*  free space in tx buffer
*/
static uint8_t GetNumTxFreeChar(TChanDesc *pChan) {
    uint8_t rdIdx;
    uint8_t wrIdx;
    uint8_t txBufSize;

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

    flag = DISABLE_INT;

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
    bufSize = min(bufSize, rxBufSize - 1);

    flag = DISABLE_INT;
    /* set back read index. so rx interrupt cannot write to undo buffer */
    rdIdx = pChan->rxBufRdIdx;
    rdIdx -= bufSize;
    rdIdx &= (rxBufSize - 1);

    /* set back write index if necessary */
    rxFreeLen = rxBufSize - 1 - SioGetNumRxChar(handle);
    if (bufSize > rxFreeLen) {
        pChan->rxBufWrIdx -= (bufSize - rxFreeLen);
    }
    pChan->rxBufRdIdx = rdIdx;
    RESTORE_INT(flag);

    rdIdx++;
    rdIdx &= (rxBufSize - 1);
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
    bool lastChar = false;
    TChanDesc  *pChan = &sChan[handle];
    uint8_t rdIdx = pChan->txBufRdIdx;
    uint8_t txChar;

    if (pChan->comm.state == eTxJamming) {
        pChan->comm.txJamCnt++;
        if (pChan->comm.txJamCnt < MAX_JAM_CNT) {
            *pChan->udr = 0; // 0 is the jam character
        } else {
            // stop jamming
            lastChar = true;
        } 
    } else if (pChan->txBufWrIdx != rdIdx) {
        if (pChan->comm.state == eRxing) {
            pChan->comm.txDelayTicks = INTERCHAR_TIMEOUT;
            TimerStart(pChan, INTERCHAR_TIMEOUT);
            pChan->comm.state = eTxPending;
            StopTx(pChan);
            *pChan->ucsrb &= ~(1 << UDRIE);
            return;
        }
        /* get next character from tx buffer */
        rdIdx++;
        rdIdx &= (pChan->txBufSize - 1);
        txChar = pChan->pTxBuf[rdIdx];

        if ((pChan->comm.txStartup) &&
            (pChan->extIntEIFR != 0)) {
            pChan->comm.txStartup = false;
            if ((*pChan->extIntEIFR & pChan->extIntEIFRVal) == 0) {
                *pChan->udr = txChar;
            } else {
                *pChan->extIntEIFR = pChan->extIntEIFRVal;
                pChan->comm.txDelayTicks = INTERCHAR_TIMEOUT;
                TimerStart(pChan, INTERCHAR_TIMEOUT);
                pChan->comm.state = eTxPending;
                StopTx(pChan);
                *pChan->ucsrb &= ~(1 << UDRIE);
                return;
            }
        } else {
            *pChan->udr = txChar;
        }
        pChan->txBufRdIdx = rdIdx;
        pChan->comm.state = eTxing;

        /* quit transmit complete interrupt (by writing a 1)      */
        if ((*pChan->ucsra & (1 << TXC)) != 0) {
            *pChan->ucsra |= (1 << TXC);
        }
    } else {
        lastChar = true;
    }

    if (lastChar) {
        /* enable transmit complete interrupt */
        *pChan->ucsrb |= (1 << TXCIE);
        /* no character in txbuffer -> disable UART UDRE Interrupt */
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
    /* disable transmit complete interrupt */
    *pChan->ucsrb &= ~(1 << TXCIE);

    if (pChan->comm.state == eTxJamming) {
        pChan->comm.state = eTxStopped;
        TimerStart(pChan, INTERCHAR_TIMEOUT);
    } else {
        pChan->comm.state = eIdle;
        if (pChan->extIntEIMSK != 0) {
            *pChan->extIntEIFR = pChan->extIntEIFRVal;
            *pChan->extIntEIMSK |= pChan->extIntEIMSKVal;
        }
    }
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
    bool doRx = false;
    uint8_t rxBufSize;
    TChanDesc  *pChan = &sChan[handle];

    switch (pChan->comm.state) {
    case eTxing:
        // read back char from tx received
        if (*pChan->udr == pChan->pTxBufBuffered[pChan->comm.txRxComparePos]) {
            pChan->comm.txRxComparePos++;
            if (pChan->txBufBufferedPos == pChan->comm.txRxComparePos) {
                // all chars have been sent correctly
                pChan->txBufBufferedPos = 0;
                pChan->comm.txRetryCnt = 0;
                TimerStop(pChan);
            } else {
                TimerStart(pChan, INTERCHAR_TIMEOUT);
            }
        } else {
            // collision detected
            pChan->comm.state = eTxJamming;
            TimerStop(pChan);
            StopTx(pChan);
            *pChan->udr = 0; // tx jam character
            pChan->comm.txRetryCnt++;
            pChan->comm.txJamCnt = 0;
            // after stopping tx some chars are sent till tx buffer and shift reg
            // are empty. comm.state will be set to eTxPending in Timeout
            // function after these chars are flushed out
        }
        break;
    case eTxJamming:
        TimerStop(pChan);
        // discard received char
        *pChan->udr;
        break;
    case eTxStopped:
        // discard received char
        *pChan->udr;
        break;
    case eTxPending:
        TimerStart(pChan, pChan->comm.txDelayTicks);
        doRx = true;
        break;
    case eIdle:
    default:
        pChan->comm.state = eRxing;
        TimerStart(pChan, INTERCHAR_TIMEOUT);
        doRx = true;
        break;
    }
    if (doRx) {
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
}

/*-----------------------------------------------------------------------------
*  setup Timer1 as free running timer (normal mode)
*  CompareA is used to generate USART0 timeout interrupt
*  CompareB is used to generate USART1 timeout interrupt
*/
static void TimerInit(void) {
    /* Timer 1 normal mode */
    TCCR1A = 0;
    TCCR1B = TIMER1_PRESCALER;
    TCCR1C = 0;
}

/*-----------------------------------------------------------------------------
* after delayTicks the timer ISR is called
*/
static void TimerStart(TChanDesc *pChan, uint16_t delayTicks) {
    *pChan->timerOcr = TCNT1 + delayTicks;

    if ((TIMSK & pChan->timerIntMsk) == 0) {
        TIFR = pChan->timerIntFlg; // clear by writing 1
        TIMSK |= pChan->timerIntMsk;
    }
}

/*-----------------------------------------------------------------------------
* disable the timer interrupt
*/
static void TimerStop(TChanDesc *pChan) {
    TIMSK &= ~(pChan->timerIntMsk);
}

/*-----------------------------------------------------------------------------
* Timer ISR
*/
static void Timeout(int handle) {
    TChanDesc  *pChan;

    pChan = &sChan[handle];
    TimerStop(pChan);
    switch (pChan->comm.state) {
    case eRxing:
        pChan->comm.state = eIdle;
        if (pChan->extIntEIMSK != 0) {
            *pChan->extIntEIFR = pChan->extIntEIFRVal;
            *pChan->extIntEIMSK |= pChan->extIntEIMSKVal;
        }
        break;
    case eTxStopped:
        if (pChan->comm.txRetryCnt < MAX_TX_RETRY) {
            // timeout on eTxing will appear on tx collision detection
            pChan->comm.state = eTxPending;
            // txRetryCnt = 1:  delayTicks = 2 .. 17 ms
            // txRetryCnt = 2:  delayTicks = 2 .. 33 ms
            // txRetryCnt = 3:  delayTicks = 2 .. 49 ms
            // ...
            // txRetryCnt = 15: delayTicks = 2 .. 241 ms
            pChan->comm.txDelayTicks = INTERCHAR_TIMEOUT + MyRand() % (pChan->comm.txRetryCnt * 16) * TIMER1_MS1;
            TimerStart(pChan, pChan->comm.txDelayTicks);
        } else {
            // skip current BufferedSend buffer
            pChan->txBufBufferedPos = 0;
            pChan->comm.txRetryCnt = 0;
        }
        break;
    case eTxPending:
        pChan->comm.state = eIdle;
        SioSendBuffer(handle);
        break;
    default:
        pChan->comm.state = eIdle;
        break;
    }
}

/*-----------------------------------------------------------------------------
*  timer interrupt for USART0
*/
ISR(TIMER1_COMPA_vect)  {
    Timeout(0);
}

/*-----------------------------------------------------------------------------
*  timer interrupt for USART1
*/
ISR(TIMER1_COMPB_vect)  {
    Timeout(1);
}

/*-----------------------------------------------------------------------------
*  interrupt rx start bit
*/
static void RxStartDetectionInit(void) {
#ifdef USART0_EXTINT
    *USART0_EICR  = (*USART0_EICR & ~USART0_EICR_MSK) | USART0_EICR_VAL;
    *USART0_EIFR  = USART0_EIFR_VAL;
    *USART0_EIMSK |= USART0_EIMSK_VAL;
#endif
#ifdef USART1_EXTINT
    *USART1_EICR  = (*USART1_EICR & ~USART1_EICR_MSK) | USART1_EICR_VAL;
    *USART1_EIFR  = USART1_EIFR_VAL;
    *USART1_EIMSK |= USART1_EIMSK_VAL;
#endif
}



static void RxStart(int handle) {
    TChanDesc  *pChan = &sChan[handle];

    if (pChan->comm.state == eIdle) {
        pChan->comm.state = eRxing;
        TimerStart(pChan, INTERCHAR_TIMEOUT);
    }
    *pChan->extIntEIMSK &= ~pChan->extIntEIMSKVal;
}

#ifdef USART1_EXTVECT 
ISR(USART1_EXTVECT) {
    RxStart(1);
}
#endif
