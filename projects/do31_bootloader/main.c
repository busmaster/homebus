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
*  ATMega128 fuse bit settings (3.6864 MHz crystal)
*   
*  CKSEL0    = 1
*  CKSEL1    = 1
*  CKSEL2    = 1
*  CKSEL3    = 1
*  SUT0      = 1
*  SUT1      = 0
*  BODEN     = 0
*  BODLEVEL  = 0
*  BOOTRST   = 0
*  BOOTSZ0   = 0
*  BOOTSZ1   = 0
*  EESAVE    = 0
*  CKOPT     = 1
*  JTAGEN    = 1
*  OCDEN     = 1
*  WDTON     = 1
*  MC103C    = 1
*/                    

/*-----------------------------------------------------------------------------
*  Macros  
*/                    
/* eigene Adresse am Bus */
#define MY_ADDR    sMyAddr   
/* im letzten Byte im Flash liegt die eigene Adresse */
#define FLASH_ADDR 0x1ffff                              


/* Bits in MCUCR */
#define IVCE     0
#define IVSEL    1

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
                               
/* statemachine für Flashupdate */
#define WAIT_FOR_UPD_ENTER          0  
#define WAIT_FOR_UPD_ENTER_TIMEOUT  1  
#define WAIT_FOR_UPD_DATA           2            

#define EEPROM_SIZE   4096
                           
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
char version[] = "DO31_Bootloader_1.10";

/*-----------------------------------------------------------------------------
*  Functions
*/
extern void ApplicationEntry(void);

static void PortInit(void);   
static void TimerInit(void);
static void ProcessBus(uint8_t ret);      
static uint8_t CheckTimeout(void); 
static void EepromDelete(void); 
static uint8_t ReadFlash(uint32_t address);
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
   int     sioHdl;

   sMyAddr = ReadFlash(FLASH_ADDR);

   PortInit();
   TimerInit();
   LedInit();
   FlashInit();

   SioInit();

   /* sio for bus interface */
   sioHdl = SioOpen("USART1", eSioBaud9600, eSioDataBits8, eSioParityNo, 
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
      
   /* warten bis Betriebsspannung auf 24 V-Seite volle Höhe (> 20 V) erreicht hat */
   while (!POWER_GOOD); 

   LedSet(eLedRedOff);

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
 
                  /* ganzes EEPROM löschen (Ausgangszustände) */
                  EepromDelete();
               
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
ISR(TIMER0_COMP_vect)  {

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
   TCCR0 = (0b010 << CS00) | (0 << WGM00) | (1 << WGM01); 
   OCR0 = 250 - 1;
#elif (F_CPU == 1600000UL) 
   /* 16 MHz */
   TCCR0 = (0b110 << CS00) | (0 << WGM00) | (1 << WGM01); ; 
   OCR0 = 125 - 1;
#elif (F_CPU == 3686400UL) 
   /* 3.6864 MHz */
   TCCR0 = (0b100 << CS00) | (0 << WGM00) | (1 << WGM01); ; 
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


/*-----------------------------------------------------------------------------
*  EEPROM löschen
*/
static void EepromDelete(void) {
   
   uint16_t i;
   for (i = 0; i < EEPROM_SIZE; i += 2) {
      if (eeprom_read_word((uint16_t *)i) != 0xffff) {
         eeprom_write_word((uint16_t *)i, 0xffff); 
      }
   }
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

/*-----------------------------------------------------------------------------
*  von Adresse im flash lesen
*  Aufruf unter gesperrtem Interrupt
*/
static uint8_t ReadFlash(uint32_t address) {

   return pgm_read_byte_far(address);
}

