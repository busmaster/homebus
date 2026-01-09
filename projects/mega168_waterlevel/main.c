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

#define CLIENT_ADDRESS_BASE   7


#define MOVING_AVERAGE          16    /* array size, needs MOVING_AVERAGE * 2 bytes of RAM */


#ifdef BUSVAR
/* non volatile bus variables memory */
#define BUSVAR_NV_START         0x100
#define BUSVAR_NV_END           0x1ff
#endif


/* Button pressed telegram is repeated for maximum time if button stays closed */
#define BUTTON_TELEGRAM_REPEAT_TIMEOUT_B1   (BUS_MAX_CLIENT_NUM + CLIENT_ADDRESS_BASE) /* time in 100 ms */
#define BUTTON_TELEGRAM_REPEAT_TIMEOUT_B2   (BUTTON_TELEGRAM_REPEAT_TIMEOUT_B1 + 1) /* time in 100 ms */

#define STARTUP_DELAY  10 /* delay in seconds */

/* inhibit time after sending a client request */
/* if switch state is changed within this time */
/* the client request has to be delayed, because */
/* bus collision is likely (because of client's response) */
#define RESPONSE_TIMEOUT1_MS  20  /* time in ms */

/* timeout for client's response */
#define RESPONSE_TIMEOUT2_MS  100 /* time in ms */

/* timeout for unreachable client  */
/* after this time retries are stopped */
#define RETRY_TIMEOUT2_MS     10000 /* time in ms */


/* Button pressed telegram repetition time (time granularity is 16 ms) */
#define BUTTON_TELEGRAM_REPEAT_DIFF    48  /* time in ms */


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
   eSkip,
   eWaitForConfirmation1,
   eWaitForConfirmation2,
   eConfirmationOK
} TState;

typedef struct {
   uint8_t  address;
   TState state;
   uint16_t requestTimeStamp;
} TClient;


/*-----------------------------------------------------------------------------
*  Variables
*/
char version[] = "Sw168pa 1.06hAT";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t  gTimeMs;
volatile uint16_t gTimeMs16;
volatile uint32_t gTimeMs32;
volatile uint16_t gTimeS;

static TClient sClient[BUS_MAX_CLIENT_NUM];

static uint8_t   sMyAddr;

static uint8_t   sButtonTRTimeout[3] = {80,80,80};

static uint8_t   sInputType;

static uint8_t   sSwitchStateOld;
static uint8_t   sSwitchStateActual;

static uint8_t   sIdle = 0;

static uint32_t  sOldTime=0;

static uint16_t  sMovAv[MOVING_AVERAGE];
static volatile  uint16_t  sAdcAv;
static uint32_t  sAdcAvSum;
static uint8_t   sAdcAvIdx = 0;

uint16_t BusVar0 = 0;
uint16_t BusVar2 = 0;
uint32_t BusVar3 = 0;
uint32_t BusVar1 = 0;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void    PortInit(void);
static void    TimerInit(void);
static void    Idle(void);
static void    ProcessSwitch(uint8_t switchState);
static void    ProcessButton(uint8_t buttonState);
static void    ProcessBus(void);
static uint8_t GetUnconfirmedClient(uint8_t actualClient);
static void    SendStartupMsg(void);
static void    IdleSio(bool setIdle);
static void    BusTransceiverPowerDown(bool powerDown);
static uint8_t GetInputState(void);
#ifdef BUSVAR
static bool BusVarNv(uint16_t address, void *buf, uint8_t bufSize, TBusVarDir dir);
static void ApplicationInit(void);
static void ApplicationCheck(void);
#endif

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

    sButtonTRTimeout[2] = eeprom_read_byte((const uint8_t *)BUTTON_TELEGRAM_REPEAT_TIMEOUT_B1);
	sButtonTRTimeout[1] = eeprom_read_byte((const uint8_t *)BUTTON_TELEGRAM_REPEAT_TIMEOUT_B2);
	if (sButtonTRTimeout[1] == 0xFF)
		sButtonTRTimeout[1] = sButtonTRTimeout[2];
	sButtonTRTimeout[0] = max(sButtonTRTimeout[2],sButtonTRTimeout[1]);

    sInputType = eeprom_read_byte((const uint8_t *)INPUT_TYPE);
    if (sInputType > INPUT_TYPE_MOTION_DETECTOR) {
        sInputType = INPUT_TYPE_DUAL_BUTTON;
    }
    PortInit();
    TimerInit();
#ifdef BUSVAR
    // ApplicationInit might use BusVar
    BusVarInit(sMyAddr, BusVarNv);
    ApplicationInit();
#endif
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
        inputState = GetInputState();
        sSwitchStateOld = ~inputState;
        ProcessSwitch(inputState);
    }

    while (1) {
        Idle();

        inputState = GetInputState();
        if ((sInputType == INPUT_TYPE_DUAL_SWITCH) ||
            (sInputType == INPUT_TYPE_MOTION_DETECTOR)) {
            ProcessSwitch(inputState);
        } else if (sInputType == INPUT_TYPE_DUAL_BUTTON) {
            ProcessButton(inputState);
        }
        ProcessBus();
#ifdef BUSVAR
      BusVarProcess();
      ApplicationCheck();
#endif
    }
    return 0;  /* never reached */
}

#ifdef BUSVAR
/*-----------------------------------------------------------------------------
*  NV memory for persist bus variables
*/
static bool BusVarNv(uint16_t address, void *buf, uint8_t bufSize, TBusVarDir dir) {
    return true;
}

static void ApplicationInit(void) {

    uint16_t adc;
    uint16_t i;

    /* Wasserstand ADC_value */
    BusVarAdd(0, sizeof(uint16_t), false);

    /* Impulse Liter */
    BusVarAdd(1, sizeof(uint32_t), false);

    /* Wasserstand cm Filter */
    BusVarAdd(2, sizeof(uint16_t), false);

    /* Liter */
    BusVarAdd(3, sizeof(uint32_t), false);


    sOldTime = 0;
   /* ADC */
   /* RESF1:0 = 01     select AVCC as AREF */
   /* ADLAR = 0        rigth adjust         */
   /* MUX3: = 0001     select ADC1 (PC1)   */
   ADMUX = 0b01000001;

   /* disable input disable for ADC1 */
   DIDR0 = 0b00000010;

   /* ADEN = 1         ADC enable */
   /* ADPS2:0 = 100    prescaler, division factor is 16 -> 1MHz/16 = 62500 Hz*/
   ADCSRA = 0b10000100;

   // trigger adc conversion and wait for end
   ADCSRA |= (1 << ADSC);
   while ((ADCSRA & (1 << ADSC)) != 0);

   adc = ADCW;
   sAdcAvSum = 0;
   for (i = 0; i < MOVING_AVERAGE; i++) {
      sMovAv[i] = adc;
      sAdcAvSum += adc;
   }
   sAdcAv = sAdcAvSum / MOVING_AVERAGE;
}

static void ApplicationCheck(void) {

    TBusVarResult result;
    uint32_t actualTime;
    uint16_t adc;

    GET_TIME_MS32(actualTime);
    if((actualTime - sOldTime) > 5000 ) {
       adc = ADCW;
       sAdcAvSum -= sMovAv[sAdcAvIdx];
       sAdcAvSum += adc;
       sMovAv[sAdcAvIdx] = adc;
       sAdcAvIdx++;
       sAdcAvIdx %= MOVING_AVERAGE;
       sAdcAv = sAdcAvSum / MOVING_AVERAGE;
       // trigger next conversion
       ADCSRA |= (1 << ADSC);
       BusVar0 = sAdcAv;
       BusVar2 =63*sAdcAv/100 - 125;
       BusVar3 = 256*BusVar1/360;
       BusVarWrite(0,&BusVar0,sizeof(BusVar0),&result);
       BusVarWrite(1,&BusVar1,sizeof(BusVar1),&result);
       BusVarWrite(2,&BusVar2,sizeof(BusVar2),&result);
       BusVarWrite(3,&BusVar3,sizeof(BusVar3),&result);
       sOldTime = actualTime;
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
*  send switch state change to clients
*  - client address list is read from eeprom
*  - if there is no confirmation from client within RESPONSE_TIMEOUT2
*    the next client in list is processed
*  - telegrams to clients without response are repeated again and again
*    til telegrams to all clients in list are confirmed
*  - if switchStateChanged occurs while client confirmations are missing
*    actual client process is canceled and the new state telegram is sent
*    to clients
*/
static void ProcessSwitch(uint8_t switchState) {

    static uint8_t  sActualClient = 0xff; /* actual client's index being processed */
    static uint16_t sChangeTimeStamp;
    TClient         *pClient;
    uint8_t         i;
    uint16_t        actualTime16;
    uint8_t         actualTime8;
    bool            switchStateChanged;
    bool            startProcess;

    if ((switchState ^ sSwitchStateOld) != 0) {
        switchStateChanged = true;
    } else {
        switchStateChanged = false;
    }

    if (switchStateChanged) {
        if (sActualClient < BUS_MAX_CLIENT_NUM) {
            pClient = &sClient[sActualClient];
            if (pClient->state == eWaitForConfirmation1) {
                startProcess = false;
            } else {
                startProcess = true;
                sSwitchStateOld = switchState;
            }
        } else {
            startProcess = true;
            sSwitchStateOld = switchState;
        }
        if (startProcess == true) {
            sSwitchStateActual = switchState;
            GET_TIME_MS16(sChangeTimeStamp);
            /* set up client descriptor array */
            sActualClient = 0;
            pClient = &sClient[0];
            for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
                pClient->address = eeprom_read_byte((const uint8_t *)(CLIENT_ADDRESS_BASE + i));
                if (pClient->address != BUS_CLIENT_ADDRESS_INVALID) {
                    pClient->state = eInit;
                } else {
                    pClient->state = eSkip;
                }
                pClient++;
            }
        }
    }

    if (sActualClient < BUS_MAX_CLIENT_NUM) {
        GET_TIME_MS16(actualTime16);
        if (((uint16_t)(actualTime16 - sChangeTimeStamp)) < RETRY_TIMEOUT2_MS) {
            pClient = &sClient[sActualClient];
            if (pClient->state == eConfirmationOK) {
                /* get next client */
                sActualClient = GetUnconfirmedClient(sActualClient);
                if (sActualClient < BUS_MAX_CLIENT_NUM) {
                    pClient = &sClient[sActualClient];
                } else {
                    /* we are ready */
                }
            }
            switch (pClient->state) {
            case eInit:
                sTxBusMsg.type = eBusDevReqSwitchState;
                sTxBusMsg.senderAddr = MY_ADDR;
                sTxBusMsg.msg.devBus.receiverAddr = pClient->address;
                sTxBusMsg.msg.devBus.x.devReq.switchState.switchState = sSwitchStateActual;
                BusSend(&sTxBusMsg);
                pClient->state = eWaitForConfirmation1;
                GET_TIME_MS16(pClient->requestTimeStamp);
                break;
            case eWaitForConfirmation1:
                actualTime8 = GET_TIME_MS;
                if (((uint8_t)(actualTime8 - pClient->requestTimeStamp)) >= RESPONSE_TIMEOUT1_MS) {
                    pClient->state = eWaitForConfirmation2;
                    GET_TIME_MS16(pClient->requestTimeStamp);
                }
                break;
            case eWaitForConfirmation2:
                GET_TIME_MS16(actualTime16);
                if (((uint16_t)(actualTime16 - pClient->requestTimeStamp)) >= RESPONSE_TIMEOUT2_MS) {
                    /* set client for next try after the other clients */
                    pClient->state = eInit;
                    /* try next client */
                    sActualClient = GetUnconfirmedClient(sActualClient);
                }
                break;
            default:
                break;
            }
        } else {
            sActualClient = 0xff;
        }
    }
}

/*-----------------------------------------------------------------------------
*  send button pressed telegram
*  telegram is repeated every 48 ms for max 8 secs
*/
static void ProcessButton(uint8_t buttonState) {

    static uint8_t  sSequenceState = BUTTON_TX_OFF;
    static uint16_t sStartTimeSm;
    static uint8_t  sTimeStampMs;
    bool            pressed;
    uint16_t        actualTimeSm;
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
            GET_TIME_MS16(sStartTimeSm);
            sTimeStampMs = GET_TIME_MS;
            sTxBusMsg.senderAddr = MY_ADDR;
            BusSend(&sTxBusMsg);
            sSequenceState = BUTTON_TX_ON;
        }
        break;
    case BUTTON_TX_ON:
        if (pressed) {
            GET_TIME_MS16(actualTimeSm);
            if ((uint16_t)(actualTimeSm - sStartTimeSm) <  (sButtonTRTimeout[min(2,buttonState)]*63)) {
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
*  get next client array index
*  if all clients are processed 0xff is returned
*/
static uint8_t GetUnconfirmedClient(uint8_t actualClient) {

    uint8_t    i;
    uint8_t    nextClient;
    TClient  *pClient;

    if (actualClient >= BUS_MAX_CLIENT_NUM) {
        return 0xff;
    }

    for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
        nextClient = actualClient + i +1;
        nextClient %= BUS_MAX_CLIENT_NUM;
        pClient = &sClient[nextClient];
        if ((pClient->state != eSkip) &&
            (pClient->state != eConfirmationOK)) {
            break;
        }
    }
    if (i == BUS_MAX_CLIENT_NUM) {
        /* all client's confirmations received */
        nextClient = 0xff;
    }
    return nextClient;
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
    uint8_t       val8;
    bool        msgForMe = false;

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
#ifdef BUSVAR
        case eBusDevReqGetVar:
        case eBusDevReqSetVar:
        case eBusDevRespGetVar:
        case eBusDevRespSetVar:
#endif
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
        case eBusDevRespSwitchState:
            pClient = &sClient[0];
            for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
                if (pClient->state == eSkip) {
                    pClient++;
                    continue;
                } else if ((pClient->address == spRxBusMsg->senderAddr) &&
                           (spRxBusMsg->msg.devBus.x.devResp.switchState.switchState == sSwitchStateActual) &&
                           ((pClient->state == eWaitForConfirmation1) ||
                            (pClient->state == eWaitForConfirmation2))) {
                    pClient->state = eConfirmationOK;
                    break;
                }
                pClient++;
            }
            break;
        case eBusDevReqActualValue:
            sTxBusMsg.senderAddr = MY_ADDR;
            sTxBusMsg.type = eBusDevRespActualValue;
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxBusMsg.msg.devBus.x.devResp.actualValue.devType = eBusDevTypeSw8;
            sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.sw8.state = sSwitchStateActual;
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
#ifdef BUSVAR
    case eBusDevReqGetVar:
        val8 = spRxBusMsg->msg.devBus.x.devReq.getVar.index;
        sTxBusMsg.msg.devBus.x.devResp.getVar.length =
            BusVarRead(val8, sTxBusMsg.msg.devBus.x.devResp.getVar.data,
                       sizeof(sTxBusMsg.msg.devBus.x.devResp.getVar.data),
                       &sTxBusMsg.msg.devBus.x.devResp.getVar.result);
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.type = eBusDevRespGetVar;
        sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxBusMsg.msg.devBus.x.devResp.getVar.index = val8;
        BusSend(&sTxBusMsg);
        break;
    case eBusDevReqSetVar:
        val8 = spRxBusMsg->msg.devBus.x.devReq.setVar.index;
        BusVarWrite(val8, spRxBusMsg->msg.devBus.x.devReq.setVar.data,
                    spRxBusMsg->msg.devBus.x.devReq.setVar.length,
                    &sTxBusMsg.msg.devBus.x.devResp.setVar.result);
        sTxBusMsg.senderAddr = MY_ADDR;
        sTxBusMsg.type = eBusDevRespSetVar;
        sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxBusMsg.msg.devBus.x.devResp.setVar.index = val8;
        BusSend(&sTxBusMsg);
        break;
    case eBusDevRespSetVar:
        BusVarRespSet(spRxBusMsg->senderAddr, &spRxBusMsg->msg.devBus.x.devResp.setVar);
        break;
    case eBusDevRespGetVar:
        BusVarRespGet(spRxBusMsg->senderAddr, &spRxBusMsg->msg.devBus.x.devResp.getVar);
        break;
#endif
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

    /* configure Timer 2 */
    /* prescaler clk/64 -> Interrupt period 256/1000000 * 64 = 16.384 ms */
    TCCR2B = 4 << CS20;
    TIMSK2 = 1 << TOIE2;

    TCCR0B = 7 << CS20;
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
*  Timer 2 overflow ISR
*  period:  16.384 ms
*/
ISR(TIMER2_OVF_vect) {

    static uint8_t intCnt = 0;
    /* ms counter */
    gTimeMs32 += 16;
    gTimeMs16 += 16;
    gTimeMs += 16;
    intCnt++;
    if (intCnt >= 61) { /* 16.384 ms * 61 = 1 s*/
        intCnt = 0;
        gTimeS++;
    }
}

/*-----------------------------------------------------------------------------
*  Timer 0 overflow ISR
*  period:  16.384 ms
*/
ISR(TIMER0_OVF_vect) {
    BusVar1++;
}
