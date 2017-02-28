/*
 * main.c
 *
 * Copyright 2019 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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
#include "pwm.h"
#include "application.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
/* Bits in WDTCR */
#define WDCE     4
#define WDE      3
#define WDP0     0
#define WDP1     1
#define WDP2     2

/* offset addresses in EEPROM */
#define MODUL_ADDRESS           0  /* 1 byte */
#define CLIENT_ADDRESS_BASE     1  /* BUS_MAX_CLIENT_NUM from bus.h (16 byte) */
#define CLIENT_RETRY_CNT        17 /* size: 16 byte (BUS_MAX_CLIENT_NUM)      */

/* DO restore after power fail */
#define EEPROM_PWM_RESTORE_START  (uint8_t *)512
#define EEPROM_PWM_RESTORE_END    (uint8_t *)1023

/* our bus address */
#define MY_ADDR    sMyAddr

#define IDLE_SIO1  0x01

/* acual value event */
#define RESPONSE_TIMEOUT_MS         100  /* time in ms */
/* timeout for unreachable client  */
#define RETRY_CYCLE_TIME_MS         200 /* time in ms */
#define CHANGE_DETECT_CYCLE_TIME_MS 500 /* time in ms */

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

/*-----------------------------------------------------------------------------
*  Variables
*/
volatile uint8_t  gTimeMs = 0;
volatile uint16_t gTimeMs16 = 0;
volatile uint16_t gTime10Ms16 = 0;
volatile uint32_t gTimeMs32 = 0;
volatile uint16_t gTimeS = 0;

static TBusTelegram *spBusMsg;
static TBusTelegram  sTxBusMsg;

static uint8_t       *spNextPtrToEeprom;
static uint8_t       sMyAddr;

static uint8_t       sIdle = 0;

static TClient       sClient[BUS_MAX_CLIENT_NUM];
static uint8_t       sNumClients;

static uint16_t      sOldPwmActVal[NUM_PWM_CHANNEL];
static uint16_t      sCurPwmActVal[NUM_PWM_CHANNEL];

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
static void RestorePwm(void);
static void Idle(void);
static void IdleSio1(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);
static void CheckEvent(void);
static void GetClientListFromEeprom(void);

/*-----------------------------------------------------------------------------
*  main
*/
int main(void) {

   uint8_t ret;
   int   sioHdl;

   /* set clock prescaler to 2 (set clock to 7.3928 MHz) */
   CLKPR = 1 << CLKPCE;
   CLKPR = 1;

   /* get module address from EEPROM */
   sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
   GetClientListFromEeprom();
    
   PortInit();
   TimerInit();
   ButtonInit();
   PwmInit();
   ApplicationInit();

   SioInit();
   SioRandSeed(sMyAddr);

   /* sio for bus interface */
   sioHdl = SioOpen("USART1", eSioBaud9600, eSioDataBits8, eSioParityNo,
                    eSioStopBits1, eSioModeHalfDuplex);

   SioSetIdleFunc(sioHdl, IdleSio1);
   SioSetTransceiverPowerDownFunc(sioHdl, BusTransceiverPowerDown);
   BusTransceiverPowerDown(true);

   BusInit(sioHdl);
   spBusMsg = BusMsgBufGet();

   /* warten for full operation voltage */
   while (!POWER_GOOD);

   /* enable ints beforen RestorePwm() */
   ENABLE_INT;
   TimerStart();
   RestorePwm();

   /* ext int for power fail: INT0 low level sensitive */
   EICRA &= ~((1 << ISC01) | (1 << ISC00));
   EIMSK |= (1 << INT0);

   ApplicationStart();
   
   /* Hauptschleife */
   while (1) {
      Idle();
      ret = BusCheck();
      ProcessBus(ret);
      CheckButton();
      PwmCheck();
      ApplicationCheck();
      CheckEvent();
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
        PwmGetAll(sCurPwmActVal, sizeof(sCurPwmActVal));
        if (memcmp(sCurPwmActVal, sOldPwmActVal, sizeof(sCurPwmActVal)) == 0) {
            actValChanged = false;
        } else {
            actValChanged = true;
        }
     
        if (actValChanged) {
            memcpy(sOldPwmActVal, sCurPwmActVal, sizeof(sOldPwmActVal));
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
        pActVal->devType = eBusDevTypePwm4;
    
        memcpy(pActVal->actualValue.pwm4.pwm, sCurPwmActVal, 
               sizeof(pActVal->actualValue.pwm4.pwm));
        
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
static void IdleSio1(bool setIdle) {

   if (setIdle == true) {
      sIdle &= ~IDLE_SIO1;
   } else {
      sIdle |= IDLE_SIO1;
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
*  restore pwm output state
*/
static void RestorePwm(void) {

    uint8_t     *ptrToEeprom;
    uint8_t     flags;
    uint8_t     sizeRestore = NUM_PWM_CHANNEL * sizeof(uint16_t) + 1;
    uint8_t     pwmLow;
    uint8_t     pwmHigh;
    uint8_t     i;

    /* find the newest state */
    for (ptrToEeprom = EEPROM_PWM_RESTORE_START; 
         ptrToEeprom < (EEPROM_PWM_RESTORE_END - sizeRestore);
         ptrToEeprom += sizeRestore) {
        if (eeprom_read_byte(ptrToEeprom) == 0x00) {
            break;
        }
    }
    if (ptrToEeprom > (EEPROM_PWM_RESTORE_END - sizeRestore)) {
        /* not found -> no restore */
        spNextPtrToEeprom = EEPROM_PWM_RESTORE_START;
        return;
    }

    spNextPtrToEeprom = ptrToEeprom + sizeRestore;
    if (spNextPtrToEeprom > (EEPROM_PWM_RESTORE_END - sizeRestore)) {
        spNextPtrToEeprom = EEPROM_PWM_RESTORE_START;
    }

    /* restore pwm output */
    flags = DISABLE_INT;
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        pwmLow  = eeprom_read_byte(ptrToEeprom + 1 + i * sizeof(uint16_t));
        pwmHigh = eeprom_read_byte(ptrToEeprom + 1 + i * sizeof(uint16_t) + 1);
        PwmSet(i, pwmLow + 256 * pwmHigh);
    }
    /* delete  */
    eeprom_write_byte((uint8_t *)ptrToEeprom, 0xff);
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        eeprom_write_byte(ptrToEeprom + 1 + i * 2, 0xff);
        eeprom_write_byte(ptrToEeprom + 1 + i * 2 + 1, 0xff);
    }
    RESTORE_INT(flags);
}

/*-----------------------------------------------------------------------------
*  detect button release
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
*  process bus telegrams
*/
static void ProcessBus(uint8_t ret) {
    TBusMsgType            msgType;
    uint8_t                i;
    bool                   msgForMe = false;
    uint8_t                state;
    uint8_t                mask8;
    uint8_t                action;
    TBusDevRespInfo        *pInfo;
    TBusDevRespActualValue *pActVal;
    TClient                *pClient;
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
        pInfo->devType = eBusDevTypePwm4;
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
        pActVal->devType = eBusDevTypePwm4;
        PwmGetAll(pActVal->actualValue.pwm4.pwm, sizeof(pActVal->actualValue.pwm4.pwm));
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetValue:
        if (spBusMsg->msg.devBus.x.devReq.setValue.devType != eBusDevTypePwm4) {
            break;
        }
        mask8 = spBusMsg->msg.devBus.x.devReq.setValue.setValue.pwm4.set;
        for (i = 0; i < NUM_PWM_CHANNEL; i++) {
            action = (0x3 << (i * 2) & mask8) >> (i * 2);
            switch (action) {
            case 0x00:
                /* no action, ignore pwm[] from telegram */
                break;
            case 0x01:
                /* set current pwm, ignore pwm[] from telegram */
                PwmOn(i, true);
                break;
            case 0x02:
                /* set to pwm[] from telegram */
                PwmSet(i, spBusMsg->msg.devBus.x.devReq.setValue.setValue.pwm4.pwm[i]);
                PwmOn(i, true);
                break;
            case 0x03:
                /* off, ignore pwm[] from telegram  */
                PwmOn(i, false);
                break;
            default:
                break;
            }    
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
                TBusDevActualValuePwm4 *p;
                uint16_t buf[NUM_PWM_CHANNEL];

                PwmGetAll(buf, sizeof(buf));

                p = &spBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.pwm4;
                if (memcmp(p->pwm, buf, sizeof(buf)) == 0) {
                    pClient->state = eEventConfirmationOK;
                }
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
            uint8_t *p = &spBusMsg->msg.devBus.x.devReq.setClientAddr.clientAddr[i];
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
            uint8_t *p = &sTxMsg.msg.devBus.x.devResp.getClientAddr.clientAddr[i];
            *p = eeprom_read_byte((const uint8_t *)(CLIENT_ADDRESS_BASE + i));
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    default:
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
*  Power-Fail Interrupt (Ext. Int 0)
*/
ISR(INT0_vect) {
    uint8_t  *ptrToEeprom;
    uint16_t pwm[NUM_PWM_CHANNEL];
    uint8_t  i;
    bool     on;

    PwmGetAll(pwm, sizeof(pwm));
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        PwmIsOn(i, &on);
        if (!on) {
            pwm[i] = 0;
        }
    }
    /* switch off all outputs */
    PwmExit();

    /* save pwm state to eeprom */
    ptrToEeprom = spNextPtrToEeprom;

    eeprom_write_byte(ptrToEeprom, 0);
    ptrToEeprom++;
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        eeprom_write_byte(ptrToEeprom, pwm[i] & 0xff);
        ptrToEeprom++;
        eeprom_write_byte(ptrToEeprom, pwm[i] >> 8);
        ptrToEeprom++;
    }

   /* Wait for completion of previous write */
   while (!eeprom_is_ready());

   /* auf Powerfail warten */
   /* falls sich die Versorgungsspannung wieder erholt hat und */
   /* daher kein Power-Up Reset passiert, wird ein Reset über */
   /* den Watchdog ausgelöst */
   while (!POWER_GOOD);

   /* Versorgung wieder OK */
   /* Watchdogtimeout auf 2 s stellen */
   wdt_enable(WDTO_2S);

   /* wait for reset */
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
   
   /* prescaler @ 1.8432/3.6864/7.3728 MHz: 256  */
   /* compare match pin not used: COM3A[1:0] = 00 */
   /* compare register OCR3A:  */
   /* 1.8432 MHz: 36 -> 5 ms */
   /* 3.6864 MHz: 72 -> 5 ms */
   /* 7.3728 MHz: 144 -> 5 ms */
   /* timer mode 0: normal: WGM3[3:0]= 0000 */

   TCCR3A = (0 << COM3A1) | (0 << COM3A0) | (0 << COM3B1) | (0 << COM3B0) | (0 << WGM31) | (0 << WGM30);
   TCCR3B = (0 << ICNC3) | (0 << ICES3) |
            (0 << WGM33) | (0 << WGM32) | 
            (1 << CS32)  | (0 << CS31)  | (0 << CS30); 

#if (F_CPU == 1843200UL)
   #define TIMER_TCNT_INC    36
   #define TIMER_INC_MS      5
#elif (F_CPU == 3686400UL)
   #define TIMER_TCNT_INC    72
   #define TIMER_INC_MS      5
#elif (F_CPU == 7372800UL)
   #define TIMER_TCNT_INC    144
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
*  port settings
*/
static void PortInit(void) {

    /* PB.7: digout: output low */
    /* PB.6, digout: output low */
    /* PB.5: digout: output low */
    /* PB.4: unused: output low */
    /* PB.3: MISO: output low */
    /* PB.2: MOSI: output low */
    /* PB.1: SCK: output low */
    /* PB.0: unused: output low */
    PORTB = 0b00000000;
    DDRB  = 0b11111111;

    /* PC.7: digout: output low  */
    /* PC.6: unused: output low */
    PORTC = 0b00000000;
    DDRC  = 0b11000000;

    /* PD.7: unused: output low */
    /* PD.6: unused: output low */
    /* PD.5: transceiver power, output high */
    /* PD.4: unused: output low */
    /* PD.3: TX1: output high */
    /* PD.2: RX1: input pull up */
    /* PD.1: input high z */
    /* PD.0: input high z, power fail */
    PORTD = 0b00101100;
    DDRD  = 0b11111000;
    
    /* PE.6: unused: output low  */
    /* PE.2: unused: output low */
    PORTE = 0b00000000;
    DDRE  = 0b01000100;

    /* PF.7: unused: output low  */
    /* PF.6: unused: output low  */
    /* PF.5: unused: output low  */
    /* PF.4: unused: output low  */
    /* PF.1: unused: output low  */
    /* PF.0: unused: output low */
    PORTF = 0b00000000;
    DDRF  = 0b11110011;
}
