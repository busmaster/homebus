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
*  ATMega1284 fuse bit settings (3.6864 MHz crystal)
*
* avrdude -p ATmega1284p -V -e -v -c stk500v2 -P /dev/stk500 -U flash:w:pwm16_bootloader.hex:i -U lfuse:w:0xfd:m -U hfuse:w:0xd0:m -U efuse:w:0xfc:m
*/

/*-----------------------------------------------------------------------------
*  Macros
*/

/* Bits in TCCR0 */
//#define CS00     0
//#define OCIE0    1
//#define WGM00    6
//#define WGM01    3

/* Bits in WDTCR */
//#define WDCE     4
//#define WDE      3
//#define WDP0     0
//#define WDP1     1
//#define WDP2     2

/* Bits in EECR */
//#define EEWE     1

/* eigene Adresse am Bus */
#define MY_ADDR    sMyAddr

/* the module address is stored in the first byte of eeprom */
#define MODUL_ADDRESS        0

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
*  Typedefs
*/

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
char version[] = "pwm16_Bootloader_2.00";

/*-----------------------------------------------------------------------------
*  Functions
*/
extern void ApplicationEntry(void);

static void PortInit(void);
static void TimerInit(void);
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

   cli();
   MCUSR = 0x00;
   wdt_disable();

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

   LedSet(eLedGreenOn);
   LedSet(eLedRedOn);
   /* warten bis Betriebsspannung auf 24 V-Seite volle Höhe (> 20 V) erreicht hat */
   while (!POWER_GOOD);


   /* Prüfsumme der Applikation berechnen */
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

   LedSet(eLedRedOff);
   LedSet(eLedGreenOn);
   ENABLE_INT;

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
   LedSet(eLedGreenOff);
   cli();
   /* Umschaltung der Interruptvektor-Tabelle */
   MCUCR = (1 << IVCE);
   /* In Applikationsbereich verschieben */
   MCUCR = (0 << IVSEL);
   LedSet(eLedRedOff);
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

   if (actTimeS < 2) {
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
		    LedSet(eLedGreenOn);
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
*  Timer-Interrupt für Zeitbasis
*/
ISR(TIMER0_COMPA_vect)  {

   static uint16_t sCounter = 0;
   /* Interrupt alle 2ms */
   gTimeMs += 2;
   gTimeMs16 += 2;
   sCounter++;
   if (sCounter >= 500) {
      sCounter = 0;
      /* Sekundenzähler */
      gTimeS++;
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
*  Einstellung der Portpins
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
    PORTB = 0b00000100;
    DDRB  = 0b11111011;
	
    /* PC.7: unused: output low */
    /* PC.6: unused: output low */
    /* PC.5: unused: output low */		
    /* PC.4: unused: output low */
    /* PC.3: unused: output low */
    /* PC.2: /OE: output high */	
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
