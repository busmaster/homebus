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
#include "digout.h"
#include "shader.h"
#include "application.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

/* Bits in TCCR0 */
#define CS00     0
#define OCIE0    1
#define WGM00    6
#define WGM01    3

/* Bits in WDTCR */
#define WDCE     4
#define WDE      3
#define WDP0     0
#define WDP1     1
#define WDP2     2

/* Bits in EECR */
#define EEWE     1

/* offset addresses in EEPROM */
#define MODUL_ADDRESS           0  /* 1 byte */
#define CLIENT_ADDRESS_BASE     1  /* BUS_MAX_CLIENT_NUM from bus.h (16 byte) */
#define CLIENT_RETRY_CNT        17 /* size: 16 byte (BUS_MAX_CLIENT_NUM)      */

#ifdef BUSVAR
/* non volatile bus variables memory */
#define BUSVAR_NV_START         0x100
#define BUSVAR_NV_END           0x2ff
#endif

/* DO restore after power fail */
#define EEPROM_DO_RESTORE_START  (uint8_t *)3072
#define EEPROM_DO_RESTORE_END    (uint8_t *)4095

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

static uint8_t sOldDigOutActVal[BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE];
static uint8_t sCurDigOutActVal[BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE];
static uint8_t sOldShaderActVal[BUS_DO31_SHADER_SIZE_ACTUAL_VALUE];
static uint8_t sCurShaderActVal[BUS_DO31_SHADER_SIZE_ACTUAL_VALUE];

static TClockCalib sClockCalib;

static uint8_t sSw8State[256];

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void TimerInit(void);
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
   LedInit();
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

   /* warten bis Betriebsspannung auf 12 V-Seite volle Höhe erreicht hat */
   while (!POWER_GOOD);

   /* für Delay wird timer-Interrupt benötigt (DigOutAll() in RestoreDigOut()) */
   ENABLE_INT;

   RestoreDigOut();

   /* ext int for power fail: INT0 low level sensitive */
   EICRA &= ~((1 << ISC01) | (1 << ISC00));
   EIMSK |= (1 << INT0);

   LedSet(eLedGreenFlashSlow);

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
        pActVal->devType = eBusDevTypeDo31;

        memcpy(pActVal->actualValue.do31.digOut, sCurDigOutActVal,
               sizeof(pActVal->actualValue.do31.digOut));
        memcpy(pActVal->actualValue.do31.shader, sCurShaderActVal,
               sizeof(pActVal->actualValue.do31.shader));

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
*  Ausgangszustand wiederherstellen
*  im EEPROM liegen jeweils 4 Byte mit den 31 Ausgangszuständen. Das MSB im
*  letzten Byte zeigt mit dem Wert 0 die Gültigkeit der Daten an
*/
static void RestoreDigOut(void) {

   uint8_t *ptrToEeprom;
   uint8_t buf[4];
   uint8_t flags;

   /* zuletzt gespeicherten Zustand der Ausgänge suchen */
   for (ptrToEeprom = EEPROM_DO_RESTORE_START + 3; 
        ptrToEeprom <= EEPROM_DO_RESTORE_END;
        ptrToEeprom += 4) {
      if ((eeprom_read_byte(ptrToEeprom) & 0x80) == 0x00) {
         break;
      }
   }
   if (ptrToEeprom > EEPROM_DO_RESTORE_END) {
      /* nichts im EEPROM gefunden -> Ausgänge bleiben ausgeschaltet */
      spNextPtrToEeprom = EEPROM_DO_RESTORE_START;
      return;
   }

   /* wieder auf durch vier teilbare Adresse zurückrechnen */
   ptrToEeprom -= 3;
   /* Schreibhäufigkeit im EEPROM auf alle Adressen verteilen (wegen Lebensdauer) */
   spNextPtrToEeprom = (const uint8_t *)((int)ptrToEeprom + 4);
   if (spNextPtrToEeprom > EEPROM_DO_RESTORE_END) {
      spNextPtrToEeprom = EEPROM_DO_RESTORE_START;
    }

   /* Ausgangszustand wiederherstellen */
   flags = DISABLE_INT;
   buf[0] = eeprom_read_byte(ptrToEeprom);
   buf[1] = eeprom_read_byte(ptrToEeprom + 1);
   buf[2] = eeprom_read_byte(ptrToEeprom + 2);
   buf[3] = eeprom_read_byte(ptrToEeprom + 3);
   RESTORE_INT(flags);

   DigOutAll(buf, 4);

   /* alte Ausgangszustand löschen */
   flags = DISABLE_INT;
   eeprom_write_byte((uint8_t *)ptrToEeprom, 0xff);
   eeprom_write_byte((uint8_t *)ptrToEeprom + 1, 0xff);
   eeprom_write_byte((uint8_t *)ptrToEeprom + 2, 0xff);
   eeprom_write_byte((uint8_t *)ptrToEeprom + 3, 0xff);
   RESTORE_INT(flags);
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
    TShaderState           shaderState;
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

    if (sTxRetry) {
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        return;
    }
	
    if (ret == BUS_MSG_OK) {
        msgType = spBusMsg->type;
        switch (msgType) {
        case eBusDevReqReboot:
        case eBusDevReqInfo:
        case eBusDevReqGetState:
        case eBusDevReqSetState:
        case eBusDevReqActualValue:
        case eBusDevReqSetValue:
        case eBusDevReqSwitchState:
        case eBusDevReqSetAddr:
        case eBusDevReqEepromRead:
        case eBusDevReqEepromWrite:
        case eBusDevReqSetClientAddr:
        case eBusDevReqGetClientAddr:
        case eBusDevRespActualValueEvent:
        case eBusDevReqActualValueEvent:
        case eBusDevReqClockCalib:
        case eBusDevRespDoClockCalib:
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
        t.pInfo = &sTxMsg.msg.devBus.x.devResp.info;
        sTxMsg.type = eBusDevRespInfo;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        t.pInfo->devType = eBusDevTypeDo31;
        strncpy((char *)(t.pInfo->version), ApplicationVersion(), sizeof(t.pInfo->version));
        t.pInfo->version[sizeof(t.pInfo->version) - 1] = '\0';
        for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
            ShaderGetConfig(i, &(t.pInfo->devInfo.do31.onSwitch[i]), &(t.pInfo->devInfo.do31.dirSwitch[i]));
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqGetState:
        /* response packet */
        t.pGetState = &sTxMsg.msg.devBus.x.devResp.getState;
        sTxMsg.type = eBusDevRespGetState;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        t.pGetState->devType = eBusDevTypeDo31;
        DigOutStateAll(t.pGetState->state.do31.digOut, BUS_DO31_DIGOUT_SIZE_GET);

        /* array init */
        for (i = 0; i < BUS_DO31_SHADER_SIZE_GET; i++) {
            t.pGetState->state.do31.shader[i] = 0;
        }

        for (i = 0; i < eShaderNum; i++) {
            if (ShaderGetState(i, &shaderState) == true) {
                switch (shaderState) {
                case eShaderClosing:
                    val8 = 0x02;
                    break;
                case eShaderOpening:
                    val8 = 0x01;
                    break;
                case eShaderStopped:
                    val8 = 0x03;
                    break;
                default:
                    val8 = 0x00;
                    break;
                }
            } else {
                val8 = 0x00;
            }
            if ((i / 4) >= BUS_DO31_SHADER_SIZE_GET) {
                /* falls im Bustelegram zuwenig Platz -> abbrechen */
                /* (sollte nicht auftreten) */
                break;
            }
            t.pGetState->state.do31.shader[i / 4] |= (val8 << ((i % 4) * 2));
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetState:
        if (spBusMsg->msg.devBus.x.devReq.setState.devType != eBusDevTypeDo31) {
            break;
        }
        for (i = 0; i < eDigOutNum; i++) {
            /* für Rollladenfunktion konfigurierte Ausgänge werden nicht geändert */
            if (!DigOutGetShaderFunction(i)) {
                uint8_t action = (spBusMsg->msg.devBus.x.devReq.setState.state.do31.digOut[i / 4] >>
                                 ((i % 4) * 2)) & 0x03;
                switch (action) {
                case 0x00:
                case 0x01:
                    break;
                case 0x02:
                    DigOutOff(i);
                    break;
                case 0x3:
                    DigOutOn(i);
                    break;
                default:
                    break;
                }
            }
        }
        for (i = 0; i < eShaderNum; i++) {
            uint8_t action = (spBusMsg->msg.devBus.x.devReq.setState.state.do31.shader[i / 4] >>
                             ((i % 4) * 2)) & 0x03;
            switch (action) {
            case 0x00:
                /* no action */
                break;
            case 0x01:
                ShaderSetAction(i, eShaderOpen);
                break;
            case 0x02:
                ShaderSetAction(i, eShaderClose);
                break;
            case 0x03:
                ShaderSetAction(i, eShaderStop);
                break;
            default:
                break;
            }
        }
        /* response packet */
        sTxMsg.type = eBusDevRespSetState;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqActualValue:
        /* response packet */
        t.pActVal = &sTxMsg.msg.devBus.x.devResp.actualValue;
        sTxMsg.type = eBusDevRespActualValue;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        t.pActVal->devType = eBusDevTypeDo31;
        DigOutStateAll(t.pActVal->actualValue.do31.digOut, BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE);

        /* array init */
        for (i = 0; i < BUS_DO31_SHADER_SIZE_ACTUAL_VALUE; i++) {
            t.pActVal->actualValue.do31.shader[i] = 0;
        }
        for (i = 0; i < eShaderNum; i++) {
            t.pActVal->actualValue.do31.shader[i] = GetActualValueShader(i);
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;				
    case eBusDevReqSetValue:
        if (spBusMsg->msg.devBus.x.devReq.setValue.devType != eBusDevTypeDo31) {
            break;
        }
        for (i = 0; i < eDigOutNum; i++) {
            /* für Rollladenfunktion konfigurierte Ausgänge werden nicht geändert */
            if (!DigOutGetShaderFunction(i)) {
                uint8_t action = (spBusMsg->msg.devBus.x.devReq.setValue.setValue.do31.digOut[i / 4] >>
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
        for (i = 0; i < eShaderNum; i++) {
            uint8_t position = spBusMsg->msg.devBus.x.devReq.setValue.setValue.do31.shader[i];
            if (position <= 100) {
                ShaderSetPosition(i, position);
            } else if(position <= 228 && position > 127) {
			    ShaderSetHandPosition(i, position&~0x80);
		    } else if (position == 255) {
                // stop
                ShaderSetAction(i, eShaderStop);
			} else if (position == 250) {
                // 1 % open
                ShaderSetSwipp(i, 1);
            } else if (position == 251) {
                // 2 % open
                ShaderSetSwipp(i, 2);
            } else if (position == 252) {
                // 3 % open
                ShaderSetSwipp(i, 3);
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
    case eBusDevRespActualValueEvent:
        pClient = sClient;
        for (i = 0; i < sNumClients; i++) {
            if ((pClient->address == spBusMsg->senderAddr) &&
                (pClient->state == eEventWaitForConfirmation)) {
                TBusDevActualValueDo31 *p;
                uint8_t j;
                uint8_t buf[BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE];

                DigOutStateAllStandard(buf, sizeof(buf));

                p = &spBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.do31;
                for (j = 0;
                     (j < BUS_DO31_SHADER_SIZE_ACTUAL_VALUE) &&
                     (p->shader[j] == GetActualValueShader(j));
                     j++);
                if ((j == BUS_DO31_SHADER_SIZE_ACTUAL_VALUE) &&
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

    for (mask = 1, i = 0; i < 8; i++, mask <<= 1) {
        if ((sSw8State[address] ^ state) & mask) {
            SwitchEvent(address, i + 1, state & mask);
        }
    }
    sSw8State[address] = state;
}

/*-----------------------------------------------------------------------------
*  Power-Fail Interrupt (Ext. Int 0)
*/
ISR(INT0_vect) {
   uint8_t *ptrToEeprom;
   uint8_t buf[4];
   uint8_t i;

   LedSet(eLedGreenOff);
   LedSet(eLedRedOff);

   /* Ausgangszustände lesen */
   DigOutStateAllStandard(buf, sizeof(buf));
   /* zum Stromsparen alle Ausgänge abschalten */
   DigOutOffAll();

   /* neue Zustände speichern */
   ptrToEeprom = (uint8_t *)spNextPtrToEeprom;
   /* Kennzeichnungsbit löschen */
   buf[3] &= ~0x80;
   for (i = 0; i < sizeof(buf); i++) {
      eeprom_write_byte(ptrToEeprom, buf[i]);
      ptrToEeprom++;
   }

   /* Wait for completion of previous write */
   while (!eeprom_is_ready());

   LedSet(eLedRedOn);

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
*  Timer-Interrupt für Zeitbasis (Timer 0 Compare)
*/
ISR(TIMER0_COMP_vect)  {
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
   TCCR0 = (0b010 << CS00) | (0 << WGM00) | (1 << WGM01);
   OCR0 = 250 - 1;
#elif (F_CPU == 1600000UL)
   /* 16 MHz */
   TCCR0 = (0b110 << CS00) | (0 << WGM00) | (1 << WGM01);
   OCR0 = 125 - 1;
#elif (F_CPU == 3686400UL)
   /* 3.6864 MHz */
   TCCR0 = (0b100 << CS00) | (0 << WGM00) | (1 << WGM01);
   OCR0 = 115 - 1;
#else
#error adjust timer settings for your CPU clock frequency
#endif
   /* Timer Compare Match Interrupt enable */
   TIMSK |= 1 << OCIE0;
}

/*-----------------------------------------------------------------------------
*  Einstellung der Portpins
*/
static void PortInit(void) {

   /* PortA: DO0 .. DO7 */
   PORTA = 0b00000000;   /* alle PortA-Leitung auf low */
   DDRA  = 0b11111111;   /* alle PortA-Leitungen auf Ausgang */

   /* PortC: DO8 .. DO15 */
   PORTC = 0b00000000;   /* alle PortC-Leitung auf low */
   DDRC  = 0b11111111;   /* alle PortC-Leitungen auf Ausgang */

   /* PortB.4 .. PortB7: DO16 .. DO19: Ausgang low */
   /* PortB.0, PortB.2, PortB.3: nicht benutzt, Ausgang, low*/
   /* PortB.1: SCK-Eingang: Eingang, PullUp */
   PORTB = 0b00000010;
   DDRB  = 0b11111101;

   /* PortD.0: Interrupteingang für PowerFail: Eingang, kein PullUp*/
   /* PortD.1: unbenutzt oder mit RXD1 verbunden, Eingang, PullUp */
   /* PortD.2: UART RX, Eingang, PullUp */
   /* PortD.3: UART TX, Ausgang, high */
   /* PortD.4 .. PortD7: DO20 .. DO23 */
   PORTD = 0b00001110;
   DDRD  = 0b11111000;

   /* PortE.0: RX/MOSI: Eingang, PullUp*/
   /* PortE.1: TX/MISO, Ausgang high */
   /* PortE.2 .. PortE.6: unbenutzt, Ausgang, low */
   /* PortE.7: unbenutzt oder Transceiversteuerung, Ausgang, high*/
   PORTE = 0b10000011;
   DDRE  = 0b11111110;

   /* PortF: DO24 .. DO30/31 */
   PORTF = 0b00000000;    /* alle PortF-Leitung auf low */
   DDRF  = 0b11111111;    /* alle PortF-Leitungen auf Ausgang */

   /* PortG.0 .. PortG.2 unbenutzt, Ausgang, low*/
   /* PortG.3, PortG.4 LED, Ausgang, high*/
   PORTG = 0b00011000;
   DDRG  = 0b00011111;
}
