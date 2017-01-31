/*
 * siosingle.c for all ATmega (single channel support)
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
#include "sio_c.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
#if (F_CPU == 1000000UL)
#define UBRR_9600   12     /* 9600 @ 1MHz  */
#define BRX2_9600   true   /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

#ifdef SIO_TIMER_INIT_TCCR
/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER_PRESCALER (0b100 << SIO_TIMER_TCCRB_CS) // prescaler clk/256
#define TIMER_MS1       4               // 4 * 256 / 1000000 = 1
#define TIMER_MS2       8               // 8 * 256 / 1000000 = 2
#endif

#elif (F_CPU == 1843200UL)
#define UBRR_9600   11     /* 9600 @ 1.8432MHz  */
#define BRX2_9600   false  /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

#ifdef SIO_TIMER_INIT_TCCR
/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER_PRESCALER (0b100 << SIO_TIMER_TCCRB_CS) // prescaler clk/256
#define TIMER_MS1       8              // 8 * 256 / 1843200 = 1.1
#define TIMER_MS2      16              // 16 * 1024 / 3686400 = 2.2
#endif

#elif (F_CPU == 3686400UL)
#define UBRR_9600   23     /* 9600 @ 3.6864MHz  */
#define BRX2_9600   false  /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

#ifdef SIO_TIMER_INIT_TCCR
/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER_PRESCALER (0b101 << SIO_TIMER_TCCRB_CS) // prescaler clk/1024
#define TIMER_MS1       4              // 4 * 1024 / 3686400 = 1.1
#define TIMER_MS2       8              // 8 * 1024 / 3686400 = 2.2
#endif

#elif (F_CPU == 7372800UL)
#define UBRR_9600   47     /* 9600 @ 7.3728MHz  */
#define BRX2_9600   false  /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

#ifdef SIO_TIMER_INIT_TCCR
/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER_PRESCALER (0b101 << SIO_TIMER_TCCRB_CS) // prescaler clk/1024
#define TIMER_MS1       8              // 8 * 1024 / 7372800 = 1.1
#define TIMER_MS2       16             // 16 * 1024 / 7372800 = 2.2
#endif

#elif (F_CPU == 8000000UL)
#define UBRR_9600   51     /* 9600 @ 8MHz  */
#define BRX2_9600   false  /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

#ifdef SIO_TIMER_INIT_TCCR
/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER_PRESCALER (0b101 << SIO_TIMER_TCCRB_CS) // prescaler clk/1024
#define TIMER_MS1       8               // 8  * 1024 / 8000000 = 1
#define TIMER_MS2       16              // 16 * 1024 / 8000000 = 2
#endif

#elif (F_CPU == 14745600UL)
#define UBRR_9600   95     /* 9600 @ 14.7456MHz  */
#define BRX2_9600   false  /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

#ifdef SIO_TIMER_INIT_TCCR
/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER_PRESCALER (0b101 << SIO_TIMER_TCCRB_CS) // prescaler clk/1024
#define TIMER_MS1       18              // 18 * 1024 / 18432000 = 1
#define TIMER_MS2       36              // 36 * 1024 / 18432000 = 2
#endif

#elif (F_CPU == 18432000UL)
#define UBRR_9600   119    /* 9600 @ 18.432MHz  */
#define BRX2_9600   false  /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

#ifdef SIO_TIMER_INIT_TCCR
/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER_PRESCALER (0b101 << SIO_TIMER_TCCRB_CS) // prescaler clk/1024
#define TIMER_MS1       18              // 18 * 1024 / 18432000 = 1
#define TIMER_MS2       36              // 36 * 1024 / 18432000 = 2
#endif

#else
#error adjust baud rate settings for your CPU clock frequency
#endif

#define MAX_TX_RETRY  16
#define INTERCHAR_TIMEOUT  TIMER_MS2

#define MAX_JAM_CNT   8

#define PASTER(x,y)       x ## y
#define EVALUATOR(x,y)    PASTER(x,y)
#define N(x)              EVALUATOR(x,SIO)
#define PASTER2(x,y,z)    x##y##z
#define EVALUATOR2(x,y,z) PASTER2(x,y,z)
#define N2(x,z)           EVALUATOR2(x,SIO,z)

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

/*-----------------------------------------------------------------------------
*  Variables
*/
static TIdleStateFunc                 sIdleFunc;

static uint8_t                        sRxBuffer[SIO_RX_BUF_SIZE];
static uint8_t                        sRxBufWrIdx;
static uint8_t                        sRxBufRdIdx;
static uint8_t                        sTxBuffer[SIO_TX_BUF_SIZE];
static uint8_t                        sTxBufWrIdx;
static uint8_t                        sTxBufRdIdx;
/* interrupt buffer sTxBuffer can be filled up to (SIO_TX_BUF_SIZE - 1) only */
static uint8_t                        sTxBufferBuffered[SIO_TX_BUF_SIZE - 1];
static uint8_t                        sTxBufBufferedPos;
static TBusTransceiverPowerDownFunc   sBusTransceiverPowerDownFunc;
static TCommState                     sComm;

static uint16_t sRand;

/*-----------------------------------------------------------------------------
*  Functions
*/
static uint8_t GetNumTxFreeChar(int handle);
static void TimerInit(void);
static void TimerExit(void);
static void TimerStart(uint16_t delayTicks);
static void TimerStop(void);
static void RxStartDetectionInit(void);
static void RxStartDetectionExit(void);
static uint8_t Write(int handle, uint8_t *pBuf, uint8_t bufSize);

/*-----------------------------------------------------------------------------
*  init sio module
*/
void SioInit(void) {
   
    TimerInit();
    RxStartDetectionInit();
}

void SioExit(void) {
    TimerExit();
    RxStartDetectionExit();
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
        ucsrc |= (1 << N2(UCSZ,1)) | (1 << N2(UCSZ,0));
        ucsrb &= ~(1 << N2(UCSZ,2));
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
        ucsrc &= ~(1 << N2(UPM,1)) & ~(1 << N2(UPM,0));
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
        ucsrc &= ~(1 << N(USBS));
        break;
    default:
        error = true;
        break;
    }
    if (error == true) {
        return -1;
    }

    /* set baud rate */
    N2(UBRR,H) = (unsigned char)(ubrr >> 8);
    N2(UBRR,L) = (unsigned char)ubrr;

    N2(UCSR,A) = 1 << N(TXC);
    N(UDR);

    if (doubleBr == true) {
        /* double baud rate */
        N2(UCSR,A) |= 1 << N(U2X);
    }

    N2(UCSR,C) = ucsrc;

    sComm.state = eIdle;
    sComm.txRetryCnt = 0;
    sComm.txRxComparePos = 0;
    sComm.txDelayTicks = 0;
    sComm.txStartup = false;

    sIdleFunc = 0;
    sBusTransceiverPowerDownFunc = 0;
    sRxBufWrIdx = 0;
    sRxBufRdIdx = 0;
    sTxBufWrIdx = 0;
    sTxBufRdIdx = 0;
    sTxBufBufferedPos = 0;

    /* enable the receiver und transmitter */
    ucsrb |= (1 << N(RXEN)) | (1 << N(RXCIE)) | (1 << N(TXEN));
    ucsrb &= ~(1 << N(UDRIE));
    N2(UCSR,B) = ucsrb;

    return 0;
}

/*-----------------------------------------------------------------------------
*  set sio to reset state
*/
int SioClose(int handle) {

    N2(UCSR,B) = 0;
    N2(UCSR,C) = (1 << N2(UCSZ,1)) | (1 << N2(UCSZ,0));
    N2(UCSR,A) = 1 << N(TXC);
    N2(UBRR,H) = 0;
    N2(UBRR,L) = 0;
    N(UDR);

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
static uint8_t Write(int handle, uint8_t *pBuf, uint8_t bufSize) {
    uint8_t *pStart;
    uint8_t i;
    uint8_t txFree;
    uint8_t wrIdx;
    bool  flag;

    if (bufSize == 0) {
        return 0;
    }

    pStart = &sTxBuffer[0];
    txFree = GetNumTxFreeChar(handle);
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
    if (sTxBufWrIdx == sTxBufRdIdx) {
        sComm.txStartup = true;
        /* enable UART UDRE Interrupt */
        if ((N2(UCSR,B) & (1 << N(UDRIE))) == 0) {
            N2(UCSR,B) |= 1 << N(UDRIE);
        }
        /* disable transmit complete interrupt */
        if ((N2(UCSR,B) & (1 << N(TXCIE))) != 0) {
            N2(UCSR,B) &= ~(1 << N(TXCIE));
        }
        if (sBusTransceiverPowerDownFunc != 0) {
            sBusTransceiverPowerDownFunc(false); /* enable bus transmitter */
        }
    }
    sTxBufWrIdx = wrIdx;

    RESTORE_INT(flag);

    return bufSize;
}

/*-----------------------------------------------------------------------------
*  stop tx and clear tx buffer
*/
static void StopTx(void) {
    sTxBufRdIdx = sTxBufWrIdx;
}

/*-----------------------------------------------------------------------------
*  write data to tx buffer - do not yet start with tx
*/
uint8_t SioWriteBuffered(int handle, uint8_t *pBuf, uint8_t bufSize) {
    uint8_t   len;

    if ((sComm.state == eIdle) ||
        (sComm.state == eRxing)) {
        len = sizeof(sTxBufferBuffered) - sTxBufBufferedPos;
        len = min(len, bufSize);
        memcpy(&sTxBufferBuffered[sTxBufBufferedPos], pBuf, len);
        sTxBufBufferedPos += len;
    } else {
        return 0;
    }

    return len;
}

/*-----------------------------------------------------------------------------
*  trigger tx of buffer
*/
bool SioSendBuffer(int handle) {
    uint8_t bytesWritten;
    bool    rc;
    bool    flag;

    flag = DISABLE_INT;

    if (sComm.state == eIdle) {
        sComm.txRxComparePos = 0;
        bytesWritten = Write(handle, sTxBufferBuffered, sTxBufBufferedPos);
        if (bytesWritten == sTxBufBufferedPos) {
            rc = true;
        } else {
            rc = false;
        }
    } else if (sComm.state == eRxing) {
        sComm.txDelayTicks = INTERCHAR_TIMEOUT;
        sComm.state = eTxPending;
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
static uint8_t GetNumTxFreeChar(int handle) {
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

    flag = DISABLE_INT;

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
    bool    flag;

    bufSize = min(bufSize, SIO_RX_BUF_SIZE - 1);

    flag = DISABLE_INT;
    /* set back read index. so rx interrupt cannot write to undo buffer */
    rdIdx = sRxBufRdIdx;
    rdIdx -= bufSize;
    rdIdx &= (SIO_RX_BUF_SIZE - 1);

    /* set back write index if necessary */
    rxFreeLen = SIO_RX_BUF_SIZE - 1 - SioGetNumRxChar(handle);
    if (bufSize > rxFreeLen) {
        sRxBufWrIdx -= (bufSize - rxFreeLen);
    }
    sRxBufRdIdx = rdIdx;
    RESTORE_INT(flag);

    rdIdx++;
    rdIdx &= (SIO_RX_BUF_SIZE - 1);
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
ISR(SIO_UDRE_INT_VEC) {
    bool lastChar = false;
    uint8_t rdIdx = sTxBufRdIdx;
    uint8_t txChar;

    if (sComm.state == eTxJamming) {
        sComm.txJamCnt++;
        if (sComm.txJamCnt < MAX_JAM_CNT) {
            N(UDR) = 0; // 0 is the jam character
        } else {
            // stop jamming
            lastChar = true;
        }
    } else if (sTxBufWrIdx != rdIdx) {
        if (sComm.state == eRxing) {
            sComm.txDelayTicks = INTERCHAR_TIMEOUT;
            TimerStart(INTERCHAR_TIMEOUT);
            sComm.state = eTxPending;
            StopTx();
            N2(UCSR,B) &= ~(1 << N(UDRIE));
            return;
        }
        /* get next character from tx buffer */
        rdIdx++;
        rdIdx &= (SIO_TX_BUF_SIZE - 1);
        txChar = sTxBuffer[rdIdx];

        if (sComm.txStartup) {
            sComm.txStartup = false;
            if ((EIFR & (1 << SIO_EI_EIFR)) == 0) {
                N(UDR) = txChar;
            } else {
                EIFR = 1 << SIO_EI_EIFR;
                sComm.txDelayTicks = INTERCHAR_TIMEOUT;
                TimerStart(INTERCHAR_TIMEOUT);
                sComm.state = eTxPending;
                StopTx();
                N2(UCSR,B) &= ~(1 << N(UDRIE));
                return;
            }
        } else {
            N(UDR) = txChar;
        }
        sTxBufRdIdx = rdIdx;
        sComm.state = eTxing;

        /* quit transmit complete interrupt (by writing a 1)      */
        if ((N2(UCSR,A) & (1 << N(TXC))) != 0) {
            N2(UCSR,A) |= (1 << N(TXC));
        }
    } else {
        lastChar = true;
    }

    if (lastChar) {
        /* enable transmit complete interrupt */
        N2(UCSR,B) |= (1 << N(TXCIE));
        /* no character in tx buffer -> disable UART UDRE Interrupt */
        N2(UCSR,B) &= ~(1 << N(UDRIE));
    }
}

/*-----------------------------------------------------------------------------
*  USART tx complete interrupt
*  used for half duplex mode
*/
ISR(SIO_TXC_INT_VEC) {
    if (sBusTransceiverPowerDownFunc != 0) {
        sBusTransceiverPowerDownFunc(true); /* disable bus transmitter */
    }
    /* disable transmit complete interrupt */
    N2(UCSR,B) &= ~(1 << N(TXCIE));

    if (sComm.state == eTxJamming) {
        sComm.state = eTxStopped;
        TimerStart(INTERCHAR_TIMEOUT);
    } else {
        sComm.state = eIdle;
        EIFR = 1 < SIO_EI_EIFR;
        EIMSK |= (1 << SIO_EI_EIMSK);
    }
}

/*-----------------------------------------------------------------------------
*  USART rx complete interrupt
*/
ISR(SIO_RXC_INT_VEC) {
    uint8_t wrIdx;
    bool doRx = false;

    switch (sComm.state) {
    case eTxing:
        // read back char from tx received
        if (N(UDR) == sTxBufferBuffered[sComm.txRxComparePos]) {
            sComm.txRxComparePos++;
            if (sTxBufBufferedPos == sComm.txRxComparePos) {
                // all chars have been sent correctly
                sTxBufBufferedPos = 0;
                sComm.txRetryCnt = 0;
                TimerStop();
            } else {
                TimerStart(INTERCHAR_TIMEOUT);
            }
        } else {
            // collision detected
            sComm.state = eTxJamming;
            TimerStop();
            StopTx();
            N(UDR) = 0; // tx jam character
            sComm.txRetryCnt++;
            sComm.txJamCnt = 0;
            // after stopping tx some chars are sent till tx buffer and shift reg
            // are empty. comm.state will be set to eTxPending in Timeout
            // function after these chars are flushed out
        }
        break;
    case eTxJamming:
        TimerStop();
        // discard received char
        N(UDR);
        break;
    case eTxStopped:
        // discard received char
        N(UDR);
        break;
    case eTxPending:
        TimerStart(sComm.txDelayTicks);
        doRx = true;
        break;
    case eIdle:
    default:
        sComm.state = eRxing;
        TimerStart(INTERCHAR_TIMEOUT);
        doRx = true;
        break;
    }
    if (doRx) {
        wrIdx = sRxBufWrIdx;
        wrIdx++;
        wrIdx &= (SIO_RX_BUF_SIZE - 1);
        if (wrIdx != sRxBufRdIdx) {
            sRxBuffer[wrIdx] = N(UDR);
        } else {
            /* buffer full */
            wrIdx--;
            wrIdx &= (SIO_RX_BUF_SIZE - 1);
            /* must read character to quit interrupt */
            N(UDR);
        }
        sRxBufWrIdx = wrIdx;
        /* do not go idle when characters are in rxbuffer */
        if (sIdleFunc != 0) {
            sIdleFunc(false);
        }
    }
}

/*-----------------------------------------------------------------------------
*  setup TimerX as free running timer (normal mode)
*  CompareA is used to generate USART timeout interrupt
*/
static void TimerInit(void) {

#ifdef SIO_TIMER_INIT_TCCR
    /* Timer normal mode */
    SIO_TIMER_TCCRA = 0;
    SIO_TIMER_TCCRB = TIMER_PRESCALER;
    SIO_TIMER_TCCRC = 0;
#endif
}

/*-----------------------------------------------------------------------------
*  reset Timer
*/
static void TimerExit(void) {
    /* Timer 1 reset state */
    SIO_TIMER_TIMSK &= ~(1 << SIO_TIMER_TIMSK_OCIE);
#ifdef SIO_TIMER_INIT_TCCR
    SIO_TIMER_TCCRA = 0;
    SIO_TIMER_TCCRB = 0;
    SIO_TIMER_TCCRC = 0;
#endif
    SIO_TIMER_OCR = 0;
    SIO_TIMER_TIFR = 1 << SIO_TIMER_TIFR_OCF; // clear by writing 1
}

/*-----------------------------------------------------------------------------
* after delayTicks the timer ISR is called
*/
static void TimerStart(uint16_t delayTicks) {
    
    SIO_TIMER_OCR = SIO_TIMER_TCNT + delayTicks;

    if ((SIO_TIMER_TIMSK & (1 << SIO_TIMER_TIMSK_OCIE)) == 0) {
        SIO_TIMER_TIFR = 1 << SIO_TIMER_TIFR_OCF; // clear by writing 1
        SIO_TIMER_TIMSK |= 1 << SIO_TIMER_TIMSK_OCIE;
    }
}

/*-----------------------------------------------------------------------------
* disable the timer interrupt
*/
static void TimerStop(void) {
    SIO_TIMER_TIMSK &= ~(1 << SIO_TIMER_TIMSK_OCIE);
}

/*-----------------------------------------------------------------------------
* Timer ISR
*/
static void Timeout(void) {
    TimerStop();
    switch (sComm.state) {
    case eRxing:
        sComm.state = eIdle;
        EIFR = 1 << SIO_EI_EIFR;
        EIMSK |= 1 << SIO_EI_EIMSK;
        break;
    case eTxStopped:
        if (sComm.txRetryCnt < MAX_TX_RETRY) {
            // timeout on eTxing will appear on tx collision detection
            sComm.state = eTxPending;
            // txRetryCnt = 1:  delayTicks = 2 .. 17 ms
            // txRetryCnt = 2:  delayTicks = 2 .. 33 ms
            // txRetryCnt = 3:  delayTicks = 2 .. 49 ms
            // ...
            // txRetryCnt = 15: delayTicks = 2 .. 241 ms
            sComm.txDelayTicks = INTERCHAR_TIMEOUT + MyRand() % (sComm.txRetryCnt * 16) * TIMER_MS1;
            TimerStart(sComm.txDelayTicks);
        } else {
            // skip current BufferedSend buffer
            sTxBufBufferedPos = 0;
            sComm.txRetryCnt = 0;
        }
        break;
    case eTxPending:
        sComm.state = eIdle;
        SioSendBuffer(0);
        break;
    default:
        sComm.state = eIdle;
        break;
    }
}

/*-----------------------------------------------------------------------------
*  check handle
*/
bool SioHandleValid(int handle) {
    return true;
}

/*-----------------------------------------------------------------------------
*  timer interrupt for USART
*/
ISR(SIO_TIMER_VEC)  {
    Timeout();
}

/*-----------------------------------------------------------------------------
*  interrupt rx start bit
*/
static void RxStartDetectionInit(void) {
    /* ext. Int for begin of Rx (falling edge of start bit) */
    EICRA |= 1 << SIO_EI_EICRA_SC1;
    EICRA &= ~(1 << SIO_EI_EICRA_SC0); /* falling edge of INTX */
    EIFR = 1 < SIO_EI_EIFR;
    EIMSK |= 1 << SIO_EI_EIMSK;
}

static void RxStartDetectionExit(void) {

    EIMSK &= ~(1 << SIO_EI_EIMSK);
    EIFR = 1 < SIO_EI_EIFR;
    EICRA &= ~(1 << SIO_EI_EICRA_SC1);
}


ISR(SIO_EI_VEC) {
    if (sComm.state == eIdle) {
        sComm.state = eRxing;
        TimerStart(INTERCHAR_TIMEOUT);
    }
    EIMSK &= ~(1 << SIO_EI_EIMSK);
}
