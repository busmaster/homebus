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
#include "board.h"
#include "bus.h"
#include "button.h"
#include "led.h"
#include "application.h"

#include "rs485.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
/* offset addresses in EEPROM */
#define MODUL_ADDRESS           0  /* 1 byte */
#define CLIENT_ADDRESS_BASE     1  /* BUS_MAX_CLIENT_NUM from bus.h (16 byte) */
#define CLIENT_RETRY_CNT        17 /* size: 16 byte (BUS_MAX_CLIENT_NUM)      */

/* DO restore after power fail */
#define EEPROM_DO_RESTORE_START  (uint8_t *)3072
#define EEPROM_DO_RESTORE_END    (uint8_t *)4095

/* our bus address */
#define MY_ADDR    sMyAddr

#define IDLE_SIO0  0x01

/* acual value event */
#define RESPONSE_TIMEOUT_MS         100  /* time in ms */
/* timeout for unreachable client  */
#define RETRY_CYCLE_TIME_MS         200 /* time in ms */
#define CHANGE_DETECT_CYCLE_TIME_MS 500 /* time in ms */

/* timeout for doClockCalibReq */
#define CLOCK_CALIB_TIMEOUT_MS 200 /* time in ms */

/*-----------------------------------------------------------------------------
*  Typedefs
*/
typedef struct {
    uint8_t  address;
    uint8_t  maxRetry;
    uint8_t  curRetry;
    enum {
        eEventInit,
        eEventWaitForConfirmation,
        eEventConfirmationOK,
        eEventMaxRetry
    } state;
    uint16_t requestTimeStamp;
} TClient;

typedef struct {
    enum {
        eCalibIdle,
        eCalibInit,
        eCalibContinue,
        eCalibWaitForResponse,
        eCalibSuccess,
        eCalibError,
        eCalibInternalError,
    } state;
    uint8_t address;
} TClockCalib;

/*-----------------------------------------------------------------------------
*  Variables
*/
volatile uint8_t  gTimeMs = 0;
volatile uint16_t gTimeMs16 = 0;
volatile uint32_t gTimeMs32 = 0;
volatile uint16_t gTimeS = 0;

static TBusTelegram *spBusMsg;
static TBusTelegram  sTxBusMsg;

static uint8_t sMyAddr;

static uint8_t sIdle = 0;

static TClient sClient[BUS_MAX_CLIENT_NUM];
static uint8_t sNumClients;

static TClockCalib sClockCalib;

static uint8_t buf[] = {0, 1 /* address */, 50, 50, 50, 50, 50, 50, 50, 50 ,50};

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void TimerInit(void);
static void TimerStart(void);
static void CheckButton(void);
static void ButtonEvent(uint8_t address, uint8_t button);
static void SwitchEvent(uint8_t address, uint8_t button, bool pressed);
static void ProcessBus(uint8_t ret);
static void RestoreOut(void);
static void Idle(void);
static void IdleSio0(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);
static void CheckEvent(void);
static void GetClientListFromEeprom(void);
static void ClockCalibTask(void);

/*-----------------------------------------------------------------------------
*  main
*/
int main(void) {

    uint8_t  ret;
    int      sioHdl;
    uint16_t t_curr;
    uint16_t t_last;

    cli();
    MCUSR = 0;
    wdt_disable();

    /* get module address from EEPROM */
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
    GetClientListFromEeprom();
    
    PortInit();
    LedInit();
    TimerInit();
    ButtonInit();
    ApplicationInit();

    SioInit();
    SioRandSeed(sMyAddr);

    /* sio for bus interface */
    sioHdl = SioOpen("USART0", eSioBaud9600, eSioDataBits8, eSioParityNo,
                     eSioStopBits1, eSioModeHalfDuplex);

    SioSetIdleFunc(sioHdl, IdleSio0);
    SioSetTransceiverPowerDownFunc(sioHdl, BusTransceiverPowerDown);
    BusTransceiverPowerDown(true);

    BusInit(sioHdl);
    spBusMsg = BusMsgBufGet();

    Rs485Init();

    /* wait for good power */
    while (!POWER_GOOD);

    ENABLE_INT;

    TimerStart();
    RestoreOut();

    /* ext int for power fail: PCINT22 low level sensitive */
    PCICR |= (1 << PCIE2);
    PCMSK2 |= (1 << PCINT22);

    LedSet(eLedGreenFlashSlow);

    ApplicationStart();
   
    sClockCalib.state = eCalibIdle;

    GET_TIME_MS16(t_curr);
    t_last = t_curr;

    /* Hauptschleife */
    while (1) {
        Idle();
        ret = BusCheck();
        ProcessBus(ret);
        ClockCalibTask();
        CheckButton();
        LedCheck();
        ApplicationCheck();
        CheckEvent();
        
        GET_TIME_MS16(t_curr);
        if (((uint16_t)(t_curr - t_last)) >= 500) {
            t_last = t_curr;
            Rs485Write(buf, sizeof(buf));
        }
        
    }
    return 0;
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
        if (pClient->state == eEventMaxRetry) {
            continue;
        }
        if (pClient->state != eEventConfirmationOK) {
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
        pClient->state = eEventInit;
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
            pClient->state = eEventInit;
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
    bool             actValChanged;
    static bool      sNewClientCycleDelay = false;
    static uint16_t  sNewClientCycleTimeStamp;
    TBusDevReqActualValueEvent *pActVal;
    bool             getNextClient;
    uint8_t          nextClient;
   
    if (sNumClients == 0) {
        return;
    }
    
    /* do the change detection not in each cycle */
    GET_TIME_MS16(actualTime16);
    if (((uint16_t)(actualTime16 - sChangeTestTimeStamp)) >= CHANGE_DETECT_CYCLE_TIME_MS) {
        if (0) {
            actValChanged = false;
        } else {
            actValChanged = true;
        }
     
        if (actValChanged) {
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
    case eEventInit:
        pActVal = &sTxBusMsg.msg.devBus.x.devReq.actualValueEvent;
        sTxBusMsg.type = eBusDevReqActualValueEvent;
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.msg.devBus.receiverAddr = pClient->address;
        pActVal->devType = eBusDevTypeRs485If;
    
        if (BusSend(&sTxBusMsg) == BUS_SEND_OK) {
            pClient->state = eEventWaitForConfirmation;
            pClient->requestTimeStamp = actualTime16;
        } else {
            getNextClient = false;
        }
        break;
    case eEventWaitForConfirmation:
        if ((((uint16_t)(actualTime16 - pClient->requestTimeStamp)) >= RESPONSE_TIMEOUT_MS) &&
            (pClient->state != eEventMaxRetry)) {
            if (pClient->curRetry < pClient->maxRetry) {
                /* try again */
                pClient->curRetry++;
                getNextClient = false;
                pClient->state = eEventInit;
            } else {
                pClient->state = eEventMaxRetry;
            }
        }
        break;
    case eEventConfirmationOK:
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
*   switch to idle mode
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
static void IdleSio0(bool setIdle) {

   if (setIdle == true) {
      sIdle &= ~IDLE_SIO0;
   } else {
      sIdle |= IDLE_SIO0;
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
*  Ausgangszustand wiederherstellen
*/
static void RestoreOut(void) {

}

/*-----------------------------------------------------------------------------
*  Erkennung von Tasten-Loslass-Ereignissen
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
*  Verarbeitung der Bustelegramme
*/
static void ProcessBus(uint8_t ret) {
    TBusMsgType            msgType;
    uint8_t                i;
    bool                   msgForMe = false;
    uint8_t                state;
    TBusDevRespInfo        *pInfo;
    TBusDevRespActualValue *pActVal;
    TClient                *pClient;
    TClockCalibState       calibState;
    static TBusTelegram    sTxMsg;
    static bool            sTxRetry = false;

    if (sTxRetry) {
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        return;
    }

    if (ret == BUS_MSG_OK) {
        msgType = spBusMsg->type;
        switch (msgType) {
        case eBusDevReqReboot:
        case eBusDevReqInfo:
        case eBusDevReqActualValue:
        case eBusDevReqSetValue:
        case eBusDevReqSwitchState:
        case eBusDevReqSetAddr:
        case eBusDevReqEepromRead:
        case eBusDevReqEepromWrite:
        case eBusDevReqSetClientAddr:
        case eBusDevReqGetClientAddr:
        case eBusDevRespActualValueEvent:
        case eBusDevReqClockCalib:
        case eBusDevRespDoClockCalib:
            if (spBusMsg->msg.devBus.receiverAddr == MY_ADDR) {
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
    } else if (ret == BUS_MSG_ERROR) {
        LedSet(eLedRedBlinkOnceShort);
        ButtonTimeStampRefresh();
    } 

    if (msgForMe == false) {
       return;
    }
   
    switch (msgType) {
    case eBusDevReqReboot:
        /* use watchdog to reboot */
        /* set the watchdog timeout as short as possible (14 ms) */
        cli();
        wdt_enable(WDTO_15MS);
        /* wait for reset */
        while (1);
        break;
    case eBusButtonPressed1:
        ButtonEvent(spBusMsg->senderAddr, 1);
        break;
    case eBusButtonPressed2:
        ButtonEvent(spBusMsg->senderAddr, 2);
        break;
    case eBusButtonPressed1_2:
        ButtonEvent(spBusMsg->senderAddr, 1);
        ButtonEvent(spBusMsg->senderAddr, 2);
        break;
    case eBusDevReqInfo:
        /* response packet */
        pInfo = &sTxMsg.msg.devBus.x.devResp.info;
        sTxMsg.type = eBusDevRespInfo;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        pInfo->devType = eBusDevTypeRs485If;
        strncpy((char *)(pInfo->version), ApplicationVersion(), sizeof(pInfo->version));
        pInfo->version[sizeof(pInfo->version) - 1] = '\0';
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqActualValue:
        /* response packet */
        pActVal = &sTxMsg.msg.devBus.x.devResp.actualValue;
        sTxMsg.type = eBusDevRespActualValue;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        pActVal->devType = eBusDevTypeRs485If;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetValue:
        if (spBusMsg->msg.devBus.x.devReq.setValue.devType != eBusDevTypeRs485If) {
            break;
        }
        /* response packet */
        sTxMsg.type = eBusDevRespSetValue;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSwitchState:
        state = spBusMsg->msg.devBus.x.devReq.switchState.switchState;
        if ((state & 0x01) != 0) {
            SwitchEvent(spBusMsg->senderAddr, 1, true);
        } else {
            SwitchEvent(spBusMsg->senderAddr, 1, false);
        }
        if ((state & 0x02) != 0) {
            SwitchEvent(spBusMsg->senderAddr, 2, true);
        } else {
            SwitchEvent(spBusMsg->senderAddr, 2, false);
        }
        /* response packet */
        sTxMsg.type = eBusDevRespSwitchState;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.switchState.switchState = state;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetAddr:
        sTxMsg.senderAddr = MY_ADDR; 
        sTxMsg.type = eBusDevRespSetAddr;  
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        eeprom_write_byte((uint8_t *)MODUL_ADDRESS, spBusMsg->msg.devBus.x.devReq.setAddr.addr);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqEepromRead:
        sTxMsg.senderAddr = MY_ADDR; 
        sTxMsg.type = eBusDevRespEepromRead;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.readEeprom.data = 
            eeprom_read_byte((const uint8_t *)spBusMsg->msg.devBus.x.devReq.readEeprom.addr);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;  
        break;
    case eBusDevReqEepromWrite:
        sTxMsg.senderAddr = MY_ADDR; 
        sTxMsg.type = eBusDevRespEepromWrite;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        eeprom_write_byte((uint8_t *)spBusMsg->msg.devBus.x.devReq.readEeprom.addr, 
                          spBusMsg->msg.devBus.x.devReq.writeEeprom.data);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;  
        break;
    case eBusDevRespActualValueEvent:
        pClient = sClient;
        for (i = 0; i < sNumClients; i++) {
            if ((pClient->address == spBusMsg->senderAddr) &&
                (pClient->state == eEventWaitForConfirmation)) {
                /* todo: compare with current state */
                pClient->state = eEventConfirmationOK;
                break;
            }
            pClient++;
        }
        break;
    case eBusDevReqSetClientAddr:
        sTxMsg.senderAddr = MY_ADDR; 
        sTxMsg.type = eBusDevRespSetClientAddr;  
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
            uint8_t *p = &(sTxMsg.msg.devBus.x.devReq.setClientAddr.clientAddr[i]);
            eeprom_write_byte((uint8_t *)(CLIENT_ADDRESS_BASE + i), *p);
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        GetClientListFromEeprom();
        break;
    case eBusDevReqGetClientAddr:
        sTxMsg.senderAddr = MY_ADDR; 
        sTxMsg.type = eBusDevRespGetClientAddr;  
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
            uint8_t *p = &(sTxMsg.msg.devBus.x.devResp.getClientAddr.clientAddr[i]);
            *p = eeprom_read_byte((const uint8_t *)(CLIENT_ADDRESS_BASE + i));
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqClockCalib:
        sTxMsg.type = eBusDevRespClockCalib;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;

        if (spBusMsg->msg.devBus.x.devReq.clockCalib.command == eBusClockCalibCommandIdle) {
            sClockCalib.state = eCalibIdle;
            calibState = eBusClockCalibStateIdle;
        } else if ((spBusMsg->msg.devBus.x.devReq.clockCalib.command == eBusClockCalibCommandCalibrate) &&
                   (sClockCalib.state == eCalibIdle)) {
            sClockCalib.state = eCalibInit;
            sClockCalib.address = spBusMsg->msg.devBus.x.devReq.clockCalib.address;
            calibState = eBusClockCalibStateBusy;
        } else if (spBusMsg->msg.devBus.x.devReq.clockCalib.command == eBusClockCalibCommandGetState) {
            switch (sClockCalib.state) {
            case eCalibIdle:
                calibState = eBusClockCalibStateIdle;
                break;
            case eCalibInit:
            case eCalibContinue:
            case eCalibWaitForResponse:
                calibState = eBusClockCalibStateBusy;
                break;
            case eCalibSuccess:
                calibState = eBusClockCalibStateSuccess;
                break;
            case eCalibError:
                calibState = eBusClockCalibStateError;
                break;
            default:
                calibState = eBusClockCalibStateInternalError;
                break;
            }
        } else {
            calibState = eBusClockCalibStateInvalidCommand;
        }
        sTxMsg.msg.devBus.x.devResp.clockCalib.state = calibState;
        sTxMsg.msg.devBus.x.devResp.clockCalib.address = sClockCalib.address;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevRespDoClockCalib:
        switch (spBusMsg->msg.devBus.x.devResp.doClockCalib.state) {
        case eBusDoClockCalibStateSuccess:
            sClockCalib.state = eCalibSuccess;
            break;
        case eBusDoClockCalibStateContiune:
            sClockCalib.state = eCalibContinue;
            break;
        case eBusDoClockCalibStateError:
            sClockCalib.state = eCalibError;
            break;
        default:
            sClockCalib.state = eCalibInternalError;
            break;
        }
        break;
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------
*   clock calibration state machine
*/
static void ClockCalibTask(void) {
 
    uint8_t  ch;
    uint8_t  i;
    uint16_t actualTime16;
    static uint16_t sReqTimeStamp;
    
    if (sClockCalib.state == eCalibIdle) {
        return;
    }

    GET_TIME_MS16(actualTime16);
    switch (sClockCalib.state) {
    case eCalibInit:
    case eCalibContinue:
        sTxBusMsg.type = eBusDevReqDoClockCalib;
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.msg.devBus.receiverAddr = sClockCalib.address;
        if (sClockCalib.state == eCalibInit) {
            sTxBusMsg.msg.devBus.x.devReq.doClockCalib.command = eBusDoClockCalibInit;
        } else {
            sTxBusMsg.msg.devBus.x.devReq.doClockCalib.command = eBusDoClockCalibContiune;
        }
        if (BusSendToBuf(&sTxBusMsg) == BUS_SEND_OK) {
            /* send calib sequence 0xff, 0xff, 0x00 .. 0x00 (64) */
            ch = 0xff;
            BusSendToBufRaw(&ch, sizeof(ch));
            BusSendToBufRaw(&ch, sizeof(ch));
            ch = 0;
            for (i = 0; i < 64; ) {
                BusSendToBufRaw(&ch, sizeof(ch));
                i++;
            }
            BusSendBuf();
            sClockCalib.state = eCalibWaitForResponse;
            sReqTimeStamp = actualTime16;
        }
        break;
    case eCalibWaitForResponse:
        if (((uint16_t)(actualTime16 - sReqTimeStamp)) >= CLOCK_CALIB_TIMEOUT_MS) {
            sClockCalib.state = eCalibError;
        }
        break;
    case eCalibSuccess:
    case eCalibError:
    case eCalibInternalError:
        break;
    default:
        sClockCalib.state = eCalibInternalError;
        break;
    }
}

/*-----------------------------------------------------------------------------
*  create button event for application
*/
static void ButtonEvent(uint8_t address, uint8_t button) {
   TButtonEvent buttonEventData;

   if (ButtonNew(address, button) == true) {
      buttonEventData.address = spBusMsg->senderAddr;
      buttonEventData.pressed = true;
      buttonEventData.buttonNr = button;
      ApplicationEventButton(&buttonEventData);
   }
}


/*-----------------------------------------------------------------------------
*  create switch event for application
*/
static void SwitchEvent(uint8_t address, uint8_t button, bool pressed) {
   TButtonEvent buttonEventData;

   buttonEventData.address = spBusMsg->senderAddr;
   buttonEventData.pressed = pressed;
   buttonEventData.buttonNr = button;
   ApplicationEventButton(&buttonEventData);
}


/*-----------------------------------------------------------------------------
*  Power-Fail Interrupt (PCINT22)
*/
ISR(PCINT2_vect) {

    /* we are interested in PC6 = 0 only */
    if ((PINC & (1 << PINC6)) != 0) {
        return;
    }

   LedSet(eLedGreenOff);
   LedSet(eLedRedOff);

   /* todo: save to eeprom */

   /* auf Powerfail warten */
   /* falls sich die Versorgungsspannung wieder erholt hat und */
   /* daher kein Power-Up Reset passiert, wird ein Reset über */
   /* den Watchdog ausgelöst */
   while (!POWER_GOOD);

   /* Versorgung wieder OK */
   /* Watchdogtimeout auf 2 s stellen */
   wdt_enable(WDTO_2S);

   /* warten auf Reset */
   while (1);
}

/*-----------------------------------------------------------------------------
*  Timerinitialisierung
*/
static void TimerInit(void) {

   /* use timer 3/output compare A */
   /* timer3 compare B is used for sio timing - do not change the timer mode WGM 
    * and change sio timer settings when changing the prescaler!
    */
   
   /* prescaler @ 3.6864 MHz:  1024  */
   /*           @ 18.432 MHz:  1024  */
   /* compare match pin not used: COM3A[1:0] = 00 */
   /* compare register OCR3A:  */
   /* 3.6864 MHz: 18 -> 5 ms */
   /* 18.432 MHz: 90 -> 5 ms */
   /* timer mode 0: normal: WGM3[3:0]= 0000 */

   TCCR3A = (0 << COM3A1) | (0 << COM3A0) | (0 << COM3B1) | (0 << COM3B0) | (0 << WGM31) | (0 << WGM30);
   TCCR3B = (0 << ICNC3) | (0 << ICES3) |
            (0 << WGM33) | (0 << WGM32) | 
            (1 << CS32)  | (0 << CS31)  | (1 << CS30); 

#if (F_CPU == 3686400UL)
   #define TIMER_TCNT_INC    18
   #define TIMER_INC_MS      5
#elif (F_CPU == 18432000UL)
   #define TIMER_TCNT_INC    90
   #define TIMER_INC_MS      5
#else
#error adjust timer settings for your CPU clock frequency
#endif
}

static void TimerStart(void) {

   OCR3A = TCNT3 + TIMER_TCNT_INC;
   TIFR3 = 1 << OCF3A;
   TIMSK3 |= 1 << OCIE3A;
}


/*-----------------------------------------------------------------------------
*  time interrupt for time counter
*/
ISR(TIMER3_COMPA_vect)  {

    static uint16_t sCounter = 0;

    OCR3A = OCR3A + TIMER_TCNT_INC;

    /* ms */
    gTimeMs += TIMER_INC_MS;
    gTimeMs16 += TIMER_INC_MS;
    gTimeMs32 += TIMER_INC_MS;
    sCounter++;
    if (sCounter >= (1000 / TIMER_INC_MS)) {
        sCounter = 0;
        /* s */
        gTimeS++;
    }
}
/*-----------------------------------------------------------------------------
*  Einstellung der Portpins
*/
static void PortInit(void) {

   /* PA.6, PA.7: LED1, LED2: output low */
   /* PA.5: unused: output low */
   /* PA.4: input: high z */
   /* PA.3 .. PA.0: input pull up */
   PORTA = 0b00001111;
   DDRA  = 0b11100000;

   /* PB.7: SCK: output low */
   /* PB.6, MISO: output low */
   /* PB.5: MOSI: output low */
   /* PB.4 .. PB.3: input, high z */
   /* PB.2: input, high z */
   /* PB.1: unused, output low */
   /* PB.0: transceiver power, output high */
   PORTB = 0b00000001;
   DDRB  = 0b11100011;
   
   /* PC.7: unused: output low */
   /* PC.6: power fail input high z */
   /* PC.5: unused: output low */
   /* PC.4: unused: output low */
   /* PC.3: RS485 TXEN: output low */
   /* PC.2: RS485 nRXEN: output high */
   /* PC.1: unused: output low */
   /* PC.0: unused: output low */
   PORTC = 0b00000100;
   DDRC  = 0b10111111;

   /* PD.7: input high z */
   /* PD.6: input high z */
   /* PD.5: unused: output low */
   /* PD.4: digout: output low */
   /* PD.3: TX1: output high */
   /* PD.2: RX1: input pull up */
   /* PD.1: TX0: output high */
   /* PD.0: RX0: input pull up */
   PORTD = 0b00001111;
   DDRD  = 0b00111010;
}
