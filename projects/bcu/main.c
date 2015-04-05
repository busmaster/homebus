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

#define COLOR_LED_BASE 40	/* offset für LED Farben beim startup */

#define BUTTON_EVENT_ADRESS_BASE      50  /* offset für die Sender-Adressen */
#define NUM_BUTTON_EVENT_ADDRESSES    4

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

#define BUS_SW16_LED_SIZE_SET_VALUE    4    /* 4 byte for 8 Leds */
#define NR_OF_LEDS                     8    /* 8 Leds */


/*-----------------------------------------------------------------------------
*  Variables
*/  
char version[] = "bcu 0.01";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t  gTimeMs;
volatile uint16_t gTimeMs16;
volatile uint16_t gTimeS;

static uint8_t   sMyAddr;

static uint8_t	 sMySwitchAddr[NUM_BUTTON_EVENT_ADDRESSES];

static uint8_t   sInputState;

static uint8_t   sIdle = 0;

static uint8_t   sNewLedData[BUS_SW16_LED_SIZE_SET_VALUE];

static uint8_t   sLedData[BUS_SW16_LED_SIZE_SET_VALUE];

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void TimerInit(void);
static void Idle(void);
static void ProcessButton(uint8_t buttonState);
static void ProcessBus(void);
static void SendStartupMsg(void);
static void IdleSio(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);


/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {                      

    int     sioHandle;
    uint8_t i;
    uint8_t u8;

    MCUSR = 0;
    wdt_disable();

    /* get module address from EEPROM */
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
    for (i = 0; i < NUM_BUTTON_EVENT_ADDRESSES; i++) {
        sMySwitchAddr[i] = eeprom_read_byte((const uint8_t *)(BUTTON_EVENT_ADRESS_BASE + i));
    }
   
    PortInit();
    TimerInit();
    SioInit();
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

    button_register = 0;

    init_BJ(SLAVE_ADRESSE);

    for (i = 0; i < NR_OF_LEDS; i++) {
        u8 = eeprom_read_byte((const uint8_t *)(COLOR_LED_BASE + i));
        set_LED(i, u8);
        sNewLedData[i / 2] |= (u8 & 0x0f) << ((i % 2) ? 4 : 0);
    }

    i2c_slave(SLAVE_ADRESSE);

    SendStartupMsg();
   
    while (1) { 
        Idle();
        if (send_startup == 1) {
            init_BJ(SLAVE_ADRESSE);
            for (i = 0; i < NR_OF_LEDS; i++) {
                set_LED(i, eeprom_read_byte((const uint8_t *)(COLOR_LED_BASE + i)));
            }
        }
        sInputState = button_register; 
        ProcessButton(sInputState);

        ProcessBus();

        for (i = 0; i < BUS_SW16_LED_SIZE_SET_VALUE; i++) {
            if ((sNewLedData[i] & 0x0f) != (sLedData[i] & 0x0f) ) { // linke LED
                set_LED(i * 2, sNewLedData[i] & 0x0f);
                i2c_slave(SLAVE_ADRESSE);
            }
            if ((sNewLedData[i] & 0xf0) != (sLedData[i] & 0xf0) ) { // rechte LED
                set_LED(i * 2 + 1, (sNewLedData[i] & 0xf0) >> 4);
                i2c_slave(SLAVE_ADRESSE);
            }
            sLedData[i] = sNewLedData[i];
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
*  send button pressed telegram
*  telegram is repeated every 48 ms for max 8 secs 
*/
static void ProcessButton(uint8_t buttonState) {

    static uint8_t  sSequenceState = BUTTON_TX_OFF;
    static uint16_t sStartTimeS;
    static uint8_t  sTimeStampMs;
    bool            pressed;
    uint16_t        actualTimeS;
    uint8_t         actualTimeMs;
    uint8_t         address;

    /* TODO: gleichzeitiges druecken von Tasten */

    switch (buttonState) {
    case 0b00000001:
        sTxBusMsg.type = eBusButtonPressed1;
        pressed = true;
        address = sMySwitchAddr[0];
        break;
    case 0b00000010:
        sTxBusMsg.type = eBusButtonPressed2;
        pressed = true;
        address = sMySwitchAddr[0];
        break;
    case 0b00000011:
        sTxBusMsg.type = eBusButtonPressed1_2;
        pressed = true;
        address = sMySwitchAddr[0];
        break;
    case 0b00000100:
        sTxBusMsg.type = eBusButtonPressed1;
        pressed = true;
        address = sMySwitchAddr[1];
        break;
    case 0b00001000:
        sTxBusMsg.type = eBusButtonPressed2;
        pressed = true;
        address = sMySwitchAddr[1];
        break;
    case 0b00001100:
        sTxBusMsg.type = eBusButtonPressed1_2;
        pressed = true;
        address = sMySwitchAddr[1];
        break;
    case 0b00010000:
        sTxBusMsg.type = eBusButtonPressed1;
        pressed = true;
        address = sMySwitchAddr[2];
        break;
    case 0b00100000:
        sTxBusMsg.type = eBusButtonPressed2;
        pressed = true;
        address = sMySwitchAddr[2];
        break;
    case 0b00110000:
        sTxBusMsg.type = eBusButtonPressed1_2;
        pressed = true;
        address = sMySwitchAddr[2];
        break;
    case 0b01000000:
        sTxBusMsg.type = eBusButtonPressed1;
        pressed = true;
        address = sMySwitchAddr[3];
        break;
    case 0b10000000:
        sTxBusMsg.type = eBusButtonPressed2;
        pressed = true;
        address = sMySwitchAddr[3];
        break;
    case 0b11000000:
        sTxBusMsg.type = eBusButtonPressed1_2;
        pressed = true;
        address = sMySwitchAddr[3];
        break;
    default:
        pressed = false;
        address = MY_ADDR;
        break;
    }

    switch (sSequenceState) {
    case BUTTON_TX_OFF:
        if (pressed) {
            /* begin transmission of eBusButtonPressed* telegram */
            GET_TIME_S(sStartTimeS);
            sTimeStampMs = GET_TIME_MS;
            sTxBusMsg.senderAddr = address;
            BusSend(&sTxBusMsg);
            sSequenceState = BUTTON_TX_ON;
        }
        break;
    case BUTTON_TX_ON:
        if (pressed) {
            GET_TIME_S(actualTimeS);
            if ((uint16_t)(actualTimeS - sStartTimeS) < BUTTON_TELEGRAM_REPEAT_TIMEOUT) {
                actualTimeMs = GET_TIME_MS; 
                if ((uint8_t)(actualTimeMs - sTimeStampMs) >= BUTTON_TELEGRAM_REPEAT_DIFF) {
                    sTimeStampMs = GET_TIME_MS;
                    sTxBusMsg.senderAddr = address;
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
*  process received bus telegrams
*/
static void ProcessBus(void) {

   uint8_t       ret;
   TBusMsgType   msgType;
   uint8_t       *p;
   bool          msgForMe = false;
   
   ret = BusCheck();

   if (ret == BUS_MSG_OK) {
      msgType = spRxBusMsg->type; 
      switch (msgType) {  
         case eBusDevReqReboot:
         case eBusDevReqActualValue:
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
         case eBusDevReqActualValue:
            sTxBusMsg.senderAddr = MY_ADDR; 
            sTxBusMsg.type = eBusDevRespActualValue;
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxBusMsg.msg.devBus.x.devResp.actualValue.devType = eBusDevTypeSw16;
            sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.sw16.input_state = sInputState;
            memcpy(sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.sw16.led_state, 
                   sLedData, BUS_SW16_LED_SIZE_SET_VALUE);
            BusSend(&sTxBusMsg);  
            break;            
         case eBusDevReqInfo:
            sTxBusMsg.type = eBusDevRespInfo;  
            sTxBusMsg.senderAddr = MY_ADDR; 
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxBusMsg.msg.devBus.x.devResp.info.devType = eBusDevTypeSw16;
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
            memcpy(sNewLedData, spRxBusMsg->msg.devBus.x.devReq.setValue.setValue.sw16.led_state,
                   BUS_SW16_LED_SIZE_SET_VALUE);
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
