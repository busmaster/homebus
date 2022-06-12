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

#include "twislave.h"
#define SLAVE_ADRESSE 0x3F 								// TWI - Slave-Adresse

/*-----------------------------------------------------------------------------
*  Macros  
*/         
/* offset addresses in EEPROM */
#define MODUL_ADDRESS       0
#define OSCCAL_CORR         1
#define CLIENT_ADDRESS_BASE   7

#define COLOR_LED_BASE 40	/* offset für LED Farben beim startup */

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

/* Button pressed telegram is repeated for maximum time if button stays closed */
#define BUTTON_TELEGRAM_REPEAT_TIMEOUT 8  /* time in seconds */

/* our bus address */
#define MY_ADDR    sMyAddr

#define IDLE_SIO  0x01

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
char version[] = "bcu_bwm 0.01";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t  gTimeMs;
volatile uint16_t gTimeMs16;
volatile uint16_t gTimeS;

static uint16_t sBwmStartS;
static uint8_t  sBwmNow;

static TClient   sClient[BUS_MAX_CLIENT_NUM];

static uint8_t   sMyAddr;

static uint8_t   sInputState;
static uint8_t   sSwitchStateOld;
static uint8_t   sSwitchStateActual;

static uint8_t   sIdle = 0;


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
static uint8_t GetUnconfirmedClient(uint8_t actualClient);
static void ProcessSwitch(uint8_t switchState);
static uint8_t CalcBWMState(void);
static void BwmInitVar(void);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {                      

    int     sioHandle;

    MCUSR = 0;
    wdt_disable();

    /* get module address from EEPROM */
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
   
    PortInit();
    TimerInit();
    SioInit();
	BwmInitVar();
    SioRandSeed(MY_ADDR);
    sioHandle = SioOpen("USART0", eSioBaud9600, eSioDataBits8, eSioParityNo, 
                        eSioStopBits1, eSioModeHalfDuplex);
    SioSetIdleFunc(sioHandle, IdleSio);
    SioSetTransceiverPowerDownFunc(sioHandle, BusTransceiverPowerDown);

    BusTransceiverPowerDown(true);
   
    BusInit(sioHandle);
	
    spRxBusMsg = BusMsgBufGet();

    /* enable global interrupts */
    ENABLE_INT;  
    i2c_slave(SLAVE_ADRESSE);

    init_BJ(SLAVE_ADRESSE);

    i2c_slave(SLAVE_ADRESSE);

    SendStartupMsg();

    sInputState = 0;
    sSwitchStateOld = ~sInputState;
    ProcessSwitch(sInputState);
   
    while (1) { 
        Idle();
        sInputState = CalcBWMState(); 
        ProcessSwitch(sInputState);
        ProcessBus();
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
   TBusMsgType   msgType;
   uint8_t     i;
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
         case eBusDevReqSetValue:
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
            sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.sw8.state = sInputState;
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
         case eBusDevReqSetValue:
            sTxBusMsg.senderAddr = MY_ADDR;
            sTxBusMsg.type = eBusDevRespSetValue;
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
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

   /* TODO: define correct port init */

   /* configure pins to input/pull up */
   PORTB = 0b11111111;
   DDRB =  0b00000000;

   PORTC = 0b11111111;
   DDRC =  0b00000000;

   /* set PD2 and PD5 to output mode */
   PORTD = 0b11111111;
   DDRD =  0b00100010;
}

/*-----------------------------------------------------------------------------
*  port init
*/
static void TimerInit(void) {
   /* configure Timer 0 */
   /* prescaler clk/256 -> Interrupt period 256/1000000 * 256 = 8.192 ms */
   TCCR0B = 4 << CS00; 
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
*  period:  8.192 ms
*/
ISR(TIMER0_OVF_vect) {

  static uint8_t intCnt = 0;
  /* ms counter */
  gTimeMs16 += 8;  
  gTimeMs += 8;
  intCnt++;
  if (intCnt >= 122) { /* 8.192 ms * 122 = 1 s*/
     intCnt = 0;
     gTimeS++;
  }
}

static uint8_t CalcBWMState(void) {
	
   static uint8_t       sBwmState = 0;
   static uint8_t       sAutomatik = 0;

   if( (switch_register&0x80) == 0x80) {
	   switch_register = switch_register&0x7F; // Bit neu löschen
	   switch (switch_register) {
		   case 0x01: sBwmState = 0; sAutomatik = 0; break;
		   case 0x02: sBwmState = 1; sAutomatik = 0; break;
		   case 0x00: sAutomatik = 1; sBwmStartS = gTimeS; break;
		   default: break;
	   }
   }

   if ( sAutomatik == 1) {
	   if (sBwmState == 1) {
		   if ((gTimeS-sBwmStartS) > time_poti) { // Zeit abgelaufen
			   if ((bwm_on_register&0x01) == 0) {
				   sBwmState = 0;				   
			   }
		   }
		   else if ((bwm_on_register&0x01) == 1) { // nachtriggern
			   sBwmStartS = gTimeS;
		   }
       }
	   else {
           if( lux_register <= lux_poti ) {
			   if( (bwm_on_register&0x01) == 1) { // Einschalten
				   sBwmState = 1;
				   sBwmStartS = gTimeS;
			   }
		   }
	   }
   }
   
  return sBwmState;
}

static void BwmInitVar(void) {

    bwm_on_register = 0;
    switch_register = 0;
    time_poti = 0x0A;
    lux_poti = 0xFF;
    lux_register = 0;	
    switch_register = 0;
    sBwmStartS = 0;
    sBwmNow = 0;	
}