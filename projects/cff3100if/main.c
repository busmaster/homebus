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
#define MODUL_ADDRESS      0
#define OSCCAL_CORR        1

/* keycode offsets in EEPROM */
#define KEYCODE_UNLOCK_LEN 16
#define KEYCODE_UNLOCK     17
#define KEYCODE_GARAGE_LEN 32
#define KEYCODE_GARAGE     33
#define KEYCODE_ETO_LEN    48
#define KEYCODE_ETO        49
#define KEYCODE_BUTTON_LEN 64
#define KEYCODE_BUTTON     65

/* send button eBusButtonPressedX telegram on BUTTON key code */
#define BUTTON_INPUT       80

/* max 15 bytes code */
#define MAX_KEYCODE_LEN    15
#define KEY_FIFO_SIZE      (MAX_KEYCODE_LEN + 1)

/* our bus address */
#define MY_ADDR            sMyAddr

#define IDLE_SIO           0x01

/* lock/unlock button (pull to GND) on portc.0 and portc.1 */
#define LOCK_PRESS         PORTC &= ~(1 << 0)
#define LOCK_RELEASE       PORTC |= (1 << 0)
#define UNLOCK_PRESS       PORTC &= ~(1 << 1)
#define UNLOCK_RELEASE     PORTC |= (1 << 1)
#define BUTTONS_RELEASED   ((PINC & 0x03) == 0x03)

#define ETO_PRESS          PORTD |= (1 << 7)
#define ETO_RELEASE        PORTD &= ~(1 << 7)

#define LED_RED            (PINC & (1 << 2)) /* unlocked */
#define LED_GREEN          (PINC & (1 << 3)) /* locked */
#define LED_STATE          (PINC & 0x0c)
#define LEDS_ON            ((PINC & 0x0c) == 0x0c)

/* clock calibraation */
#define TIMER1_PRESCALER   (0b001 << CS10) // prescaler clk/1
#define MAX_CAL_TEL        31

/* Port D bit 3 controls bus transceiver power down (inverted) */
#define BUS_TRANSCEIVER_POWER_DOWN  PORTD &= ~(1 << 3)
#define BUS_TRANSCEIVER_POWER_UP    PORTD |= (1 << 3)

/*-----------------------------------------------------------------------------
*  Typedes
*/
typedef enum {
    eRcIdle,
    eRcPressedLock,
    eRcPressedUnlock,
    eRcPressedLockUnlock,
    eRcPressedEto,
    eRcRetryGarageTrigger,
    eRcRetryButton,
} TRcState;

typedef enum {
    eRcActionIdle,
    eRcActionPressUnlock,
    eRcActionPressLock,
    eRcActionPressLockUnlock,
    eRcActionPressEto,
    eRcActionGarageTrigger,
    eRcActionButton,
} TRcAction;

typedef enum {
    eLockStateIdle,
    eLockStateWaitReleaseLockUnlock,
    eLockStateWaitLedsOn,
    eLockStateWaitLedsOff,
    eLockStateWaitRead,
    eLockStateRead,
    eLockStateTerminate,
    eLockStateWaitAlternatingBlink
} TLockState;

/*-----------------------------------------------------------------------------
*  Variables
*/
char version[] = "keyrc 1.02";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t  gTimeMs;
volatile uint16_t gTimeMs16;
volatile uint16_t gTimeS;

static uint8_t   sMyAddr;

static uint8_t   sIdle = 0;

static int       sSioHandle;

static uint8_t   sKeybCur = 0;

static TRcAction sRcAction = eRcActionIdle;

static uint8_t   sKeyFifo[KEY_FIFO_SIZE];
static uint8_t   sWriteIdx = 0;
static uint8_t   sFirstIdx = 0;

static uint8_t   sKeyCodeLenUnlock;
static uint8_t   sKeyCodeUnlock[MAX_KEYCODE_LEN];

static uint8_t   sKeyCodeLenGarage;
static uint8_t   sKeyCodeGarage[MAX_KEYCODE_LEN];

static uint8_t   sKeyCodeLenEto;
static uint8_t   sKeyCodeEto[MAX_KEYCODE_LEN];

static uint8_t   sKeyCodeLenButton;
static uint8_t   sKeyCodeButton[MAX_KEYCODE_LEN];
static uint8_t   sButtonInput;

static bool      sGetLockState = false;
static uint8_t   sGetLockStateRequestAddr;

static TRcState   sRcState = eRcIdle;
static TLockState sLockState = eLockStateIdle;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void TimerInit(void);
static void Idle(void);
static void ProcessBus(uint8_t ret);
static void ProcessKey(void);
static void ProcessRcAction(void);
static void SendStartupMsg(void);
static void IdleSio(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);
static void InitTimer1(void);
static void InitComm(void);
static void ExitComm(void);
static void CheckLockState(void);

//static void LogLedState(void);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {

   uint8_t ret;

   MCUSR = 0;
   wdt_disable();

   /* get module address from EEPROM */
   sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);

   sKeyCodeLenUnlock = eeprom_read_byte((const uint8_t *)KEYCODE_UNLOCK_LEN);
   eeprom_read_block(&sKeyCodeUnlock, (const void *)(KEYCODE_UNLOCK), MAX_KEYCODE_LEN);

   sKeyCodeLenGarage = eeprom_read_byte((const uint8_t *)KEYCODE_GARAGE_LEN);
   eeprom_read_block(&sKeyCodeGarage, (const void *)(KEYCODE_GARAGE), MAX_KEYCODE_LEN);

   sKeyCodeLenEto = eeprom_read_byte((const uint8_t *)KEYCODE_ETO_LEN);
   eeprom_read_block(&sKeyCodeEto, (const void *)(KEYCODE_ETO), MAX_KEYCODE_LEN);

   sKeyCodeLenButton = eeprom_read_byte((const uint8_t *)KEYCODE_BUTTON_LEN);
   eeprom_read_block(&sKeyCodeButton, (const void *)(KEYCODE_BUTTON), MAX_KEYCODE_LEN);
   sButtonInput = eeprom_read_byte((const uint8_t *)BUTTON_INPUT);

   PortInit();
   TimerInit();

   InitComm();

   /* enable global interrupts */
   ENABLE_INT;

   SendStartupMsg();

   while (1) {
      Idle();
      ret = BusCheck();
      ProcessBus(ret);
      ProcessKey();
      ProcessRcAction();
      CheckLockState();
//      LogLedState();
   }
   return 0;  /* never reached */
}

static void RespActval(TBusLockState state) {

    sTxBusMsg.type = eBusDevRespActualValue;
    sTxBusMsg.senderAddr = MY_ADDR;
    sTxBusMsg.msg.devBus.x.devResp.actualValue.devType = eBusDevTypeKeyRc;
    sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.keyrc.state = state;
    sTxBusMsg.msg.devBus.receiverAddr = sGetLockStateRequestAddr;
    BusSend(&sTxBusMsg);
}

static void CheckLockState(void) {

    static uint16_t sStartTime = 0;
    static uint8_t sOldLedState = 0;
    static bool sBlink = false;
    static TBusLockState resp;
    uint16_t curTime;
    uint8_t ledState;

    GET_TIME_MS16(curTime);

    switch (sLockState) {
    case eLockStateIdle:
        if (sGetLockState) {
            sRcAction = eRcActionPressLockUnlock;
            sLockState = eLockStateWaitReleaseLockUnlock;
            sStartTime = curTime;
            sGetLockState = false;
        }
        break;
    case eLockStateWaitReleaseLockUnlock:
        if (BUTTONS_RELEASED) {
            sLockState = eLockStateWaitLedsOn;
            sStartTime = curTime;
        } else if ((uint16_t)(curTime - sStartTime) > 1000) {
            sLockState = eLockStateIdle;
            RespActval(eBusLockInternal); // internal error: buttons not released
        }
        break;
    case eLockStateWaitLedsOn:
        if (LEDS_ON) {
            sLockState = eLockStateWaitLedsOff;
            sStartTime = curTime;
        } else if ((uint16_t)(curTime - sStartTime) > 200) {
            sLockState = eLockStateIdle;
            RespActval(eBusLockInvalid1); // leds not on after buttons released
        }
        break;
    case eLockStateWaitLedsOff:
        if (!LEDS_ON) {
            sLockState = eLockStateWaitRead;
            sStartTime = curTime;
        } else if ((uint16_t)(curTime - sStartTime) > 3000) {
            sLockState = eLockStateIdle;
            RespActval(eBusLockInvalid2); // leds not off after on
        }
        break;
    case eLockStateWaitRead:
        if ((uint16_t)(curTime - sStartTime) > 1000) {
            sLockState = eLockStateRead;
        }
        break;
    case eLockStateRead:
        sStartTime = curTime;
        if (LED_RED && LED_GREEN) {
            resp = eBusLockUncalib;
            sLockState = eLockStateTerminate;
        } else if (LED_RED && !LED_GREEN) {
            resp = eBusLockUnlocked;
            sLockState = eLockStateTerminate;
        } else if (!LED_RED && LED_GREEN) {
            resp = eBusLockLocked;
            sLockState = eLockStateTerminate;
        } else if (!LED_RED && !LED_GREEN) {
            // no connection or no state signal
            sLockState = eLockStateWaitAlternatingBlink;
        }
        break;
    case eLockStateTerminate:
        if (!LEDS_ON || (uint16_t)(curTime - sStartTime) > 2500) {
            RespActval(resp);
            sLockState = eLockStateIdle;
        }
        break;
    case eLockStateWaitAlternatingBlink:
        // 3.5 secs after the leds go off it will bink 4.5 secs
        ledState = LED_STATE;
        if (sOldLedState != ledState) {
            sBlink = true;
        }
        sOldLedState = ledState;
        if ((uint16_t)(curTime - sStartTime) > 10000) {
            if (sBlink) {
                RespActval(eBusLockNoConnection);
            } else {
                RespActval(eBusLockNoResp);
            }
            sLockState = eLockStateIdle;
            sBlink = false;
            sOldLedState = 0;
        }
        break;
    default:
        break;
    }
}

#if 0
static void LogLedState(void) {

    uint8_t curr;
    static uint8_t ledstate = 0;

    curr = LED_STATE;
    if (curr != ledstate) {
        ledstate = curr;
        sTxBusMsg.type = eBusDevReqActualValueEvent;
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.msg.devBus.x.devReq.actualValueEvent.devType = eBusDevTypeSw8;
        sTxBusMsg.msg.devBus.x.devReq.actualValueEvent.actualValue.sw8.state = ledstate;
        sTxBusMsg.msg.devBus.receiverAddr = 150;
        BusSend(&sTxBusMsg);
    }
}
#endif

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
 * wait for first low pulse that is long than MIN_PULSE_CNT and return the
 * start timestamp of this low pulse
 */

#define MIN_PULSE_CNT 300

#define NOP_10 asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop")

#define NOP_4  asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop")


uint16_t Synchronize(void) {

    uint16_t cnt;
    uint16_t startCnt;

    do {
        while ((PIND & 0b00000001) != 0);
        PORTB = 1; /* tigger timer capture */
        NOP_4;
        PORTB = 0;
        startCnt = ICR1;
        while ((PIND & 0b00000001) == 0);
        PORTB = 1; /* tigger timer capture */
        NOP_4;
        PORTB = 0;
        cnt = ICR1;
    } while ((cnt - startCnt) < MIN_PULSE_CNT);

    return startCnt;
}

uint16_t ClkMeasure(void) {

    uint16_t stopCnt;
    uint8_t  i;

    for (i = 0; i < 6; i++) {
        while ((PIND & 0b00000001) != 0);
        NOP_4;
        while ((PIND & 0b00000001) == 0);
        NOP_4;
    }
    while ((PIND & 0b00000001) != 0);
    NOP_4;
    while ((PIND & 0b00000001) == 0);
    PORTB = 1;  /* tigger timer capture */
    NOP_4;
    PORTB = 0;
    stopCnt = ICR1;

    return stopCnt;
}

static bool GarageToggle(void) {

    bool rc = true;

    sTxBusMsg.type = eBusDevReqSetValue;
    sTxBusMsg.senderAddr = MY_ADDR;
    sTxBusMsg.msg.devBus.x.devReq.setValue.devType = eBusDevTypeSw8;
    sTxBusMsg.msg.devBus.receiverAddr = 36;
    /* bit 2 and 3 contain the setvalue for our output pin 01: trigger pulse */
    sTxBusMsg.msg.devBus.x.devReq.setValue.setValue.sw8.digOut[0] = 0x04;
    sTxBusMsg.msg.devBus.x.devReq.setValue.setValue.sw8.digOut[1] = 0;
    if (BusSend(&sTxBusMsg) != BUS_SEND_OK) {
        rc = false;
    }

    return rc;
}

static bool Button(void) {

    bool rc = true;

    sTxBusMsg.type = sButtonInput;
    sTxBusMsg.senderAddr = MY_ADDR;
    if (BusSend(&sTxBusMsg) != BUS_SEND_OK) {
        rc = false;
    }

    return rc;
}

static void ProcessRcAction(void) {

    static uint16_t sStartTime = 0;
    uint16_t curTime;

    GET_TIME_MS16(curTime);

    switch (sRcState) {
    case eRcIdle:
        if (sRcAction == eRcActionIdle) {
            break;
        }
        if (sRcAction == eRcActionPressLock) {
            LOCK_PRESS;
            sStartTime = curTime;
            sRcState = eRcPressedLock;
        } else if (sRcAction == eRcActionPressUnlock) {
            UNLOCK_PRESS;
            sStartTime = curTime;
            sRcState = eRcPressedUnlock;
        } else if (sRcAction == eRcActionPressLockUnlock) {
            LOCK_PRESS;
            UNLOCK_PRESS;
            sStartTime = curTime;
            sRcState = eRcPressedLockUnlock;
        } else if (sRcAction == eRcActionPressEto) {
            ETO_PRESS;
            sStartTime = curTime;
            sRcState = eRcPressedEto;
        } else if (sRcAction == eRcActionGarageTrigger) {
            if (!GarageToggle()) {
                sRcState = eRcRetryGarageTrigger;
            }
        } else if (sRcAction == eRcActionButton) {
            if (!Button()) {
                sRcState = eRcRetryButton;
            }
        }
        sRcAction = eRcActionIdle;
        break;
    case eRcPressedLock:
        if ((uint16_t)(curTime - sStartTime) > 100) {
            LOCK_RELEASE;
            sRcState = eRcIdle;
        }
        break;
    case eRcPressedUnlock:
        if ((uint16_t)(curTime - sStartTime) > 100) {
            UNLOCK_RELEASE;
            sRcState = eRcIdle;
        }
        break;
    case eRcPressedLockUnlock:
        if ((uint16_t)(curTime - sStartTime) > 100) {
            LOCK_RELEASE;
            UNLOCK_RELEASE;
            sRcState = eRcIdle;
        }
        break;
    case eRcPressedEto:
        if ((uint16_t)(curTime - sStartTime) > 3000) {
            ETO_RELEASE;
            sRcState = eRcIdle;
        }
        break;
    case eRcRetryGarageTrigger:
        if (GarageToggle()) {
            sRcState = eRcIdle;
        }
        break;
    case eRcRetryButton:
        if (Button()) {
            sRcState = eRcIdle;
        }
        break;
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------
*  fifo for keys pressed (overwriting fifo, reversed readout)
*/
static void PutKey(uint8_t val) {

    sWriteIdx++;
    if (sWriteIdx == KEY_FIFO_SIZE) {
       sWriteIdx = 0;
    }
    sKeyFifo[sWriteIdx] = val;
    if (sWriteIdx == sFirstIdx) {
        sFirstIdx++;
        if (sFirstIdx == KEY_FIFO_SIZE) {
            sFirstIdx = 0;
        }
    }
}

static bool GetKeyReverse(uint8_t *val) {

    if (sWriteIdx == sFirstIdx) {
        return false;
    }
    *val = sKeyFifo[sWriteIdx];
    if (sWriteIdx == 0) {
        sWriteIdx = KEY_FIFO_SIZE - 1;
    } else {
        sWriteIdx--;
    }

    return true;
}

static uint8_t NumKeys(void) {

    if (sWriteIdx >= sFirstIdx) {
        return sWriteIdx - sFirstIdx;
    } else {
        return KEY_FIFO_SIZE - sFirstIdx + sWriteIdx;
    }
}

static void ClearKeys(void) {
    sWriteIdx = 0;
    sFirstIdx = 0;
}

static TRcAction CheckKey(const uint8_t *key, uint8_t len) {

    uint8_t i;

    /* Unlock */
    for (i = 0; (i < sKeyCodeLenUnlock) && (i < len); i++) {
        if (key[i] != sKeyCodeUnlock[sKeyCodeLenUnlock - 1 - i]) {
           break;
        }
    }
    if ((i > 0) && (i == sKeyCodeLenUnlock)) {
        return eRcActionPressUnlock;
    }

    /* gargage */
    for (i = 0; (i < sKeyCodeLenGarage) && (i < len); i++) {
        if (key[i] != sKeyCodeGarage[sKeyCodeLenGarage - 1 -i]) {
           break;
        }
    }
    if ((i > 0) && (i == sKeyCodeLenGarage)) {
        return eRcActionGarageTrigger;
    }

    /* eto */
    for (i = 0; (i < sKeyCodeLenEto) && (i < len); i++) {
        if (key[i] != sKeyCodeEto[sKeyCodeLenEto - 1 - i]) {
           break;
        }
    }
    if ((i > 0) && (i == sKeyCodeLenEto)) {
        return eRcActionPressEto;
    }

    /* eBusButtonPressedX */
    for (i = 0; (i < sKeyCodeLenButton) && (i < len); i++) {
        if (key[i] != sKeyCodeButton[sKeyCodeLenButton - 1 - i]) {
           break;
        }
    }
    if ((i > 0) && (i == sKeyCodeLenButton)) {
        return eRcActionButton;
    }

    return eRcActionIdle;
}

static bool RcIsBusy(void) {

    if (sRcState != eRcIdle) {
        return true;
    }
    if (sLockState != eLockStateIdle) {
        return true;
    }
    return false;
}


static void ProcessKey(void) {

    static uint16_t sPressTime = 0;
    uint16_t curTime;
    uint8_t key[MAX_KEYCODE_LEN];
    uint8_t i;
    uint8_t num = NumKeys();

    GET_TIME_MS16(curTime);

    if (sKeybCur == (uint8_t)'k') {
        if (!RcIsBusy()) {
            if (num == 0) {
                sRcAction = eRcActionPressLock;
            } else {
                for (i = 0; (i < sizeof(key)) && GetKeyReverse(&key[i]); i++);
                sRcAction = CheckKey(key, i);
            }
            sKeybCur = 0;
        }
    } else if (sKeybCur != 0) {
        PutKey(sKeybCur);
        sPressTime = curTime;
        sKeybCur = 0;
    } else if ((num > 0) && ((uint16_t)(curTime - sPressTime) > 5000)) {
        ClearKeys();
    }
}

/*-----------------------------------------------------------------------------
*  process received bus telegrams
*/
static void ProcessBus(uint8_t ret) {

    TBusMsgType         msgType;
    uint8_t             i;
    uint8_t             *p;
    bool                msgForMe = false;
    uint8_t             val8;
    uint8_t             flags;
    static bool         sTxRetry = false;
    static TBusTelegram sTxMsg;

    if (sTxRetry) {
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        return;
    }

    if (ret == BUS_MSG_OK) {
        msgType = spRxBusMsg->type;
        switch (msgType) {
        case eBusDevReqReboot:
        case eBusDevReqInfo:
        case eBusDevReqEepromRead:
        case eBusDevReqEepromWrite:
        case eBusDevReqActualValueEvent: /* keyb input */
        case eBusDevReqActualValue:      /* request lock state */
        case eBusDevReqSetValue:         /* set lock/unlock */
        case eBusDevReqDoClockCalib:
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
    case eBusDevReqInfo:
        sTxMsg.type = eBusDevRespInfo;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.info.devType = eBusDevTypeKeyRc;
        strncpy((char *)(sTxMsg.msg.devBus.x.devResp.info.version),
                version, BUS_DEV_INFO_VERSION_LEN);
        sTxMsg.msg.devBus.x.devResp.info.version[BUS_DEV_INFO_VERSION_LEN - 1] = '\0';
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqEepromRead:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespEepromRead;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.readEeprom.data =
            eeprom_read_byte((const uint8_t *)spRxBusMsg->msg.devBus.x.devReq.readEeprom.addr);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqEepromWrite:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespEepromWrite;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        p = &(spRxBusMsg->msg.devBus.x.devReq.writeEeprom.data);
        eeprom_write_byte((uint8_t *)spRxBusMsg->msg.devBus.x.devReq.readEeprom.addr, *p);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqActualValueEvent:
        /* evaluate actual value event from keyb devices */
        if (spRxBusMsg->msg.devBus.x.devReq.actualValueEvent.devType != eBusDevTypeKeyb) {
            break;
        }
        val8 = spRxBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.keyb.keyEvent;
        if (val8 & 0x80) {
             /* pressed */
             sKeybCur = val8 & ~0x80;
        } else {
             sKeybCur = 0;
        }
        /* the response includes the event data */
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespActualValueEvent;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.actualValueEvent.devType = eBusDevTypeKeyb;
        sTxMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.keyb.keyEvent = val8;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqActualValue:
        /* lock state requested:
         * the current lock state can't be read immediatly. From the CFF3100, it
         * has to be requested by pressing the buttons lock+unlock at the same
         * time. The lock state is reflected by the led state after a few secs.
         */
        if (!RcIsBusy()) {
            /* if no rc command in progress */
            /* no response here due to delayed state detection
             * see CheckLockState()
             */
            sGetLockState = true;
            sGetLockStateRequestAddr = spRxBusMsg->senderAddr;
        } else {
            sTxMsg.type = eBusDevRespActualValue;
            sTxMsg.senderAddr = MY_ADDR;
            sTxMsg.msg.devBus.x.devResp.actualValue.devType = eBusDevTypeKeyRc;
            sTxMsg.msg.devBus.x.devResp.actualValue.actualValue.keyrc.state = eBusLockAgain;
            sTxMsg.msg.devBus.receiverAddr = sGetLockStateRequestAddr;
            sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        }
        break;
    case eBusDevReqSetValue:
        if (spRxBusMsg->msg.devBus.x.devReq.setValue.devType != eBusDevTypeKeyRc) {
            break;
        }
        if (RcIsBusy()) {
            /* it is not possible to return an error state in eBusDevRespSetValue
             * telegram, so no response if rc command is in progress
             */
            break;
        }
        switch (spRxBusMsg->msg.devBus.x.devReq.setValue.setValue.keyrc.command) {
        case eBusLockCmdLock:
            sRcAction = eRcActionPressLock;
            break;
        case eBusLockCmdUnlock:
            sRcAction = eRcActionPressUnlock;
            break;
        case eBusLockCmdEto:
            sRcAction = eRcActionPressEto;
            break;
        case eBusLockCmdNoAction:
        default:
            break;
        }
        sTxMsg.type = eBusDevRespSetValue;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqDoClockCalib: {
        uint8_t         old_osccal;
        static uint8_t  sOsccal = 0;
        uint16_t        startCnt;
        uint16_t        stopCnt;
        uint16_t        clocks;
        uint16_t        diff;
        uint16_t        seqLen;
        static int      sCount = 0;
        static uint16_t sMinDiff = 0xffff;
        static uint8_t  sMinOsccal = 0;
        uint8_t         osccal_corr;

        if (spRxBusMsg->msg.devBus.x.devReq.doClockCalib.command == eBusDoClockCalibInit) {
            sCount = 0;
            sOsccal = 0;
            sMinDiff = 0xffff;
        } else if (sCount > MAX_CAL_TEL) {
            sTxMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateError;
            sTxMsg.senderAddr = MY_ADDR;
            sTxMsg.type = eBusDevRespDoClockCalib;
            sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
            break;
        }

        flags = DISABLE_INT;

        BUS_TRANSCEIVER_POWER_UP;
        PORTB = 0;

        /* 8 bytes 0x00: 8 * (1 start bit + 8 data bits) + 7 stop bits  */
        seqLen = 8 * 1000000 * 9 / 9600 + 7 * 1000000 * 1 / 9600; /* = 8229 us */
        old_osccal = OSCCAL;

        ExitComm();
        InitTimer1();

        TCCR1B = (1 << ICES1) | TIMER1_PRESCALER;

        OSCCAL = sOsccal;
        NOP_10;

        for (i = 0; i < 8; i++) {
            startCnt = Synchronize();
            stopCnt = ClkMeasure();

            clocks = stopCnt - startCnt;
            if (clocks > seqLen) {
                diff = clocks - seqLen;
            } else {
                diff = seqLen - clocks;
            }
            if (diff < sMinDiff) {
                sMinDiff = diff;
                sMinOsccal = OSCCAL;
            }
            OSCCAL++;
            NOP_4;
        }

        BUS_TRANSCEIVER_POWER_DOWN;

        InitTimer1();
        InitComm();
        sOsccal = OSCCAL;
        OSCCAL = old_osccal;

        RESTORE_INT(flags);

        if (sCount < MAX_CAL_TEL) {
            sTxMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateContiune;
            sCount++;
        } else {
            sTxMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateSuccess;
            /* save the osccal correction value to eeprom */
            osccal_corr = eeprom_read_byte((const uint8_t *)OSCCAL_CORR);
            osccal_corr += sMinOsccal - old_osccal;
            eeprom_write_byte((uint8_t *)OSCCAL_CORR, osccal_corr);
            OSCCAL = sMinOsccal;
            NOP_10;
        }
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespDoClockCalib;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    }
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------
*  port init
*/
static void PortInit(void) {

    /* configure pins */
    PORTB = 0b00000000;
    DDRB =  0b11111111;

    PORTC = 0b00010011; // PC4: input not used, enable pull up
    DDRC =  0b00100011;

    PORTD = 0b00000111;
    DDRD =  0b11111010;
}

/*-----------------------------------------------------------------------------
*  port init
*/
static void TimerInit(void) {

    /* prescaler clk/64
     * compare match 125
     * -> Interrupt period 125/1000000 * 64 = 8 ms */
    TCCR0A = 2 << WGM20; // CTC
    TCCR0B = 3 << CS00;
    OCR0A = 124; /* counting 0-124 */
    TIMSK0 = 1 << OCIE0A;
}

static void InitTimer1(void) {
    /* set input clock to  system clock */
    /* use software input capture       */
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1C = 0;
    TIMSK1 = 0;
    TCNT1 = 0;
    OCR1A = 0;
    OCR1B = 0;
    ICR1 = 0;
    TIFR1 = TIFR1;
}

/*-----------------------------------------------------------------------------
*  init bus communication
*/
static void InitComm(void) {

   SioInit();
   SioRandSeed(sMyAddr);
   sSioHandle = SioOpen("USART0", eSioBaud9600, eSioDataBits8, eSioParityNo,
                       eSioStopBits1, eSioModeHalfDuplex);
   SioSetIdleFunc(sSioHandle, IdleSio);
   SioSetTransceiverPowerDownFunc(sSioHandle, BusTransceiverPowerDown);

   BusTransceiverPowerDown(true);

   BusInit(sSioHandle);
   spRxBusMsg = BusMsgBufGet();
}

/*-----------------------------------------------------------------------------
*  bus communication exit
*/
static void ExitComm(void) {

    BusExit(sSioHandle);
    SioClose(sSioHandle);
    SioExit();
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
*  Timer 0 COMPA ISR
*  period:  8.000 ms
*/
ISR(TIMER0_COMPA_vect) {

    static uint8_t intCnt = 0;

    /* ms counter */
    gTimeMs16 += 8;
    gTimeMs += 8;
    intCnt++;
    if (intCnt >= 125) { /* 8 ms * 125 = 1 s*/
        intCnt = 0;
        gTimeS++;
    }
}
