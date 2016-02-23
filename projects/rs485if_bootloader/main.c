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

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#include "sio.h"
#include "sysdef.h"
#include "board.h"
#include "bus.h"
#include "led.h"
#include "flash.h"

/*-----------------------------------------------------------------------------
*  ATMega1284 fuse bit settings (18.432 MHz crystal)
*
*  CKSEL0    = 1
*  CKSEL1    = 1
*  CKSEL2    = 1
*  CKSEL3    = 0 
*  SUT0      = 1
*  SUT1      = 0
*  BODLEVEL0  = 0
*  BODLEVEL1  = 0
*  BODLEVEL2  = 1
*  BOOTRST   = 0
*  BOOTSZ0   = 0
*  BOOTSZ1   = 0
*  EESAVE    = 0
*  CKOUT     = 1
*  CKDIV8    = 1
*  JTAGEN    = 1
*  OCDEN     = 1
*  WDTON     = 1
*  SPIEN     = 0
*/

/*-----------------------------------------------------------------------------
*  Macros
*/
/* eigene Adresse am Bus */
#define MY_ADDR    sMyAddr

/* the module address is stored in the first byte of eeprom */
#define MODUL_ADDRESS        0

/* Bits in MCUCR */
#define IVCE     0
#define IVSEL    1

/* Bits in TCCR0 */
/*#define CS00     0
#define OCIE0    1
#define WGM00    6
#define WGM01    3
*/

/* Bits in WDTCR */
#define WDCE     4
#define WDE      3
#define WDP0     0
#define WDP1     1
#define WDP2     2

/* statemachine für Flashupdate */
#define WAIT_FOR_UPD_ENTER          0
#define WAIT_FOR_UPD_ENTER_TIMEOUT  1
#define WAIT_FOR_UPD_DATA           2

/* max. Größe der Firmware */
#define MAX_FIRMWARE_SIZE   (120UL * 1024UL)
#define FLASH_CHECKSUM      0x1234  /* Summe, die bei Checksum herauskommen muss */
#define CHECKSUM_BLOCK_SIZE  256 /* words */

#define IDLE_SIO1  0x01

/*-----------------------------------------------------------------------------
*  Variables
*/
volatile uint8_t  gTimeMs = 0;
volatile uint16_t gTimeMs16 = 0;
volatile uint16_t gTimeS = 0;

static TBusTelegram  *spBusMsg;
static TBusTelegram  sTxBusMsg;
static uint8_t       sFwuState = WAIT_FOR_UPD_ENTER_TIMEOUT;
static uint8_t       sMyAddr;
static uint8_t       sIdle = 0;

/* auf wordaddressbereich 0xff80 .. 0xfffe im Flash sind 255 Byte für den Versionstring reserviert */
char version[] __attribute__ ((section (".version")));
char version[] = "rs485if_bootloader_0.00";

/*-----------------------------------------------------------------------------
*  Functions
*/
extern void ApplicationEntry(void);

static void PortInit(void);
static void TimerInit(void);
static void TimerStart(void);
static void ProcessBus(uint8_t ret);
static uint8_t CheckTimeout(void);
static void Idle(void);
static void IdleSio1(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);

/*-----------------------------------------------------------------------------
*  Programstart
*/
int main(void) {

   uint8_t   ret;
   uint16_t  flashWordAddr;
   uint16_t  sum;
   int       sioHdl;

   sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);

   PortInit();
   TimerInit();
   LedInit();
   FlashInit();

   SioInit();

   /* sio for bus interface */
   sioHdl = SioOpen("USART0", eSioBaud9600, eSioDataBits8, eSioParityNo,
                    eSioStopBits1, eSioModeHalfDuplex);

   SioSetIdleFunc(sioHdl, IdleSio1);
   SioSetTransceiverPowerDownFunc(sioHdl, BusTransceiverPowerDown);

   BusTransceiverPowerDown(true);

   BusInit(sioHdl);
   spBusMsg = BusMsgBufGet();

   /* Umschaltung der Interruptvektor-Tabelle */
   MCUCR = (1 << IVCE);
   /* In Bootbereich verschieben */
   MCUCR = (1 << IVSEL);

   LedSet(eLedRedOn);
   LedSet(eLedGreenOff);

   /* wait for full supply voltage */
//   while (!POWER_GOOD);

   LedSet(eLedRedOff);

   /* calculate application checksum */
   sum = 0;
   for (flashWordAddr = 0; flashWordAddr < (MAX_FIRMWARE_SIZE / 2); flashWordAddr += CHECKSUM_BLOCK_SIZE) {
      sum += FlashSum(flashWordAddr, (uint8_t)CHECKSUM_BLOCK_SIZE);
   }

   if (sum == FLASH_CHECKSUM) {
      /* OK */
      LedSet(eLedGreenFlashFast);
   } else {
      /* Fehler */
      sFwuState = WAIT_FOR_UPD_ENTER;
      LedSet(eLedRedFlashFast);
   }

   ENABLE_INT;

   TimerStart();

   /* Startup-Msg senden */
   sTxBusMsg.type = eBusDevStartup;
   sTxBusMsg.senderAddr = MY_ADDR;
   BusSend(&sTxBusMsg);

   /* Hauptschleife */
   while (1) {
      Idle();
      ret = BusCheck();
      ProcessBus(ret);
      LedCheck();
      /* Mit timeout auf Request zum Firmwareupdate warten  */
      if (sFwuState == WAIT_FOR_UPD_ENTER_TIMEOUT) {
         ret = CheckTimeout();
         if (ret != 0) {
            /* Application starten */
            break;
         }
      }
   }

   cli();
   /* Umschaltung der Interruptvektor-Tabelle */
   MCUCR = (1 << IVCE);
   /* In Applikationsbereich verschieben */
   MCUCR = (0 << IVSEL);

   /* zur Applikation springen */
   ApplicationEntry();
   /* hier kommen wir nicht her!!*/
   return 0;
}

/*-----------------------------------------------------------------------------
*  liefert bei Timeout 1
*/
static uint8_t CheckTimeout(void) {

   uint16_t actTimeS;

   GET_TIME_S(actTimeS);

   if (actTimeS < 4) {
      return 0;
   } else {
      return 1;
   }
}

/*-----------------------------------------------------------------------------
*  Verarbeitung der Bustelegramme
*/
static void ProcessBus(uint8_t ret) {

   TBusMsgType   msgType;
   uint16_t        *pData;
   uint16_t        wordAddr;
   bool          rc;

   if (ret == BUS_MSG_OK) {
      msgType = spBusMsg->type;
      if ((msgType == eBusDevReqReboot) &&
          (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {
         /* Über Watchdog Reset auslösen */
         /* Watchdogtimeout auf kurzeste Zeit (14 ms) stellen */
         cli();
         wdt_enable(WDTO_15MS);
         /* warten auf Reset */
         while (1);
      } else {
         switch (sFwuState) {
            case WAIT_FOR_UPD_ENTER_TIMEOUT:
            case WAIT_FOR_UPD_ENTER:
               if ((msgType == eBusDevReqUpdEnter) &&
                   (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {

                  /* Applicationbereich des Flash löschen */
                  FlashErase();

                  /* Antwort senden */
                  sTxBusMsg.type = eBusDevRespUpdEnter;
                  sTxBusMsg.senderAddr = MY_ADDR;
                  sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
                  BusSend(&sTxBusMsg);
                  sFwuState = WAIT_FOR_UPD_DATA;
                  LedSet(eLedRedFlashFast);
                  LedSet(eLedGreenFlashFast);
               }
               break;
            case WAIT_FOR_UPD_DATA:
               if ((msgType == eBusDevReqUpdData) &&
                   (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {

                  wordAddr = spBusMsg->msg.devBus.x.devReq.updData.wordAddr;
                  pData = spBusMsg->msg.devBus.x.devReq.updData.data;

                  /* Flash programmieren */
                  rc = FlashProgram(wordAddr, pData, sizeof(spBusMsg->msg.devBus.x.devReq.updData.data)/2);
                  /* Antwort senden */
                  sTxBusMsg.type = eBusDevRespUpdData;
                  sTxBusMsg.senderAddr = MY_ADDR;
                  sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
                  if (rc == true) {
                     /* Falls Programmierung des Block OK: empfange wordAddr zurücksenden */
                     sTxBusMsg.msg.devBus.x.devResp.updData.wordAddr = wordAddr;
                  } else {
                     /* Problem bei Programmierung: -1 als wordAddr zurücksenden */
                     sTxBusMsg.msg.devBus.x.devResp.updData.wordAddr = -1;
                  }
                  BusSend(&sTxBusMsg);
               } else if ((msgType == eBusDevReqUpdTerm) &&
                          (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {
                  /* programmiervorgang im Flash abschließen (falls erforderlich) */
                  rc = FlashProgramTerminate();
                  /* Antwort senden */
                  sTxBusMsg.type = eBusDevRespUpdTerm;
                  sTxBusMsg.senderAddr = MY_ADDR;
                  sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
                  if (rc == true) {
                     /* Falls Programmierung OK: success auf 1 setzen */
                     sTxBusMsg.msg.devBus.x.devResp.updTerm.success = 1;
                  } else {
                     /* Porblem bei Programmierung: -1 als wordAddr zurücksenden */
                     sTxBusMsg.msg.devBus.x.devResp.updTerm.success = 0;
                  }
                  BusSend(&sTxBusMsg);
               }
               break;
            default:
               break;
         }
      }
   } else if (ret == BUS_MSG_ERROR) {
//      LedSet(eLedRedBlinkOnceLong);
//      SioDbgSendConstStr("msg error\r\n");
   }
}

/*-----------------------------------------------------------------------------
*  Timerinitialisierung
*/
static void TimerInit(void) {

   /* use timer 3/output compare A */
   /* timer3 compare B is used for sio timing - do not change the timer mode WGM 
    * and change sio timer settings when changing the prescaler!
    */
   
   /* prescaler @ 3.6864 MHz:  1024  */
   /*           @ 18.432 MHz:  1024  */
   /* compare match pin not used: COM3A[1:0] = 00 */
   /* compare register OCR3A:  */
   /* 3.6864 MHz: 18 -> 5 ms */
   /* 18.432 MHz: 90 -> 5 ms */
   /* timer mode 0: normal: WGM3[3:0]= 0000 */

   TCCR3A = (0 << COM3A1) | (0 << COM3A0) | (0 << COM3B1) | (0 << COM3B0) | (0 << WGM31) | (0 << WGM30);
   TCCR3B = (0 << ICNC3) | (0 << ICES3) |
            (0 << WGM33) | (0 << WGM32) | 
            (1 << CS32)  | (0 << CS31)  | (1 << CS30); 

#if (F_CPU == 3686400UL)
   #define TIMER_TCNT_INC    18
   #define TIMER_INC_MS      5
#elif (F_CPU == 18432000UL)
   #define TIMER_TCNT_INC    90
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
   sCounter++;
   if (sCounter >= (1000 / TIMER_INC_MS)) {
      sCounter = 0;
      /* s */
      gTimeS++;
   }
}

/*-----------------------------------------------------------------------------
*  Einstellung der Portpins
*/
static void PortInit(void) {

   /* PA.6, PA.7: LED1, LED2: output low */
   /* PA.5: unused: output low */
   /* PA.4: input: high z */
   /* PA.3 .. PA.0: input pull up */
   PORTA = 0b00001111;
   DDRA  = 0b11100000;

   /* PB.7: SCK: input pull up */
   /* PB.6, MISO: output high */
   /* PB.5: MOSI: input, pull up */
   /* PB.4 .. PB.3: input, high z */
   /* PB.2: input, high z */
   /* PB.1: unused, output low */
   /* PB.0: transceiver power, output high */
   PORTB = 0b11100001;
   DDRB  = 0b01000011;

   /* PC.7: unused: output low */
   /* PC.6: power fail input high z */
   /* PC.5: unused: output low */
   /* PC.4: unused: output low */
   /* PC.3: RS485 TXEN: output low */
   /* PC.2: RS485 nRXEN: output high */
   /* PC.1: unused: output low */
   /* PC.0: unused: output low */
   PORTC = 0b00000100;
   DDRC  = 0b10111111;

   /* PD.7: input high z */
   /* PD.6: input high z */
   /* PD.5: unused: output low */
   /* PD.4: digout: output low */
   /* PD.3: TX1: output high */
   /* PD.2: RX1: input high z */
   /* PD.1: TX0: output high */
   /* PD.0: RX0: input high z */
   PORTD = 0b00001010;
   DDRD  = 0b00111010;
}

/*-----------------------------------------------------------------------------*/
/**
*   Idle-Mode einschalten
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
