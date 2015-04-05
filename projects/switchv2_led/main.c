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

/* configure port pins                                                 */ 
/* configure unused pins to output low                                 */
#define PORT_C_CONFIG_DIR     2  /* bitmask for port c:                */
                                 /* 1 output, 0 input                  */
                                 /* 0b11111111 = 0xff */
#define PORT_C_CONFIG_PORT    3  /* bitmask for port c:                */
                                 /* 1 pullup, 0 hi-z for inputs        */
                                 /* 0b00000000 = 0x00 */

/* RXD und INT0 input without pullup                                   */
/* TXD output high                                                     */
/* other pins output low                                               */
#define PORT_D_CONFIG_DIR     4  /* bitmask for port d:                */
                                 /* 1 output, 0 input                  */
                                 /* 0b11111010 = 0xfa */
#define PORT_D_CONFIG_PORT    5  /* bitmask for port d:                */
                                 /* 1 pullup, 0 hi-z for inputs        */
                                 /* 0b00100010 = 0x22 */


/* our bus address */
#define MY_ADDR    sMyAddr   


#define IDLE_SIO  0x01

/* bidirectional led on portc.0 and portc.1 */
#define LED_GREEN           PORTC = 0b00000001
#define LED_RED             PORTC = 0b00000010
#define LED_OFF             PORTC = 0b00000000

/* Port D bit 5 controls bus transceiver power down */
#define BUS_TRANSCEIVER_POWER_DOWN \
   PORTD |= (1 << 5)
   
#define BUS_TRANSCEIVER_POWER_UP \
   PORTD &= ~(1 << 5)

#define LED_STATE_OFF          0
#define LED_STATE_GREEN        1
#define LED_STATE_RED          2
#define LED_STATE_RED_BLINK    3

/*-----------------------------------------------------------------------------
*  Typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/  
char version[] = "Sw88_led 0.01";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t  gTimeMs;
volatile uint16_t gTimeMs16;
volatile uint16_t gTimeS;

static uint8_t   sMyAddr;

static uint8_t   sIdle = 0;

static uint8_t   sLedState;

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
static void LedCycle(void);

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
   sioHandle = SioOpen("USART0", eSioBaud9600, eSioDataBits8, eSioParityNo, 
                       eSioStopBits1, eSioModeHalfDuplex);
   SioSetIdleFunc(sioHandle, IdleSio);
   SioSetTransceiverPowerDownFunc(sioHandle, BusTransceiverPowerDown);

   BusTransceiverPowerDown(true);
   
   BusInit(sioHandle);
   spRxBusMsg = BusMsgBufGet();

   sLedState = LED_STATE_OFF;

   /* enable global interrupts */
   ENABLE_INT;  

   SendStartupMsg();
   
   while (1) { 
      Idle();
      ProcessBus();
      LedCycle();
   }
   return 0;  /* never reached */
}


/*-----------------------------------------------------------------------------
*  operate led
*/
static void LedCycle(void) {
    static uint8_t sOldLedState = LED_STATE_OFF;

    if (sOldLedState != sLedState) {
        sOldLedState = sLedState;
        switch (sLedState) {
            case LED_STATE_OFF:
                LED_OFF;
                break;
            case LED_STATE_RED:
                LED_RED;
                break;
            case LED_STATE_GREEN:
                LED_GREEN;
                break;
            default:
                break;
        }
    }
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
*  process received bus telegrams
*/
static void ProcessBus(void) {

   uint8_t     ret;
   TBusMsgType msgType;
   uint8_t     *p;
   bool        msgForMe = false;
   uint8_t     switchState;
   
   ret = BusCheck();

   if (ret == BUS_MSG_OK) {
      msgType = spRxBusMsg->type; 
      switch (msgType) {  
         case eBusDevReqReboot:
         case eBusDevReqActualValue:
         case eBusDevReqSwitchState:
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
            sTxBusMsg.msg.devBus.x.devResp.actualValue.devType = eBusDevTypeLed;
            sTxBusMsg.msg.devBus.x.devResp.actualValue.actualValue.led.state = sLedState;
            BusSend(&sTxBusMsg);  
            break;
         case eBusDevReqSwitchState:
            switchState = spRxBusMsg->msg.devBus.x.devReq.switchState.switchState;
            if ((switchState & 0x01) != 0) {
                sLedState = LED_STATE_GREEN;
            } else {
                sLedState = LED_STATE_RED;
            }
            /* response telegram */
            sTxBusMsg.type = eBusDevRespSwitchState;
            sTxBusMsg.senderAddr = MY_ADDR;
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxBusMsg.msg.devBus.x.devResp.switchState.switchState = switchState;
            BusSend(&sTxBusMsg);
            break;
         case eBusDevReqInfo:
            sTxBusMsg.type = eBusDevRespInfo;  
            sTxBusMsg.senderAddr = MY_ADDR; 
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxBusMsg.msg.devBus.x.devResp.info.devType = eBusDevTypeLed;
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

    /* ms counter */
    gTimeMs16 += 16;  
    gTimeMs += 16;
    intCnt++;
    if (intCnt >= 61) { /* 16.384 ms * 61 = 1 s*/
        intCnt = 0;
        gTimeS++;
    }
}
