/*
 * main.c
 *
 * Copyright 2021 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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

/*
 * The keyboard type is a two wire keyboard that switches different resistors
 * values on keypress. Each button has a different resistor value. This firmware
 * uses the ADC on pin PC0. The keyboard is connected to GND and to a 1k pullup
 * to PC1. So a voltage divider is set up with 1k to PC1 (that is set to output
 * high) and the keyboard connected to GND.
 *
 * Supported keyboard types: gebatronic cody keypads and brandlabeled devices,
 * e.g Somfy
 *
 * */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#include "sio.h"
#include "sysdef.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
/* offset addresses in EEPROM */
#define MODUL_ADDRESS              0
#define OSCCAL_CORR                1


#define CLIENT_ADDRESS_BASE        2  /* size: 16 bytes (BUS_MAX_CLIENT_NUM) */
#define CLIENT_RETRY_CNT           (CLIENT_ADDRESS_BASE + BUS_MAX_CLIENT_NUM) 
                                   /* size: 16 bytes (BUS_MAX_CLIENT_NUM) */

/* acual value event */
#define RESPONSE_TIMEOUT_MS        100  /* time in ms */
/* timeout for unreachable client  */
#define RETRY_CYCLE_TIME_MS        200 /* time in ms */

/* our bus address */
#define MY_ADDR                    sMyAddr

#define IDLE_SIO                   0x01

/* Port D bit 5 controls bus transceiver power down */
#define BUS_TRANSCEIVER_POWER_DOWN PORTD |= (1 << 5)

#define BUS_TRANSCEIVER_POWER_UP   PORTD &= ~(1 << 5)

#define DEBOUNCE_CYCLES            4 /* 4 * Timer0 cyles */

#define BEEP_TIME_MS               100

/*-----------------------------------------------------------------------------
*  Typedefs
*/
typedef enum {
    eInit,
    eWaitForConfirmation,
    eConfirmationOK,
    eMaxRetry
} TState;

typedef struct {
    uint8_t  address;
    uint8_t  maxRetry;
    uint8_t  curRetry;
    TState   state;
    uint16_t requestTimeStamp;
} TClient;

typedef enum {
    eBeepOff,
    eBeepOn
} TBeepState;

/*-----------------------------------------------------------------------------
*  Variables
*/
static char sVersion[] = "Keyb 0.00";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t    gTimeMs;
volatile uint16_t   gTimeMs16;
volatile uint16_t   gTimeS;

static TClient      sClient[BUS_MAX_CLIENT_NUM];
static uint8_t      sNumClients;

static uint8_t      sMyAddr;
static uint8_t      sKeyCur = 0;          
static uint8_t      sKeyEvent = 0;
static uint8_t      sIdle = 0;
static bool         sStartBeep = false;
static TBeepState   sBeepState = eBeepOff;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void TimerInit(void);
static void Idle(void);
static void ProcessBus(void);
static void SendStartupMsg(void);
static void IdleSio(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);
static void CheckEvent(void);
static void GetClientListFromEeprom(void);
static void Beep(void);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {

    int sioHandle;

    MCUSR = 0;
    wdt_disable();

    /* get module address from EEPROM */
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
    GetClientListFromEeprom();

    PortInit();
    TimerInit();
    SioInit();
    SioRandSeed(sMyAddr);
    sioHandle = SioOpen("USART0", eSioBaud9600, eSioDataBits8, eSioParityNo,
                        eSioStopBits1, eSioModeHalfDuplex);
    SioSetIdleFunc(sioHandle, IdleSio);
    SioSetTransceiverPowerDownFunc(sioHandle, BusTransceiverPowerDown);

    BusTransceiverPowerDown(true);

    BusInit(sioHandle);
    spRxBusMsg = BusMsgBufGet();

    /* enable global interrupts */
    ENABLE_INT;

    SendStartupMsg();

    while (1) {
        Idle();
        CheckEvent();
        ProcessBus();
        Beep();
    }
    return 0;  /* never reached */
}

/*-----------------------------------------------------------------------------
*  Beep on button release
*/
static void Beep(void) {

    uint16_t        actualTime;
    static uint16_t sStartTime;

    switch (sBeepState) {
    case eBeepOff:
        if (sStartBeep) {
           sStartBeep = false;
           sBeepState = eBeepOn;
           GET_TIME_MS16(sStartTime);
           TCCR2B = 2 << CS00;
        }
        break;
    case eBeepOn:
        GET_TIME_MS16(actualTime);
        if (((uint16_t)(actualTime - sStartTime)) >= BEEP_TIME_MS) {
            sBeepState = eBeepOff;
            TCCR2B = 0;
            PORTC |= (1 << 1); /* ensure power supply for keyboard voltage divider */
        }
        break;
     default:
        break;
    }
}

/*-----------------------------------------------------------------------------
*  get next client array index
*  if all clients are processed 0xff is returned
*/
static uint8_t GetUnconfirmedClient(uint8_t actualClient) {

    uint8_t  i;
    uint8_t  nextClient;
    TClient  *pClient;

    if (actualClient >= sNumClients) {
        return 0xff;
    }

    for (i = 0; i < sNumClients; i++) {
        nextClient = actualClient + i +1;
        nextClient %= sNumClients;
        pClient = &sClient[nextClient];
        if (pClient->state == eMaxRetry) {
            continue;
        }
        if (pClient->state != eConfirmationOK) {
            break;
        }
    }
    if (i == sNumClients) {
        /* all client's confirmations received or retry count expired */
        nextClient = 0xff;
    }
    return nextClient;
}

static void InitClientState(void) {

    TClient *pClient;
    uint8_t  i;

    for (i = 0, pClient = sClient; i < sNumClients; i++) {
        pClient->state = eInit;
        pClient->curRetry = 0;
        pClient++;
    }
}

static void GetClientListFromEeprom(void) {

    TClient *pClient;
    uint8_t i;
    uint8_t numClients;
    uint8_t clientAddr;
    uint8_t retryCnt;

    for (i = 0, numClients = 0, pClient = sClient; i < BUS_MAX_CLIENT_NUM; i++) {
        clientAddr = eeprom_read_byte((const uint8_t *)(CLIENT_ADDRESS_BASE + i));
        retryCnt = eeprom_read_byte((const uint8_t *)(CLIENT_RETRY_CNT + i));
        if (clientAddr != BUS_CLIENT_ADDRESS_INVALID) {
            pClient->address = clientAddr;
            pClient->maxRetry = retryCnt;
            pClient->state = eInit;
            pClient++;
            numClients++;
        }
    }
    sNumClients = numClients;
}

/*-----------------------------------------------------------------------------
* post state changes to registered bus clients
*/
static void CheckEvent(void) {

    static uint8_t   sActualClient = 0xff; /* actual client's index being processed */
    TClient          *pClient;
    uint16_t         actualTime16;
    static bool      sNewClientCycleDelay = false;
    static uint16_t  sNewClientCycleTimeStamp;
    TBusDevReqActualValueEvent *pActVal;
    uint8_t          rc;
    bool             getNextClient;
    uint8_t          nextClient;
    static uint8_t   sKeyOld = 0;

    if (sNumClients == 0) {
        return;
    }

    /* do the change detection not in each cycle */
    GET_TIME_MS16(actualTime16);
    if (sKeyCur != sKeyOld) {
        if (sKeyCur != 0) {
            sKeyEvent = sKeyCur | 0x80; /* pressed */
        } else {
            sKeyEvent = sKeyOld;        /* released */
        }
        sKeyOld = sKeyCur;
        sActualClient = 0;
        sNewClientCycleDelay = false;
        InitClientState();
        if (sKeyCur == 0) {
           sStartBeep = true;
        }
    }

    if (sActualClient == 0xff) {
        return;
    }

    if (sNewClientCycleDelay) {
        if (((uint16_t)(actualTime16 - sNewClientCycleTimeStamp)) < RETRY_CYCLE_TIME_MS) {
            return;
        } else {
            sNewClientCycleDelay = false;
        }
    }

    pClient = &sClient[sActualClient];
    getNextClient = true;
    switch (pClient->state) {
    case eInit:
        pActVal = &sTxBusMsg.msg.devBus.x.devReq.actualValueEvent;
        sTxBusMsg.type = eBusDevReqActualValueEvent;
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.msg.devBus.receiverAddr = pClient->address;
        pActVal->devType = eBusDevTypeKeyb;
        pActVal->actualValue.keyb.keyEvent = sKeyEvent;
        rc = BusSend(&sTxBusMsg);
        if (rc == BUS_SEND_OK) {
            pClient->state = eWaitForConfirmation;
            pClient->requestTimeStamp = actualTime16;
        } else {
            getNextClient = false;
        }
        break;
    case eWaitForConfirmation:
        if ((((uint16_t)(actualTime16 - pClient->requestTimeStamp)) >= RESPONSE_TIMEOUT_MS) &&
            (pClient->state != eMaxRetry)) {
            if (pClient->curRetry < pClient->maxRetry) {
                /* try again */
                pClient->curRetry++;
                getNextClient = false;
                pClient->state = eInit;
            } else {
                pClient->state = eMaxRetry;
            }
        }
        break;
    case eConfirmationOK:
        break;
    default:
        break;
    }

    if (getNextClient) {
        nextClient = GetUnconfirmedClient(sActualClient);
        if (nextClient <= sActualClient) {
            sNewClientCycleDelay = true;
            sNewClientCycleTimeStamp = actualTime16;
        }
        sActualClient = nextClient;
    }
}

/*-----------------------------------------------------------------------------
*  switch to Idle mode
*/
static void Idle(void) {

    cli();
    if (sIdle == 0) {
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_enable();
        sei();
        sleep_cpu();
        sleep_disable();
    } else {
        sei();
    }
}


/*-----------------------------------------------------------------------------
*  sio idle enable
*/
static void IdleSio(bool setIdle) {

    if (setIdle == true) {
        sIdle &= ~IDLE_SIO;
    } else {
        sIdle |= IDLE_SIO;
    }
}

/*-----------------------------------------------------------------------------
*  switch bus transceiver power down/up
*  called from interrupt context or disabled interrupt, so disable interrupt
*  is not required here
*/
static void BusTransceiverPowerDown(bool powerDown) {

    if (powerDown) {
        BUS_TRANSCEIVER_POWER_DOWN;
    } else {
        BUS_TRANSCEIVER_POWER_UP;
    }
}

/* ADC values for keys (1k pullup):
3    0000   0000 .. 013f
6    0280   0140 .. 03bf
9    0500   03c0 .. 06df
bell 08c0   06e0 .. 0aff
2    0d40   0b00 .. 0fbf
5    1240   0fc0 .. 155f
8    1880   1560 .. 1cff
0    2180   1d00 .. 271f
1    2cc0   2720 .. 349f
4    3c80   34a0 .. 4a1f
7    57c0   4a20 .. 6dff
key  8440   6e00 .. 9a7f
*/
static uint8_t Adc2Key(uint16_t adc) {
    
    char key = '\0';

    if ((adc >= 0) && (adc < 0x140)) {
        key = '3';
    } else if ((adc >= 0x140)  && (adc < 0x3c0)) {
        key = '6';
    } else if ((adc >= 0x3c0)  && (adc < 0x6e0)) {
        key = '9';
    } else if ((adc >= 0x6e0)  && (adc < 0xb00)) {
        key = 'b';
    } else if ((adc >= 0xb00)  && (adc < 0xfc0)) {
        key = '2';
    } else if ((adc >= 0xfc0)  && (adc < 0x1560)) {
        key = '5';
    } else if ((adc >= 0x1560) && (adc < 0x1d00)) {
        key = '8';
    } else if ((adc >= 0x1d00) && (adc < 0x2720)) {
        key = '0';
    } else if ((adc >= 0x2720) && (adc < 0x34a0)) {
        key = '1';
    } else if ((adc >= 0x34a0) && (adc < 0x4a20)) {
        key = '4';
    } else if ((adc >= 0x4a20) && (adc < 0x6e00)) {
        key = '7';
    } else if ((adc >= 0x6e00) && (adc < 0x9a80)) {
        key = 'k';
    }

    return (uint8_t)key;
}

/*-----------------------------------------------------------------------------
*  process received bus telegrams
*/
static void ProcessBus(void) {

    uint8_t     ret;
    TClient     *pClient;
    TBusMsgType msgType;
    uint8_t     i;
    uint8_t     *p;
    bool        msgForMe = false;

    ret = BusCheck();

    if (ret == BUS_MSG_OK) {
        msgType = spRxBusMsg->type;
        switch (msgType) {
        case eBusDevReqReboot:
        case eBusDevReqInfo:
        case eBusDevReqSetAddr:
        case eBusDevReqEepromRead:
        case eBusDevReqEepromWrite:
        case eBusDevRespActualValueEvent:
            if (spRxBusMsg->msg.devBus.receiverAddr == MY_ADDR) {
               msgForMe = true;
            }
            break;
        default:
            break;
        }
    }
    if (msgForMe == false) {
        return;
    }

    switch (msgType) {
    case eBusDevReqReboot:
        /* reset controller with watchdog */
        /* set watchdog timeout to shortest value (14 ms) */
        cli();
        wdt_enable(WDTO_15MS);
        /* wait for reset */
        while (1);
        break;
    case eBusDevRespActualValueEvent:
        pClient = sClient;
        for (i = 0; i < sNumClients; i++) {
            if ((pClient->address == spRxBusMsg->senderAddr) &&
                (pClient->state == eWaitForConfirmation)) {
                if (spRxBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.keyb.keyEvent == sKeyEvent) {
                    pClient->state = eConfirmationOK;
                }
                break;
            }
            pClient++;
        }
        break;
    case eBusDevReqInfo:
        sTxBusMsg.type = eBusDevRespInfo;
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxBusMsg.msg.devBus.x.devResp.info.devType = eBusDevTypeKeyb;
        strncpy((char *)(sTxBusMsg.msg.devBus.x.devResp.info.version),
                sVersion, BUS_DEV_INFO_VERSION_LEN);
        sTxBusMsg.msg.devBus.x.devResp.info.version[BUS_DEV_INFO_VERSION_LEN - 1] = '\0';
        BusSend(&sTxBusMsg);
        break;
    case eBusDevReqSetAddr:
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.type = eBusDevRespSetAddr;
        sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        p = &(spRxBusMsg->msg.devBus.x.devReq.setAddr.addr);
        eeprom_write_byte((uint8_t *)MODUL_ADDRESS, *p);
        BusSend(&sTxBusMsg);
        break;
    case eBusDevReqEepromRead:
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.type = eBusDevRespEepromRead;
        sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxBusMsg.msg.devBus.x.devResp.readEeprom.data =
            eeprom_read_byte((const uint8_t *)spRxBusMsg->msg.devBus.x.devReq.readEeprom.addr);
        BusSend(&sTxBusMsg);
        break;
    case eBusDevReqEepromWrite:
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.type = eBusDevRespEepromWrite;
        sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        p = &(spRxBusMsg->msg.devBus.x.devReq.writeEeprom.data);
        eeprom_write_byte((uint8_t *)spRxBusMsg->msg.devBus.x.devReq.readEeprom.addr, *p);
        BusSend(&sTxBusMsg);
        break;
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------
*  port init
*/
static void PortInit(void) {

    /* configure pins b to output low */
    PORTB = 0b00000000;
    DDRB =  0b11111111;

    PORTC = 0b00000010;
    DDRC =  0b11111110; // PC1 out high, PC0 in highz 

    PORTD = 0b00100010;
    DDRD =  0b11111010;
    
    /* ADC */
    /* RESF1:0 = 01     select AVCC as AREF */
    /* ADLAR = 1        left adjust         */
    /* MUX3: = 0000     select ADC0 (PC0)   */
    ADMUX = 0b01100000;

    /* digital input disable for ADC0 */
    DIDR0 = 0b00000001;

    /* ADEN = 1         ADC enable */
    /* ADPS2:0 = 100    prescaler, division factor is 16 -> 1MHz/16 = 62500 Hz*/
    ADCSRA = 0b10000100;
    /* trigger first conversion */
    ADCSRA |= (1 << ADSC);
}

/*-----------------------------------------------------------------------------
*  timer init
*/
static void TimerInit(void) {

    /* configure Timer 0 (system timer) */
    /* prescaler clk/64 -> Interrupt period 256/1000000 * 64 = 16.384 ms */
    TCCR0B = 3 << CS00;
    TIMSK0 = 1 << TOIE0;

    /* Timer 1 is used by sio */

    /* configure Timer 2 (beep timer) */
    /* prescaler clk/8 -> Interrupt period OCR2A/1000000 * 8 = 1.024 ms -> 488 Hz Beep */
    TCCR2A = 2 << WGM20; /* CTC */
    TCCR2B = 0; /* stopped */
    OCR2A = 128;
    TIMSK2 = 1 << OCIE2A;
}

/*-----------------------------------------------------------------------------
*  send the startup msg. Delay depends on module address
*/
static void SendStartupMsg(void) {

    uint16_t startCnt;
    uint16_t cntMs;
    uint16_t diff;

    /* send startup message */
    /* delay depends on module address (address * 100 ms) */
    GET_TIME_MS16(startCnt);
    do {
        GET_TIME_MS16(cntMs);
        diff =  cntMs - startCnt;
    } while (diff < ((uint16_t)MY_ADDR * 100));

    sTxBusMsg.type = eBusDevStartup;
    sTxBusMsg.senderAddr = MY_ADDR;
    BusSend(&sTxBusMsg);
}

/*-----------------------------------------------------------------------------
*  Timer 0 overflow ISR
*  period:  16.384 ms
*/
ISR(TIMER0_OVF_vect) {

    static uint8_t intCnt = 0;
    static uint8_t sKey[DEBOUNCE_CYCLES];
    static uint8_t sKeyIdx = 0;
    uint16_t adc;
    uint8_t i;

    /* ms counter */
    gTimeMs16 += 16;
    gTimeMs += 16;
    intCnt++;
    if (intCnt >= 61) { /* 16.384 ms * 61 = 1 s*/
        intCnt = 0;
        gTimeS++;
    }
    
    if (sBeepState == eBeepOn) {
        return;
    }

    adc = ADCW;
    // trigger next conversion
    ADCSRA |= (1 << ADSC);

    if (adc < 0xffc0) {
       sKey[sKeyIdx] = Adc2Key(adc);
    } else {
       sKey[sKeyIdx] = 0;
    }
    sKeyIdx++;
    if (sKeyIdx >= DEBOUNCE_CYCLES) {
        sKeyIdx = 0;
    }
    for (i = 0; i < (ARRAY_CNT(sKey) - 1); i++) {
        if (sKey[i] != sKey[i + 1]) {
            break;
        }
    }
    if (i == (ARRAY_CNT(sKey) - 1)) {
        sKeyCur = sKey[0];
    }
}

/*-----------------------------------------------------------------------------
* Timer 2 comare A - Beep
*/
ISR(TIMER2_COMPA_vect) {

    PORTC ^= (1 << PC1);
}
