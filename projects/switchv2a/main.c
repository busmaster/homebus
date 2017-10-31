/*
 * main.c
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
#define MODUL_ADDRESS        0
#define OSCCAL_CORR          1

/* configure port pins for switch/button terminals to inputs           */
/* with internal pullup, motion detector input without pullup          */
/* configure unused pins to output low                                 */
#define PORT_C_CONFIG_DIR     2  /* bitmask for port c:                */
                                 /* 1 output, 0 input                  */
                                 /* switch/button:   0b11111100 = 0xfc */
                                 /* motion detector: 0b11111111 = 0xff */
#define PORT_C_CONFIG_PORT    3  /* bitmask for port c:                */
                                 /* 1 pullup, 0 hi-z for inputs        */
                                 /* switch/button:   0b00000011 = 0x03 */
                                 /* motion detector: 0b00000000 = 0x00 */

/* RXD und INT0 input without pullup                                   */
/* TXD output high, motion detector input without pullup               */
/* other pins output low                                               */
#define PORT_D_CONFIG_DIR     4  /* bitmask for port d:                */
                                 /* 1 output, 0 input                  */
                                 /* switch/button:   0b11111010 = 0xfa */
                                 /* motion detector: 0b11110010 = 0xf2 */
#define PORT_D_CONFIG_PORT    5  /* bitmask for port d:                */
                                 /* 1 pullup, 0 hi-z for inputs        */
                                 /* switch/button:   0b00100010 = 0x22 */
                                 /* motion detector: 0b00100010 = 0x22 */


#define INPUT_TYPE            6  /* 0: button at portc 0/1       */
                                 /* 1: switch at portc 0/1       */
                                 /* 2: switch at portd 3         */

#define CLIENT_ADDRESS_BASE   7  /* size: 16 bytes (BUS_MAX_CLIENT_NUM) */
#define CLIENT_RETRY_CNT      (CLIENT_ADDRESS_BASE + BUS_MAX_CLIENT_NUM) 
                                 /* size: 16 bytes (BUS_MAX_CLIENT_NUM) */

#define STARTUP_DELAY  10 /* delay in seconds */

/* acual value event */
#define RESPONSE_TIMEOUT_MS         100  /* time in ms */
/* timeout for unreachable client  */
#define RETRY_CYCLE_TIME_MS         200 /* time in ms */

/* Button pressed telegram repetition time (time granularity is 16 ms) */
#define BUTTON_TELEGRAM_REPEAT_DIFF    48  /* time in ms */

/* Button pressed telegram is repeated for maximum time if button stays closed */
#define BUTTON_TELEGRAM_REPEAT_TIMEOUT 8  /* time in seconds */

/* our bus address */
#define MY_ADDR    sMyAddr

#define IDLE_SIO  0x01

#define INPUT_TYPE_DUAL_BUTTON        0
#define INPUT_TYPE_DUAL_SWITCH        1
#define INPUT_TYPE_MOTION_DETECTOR    2

#define INPUT_BUTTON           (PINC & 0b00000011) /* two lower bits on port C*/
#define INPUT_SWITCH           (~PINC & 0b00000011) /* two lower bits on port C*/
#define INPUT_MOTION_DETECTOR  ((PIND & 0b00001000) >> 3) /* port D pin 3 */

/* Port D bit 5 controls bus transceiver power down */
#define BUS_TRANSCEIVER_POWER_DOWN \
   PORTD |= (1 << 5)

#define BUS_TRANSCEIVER_POWER_UP \
   PORTD &= ~(1 << 5)

/* button telegram state machine */
#define BUTTON_TX_OFF     0
#define BUTTON_TX_ON      1
#define BUTTON_TX_TIMEOUT 2

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

/*-----------------------------------------------------------------------------
*  Variables
*/
char version[] = "Sw88 2.00";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t    gTimeMs;
volatile uint16_t   gTimeMs16;
volatile uint16_t   gTimeS;

static TClient      sClient[BUS_MAX_CLIENT_NUM];
static uint8_t      sNumClients;

static uint8_t      sMyAddr;

static uint8_t      sInputType;

static uint8_t      sIoStateOld;
static uint8_t      sIoStateCur;

static uint8_t      sIdle = 0;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void    PortInit(void);
static void    TimerInit(void);
static void    Idle(void);
static void    ProcessButton(uint8_t buttonState);
static void    ProcessBus(void);
static void    SendStartupMsg(void);
static void    IdleSio(bool setIdle);
static void    BusTransceiverPowerDown(bool powerDown);
static uint8_t GetInputState(void);
static void    CheckEvent(void);
static void    GetClientListFromEeprom(void);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {

    int     sioHandle;
    uint8_t inputState;

    MCUSR = 0;
    wdt_disable();

    /* get module address from EEPROM */
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
    GetClientListFromEeprom();

    sInputType = eeprom_read_byte((const uint8_t *)INPUT_TYPE);
    if (sInputType > INPUT_TYPE_MOTION_DETECTOR) {
        sInputType = INPUT_TYPE_DUAL_BUTTON;
    }
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

    if ((sInputType == INPUT_TYPE_DUAL_SWITCH) ||
        (sInputType == INPUT_TYPE_MOTION_DETECTOR)) {
        /* wait for controller startup delay for sending first state telegram */
        DELAY_S(STARTUP_DELAY);
    }

    if ((sInputType == INPUT_TYPE_DUAL_SWITCH) ||
        (sInputType == INPUT_TYPE_MOTION_DETECTOR)) {
        sIoStateCur = GetInputState();
        sIoStateOld = ~sIoStateCur;
        CheckEvent();
    } 

    while (1) {
        Idle();

        inputState = GetInputState();
        if ((sInputType == INPUT_TYPE_DUAL_SWITCH) ||
            (sInputType == INPUT_TYPE_MOTION_DETECTOR)) {
            sIoStateCur = inputState;
            CheckEvent();
        } else if (sInputType == INPUT_TYPE_DUAL_BUTTON) {
            ProcessButton(inputState);
        }
        ProcessBus();
   }
   return 0;  /* never reached */
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

    if (sNumClients == 0) {
        return;
    }

    /* do the change detection not in each cycle */
    GET_TIME_MS16(actualTime16);
    if (sIoStateCur != sIoStateOld) {
        sIoStateOld = sIoStateCur;
        sActualClient = 0;
        sNewClientCycleDelay = false;
        InitClientState();
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
        pActVal->devType = eBusDevTypeSw8;
        pActVal->actualValue.sw8.state = sIoStateCur;
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
*  get the button or switch state
*/
static uint8_t GetInputState(void) {

    uint8_t state;

    switch (sInputType) {
    case INPUT_TYPE_DUAL_BUTTON:
        state = INPUT_BUTTON;
        break;
    case INPUT_TYPE_DUAL_SWITCH:
        state = INPUT_SWITCH;
        break;
    case INPUT_TYPE_MOTION_DETECTOR:
        state = INPUT_MOTION_DETECTOR;
        break;
    default:
        state = 0xff;
        break;
    }
    return state;
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

/*-----------------------------------------------------------------------------
*  send button pressed telegram
*  telegram is repeated every 48 ms for max 8 secs
*/
static void ProcessButton(uint8_t buttonState) {

    static uint8_t  sSequenceState = BUTTON_TX_OFF;
    static uint16_t sStartTimeS;
    static uint8_t  sTimeStampMs;
    bool            pressed;
    uint16_t        actualTimeS;
    uint8_t         actualTimeMs;

    /* buttons pull down pins to ground when pressed */
    switch (buttonState) {
    case 0:
        sTxBusMsg.type = eBusButtonPressed1_2;
        pressed = true;
        break;
    case 1:
        sTxBusMsg.type = eBusButtonPressed2;
        pressed = true;
        break;
    case 2:
        sTxBusMsg.type = eBusButtonPressed1;
        pressed = true;
        break;
    default:
        pressed = false;
        break;
    }

    switch (sSequenceState) {
    case BUTTON_TX_OFF:
        if (pressed) {
            /* begin transmission of eBusButtonPressed* telegram */
            GET_TIME_S(sStartTimeS);
            sTimeStampMs = GET_TIME_MS;
            sTxBusMsg.senderAddr = MY_ADDR;
            BusSend(&sTxBusMsg);
            sSequenceState = BUTTON_TX_ON;
        }
        break;
    case BUTTON_TX_ON:
        if (pressed) {
            GET_TIME_S(actualTimeS);
            if ((uint16_t)(actualTimeS - sStartTimeS) < BUTTON_TELEGRAM_REPEAT_TIMEOUT) {
                actualTimeMs = GET_TIME_MS;
                if ((uint8_t)(actualTimeMs - sTimeStampMs) >= BUTTON_TELEGRAM_REPEAT_DIFF) {
                    sTimeStampMs = GET_TIME_MS;
                    sTxBusMsg.senderAddr = MY_ADDR;
                    BusSend(&sTxBusMsg);
                }
            } else {
                sSequenceState = BUTTON_TX_TIMEOUT;
            }
        } else {
            sSequenceState = BUTTON_TX_OFF;
        }
        break;
    case BUTTON_TX_TIMEOUT:
        if (!pressed) {
            sSequenceState = BUTTON_TX_OFF;
        }
        break;
    default:
        sSequenceState = BUTTON_TX_OFF;
        break;
    }
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
        case eBusDevReqActualValue:
        case eBusDevReqSetClientAddr:
        case eBusDevReqGetClientAddr:
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
        case eBusDevReqActualValue:
            sTxBusMsg.senderAddr = MY_ADDR;
            sTxBusMsg.type = eBusDevRespActualValue;
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxBusMsg.msg.devBus.x.devResp.actualValue.devType = eBusDevTypeSw8;
            sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.sw8.state = sIoStateCur;
            BusSend(&sTxBusMsg);
            break;
        case eBusDevRespActualValueEvent:
            pClient = sClient;
            for (i = 0; i < sNumClients; i++) {
                if ((pClient->address == spRxBusMsg->senderAddr) &&
                    (pClient->state == eWaitForConfirmation)) {
                    if (spRxBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.sw8.state == sIoStateCur) {
                        pClient->state = eConfirmationOK;
                    }
                    break;
                }
                pClient++;
            }
            break;
        case eBusDevReqSetClientAddr:
            sTxBusMsg.senderAddr = MY_ADDR;
            sTxBusMsg.type = eBusDevRespSetClientAddr;
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
                p = &(spRxBusMsg->msg.devBus.x.devReq.setClientAddr.clientAddr[i]);
                eeprom_write_byte((uint8_t *)(CLIENT_ADDRESS_BASE + i), *p);
            }
            BusSend(&sTxBusMsg);
            break;
        case eBusDevReqGetClientAddr:
            sTxBusMsg.senderAddr = MY_ADDR;
            sTxBusMsg.type = eBusDevRespGetClientAddr;
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
                p = &(sTxBusMsg.msg.devBus.x.devResp.getClientAddr.clientAddr[i]);
                *p = eeprom_read_byte((const uint8_t *)(CLIENT_ADDRESS_BASE + i));
            }
            BusSend(&sTxBusMsg);
            break;
        case eBusDevReqInfo:
            sTxBusMsg.type = eBusDevRespInfo;
            sTxBusMsg.senderAddr = MY_ADDR;
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxBusMsg.msg.devBus.x.devResp.info.devType = eBusDevTypeSw8;
            strncpy((char *)(sTxBusMsg.msg.devBus.x.devResp.info.version),
                    version, BUS_DEV_INFO_VERSION_LEN);
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
}

/*-----------------------------------------------------------------------------
*  port init
*/
static void PortInit(void) {

    uint8_t port;
    uint8_t ddr;

    /* configure pins b to output low */
    PORTB = 0b00000000;
    DDRB =  0b11111111;

    /* get port c direction config from EEPROM */
    ddr = eeprom_read_byte((const uint8_t *)PORT_C_CONFIG_DIR);
    /* get port c port config from EEPORM*/
    port = eeprom_read_byte((const uint8_t *)PORT_C_CONFIG_PORT);
    PORTC = port;
    DDRC =  ddr;

    /* get port d direction config from EEPROM */
    ddr = eeprom_read_byte((const uint8_t *)PORT_D_CONFIG_DIR);
    /* get port d port config from EEPORM*/
    port = eeprom_read_byte((const uint8_t *)PORT_D_CONFIG_PORT);
    PORTD = port;
    DDRD =  ddr;
}

/*-----------------------------------------------------------------------------
*  timer init
*/
static void TimerInit(void) {

    /* configure Timer 0 */
    /* prescaler clk/64 -> Interrupt period 256/1000000 * 64 = 16.384 ms */
    TCCR0B = 3 << CS00;
    TIMSK0 = 1 << TOIE0;
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
    /* ms counter */
    gTimeMs16 += 16;
    gTimeMs += 16;
    intCnt++;
    if (intCnt >= 61) { /* 16.384 ms * 61 = 1 s*/
        intCnt = 0;
        gTimeS++;
    }
}
