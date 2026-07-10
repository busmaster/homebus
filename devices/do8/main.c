/*
 * main.c
 *
 * Copyright 2026 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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
#include "digout.h"
#include "shader.h"
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

#ifdef BUSVAR
/* non volatile bus variables memory */
#define BUSVAR_NV_START         0x100
#define BUSVAR_NV_END           0x1ff
#endif

/* DO restore after power fail */
#define EEPROM_DO_RESTORE_START  (uint8_t *)0x200
#define EEPROM_DO_RESTORE_END    (uint8_t *)0x3ff

/* our bus address */
#define MY_ADDR    sMyAddr

#define IDLE_SIO1  0x01

/* acual value event */
#define RESPONSE_TIMEOUT_MS         100  /* time in ms */
/* timeout for unreachable client  */
#define RETRY_CYCLE_TIME_MS         200 /* time in ms */
#define CHANGE_DETECT_CYCLE_TIME_MS 500 /* time in ms */

/* timeout for doClockCalibReq */
#define CLOCK_CALIB_TIMEOUT_MS 200 /* time in ms */

#define MAX_FIRMWARE_SIZE   (28UL * 1024UL)

#define MAX_ADDR_SW8_AVE            64

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
volatile uint16_t gTime10Ms16 = 0;
volatile uint32_t gTimeMs32 = 0;
volatile uint16_t gTimeS = 0;

static TBusTelegram *spBusMsg;
static TBusTelegram  sTxBusMsg;

static const uint8_t *spNextPtrToEeprom;
static uint8_t       sMyAddr;

static uint8_t   sIdle = 0;

static TClient sClient[BUS_MAX_CLIENT_NUM];
static uint8_t sNumClients;

static uint8_t sOldDigOutActVal[BUS_DO8_DIGOUT_SIZE_ACTUAL_VALUE];
static uint8_t sCurDigOutActVal[BUS_DO8_DIGOUT_SIZE_ACTUAL_VALUE];
static uint8_t sCurShaderActVal[BUS_DO8_SHADER_SIZE_ACTUAL_VALUE];

static TClockCalib sClockCalib;

static uint8_t sSw8State[MAX_ADDR_SW8_AVE];

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void TimerInit(void);
static void TimerStart(void);
static void CheckButton(void);
static void ButtonEvent(uint8_t address, uint8_t button);
static void SwitchEvent(uint8_t address, uint8_t button, bool pressed);
static void Sw8SwitchEvent(uint8_t address, uint8_t state);
static void ProcessBus(uint8_t ret);
static void RestoreDigOut(void);
static void Idle(void);
static void IdleSio1(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);
static void CheckEvent(void);
static void GetClientListFromEeprom(void);
static void ClockCalibTask(void);
#ifdef BUSVAR
static bool BusVarNv(uint16_t address, void *buf, uint8_t bufSize, TBusVarDir dir);
#endif
/*-----------------------------------------------------------------------------
*  main
*/
int main(void) {

   uint8_t ret;
   int   sioHdl;

   /* get module address from EEPROM */
   sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
   GetClientListFromEeprom();

   PortInit();
   TimerInit();
   ButtonInit();
   DigOutInit();
   ShaderInit();
#ifdef BUSVAR
   // ApplicationInit might use BusVar
   BusVarInit(sMyAddr, BusVarNv);
#endif
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

   /* wait for full supply voltage */
   while (!POWER_GOOD);

   /* for delay in RestoreDigout the timer interruot is required */
   ENABLE_INT;
   TimerStart();
   
   RestoreDigOut();

   /* ext int for power fail: INT0 low level sensitive */
   EICRA &= ~((1 << ISC01) | (1 << ISC00));
   EIMSK |= (1 << INT0);

   ApplicationStart();

   sClockCalib.state = eCalibIdle;

   /* Hauptschleife */
   while (1) {
      Idle();
      ret = BusCheck();
      ProcessBus(ret);
      ClockCalibTask();
      CheckButton();
      DigOutStateCheck();
      ShaderCheck();
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

static uint8_t GetActualValueShader(uint8_t shader) {

    uint8_t      state;
    TShaderState shaderState;

    state = 252; /* not configured */
    if (ShaderGetState(shader, &shaderState)) {
        switch (shaderState) {
        case eShaderStopped:
            ShaderGetPosition(shader, &state);
            break;
        case eShaderClosing:
            state = 253;
            break;
        case eShaderOpening:
            state = 254;
            break;
        }
    }
    return state;
}

/*-----------------------------------------------------------------------------
*   post state changes to registered bus clients
*/
static void CheckEvent(void) {

    static uint8_t   sActualClient = 0xff; /* actual client's index being processed */
    static uint16_t  sChangeTestTimeStamp;
    TClient          *pClient;
    uint8_t          i;
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
        DigOutStateAll(sCurDigOutActVal, sizeof(sCurDigOutActVal));
        if (memcmp(sCurDigOutActVal, sOldDigOutActVal, sizeof(sCurDigOutActVal)) == 0) {
            actValChanged = false;
        } else {
            actValChanged = true;
        }

        if (actValChanged) {
            for (i = 0; i < sizeof(sCurShaderActVal); i++) {
                sCurShaderActVal[i] = GetActualValueShader(i);
            }
            memcpy(sOldDigOutActVal, sCurDigOutActVal, sizeof(sOldDigOutActVal));
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
        pActVal->devType = eBusDevTypeDo8;

        memcpy(pActVal->actualValue.do8.digOut, sCurDigOutActVal,
               sizeof(pActVal->actualValue.do8.digOut));
        memcpy(pActVal->actualValue.do8.shader, sCurShaderActVal,
               sizeof(pActVal->actualValue.do8.shader));

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
*  restore output state
*  byte 0: 0x00 output data valid
*  byte 1: 8 bit for 8 outputs. 
*/
static void RestoreDigOut(void) {

   uint8_t *ptrToEeprom;
   uint8_t buf;
   uint8_t flags;

   /* find the newest state */
   for (ptrToEeprom = EEPROM_DO_RESTORE_START;
        ptrToEeprom < EEPROM_DO_RESTORE_END;
        ptrToEeprom += 2) {
       if (eeprom_read_byte(ptrToEeprom) == 0x00) {
           break;
       }
   }
   if (ptrToEeprom > EEPROM_DO_RESTORE_END) {
       /* not found -> no restore */
      spNextPtrToEeprom = EEPROM_DO_RESTORE_START;
      return;
   }

    spNextPtrToEeprom = ptrToEeprom + 2;
    if (spNextPtrToEeprom >= EEPROM_DO_RESTORE_END) {
        spNextPtrToEeprom = EEPROM_DO_RESTORE_START;
    }

   /* restore */
   flags = DISABLE_INT;
   buf = eeprom_read_byte(ptrToEeprom + 1);
   RESTORE_INT(flags);

   DigOutAll(&buf, 1);

   /* delete old */
   flags = DISABLE_INT;
   eeprom_write_byte(ptrToEeprom, 0xff);
   RESTORE_INT(flags);
}

/*-----------------------------------------------------------------------------
*  detect button release
*/
static void CheckButton(void) {

   uint8_t      i = 0;
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
    union { // union for saving stack usage
        TBusDevRespInfo            *pInfo;
        TBusDevRespGetState        *pGetState;
        TBusDevRespActualValue     *pActVal;
        TBusDevReqActualValueEvent *pActValEv;
    } t;
    TClient                *pClient;
    TClockCalibState       calibState;
    static TBusTelegram    sTxMsg;
    static bool            sTxRetry = false;
    uint8_t                val8;
    uint32_t               val32;

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
        case eBusDevReqDiag:
        case eBusDevReqSetClientAddr:
        case eBusDevReqGetClientAddr:
        case eBusDevRespActualValueEvent:
        case eBusDevReqActualValueEvent:
        case eBusDevReqClockCalib:
        case eBusDevRespDoClockCalib:
        case eBusDevReqGetFlashData:
#ifdef BUSVAR
        case eBusDevReqGetVar:
        case eBusDevReqSetVar:
        case eBusDevRespGetVar:
        case eBusDevRespSetVar:
#endif
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
        t.pInfo = &sTxMsg.msg.devBus.x.devResp.info;
        sTxMsg.type = eBusDevRespInfo;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        t.pInfo->devType = eBusDevTypeDo8;
        strncpy((char *)(t.pInfo->version), ApplicationVersion(), sizeof(t.pInfo->version));
        t.pInfo->version[sizeof(t.pInfo->version) - 1] = '\0';
        for (i = 0; i < BUS_DO8_NUM_SHADER; i++) {
            ShaderGetConfig(i, &(t.pInfo->devInfo.do8.onSwitch[i]), &(t.pInfo->devInfo.do8.dirSwitch[i]));
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqActualValue:
        /* response packet */
        t.pActVal = &sTxMsg.msg.devBus.x.devResp.actualValue;
        sTxMsg.type = eBusDevRespActualValue;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        t.pActVal->devType = eBusDevTypeDo8;
        DigOutStateAll(t.pActVal->actualValue.do8.digOut, BUS_DO8_DIGOUT_SIZE_ACTUAL_VALUE);

        memset(t.pActVal->actualValue.do8.shader, 0, sizeof(t.pActVal->actualValue.do8.shader));
        for (i = 0; i < NUM_SHADER; i++) {
            t.pActVal->actualValue.do8.shader[i] = GetActualValueShader(i);
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetValue:
        if (spBusMsg->msg.devBus.x.devReq.setValue.devType != eBusDevTypeDo8) {
            break;
        }
        for (i = 0; i < NUM_DIGOUT; i++) {
            /* für Rollladenfunktion konfigurierte Ausgänge werden nicht geändert */
            if (!DigOutGetShaderFunction(i)) {
                uint8_t action = (spBusMsg->msg.devBus.x.devReq.setValue.setValue.do8.digOut[i / 4] >>
                                 ((i % 4) * 2)) & 0x03;
                switch (action) {
                case 0x00:
                    break;
                case 0x01:
                    DigOutTrigger(i);
                    break;
                case 0x02:
                    DigOutOff(i);
                    break;
                case 0x03:
                    DigOutOn(i);
                    break;
                default:
                    break;
                }
            }
        }
        for (i = 0; i < NUM_SHADER; i++) {
            uint8_t position = spBusMsg->msg.devBus.x.devReq.setValue.setValue.do8.shader[i];
            if (position <= 100) {
                ShaderSetPosition(i, position);
            } else if (position == 255) {
                // stop
                ShaderSetAction(i, eShaderStop);
            }
        }
        /* response packet */
        sTxMsg.type = eBusDevRespSetValue;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqActualValueEvent:
        t.pActValEv = &spBusMsg->msg.devBus.x.devReq.actualValueEvent;
        if (t.pActValEv->devType == eBusDevTypeSw8) {
            val8 = t.pActValEv->actualValue.sw8.state;
            Sw8SwitchEvent(spBusMsg->senderAddr, val8);
            /* response packet */
            sTxMsg.type = eBusDevRespActualValueEvent;
            sTxMsg.senderAddr = MY_ADDR;
            sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
            sTxMsg.msg.devBus.x.devResp.actualValueEvent.devType = eBusDevTypeSw8;
            sTxMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.sw8.state = val8;
            sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        }
        break;
    case eBusDevReqSwitchState:
        val8 = spBusMsg->msg.devBus.x.devReq.switchState.switchState;
        Sw8SwitchEvent(spBusMsg->senderAddr, val8);
        /* response packet */
        sTxMsg.type = eBusDevRespSwitchState;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.switchState.switchState = val8;
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
    case eBusDevReqDiag:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespDiag;
        sTxMsg.msg.devBus.x.devResp.diag.devType = eBusDevTypeDo8;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        memset(sTxMsg.msg.devBus.x.devResp.diag.data, 0, sizeof(sTxMsg.msg.devBus.x.devResp.diag.data));
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevRespActualValueEvent:
        pClient = sClient;
        for (i = 0; i < sNumClients; i++) {
            if ((pClient->address == spBusMsg->senderAddr) &&
                (pClient->state == eEventWaitForConfirmation)) {
                TBusDevActualValueDo8 *p;
                uint8_t j;
                uint8_t buf[BUS_DO8_DIGOUT_SIZE_ACTUAL_VALUE];

                DigOutStateAllStandard(buf, sizeof(buf));

                p = &spBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.do8;
                for (j = 0;
                     (j < BUS_DO8_SHADER_SIZE_ACTUAL_VALUE) &&
                     (p->shader[j] == GetActualValueShader(j));
                     j++);
                if ((j == BUS_DO8_SHADER_SIZE_ACTUAL_VALUE) &&
                    (memcmp(p->digOut, buf, sizeof(buf)) == 0)) {
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
            uint8_t *p = &(spBusMsg->msg.devBus.x.devReq.setClientAddr.clientAddr[i]);
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
    case eBusDevReqGetFlashData:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespGetFlashData;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.getFlashData.addr = spBusMsg->msg.devBus.x.devReq.getFlashData.addr;
        val8 = sizeof(sTxMsg.msg.devBus.x.devResp.getFlashData.data);
        val32 = spBusMsg->msg.devBus.x.devReq.getFlashData.addr;
        if (val32 < MAX_FIRMWARE_SIZE) {
            val8 = min(val8, MAX_FIRMWARE_SIZE - val32);
        } else {
            val8 = 0;
        }
        sTxMsg.msg.devBus.x.devResp.getFlashData.numValid = val8;
        for (i = 0; i < val8; i++) {
            sTxMsg.msg.devBus.x.devResp.getFlashData.data[i] = pgm_read_byte_far(val32 + i);
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
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
      buttonEventData.address = address;
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

   buttonEventData.address = address;
   buttonEventData.pressed = pressed;
   buttonEventData.buttonNr = button;
   ApplicationEventButton(&buttonEventData);
}

static void Sw8SwitchEvent(uint8_t address, uint8_t state) {

    uint8_t i;
    uint8_t mask;

    if (address >= MAX_ADDR_SW8_AVE) {
        return;
    }

    for (mask = 1, i = 0; i < 8; i++, mask <<= 1) {
        if ((sSw8State[address] ^ state) & mask) {
            SwitchEvent(address, i + 1, state & mask);
        }
    }
    sSw8State[address] = state;
}

/*-----------------------------------------------------------------------------
*  powerfail interrupt (Ext. Int 0)
*/
ISR(INT0_vect) {

   uint8_t *ptrToEeprom;
   uint8_t buf[2];
   uint8_t i;

   /* read output state */
   DigOutStateAllStandard(&buf[1], sizeof(buf[1]));
   /* switch off */
   DigOutOffAll();

   /* save output state */
   ptrToEeprom = (uint8_t *)spNextPtrToEeprom;
   /* Kennzeichnungsbit löschen */
   buf[0] = 0;
   for (i = 0; i < sizeof(buf); i++) {
      eeprom_write_byte(ptrToEeprom, buf[i]);
      ptrToEeprom++;
   }

   /* Wait for completion of previous write */
   while (!eeprom_is_ready());

   /* wait for powerfail */
   /* if supply comes up again we use the wtd to force a reboot */
   while (!POWER_GOOD);

   /* supply ok again */
   /* reset in 2 s */
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
* Timer irq
*/
ISR(TIMER3_COMPA_vect)  {

   static uint16_t sCounter1 = 0;
   static uint8_t  sCounter2 = 0;

   OCR3A = OCR3A + TIMER_TCNT_INC;
    
   gTimeMs += TIMER_INC_MS;
   gTimeMs16 += TIMER_INC_MS;
   gTimeMs32 += TIMER_INC_MS;
   sCounter1++;
   if (sCounter1 >= (1000 / TIMER_INC_MS)) {
      sCounter1 = 0;
      /* seconds */
      gTimeS++;
   }
   sCounter2++;
   if (sCounter2 >= (10 / TIMER_INC_MS)) {
      sCounter2 = 0;
      /* 10 ms counter */
      gTime10Ms16++;
   }
}

/*-----------------------------------------------------------------------------
*  Einstellung der Portpins
*/
static void PortInit(void) {

    /* PB.7: unused: output low */
    /* PB.6, DO0: output low */
    /* PB.5: unused: output low */
    /* PB.4: unused: output low */
    /* PB.3: MISO: output low */
    /* PB.2: MOSI: output low */
    /* PB.1: SCK: output low */
    /* PB.0: unused: output low */
    PORTB = 0b00000000;
    DDRB  = 0b11111111;

    /* PC.7: DO7: output low  */
    /* PC.6: DO5: output low */
    PORTC = 0b00000000;
    DDRC  = 0b11000000;

    /* PD.7: DO1: output low */
    /* PD.6: unused: output low */
    /* PD.5: transceiver power, output high */
    /* PD.4: unused: output low */
    /* PD.3: TXD: output high */
    /* PD.2: RXD: input pull up */
    /* PD.1: INT1: input high z */
    /* PD.0: INT0: input high z */
    PORTD = 0b00101100;
    DDRD  = 0b11111000;
    
    /* PE.6: DO3: output low  */
    /* PE.2: unused: output low */
    PORTE = 0b00000000;
    DDRE  = 0b01000100;

    /* PF.7: unused: output low  */
    /* PF.6: DO6: output low  */
    /* PF.5: unused: output low  */
    /* PF.4: unused: output low  */
    /* PF.1: DO4: output low  */
    /* PF.0: DO2: output low */
    PORTF = 0b00000000;
    DDRF  = 0b11110011;
}
