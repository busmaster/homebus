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

#define TIMER1_PRESCALER (0b001 << CS10) // prescaler clk/1

#define MAX_CAL_TEL       31

/*-----------------------------------------------------------------------------
*  Variables
*/  
char version[] = "Sw88cal 1.00";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t  gTimeMs;
volatile uint16_t gTimeMs16;
volatile uint16_t gTimeS;

static uint8_t   sMyAddr;

static uint8_t   sIdle = 0;

static int       sSioHandle;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void TimerInit(void);
static void Idle(void);
static void IdleSio(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);
static void ProcessBus(void);
static void SendStartupMsg(void);
static void InitTimer1(void);
static void InitComm(void);
static void ExitComm(void);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {                      

   MCUSR = 0;
   wdt_disable();

   /* get module address from EEPROM */
   sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);

   PortInit();
   TimerInit();

   InitComm();

   /* enable global interrupts */
   ENABLE_INT;  

   SendStartupMsg();

   while (1) { 
      Idle();
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


/*-----------------------------------------------------------------------------
*  process received bus telegrams
*/
static void ProcessBus(void) {

   uint8_t     ret;
   TBusMsgType msgType;
   uint8_t     i;
   uint8_t     *p;
   bool        msgForMe = false;
   uint8_t     flags;

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
   
   ret = BusCheck();

   if (ret == BUS_MSG_OK) {
      msgType = spRxBusMsg->type; 
      switch (msgType) {  
         case eBusDevReqReboot:
         case eBusDevReqInfo:
         case eBusDevReqSetAddr:
         case eBusDevReqEepromRead:
         case eBusDevReqEepromWrite:
         case eBusDevReqDoClockCalib:
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
         case eBusDevReqInfo:
            sTxBusMsg.type = eBusDevRespInfo;  
            sTxBusMsg.senderAddr = MY_ADDR; 
            sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxBusMsg.msg.devBus.x.devResp.info.devType = eBusDevTypeSw8Cal;
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
         case eBusDevReqDoClockCalib:
            if (spRxBusMsg->msg.devBus.x.devReq.doClockCalib.command == eBusDoClockCalibInit) {
                sCount = 0;
                sOsccal = 0;
                sMinDiff = 0xffff;
            } else if (sCount > MAX_CAL_TEL) {
                sTxBusMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateError;
                sTxBusMsg.senderAddr = MY_ADDR; 
                sTxBusMsg.type = eBusDevRespDoClockCalib;
                sTxBusMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
                BusSend(&sTxBusMsg);
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
                sTxBusMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateContiune;
                sCount++;
            } else {
                sTxBusMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateSuccess;
                /* save the osccal correction value to eeprom */
                osccal_corr = eeprom_read_byte((const uint8_t *)OSCCAL_CORR);
                osccal_corr += sMinOsccal - old_osccal;
                eeprom_write_byte((uint8_t *)OSCCAL_CORR, osccal_corr);
                OSCCAL = sMinOsccal;
                NOP_10;
            }
            sTxBusMsg.senderAddr = MY_ADDR; 
            sTxBusMsg.type = eBusDevRespDoClockCalib;
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

   /* configure unused pins to input high-z */
   PORTB = 0b00000000; 
   DDRB =  0b00000001; /* PB0 is used to trigger the capture of timer 1 -> output */
   
   PORTC = 0b00000000;
   DDRC =  0b00000000;            

   PORTD = 0b00100010;
   DDRD =  0b00100010;
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
