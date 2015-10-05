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
#include "button.h"

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
                                 /* 0b11111110 = 0xfe */
#define PORT_C_CONFIG_PORT    3  /* bitmask for port c:                */
                                 /* 1 pullup, 0 hi-z for inputs        */
                                 /* 0b00000001 = 0x01 */

/* RXD und INT0 input without pullup                                   */
/* TXD output high                                                     */
/* other pins output low                                               */
#define PORT_D_CONFIG_DIR     4  /* bitmask for port d:                */
                                 /* 1 output, 0 input                  */
                                 /* 0b11111010 = 0xfa */
#define PORT_D_CONFIG_PORT    5  /* bitmask for port d:                */
                                 /* 1 pullup, 0 hi-z for inputs        */
                                 /* 0b00100010 = 0x22 */

/* offset 6 unused */

#define CLIENT_ADDRESS_BASE   7  /* BUS_MAX_CLIENT_NUM from bus.h (16 bytes) */
#define CLIENT_RETRY_CNT      (APPLICATION_BUTTON_BASE + APPLICATION_BUTTON_SIZE) 
                                 /* size: 16 bytes (BUS_MAX_CLIENT_NUM)      */

/* application button: 3 bytes per button:     */
/* 1 byte for button address                   */
/* 1 byte for input number (1 or 2)            */
/* 1 byte for pressed delay in 100 msecs       */
#define APPLICATION_BUTTON_BASE              (CLIENT_ADDRESS_BASE + BUS_MAX_CLIENT_NUM)
#define MAX_NUM_APPLICATION_BUTTONS          3  /* uses 3 * 3 = 9 bytes */
#define SIZE_APPLICATION_BUTTON_DESC         3
#define APPLICATION_BUTTON_SIZE              (MAX_NUM_APPLICATION_BUTTONS * SIZE_APPLICATION_BUTTON_DESC) 

#define STARTUP_DELAY  10 /* delay in seconds */


/* our bus address */
#define MY_ADDR    sMyAddr

#define IDLE_SIO  0x01

#define INPUT_PIN           (~PINC & 0b00000001) /* bit0 on port C*/
#define OUTPUT_PIN_ON       PORTC |=  0b00000010  /* bit1 on port C*/
#define OUTPUT_PIN_OFF      PORTC &= ~0b00000010  /* bit1 on port C*/

#define IO_STATE            (INPUT_PIN | (PINC & 0b00000010))

/* Port D bit 5 controls bus transceiver power down */
#define BUS_TRANSCEIVER_POWER_DOWN \
   PORTD |= (1 << 5)

#define BUS_TRANSCEIVER_POWER_UP \
   PORTD &= ~(1 << 5)


#define DOUT_PULSE_DURATION_MS  1000 /* ms */

/* acual value event */
#define RESPONSE_TIMEOUT_MS         100  /* time in ms */
/* timeout for unreachable client  */
#define RETRY_CYCLE_TIME_MS         200 /* time in ms */
#define CHANGE_DETECT_CYCLE_TIME_MS 500 /* time in ms */

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

typedef struct {
    uint8_t  address;
    uint8_t  inputNum;
    uint16_t pressedDelayMs;
    uint16_t pressedTimeStamp;
    bool     isPressed;
} TApplicationButton;

/*-----------------------------------------------------------------------------
*  Variables
*/
char version[] = "Sw88_grg 0.03";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t  gTimeMs;
volatile uint16_t gTimeMs16;
volatile uint16_t gTimeS;

static TClient    sClient[BUS_MAX_CLIENT_NUM];
static uint8_t    sNumClients;

static uint8_t    sMyAddr;

static uint8_t    sIoStateOld;
static uint8_t    sIoStateCur;

static uint8_t    sIdle = 0;

static TApplicationButton sApplicationButton[MAX_NUM_APPLICATION_BUTTONS];

static bool       sDigOutIsOn = false;
static uint16_t   sDigoutOnTimestamp;

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
static void ButtonEvent(uint8_t address, uint8_t button);
static void CheckButton(void);
static void ApplicationEventButton(TButtonEvent *pButtonEvent);
static void ApplicationButtonInit(void);
static void CheckApplication(void);
static void CheckDigout(void);
static void CheckEvent(void);
static void GetClientListFromEeprom(void);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {

    int     sioHandle;

    MCUSR = 0;
    wdt_disable();

    /* get module address from EEPROM */
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
    GetClientListFromEeprom();    

    PortInit();
    TimerInit();
    ButtonInit();
    SioInit();
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

    /* wait for controller startup delay for sending first state telegram */
    DELAY_S(STARTUP_DELAY);

    ApplicationButtonInit();

    while (1) {
        Idle();
        ProcessBus();
        CheckButton();
        CheckApplication();
        CheckDigout();
        CheckEvent();        
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
*   post state changes to registered bus clients
*/
static void CheckEvent(void) {

    static uint8_t   sActualClient = 0xff; /* actual client's index being processed */
    static uint16_t  sChangeTestTimeStamp;
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
    if (((uint16_t)(actualTime16 - sChangeTestTimeStamp)) >= CHANGE_DETECT_CYCLE_TIME_MS) {
        sIoStateCur = IO_STATE;
        if (sIoStateCur != sIoStateOld) {
            sIoStateOld = sIoStateCur;
            sActualClient = 0;
            sNewClientCycleDelay = false;
            InitClientState();
        }
        sChangeTestTimeStamp = actualTime16;
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
*  digout
*/
void DigOutOn(void) {

    OUTPUT_PIN_ON;
}
void DigOutOff(void) {

    OUTPUT_PIN_OFF;
}

void DigOutTrigger(void) {

    DigOutOn();
    sDigOutIsOn = true;
    GET_TIME_MS16(sDigoutOnTimestamp);
}

static void CheckDigout(void) {

    uint16_t actualTime16;

    if (sDigOutIsOn) {
        GET_TIME_MS16(actualTime16);
        if (((uint16_t)(actualTime16 - sDigoutOnTimestamp)) >= DOUT_PULSE_DURATION_MS) {
            DigOutOff();
            sDigOutIsOn = false;
        }
    }
}

/*-----------------------------------------------------------------------------
*  check for button release
*/
static void CheckButton(void) {
    uint8_t        i = 0;
    TButtonEvent buttonEventData;

    while (ButtonReleased(&i) == true) {
        if (ButtonGetReleaseEvent(i, &buttonEventData) == true) {
            ApplicationEventButton(&buttonEventData);
        }
    }
}

/*-----------------------------------------------------------------------------
*  create button event for application
*/
static void ButtonEvent(uint8_t address, uint8_t button) {
    TButtonEvent buttonEventData;

    if (ButtonNew(address, button) == true) {
        buttonEventData.address = spRxBusMsg->senderAddr;
        buttonEventData.pressed = true;
        buttonEventData.buttonNr = button;
        ApplicationEventButton(&buttonEventData);
    }
}

/*-----------------------------------------------------------------------------
*  read application button data from eeprom
*/
static void ApplicationButtonInit(void) {

    uint8_t i;
    TApplicationButton *pAppButton = sApplicationButton;

    for (i = 0;
         i < (MAX_NUM_APPLICATION_BUTTONS * SIZE_APPLICATION_BUTTON_DESC);
         i += SIZE_APPLICATION_BUTTON_DESC) {
        pAppButton->address = eeprom_read_byte((const uint8_t *)(APPLICATION_BUTTON_BASE + i));
        pAppButton->inputNum = eeprom_read_byte((const uint8_t *)(APPLICATION_BUTTON_BASE + i + 1));
        pAppButton->pressedDelayMs = eeprom_read_byte((const uint8_t *)(APPLICATION_BUTTON_BASE + i + 2)) * 100;
        pAppButton->isPressed = false;

        pAppButton++;
    }
}


static void ApplicationEventButton(TButtonEvent *pButtonEvent) {

    uint8_t i;
    TApplicationButton *pAppButton = sApplicationButton;

    for (i = 0; i < MAX_NUM_APPLICATION_BUTTONS; i ++) {
        if ((pAppButton->address == pButtonEvent->address) &&
            (pAppButton->inputNum == pButtonEvent->buttonNr)) {
            if (pButtonEvent->pressed) {
                pAppButton->isPressed = true;
                if (pAppButton->pressedDelayMs == 0) {
                    DigOutOn();
                } else {
                    GET_TIME_MS16(pAppButton->pressedTimeStamp);
                }
            } else {
                DigOutOff();
                pAppButton->isPressed = false;
            }
        }
        pAppButton++;
    }
}

static void CheckApplication(void) {

    uint8_t i;
    TApplicationButton *pAppButton = sApplicationButton;
    uint16_t actualTime16;

    for (i = 0; i < MAX_NUM_APPLICATION_BUTTONS; i ++) {
        if ((pAppButton->isPressed) &&
            (pAppButton->pressedDelayMs > 0)) {
            GET_TIME_MS16(actualTime16);
            if (((uint16_t)(actualTime16 - pAppButton->pressedTimeStamp)) >= pAppButton->pressedDelayMs) {
                DigOutOn();
                pAppButton->isPressed = false;
            }
        }
        pAppButton++;
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


/*-----------------------------------------------------------------------------
*  process received bus telegrams
*/
static void ProcessBus(void) {

    uint8_t       ret;
    TClient     *pClient;
    TBusMsgType msgType;
    uint8_t       i;
    uint8_t       *p;
    bool        msgForMe = false;

    ret = BusCheck();

    if (ret == BUS_MSG_OK) {
        msgType = spRxBusMsg->type;
        switch (msgType) {
        case eBusDevReqReboot:
        case eBusDevReqActualValue:
        case eBusDevReqSetValue:
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
        case eBusButtonPressed1:
        case eBusButtonPressed2:
        case eBusButtonPressed1_2:
            msgForMe = true;
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
            sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.sw8.state = IO_STATE;
            BusSend(&sTxBusMsg);
            break;
        case eBusDevReqSetValue:
            if (spRxBusMsg->msg.devBus.x.devReq.setValue.devType == eBusDevTypeSw8) {
                /* bit 2 and 3 contains the setvalue for our output pin */
                uint8_t action = (spRxBusMsg->msg.devBus.x.devReq.setValue.setValue.sw8.digOut[0] & 0x0c) >> 2;
                /* support only trigger */
                if (action == 1) {
                    DigOutTrigger();
                }
                /* respone packet */
                sTxBusMsg.type = eBusDevRespSetValue;
                sTxBusMsg.senderAddr = MY_ADDR;
                sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
                BusSend(&sTxBusMsg);
            }
            break;
        case eBusDevRespActualValueEvent:
            pClient = sClient;
            for (i = 0; i < sNumClients; i++) {
                if ((pClient->address == spRxBusMsg->senderAddr) &&
                    (pClient->state == eWaitForConfirmation)) {
                    if (spRxBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.sw8.state == IO_STATE) {
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
        case eBusButtonPressed1:
            ButtonEvent(spRxBusMsg->senderAddr, 1);
            break;
        case eBusButtonPressed2:
            ButtonEvent(spRxBusMsg->senderAddr, 2);
            break;
        case eBusButtonPressed1_2:
            ButtonEvent(spRxBusMsg->senderAddr, 1);
            ButtonEvent(spRxBusMsg->senderAddr, 2);
            break;
        default:
            break;
        }
    } else if (ret == BUS_MSG_ERROR) {
        ButtonTimeStampRefresh();
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
