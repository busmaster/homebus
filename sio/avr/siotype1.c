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
#include "sio.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

#ifndef SIO_RX_BUF_SIZE
#define SIO_RX_BUF_SIZE 16  /* must be power of 2 */
#endif
#ifndef SIO_TX_BUF_SIZE
#define SIO_TX_BUF_SIZE 32  /* must be power of 2 */
#endif

#if (F_CPU == 1000000UL)
#define UBRR_9600   12     /* 9600 @ 1MHz  */
#define BRX2_9600   true   /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER1_PRESCALER (0b100 << CS10) // prescaler clk/256
#define TIMER1_MS1       4               // 4 * 256 / 1000000 = 1
#define TIMER1_MS2       8               // 8 * 256 / 1000000 = 2

#elif (F_CPU == 8000000UL)
#define UBRR_9600   51     /* 9600 @ 8MHz  */
#define BRX2_9600   false  /* double baud rate */
#define ERR_9600    false  /* baud rate is possible */

/* intercharacter timeout: 2 characters @ 9600, 10 bit = 2 ms */
#define TIMER1_PRESCALER (0b101 << CS10) // prescaler clk/1024
#define TIMER1_MS1       8               // 8  * 1024 / 8000000 = 1
#define TIMER1_MS2       16              // 16 * 1024 / 8000000 = 2

#else
#error adjust baud rate settings for your CPU clock frequency
#endif

#define MAX_TX_RETRY  16
#define INTERCHAR_TIMEOUT  TIMER1_MS2

#define MAX_JAM_CNT   8

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

    UCSR0A = 1 << TXC0;
    UDR0;

    if (doubleBr == true) {
        /* double baud rate */
        UCSR0A |= 1 << U2X0;
    }

    UCSR0C = ucsrc;

    sComm.state = eIdle;
    sComm.txRetryCnt = 0;
    sComm.txRxComparePos = 0;
    sComm.txDelayTicks = 0;
    sComm.txStartup = false;

    sIdleFunc = 0;
    sBusTransceiverPowerDownFunc = 0;
    sRxBufWrIdx = 0;
    sRxBufRdIdx = SIO_RX_BUF_SIZE - 1; /* last index read */
    sTxBufWrIdx = 0;
    sTxBufRdIdx = 0;
    sTxBufBufferedPos = 0;

    /* enable the receiver und transmitter */
    ucsrb |= (1 << RXEN0) | (1 << RXCIE0) | (1 << TXEN0);
    ucsrb &= ~(1 << UDRIE0);
    UCSR0B = ucsrb;

    return 0;
}

/*-----------------------------------------------------------------------------
*  set sio to reset state
*/
int SioClose(int handle) {
    
    UCSR0B = 0;
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0A = 1 << TXC0;
    UBRR0H = 0;
    UBRR0L = 0;
    UDR0;
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
ISR(USART_UDRE_vect) {
    bool lastChar = false;
    uint8_t rdIdx = sTxBufRdIdx;
    uint8_t txChar;

    if (sComm.state == eTxJamming) {
        sComm.txJamCnt++;
        if (sComm.txJamCnt < MAX_JAM_CNT) {
            UDR0 = 0; // 0 is the jam character
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
            UCSR0B &= ~(1 << UDRIE0);
            return;
        }
        /* get next character from tx buffer */
        rdIdx++;
        rdIdx &= (SIO_TX_BUF_SIZE - 1);
        txChar = sTxBuffer[rdIdx];

        if (sComm.txStartup) {
            sComm.txStartup = false;
            if ((EIFR & (1 << INTF0)) == 0) {
                UDR0 = txChar;
            } else {
                EIFR = 1 << INTF0;
                sComm.txDelayTicks = INTERCHAR_TIMEOUT;
                TimerStart(INTERCHAR_TIMEOUT);
                sComm.state = eTxPending;
                StopTx();
                UCSR0B &= ~(1 << UDRIE0);
                return;
            }
        } else {
            UDR0 = txChar;
        }
        sTxBufRdIdx = rdIdx;
        sComm.state = eTxing;

        /* quit transmit complete interrupt (by writing a 1)      */
        if ((UCSR0A & (1 << TXC0)) != 0) {
            UCSR0A |= (1 << TXC0);
        }
    } else {
        lastChar = true;
    }

    if (lastChar) {
        /* enable transmit complete interrupt */
        UCSR0B |= (1 << TXCIE0);
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
    /* disable transmit complete interrupt */
    UCSR0B &= ~(1 << TXCIE0);

    if (sComm.state == eTxJamming) {
        sComm.state = eTxStopped;
        TimerStart(INTERCHAR_TIMEOUT);
    } else {
        sComm.state = eIdle;
        EIFR = 1 < INTF0;
        EIMSK |= (1 << INT0);
    }
}

/*-----------------------------------------------------------------------------
*  USART rx complete interrupt
*/
ISR(USART_RX_vect) {
    uint8_t wrIdx;
    bool doRx = false;

    switch (sComm.state) {
    case eTxing:
        // read back char from tx received
        if (UDR0 == sTxBufferBuffered[sComm.txRxComparePos]) {
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
            UDR0 = 0; // tx jam character
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
        UDR0;
        break;
    case eTxStopped:
        // discard received char
        UDR0;
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
            sRxBuffer[wrIdx] = UDR0;
        } else {
            /* buffer full */
            wrIdx--;
            wrIdx &= (SIO_RX_BUF_SIZE - 1);
            /* must read character to quit interrupt */
            UDR0;
        }
        sRxBufWrIdx = wrIdx;
        /* do not go idle when characters are in rxbuffer */
        if (sIdleFunc != 0) {
            sIdleFunc(false);
        }
    }
}

/*-----------------------------------------------------------------------------
*  setup Timer1 as free running timer (normal mode)
*  CompareA is used to generate USART timeout interrupt
*/
static void TimerInit(void) {
    /* Timer 1 normal mode */
    TCCR1A = 0;
    TCCR1B = TIMER1_PRESCALER;
    TCCR1C = 0;
}

/*-----------------------------------------------------------------------------
*  reset Timer1
*/
static void TimerExit(void) {
    /* Timer 1 reset state */
    TIMSK1 = 0;
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1C = 0;
    TCNT1 = 0;
    OCR1A = 0;
    OCR1B = 0;
    ICR1 = 0;
    TIFR1 = TIFR1;
}

/*-----------------------------------------------------------------------------
* after delayTicks the timer ISR is called
*/
static void TimerStart(uint16_t delayTicks) {
    OCR1A = TCNT1 + delayTicks;

    if ((TIMSK1 & (1 << OCIE1A)) == 0) {
        TIFR1 = 1 << OCF1A; // clear by writing 1
        TIMSK1 |= 1 << OCIE1A;
    }
}

/*-----------------------------------------------------------------------------
* disable the timer interrupt
*/
static void TimerStop(void) {
    TIMSK1 &= ~(1 << OCIE1A);
}

/*-----------------------------------------------------------------------------
* Timer ISR
*/
static void Timeout(void) {
    TimerStop();
    switch (sComm.state) {
    case eRxing:
        sComm.state = eIdle;
        EIFR = 1 << INTF0;
        EIMSK |= 1 << INT0;
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
            sComm.txDelayTicks = INTERCHAR_TIMEOUT + MyRand() % (sComm.txRetryCnt * 16) * TIMER1_MS1;
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
ISR(TIMER1_COMPA_vect)  {
    Timeout();
}

/*-----------------------------------------------------------------------------
*  interrupt rx start bit
*/
static void RxStartDetectionInit(void) {
    /* ext. Int for begin of Rx (falling edge of start bit) */
    EICRA |= 1 << ISC01;
    EICRA &= ~(1 << ISC00); /* falling edge of INT0 */
    EIFR = 1 < INTF0;
    EIMSK |= 1 << INT0;
}

static void RxStartDetectionExit(void) {

    EIMSK &= ~(1 << INT0);
    EIFR = 1 < INTF0;
    EICRA &= ~(1 << ISC01);
}


ISR(INT0_vect) {
    if (sComm.state == eIdle) {
        sComm.state = eRxing;
        TimerStart(INTERCHAR_TIMEOUT);
    }
    EIMSK &= ~(1 << INT0);
}
