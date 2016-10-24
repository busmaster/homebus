/*
 * main.c
 *
 * Copyright 2015 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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
#include <util/delay.h>

#include "sio.h"
#include "sysdef.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
/* offset addresses in EEPROM */
#define MODUL_ADDRESS           0
#define OSCCAL_CORR             1

#define CLIENT_ADDRESS_BASE     2 /* BUS_MAX_CLIENT_NUM from bus.h (16 byte) */

#define WIND_THRESHOLD1         18
#define WIND_THRESHOLD2         19

#define STARTUP_DELAY  10 /* delay in seconds */

/* our bus address */
#define MY_ADDR    sMyAddr

#define IDLE_SIO  0x01

/* Port D bit 5 controls bus transceiver power down */
#define BUS_TRANSCEIVER_POWER_DOWN \
   PORTD |= (1 << 5)

#define BUS_TRANSCEIVER_POWER_UP \
   PORTD &= ~(1 << 5)

#define INPUT_SENSOR           (PINC & 0b00000001)

/* acual value event */
#define RESPONSE_TIMEOUT_MS         100  /* time in ms */
/* timeout for unreachable client  */
/* after this time retries are stopped */
#define RETRY_TIMEOUT_MS            10000 /* time in ms */
#define RETRY_CYCLE_TIME_MS         200 /* time in ms */

/*-----------------------------------------------------------------------------
*  Typedefs
*/
typedef enum {
   eInit,
   eWaitForConfirmation,
   eConfirmationOK
} TState;

typedef struct {
   uint8_t  address;
   TState   state;
   uint16_t requestTimeStamp;
} TClient;

/*-----------------------------------------------------------------------------
*  Variables
*/
char version[] = "Sw88_wind 0.03";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t  gTimeMs;
volatile uint16_t gTimeMs16;
volatile uint16_t gTimeS;

static TClient   sClient[BUS_MAX_CLIENT_NUM];
static uint8_t   sNumClients;

static uint8_t   sMyAddr;

static uint8_t   sIdle = 0;

static uint8_t   sWindSwitchOld;
static uint8_t   sWindSwitch;

static volatile uint8_t sWind;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void TimerInit(void);
static void Idle(void);
static void ProcessBus(void);
static uint8_t GetUnconfirmedClient(uint8_t actualClient);
static void GetClientListFromEeprom(void);
static void ProcessSwitch(void);
static void SendStartupMsg(void);
static void IdleSio(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {

    int     sioHandle;
    uint8_t windThreshold1;
    uint8_t windThreshold2;

    MCUSR = 0;
    wdt_disable();

    /* get module address from EEPROM */
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
    windThreshold1 = eeprom_read_byte((const uint8_t *)WIND_THRESHOLD1);
    windThreshold2 = eeprom_read_byte((const uint8_t *)WIND_THRESHOLD2);
    GetClientListFromEeprom();

    PortInit();
    TimerInit();
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

    while (1) {
        Idle();
        ProcessSwitch();
        ProcessBus();

        if (sWind >= windThreshold1) {
            sWindSwitch |= 0x01;
        } else {
            sWindSwitch &= ~0x01;
        }
        if (sWind >= windThreshold2) {
            sWindSwitch |= 0x02;
        } else {
            sWindSwitch &= ~0x02;
        }
    }
    return 0;  /* never reached */
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
        if (pClient->state != eConfirmationOK) {
            break;
        }
    }
    if (i == sNumClients) {
        /* all client's confirmations received */
        nextClient = 0xff;
    }
    return nextClient;
}

static void InitClientState(void) {

    TClient *pClient;
    uint8_t  i;

    for (i = 0, pClient = sClient; i < sNumClients; i++) {
        pClient->state = eInit;
    }
}

static void GetClientListFromEeprom(void) {

    TClient *pClient;
    uint8_t i;
    uint8_t numClients;
    uint8_t clientAddr;

    for (i = 0, numClients = 0, pClient = sClient; i < BUS_MAX_CLIENT_NUM; i++) {
        clientAddr = eeprom_read_byte((const uint8_t *)(CLIENT_ADDRESS_BASE + i));
        if (clientAddr != BUS_CLIENT_ADDRESS_INVALID) {
            pClient->address = clientAddr;
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
static void ProcessSwitch(void) {

    static uint8_t   sActualClient = 0xff; /* actual client's index being processed */
    static uint16_t  sChangeTimeStamp;
    TClient          *pClient;
    uint16_t         actualTime16;
    static bool      sNewClientCycleDelay = false;
    static uint16_t  sNewClientCycleTimeStamp;
    uint8_t          rc;
    bool             getNextClient;
    uint8_t          nextClient;
    bool             switchStateChanged;

    if (sNumClients == 0) {
        return;
    }

    if ((sWindSwitch ^ sWindSwitchOld) != 0) {
        switchStateChanged = true;
    } else {
        switchStateChanged = false;
    }

    /* do the change detection not in each cycle */
    GET_TIME_MS16(actualTime16);
    if (switchStateChanged) {
        sChangeTimeStamp = actualTime16;
        sActualClient = 0;
        sNewClientCycleDelay = false;
        InitClientState();
        sWindSwitchOld = sWindSwitch;
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
        sTxBusMsg.type = eBusDevReqSwitchState;  
        sTxBusMsg.senderAddr = MY_ADDR; 
        sTxBusMsg.msg.devBus.receiverAddr = pClient->address;
        sTxBusMsg.msg.devBus.x.devReq.switchState.switchState = sWindSwitch;
        rc = BusSend(&sTxBusMsg);
        if (rc == BUS_SEND_OK) {
            pClient->state = eWaitForConfirmation;
            pClient->requestTimeStamp = actualTime16;
        } else {
            getNextClient = false;
        }
        break;
    case eWaitForConfirmation:
        if (((uint16_t)(actualTime16 - pClient->requestTimeStamp)) >= RESPONSE_TIMEOUT_MS) {
            /* try once more */
            pClient->state = eInit;
            getNextClient = false;
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

    if (((uint16_t)(actualTime16 - sChangeTimeStamp)) > RETRY_TIMEOUT_MS) {
        sActualClient = 0xff; // stop
    }
}

/*-----------------------------------------------------------------------------
*  process received bus telegrams
*/
static void ProcessBus(void) {

    uint8_t       ret;
    TClient       *pClient;
    TBusMsgType   msgType;
    uint8_t       i;
    uint8_t       *p;
    bool          msgForMe = false;

    ret = BusCheck();

    if (ret == BUS_MSG_OK) {
        msgType = spRxBusMsg->type;
        switch (msgType) {
        case eBusDevReqReboot:
        case eBusDevRespSwitchState:
        case eBusDevReqActualValue:
        case eBusDevReqSetClientAddr:
        case eBusDevReqGetClientAddr:
        case eBusDevReqInfo:
        case eBusDevReqSetAddr:
        case eBusDevReqEepromRead:
        case eBusDevReqEepromWrite:
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
    case eBusDevRespSwitchState:
        pClient = sClient;
        for (i = 0; i < sNumClients; i++) {
            if ((pClient->address == spRxBusMsg->senderAddr) &&
                (pClient->state == eWaitForConfirmation)) {
                if (spRxBusMsg->msg.devBus.x.devResp.switchState.switchState == sWindSwitch) {
                    pClient->state = eConfirmationOK;
                }
                break;
            }
            pClient++;
        }
        break;
    case eBusDevReqActualValue:
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.type = eBusDevRespActualValue;
        sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxBusMsg.msg.devBus.x.devResp.actualValue.devType = eBusDevTypeWind;
        sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.wind.state = sWindSwitch;
        sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.wind.wind = sWind;
        BusSend(&sTxBusMsg);
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
        GetClientListFromEeprom();
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
        sTxBusMsg.msg.devBus.x.devResp.info.devType = eBusDevTypeWind;
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

/*-----------------------------------------------------------------------------
*  port init
*/
static void PortInit(void) {

    /* configure port b pins to output low (unused pins) */
    PORTB = 0b00000000;
    DDRB =  0b11111111;

    /* port c: */
    /* unused pins: output low */
    /* pc.0: input pin hi-z */
    /* pc.1: output pin hi */
    PORTC = 0b00000010;
    DDRC =  0b11111110;

    /* port d: */
    /* pd0: input, no pull up (RXD) */
    /* pd1: output (TXD)            */
    /* pd2: input, no pull up (TXD) */
    /* pd5: output (bus transceiver power state) */
    /* other pd: unused, output low */
    PORTD = 0b00100010;
    DDRD =  0b11111010;
}

/*-----------------------------------------------------------------------------
*  port init
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
    static uint8_t oldSensor = 0;
    static uint8_t windCnt;
    uint8_t sensor;

    sensor = INPUT_SENSOR;
    if ((sensor ^ oldSensor) != 0) {
        oldSensor = sensor;
        windCnt++;
    }
    /* ms counter */
    gTimeMs16 += 16;
    gTimeMs += 16;
    intCnt++;
    if (intCnt >= 61) { /* 16.384 ms * 61 = 1 s*/
        intCnt = 0;
        gTimeS++;
        sWind = windCnt;
        windCnt = 0;
    }
}
