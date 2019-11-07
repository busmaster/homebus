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
#include "button.h"
#include "led.h"
#include "pwm.h"
#include "application.h"
#include "i2cmaster.h"
#include "pca9685.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

/* offset addresses in EEPROM */
#define MODUL_ADDRESS           0  /* 1 byte */
#define CLIENT_ADDRESS_BASE     1  /* BUS_MAX_CLIENT_NUM from bus.h (16 byte) */
#define CLIENT_RETRY_CNT        17 /* size: 16 byte (BUS_MAX_CLIENT_NUM)      */

/* PWM restore after power fail */
#define EEPROM_PWM_RESTORE_START  (uint8_t *)3072
#define EEPROM_PWM_RESTORE_END    (uint8_t *)4095

/* our bus address */
#define MY_ADDR    sMyAddr

#define IDLE_SIO0  0x01

/* acual value event */
#define RESPONSE_TIMEOUT_MS         100  /* time in ms */
/* timeout for unreachable client  */
#define RETRY_CYCLE_TIME_MS         200 /* time in ms */
#define CHANGE_DETECT_CYCLE_TIME_MS 400 /* time in ms */

#ifdef BUSVAR
/* non volatile bus variables memory */
#define BUSVAR_NV_START         0x100
#define BUSVAR_NV_END           0x2ff
#endif

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
static bool          sOldPwmState[NUM_PWM_CHANNEL];

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void TimerInit(void);
static void CheckButton(void);
static void ButtonEvent(uint8_t address, uint8_t button);
static void SwitchEvent(uint8_t address, uint8_t button, bool pressed);
static void ProcessBus(uint8_t ret);
static void RestorePwm(void);
static void Idle(void);
static void IdleSio0(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);
static void CheckEvent(void);
static void GetClientListFromEeprom(void);
#ifdef BUSVAR
static bool BusVarNv(uint16_t address, void *buf, uint8_t bufSize, TBusVarDir dir);
#endif
/*-----------------------------------------------------------------------------
*  main
*/
int main(void) {

   uint8_t ret;
   int   sioHdl;

   cli();
   MCUSR = 0x00;
   wdt_disable();

   /* get module address from EEPROM */
   sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
   GetClientListFromEeprom();

   PortInit();
   LedInit();
   TimerInit();
   ButtonInit();
   PwmInit();
#ifdef BUSVAR
   // ApplicationInit might use BusVar
   BusVarInit(sMyAddr, BusVarNv);
#endif
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
   /* warten for full operation voltage */
   while (!POWER_GOOD);

   /* enable ints before RestorePwm() */
   ENABLE_INT;
   RestorePwm();

   /* ext int for power fail: PCINT29 low level sensitive */

   PCICR |= (1 << PCIE3);
   PCIFR |= (1<<PCIF3);
   PCMSK3 |= (1 << PCINT29);

   LedSet(eLedGreenFlashSlow);
   ApplicationStart();

   /* Hauptschleife */
   while (1) {
      Idle();
      ret = BusCheck();
      ProcessBus(ret);
      CheckButton();
      PwmCheck();
      LedCheck();
      ApplicationCheck();
      CheckEvent();
#ifdef BUSVAR
      BusVarProcess();
#endif
   }
   return 0;
}
#ifdef BUSVAR
/*-----------------------------------------------------------------------------
*  NV memory for persist bus variables
*/
static bool BusVarNv(uint16_t address, void *buf, uint8_t bufSize, TBusVarDir dir) {

    void *eeprom;

    // range check
    if ((address + bufSize) > (BUSVAR_NV_END - BUSVAR_NV_START + 1)) {
        return false;
    }

    eeprom = (void *)(BUSVAR_NV_START + address);
    if (dir == eBusVarRead) {
        eeprom_read_block(buf, eeprom, bufSize);
    } else {
        eeprom_update_block(buf, eeprom, bufSize);
    }
    return true;
}
#endif
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
    static uint8_t   sCurPwmActVal[NUM_PWM_CHANNEL];
    static bool      sCurPwmState[NUM_PWM_CHANNEL];
    uint8_t          i;
    uint16_t          val16;

    if (sNumClients == 0) {
        return;
    }

    /* do the change detection not in each cycle */
    GET_TIME_MS16(actualTime16);
    if (((uint16_t)(actualTime16 - sChangeTestTimeStamp)) >= CHANGE_DETECT_CYCLE_TIME_MS) {
        PwmGetAll(sCurPwmActVal, sizeof(sCurPwmActVal));
        for (i = 0; i < NUM_PWM_CHANNEL; i++) {
            PwmIsOn(i, &sCurPwmState[i]);
        }
        if ((memcmp(sCurPwmActVal, sOldPwmActVal, sizeof(sCurPwmActVal)) == 0) &&
            (memcmp(sCurPwmState,  sOldPwmState,  sizeof(sCurPwmActVal)) == 0)) {
            actValChanged = false;
        } else {
            actValChanged = true;
        }
        if (actValChanged) {
            memcpy(sOldPwmActVal, sCurPwmActVal, sizeof(sOldPwmActVal));
            memcpy(sOldPwmState,  sCurPwmState,  sizeof(sOldPwmState));
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
        pActVal->devType = eBusDevTypePwm16;
        memcpy(pActVal->actualValue.pwm16.pwm, sCurPwmActVal,
               sizeof(pActVal->actualValue.pwm16.pwm));
        val16 = 0;
        for (i = 0; i < NUM_PWM_CHANNEL; i++) {
            val16 |= sCurPwmState[i] ? 1 << i : 0;
        }
        pActVal->actualValue.pwm16.state = val16;
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
*  restore pwm output state
*
*  restore block:
*       byte 0: 0b01010101:
*       byte 1: 0bxxxxxxxx: on/off mask: 0 off, 1 on , bit0 .. channel 0
*                                                      bit1 .. channel 1
*                                                      bit2 .. channel 2
*                                                      bit3 .. channel 3
*                                                      bit4 .. channel 4
*                                                      bit5 .. channel 5
*                                                      bit6 .. channel 6
*                                                      bit7 .. channel 7
*       byte 2: 0bxxxxxxxx: on/off mask: 0 off, 1 on , bit0 .. channel 8
*                                                      bit1 .. channel 9
*                                                      bit2 .. channel 10
*                                                      bit3 .. channel 11
*                                                      bit4 .. channel 12
*                                                      bit5 .. channel 13
*                                                      bit6 .. channel 14
*                                                      bit7 .. channel 15
*       byte 3: low byte channel 0
*       byte 4: low byte channel 1
*       byte 5: low byte channel 2
*       byte 6: low byte channel 3
*       byte 7: low byte channel 4
*       byte 8: low byte channel 5
*       byte 9: low byte channel 6
*       byte 10: low byte channel 7
*       byte 11: low byte channel 8
*       byte 12: low byte channel 9
*       byte 13: low byte channel 10
*       byte 14: low byte channel 11
*       byte 15: low byte channel 12
*       byte 16: low byte channel 13
*       byte 17: low byte channel 14
*       byte 18: low byte channel 15
*/
static void RestorePwm(void) {

    uint8_t     *ptrToEeprom;
    uint8_t     flags;
    uint8_t     sizeRestore = 3 + NUM_PWM_CHANNEL * sizeof(uint8_t);
    uint8_t     pwmLow;
    uint16_t     pwmMask;
    uint8_t     i;

    /* find the newest state */
    for (ptrToEeprom = EEPROM_PWM_RESTORE_START;
         ptrToEeprom < (EEPROM_PWM_RESTORE_END - sizeRestore);
         ptrToEeprom += sizeRestore) {
        pwmMask = eeprom_read_byte(ptrToEeprom);
        if (pwmMask == 0x55) {
			pwmMask = eeprom_read_byte(ptrToEeprom+1);
			pwmMask+= (eeprom_read_byte(ptrToEeprom+2)<<8);
            break;
        }
    }
    if (ptrToEeprom > (EEPROM_PWM_RESTORE_END - sizeRestore)) {
        /* not found -> no restore
         * set pwm[] = 0xffff, state off
         */
        for (i = 0; i < NUM_PWM_CHANNEL; i++) {
            PwmOn(i, false);
            PwmSet(i, 0x00);
        }
        spNextPtrToEeprom = EEPROM_PWM_RESTORE_START;
        return;
    }

    /* restore pwm output */
    flags = DISABLE_INT;
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        if (pwmMask & (1 << i)) {
            PwmOn(i, true);
        }
        pwmLow  = eeprom_read_byte(ptrToEeprom + 3 + i * sizeof(uint8_t));
        PwmSet(i, pwmLow);
    }
    /* delete  */
    eeprom_write_byte((uint8_t *)ptrToEeprom, 0xff);
    eeprom_write_byte((uint8_t *)ptrToEeprom + 1, 0xff);
    eeprom_write_byte((uint8_t *)ptrToEeprom + 2, 0xff);
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        eeprom_write_byte(ptrToEeprom + 3 + i, 0xff);
    }

    RESTORE_INT(flags);
	spNextPtrToEeprom = ptrToEeprom + sizeRestore;
    if (spNextPtrToEeprom > (EEPROM_PWM_RESTORE_END - sizeRestore)) {
        spNextPtrToEeprom = EEPROM_PWM_RESTORE_START;
    }
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
    uint8_t                action;
    TBusDevRespInfo        *pInfo;
    TBusDevRespActualValue *pActVal;
    TClient                *pClient;
    static TBusTelegram    sTxMsg;
    static bool            sTxRetry = false;
    bool                   flag;
    uint8_t                val8;
	uint16_t			   val16;
	uint32_t			   mask32;

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
#ifdef BUSVAR
        case eBusDevReqGetVar:
        case eBusDevReqSetVar:
        case eBusDevRespGetVar:
        case eBusDevRespSetVar:
#endif
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
		PWM_ALLOUT_DISABLE;
		PwmExit();
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
        pInfo->devType = eBusDevTypePwm16;
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
        pActVal->devType = eBusDevTypePwm16;
        PwmGetAll(pActVal->actualValue.pwm16.pwm, sizeof(pActVal->actualValue.pwm16.pwm));
        val16 = 0;
        for (i = 0; i < NUM_PWM_CHANNEL; i++) {
            PwmIsOn(i, &flag);
            val16 |= flag ? 1 << i: 0;
        }
        pActVal->actualValue.pwm16.state = val16;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetValue:
        if (spBusMsg->msg.devBus.x.devReq.setValue.devType != eBusDevTypePwm16) {
            break;
        }
        mask32 = spBusMsg->msg.devBus.x.devReq.setValue.setValue.pwm16.set;
        for (i = 0; i < NUM_PWM_CHANNEL; i++) {
            action = (0x3UL << (i * 2) & mask32) >> (i * 2);
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
                PwmSet(i, spBusMsg->msg.devBus.x.devReq.setValue.setValue.pwm16.pwm[i]);
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
                TBusDevActualValuePwm16 *p;
                uint8_t buf[NUM_PWM_CHANNEL];

                PwmGetAll(buf, sizeof(buf));
                val8 = 0;
                for (i = 0; i < NUM_PWM_CHANNEL; i++) {
                    PwmIsOn(i, &flag);
                    val8 |= flag ? 1 << i: 0;
                }
                p = &spBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.pwm16;
                if ((memcmp(p->pwm, buf, sizeof(buf)) == 0) &&
                    (p->state == val8)) {
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
#ifdef BUSVAR
    case eBusDevReqGetVar:
        val8 = spBusMsg->msg.devBus.x.devReq.getVar.index;
        sTxMsg.msg.devBus.x.devResp.getVar.length =
            BusVarRead(val8, sTxMsg.msg.devBus.x.devResp.getVar.data,
                       sizeof(sTxMsg.msg.devBus.x.devResp.getVar.data),
                       &sTxMsg.msg.devBus.x.devResp.getVar.result);
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespGetVar;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.getVar.index = val8;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetVar:
        val8 = spBusMsg->msg.devBus.x.devReq.setVar.index;
        BusVarWrite(val8, spBusMsg->msg.devBus.x.devReq.setVar.data,
                    spBusMsg->msg.devBus.x.devReq.setVar.length,
                    &sTxMsg.msg.devBus.x.devResp.setVar.result);
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespSetVar;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.setVar.index = val8;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevRespSetVar:
        BusVarRespSet(spBusMsg->senderAddr, &spBusMsg->msg.devBus.x.devResp.setVar);
        break;
    case eBusDevRespGetVar:
        BusVarRespGet(spBusMsg->senderAddr, &spBusMsg->msg.devBus.x.devResp.getVar);
        break;
#endif
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
*  Power-Fail Interrupt (PCINT29)
*/
ISR(PCINT3_vect) {

    uint8_t  *ptrToEeprom;
    uint8_t pwm[NUM_PWM_CHANNEL];
    uint8_t  i;
    bool     on;
    uint16_t  pwmMask = 0;

    if(!POWER_GOOD) {
       LedSet(eLedRedOn);
       PWM_ALLOUT_DISABLE;

       for (i = 0; i < NUM_PWM_CHANNEL; i++) {
           PwmGet(i, &pwm[i]);
           PwmIsOn(i, &on);
           if (on) {
               pwmMask |= (uint16_t)(1 << i);
           }
       }
       /* switch off all outputs */
       PwmExit();

       /* save pwm state to eeprom */
       ptrToEeprom = spNextPtrToEeprom;
       eeprom_write_byte(ptrToEeprom, 0x55);
	   ptrToEeprom++;
       eeprom_write_byte(ptrToEeprom, pwmMask & 0xff);
	   ptrToEeprom++;
	   eeprom_write_byte(ptrToEeprom, pwmMask >> 8);
       ptrToEeprom++;
       for (i = 0; i < NUM_PWM_CHANNEL; i++) {
           eeprom_write_byte(ptrToEeprom, pwm[i]);
           ptrToEeprom++;
       }

      /* Wait for completion of previous write */
      while (!eeprom_is_ready());
      LedSet(eLedGreenOn);
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
}

/*-----------------------------------------------------------------------------
*  Timerinitialisierung
*/
static void TimerInit(void) {

   /* Verwendung des Compare-Match Interrupt von Timer0 */
   /* Vorteiler bei 1 MHz: 8  */
   /* Vorteiler bei 3.6864 MHz: 64  */
   /* Vorteiler bei 16 MHz: 256  */
   /* Compare-Match Portpin (OC0) wird nicht verwendet: COM01:0 = 0 */
   /* Compare-Register:  */
   /* 1 MHz: 250 -> 2 ms Zyklus */
   /* 3.6864 MHz: 115 -> 1,9965 ms Zyklus */
   /* 16 MHz: 125 -> 2 ms Zyklus */
   /* Timer-Mode: CTC: WGM01:0=2 */
#if (F_CPU == 1000000UL)
   /* 1 MHz */
   TCCR0A=(0<<COM0A1) | (0<<COM0A0) | (0<<COM0B1) | (0<<COM0B0) | (1<<WGM01) | (0<<WGM00);
   TCCR0B = (0b010 << CS00) | (0 << WGM02);
   OCR0A = 250 - 1;
#elif (F_CPU == 1600000UL)
   /* 16 MHz */
   TCCR0A=(0<<COM0A1) | (0<<COM0A0) | (0<<COM0B1) | (0<<COM0B0) | (1<<WGM01) | (0<<WGM00);
   TCCR0B = (0b100 << CS00) | (0 << WGM02);
   OCR0A = 125 - 1;
#elif (F_CPU == 3686400UL)
   /* 3.6864 MHz */
   TCCR0A=(0<<COM0A1) | (0<<COM0A0) | (0<<COM0B1) | (0<<COM0B0) | (1<<WGM01) | (0<<WGM00);
   TCCR0B=(0<<WGM02) | (0b011 << CS00);
   OCR0A = 115 - 1;
#else
#error adjust timer settings for your CPU clock frequency
#endif
   /* Timer Compare Match Interrupt enable */
   TIMSK0 |= 1 << OCIE0A;
}

/*-----------------------------------------------------------------------------
*  Timer-Interrupt für Zeitbasis (Timer 0 Compare)
*/
ISR(TIMER0_COMPA_vect)  {
   static uint16_t sCounter1 = 0;
   static uint8_t  sCounter2 = 0;

   /* Interrupt alle 2ms */
   gTimeMs += 2;
   gTimeMs16 += 2;
   gTimeMs32 += 2;
   sCounter1++;
   if (sCounter1 >= 500) {
      sCounter1 = 0;
      /* Sekundenzähler */
      gTimeS++;
   }
   sCounter2++;
   if (sCounter2 >= 5) {
      sCounter2 = 0;
      /* 10 ms counter */
      gTime10Ms16++;
   } 
}

/*-----------------------------------------------------------------------------
*  port settings
*/
static void PortInit(void) {

    /* PB.7: unused: output low */
    /* PB.6, unused: output low */
    /* PB.5: unused: output low */
    /* PB.4: unused: output low */
    /* PB.3: MISO: output low */
    /* PB.2: MOSI: output low */
    /* PB.1: SCK: output low */
    /* PB.0: unused: output low */
    PORTB = 0b00000000;
    DDRB  = 0b11111011;

    /* PC.7: unused: output low */
    /* PC.6: unused: output low */
    /* PC.5: unused: output low */
    /* PC.4: unused: output low */
    /* PC.3: unused: output low */
    /* PC.2: /OE: output low */
    /* PC.1: SDA: output low  */
    /* PC.0: SCL: output low */
    PORTC = 0b00000100;
    DDRC  = 0b11111111;

    /* PD.7: led1: output low */
    /* PD.6: led2: output low */
    /* PD.5: input high z, power fail */
    /* PD.4: transceiver power, output high */
    /* PD.3: TX1: output high */
    /* PD.2: RX1: input pull up */
    /* PD.1: TX0: output high */
    /* PD.0: RX0: input pull up */

    PORTD = 0b00000101;
    DDRD  = 0b11011010;
}
