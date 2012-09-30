/*-----------------------------------------------------------------------------
*  Main.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <avr/io.h>
#include <avr/wdt.h>

#include "sio.h"
#include "sysdef.h"
#include "bus.h"
#include "flash.h"

/*-----------------------------------------------------------------------------
*  Macros  
*/                    
/* eigene Adresse am Bus */
#define MY_ADDR    sMyAddr   

#define MODUL_ADDRESS       0
#define OSCCAL_CORR         1
                               
/* statemachine für Flashupdate */
#define WAIT_FOR_UPD_ENTER          0  
#define WAIT_FOR_UPD_ENTER_TIMEOUT  1  
#define WAIT_FOR_UPD_DATA           2            

/* max. Größe der Firmware */
#define MAX_FIRMWARE_SIZE    (6UL * 1024UL)   
#define FLASH_CHECKSUM       0x1234  /* Summe, die bei Checksum herauskommen muss */
#define CHECKSUM_BLOCK_SIZE  256 /* words */

/*-----------------------------------------------------------------------------
*  Typedefs
*/  

/*-----------------------------------------------------------------------------
*  Variables
*/  
volatile uint8_t gTimeS8 = 0;        

static TBusTelegramm  *spRxBusMsg;    
static TBusTelegramm  sTxBusMsg;        
static uint8_t sFwuState = WAIT_FOR_UPD_ENTER_TIMEOUT;
static uint8_t sMyAddr;

/* auf wordaddressbereich 0xff8 .. 0xffff im Flash sind 16 Byte für den Versionstring reserviert */
char version[] __attribute__ ((section (".version")));
char version[] = "ATM8 BL 1.00";

/*-----------------------------------------------------------------------------
*  Functions
*/
extern void ApplicationEntry(void);

static void ProcessBus(uint8_t ret);      
static void SetMsg(TBusMsgType type, uint8_t receiver);

/*-----------------------------------------------------------------------------
*  Programstart
*/
int main(void) {

   uint8_t   ret;  
   uint16_t  flashWordAddr;    
   uint16_t  sum;

   /* get oscillator correction value from EEPROM */
   EEAR = OSCCAL_CORR;
   /* Start eeprom read by writing EERE */
   EECR |= (1 << EERE);
   /* read data */
   OSCCAL += EEDR;

   /* get modul adress from EEPROM */
   EEAR = MODUL_ADDRESS;
   /* Start eeprom read by writing EERE */
   EECR |= (1 << EERE);
   /* read data */
   sMyAddr = EEDR;

   /* Portpins für Schaltereingänge mit Pullup konfigurieren */
   /* nicht benutzte Pin aus Ausgang Low*/
   PORTC = 0x03;
   DDRC = 0x3C;             
   
   PORTB = 0x38;
   DDRB = 0xC7;
   
   PORTD = 0x01;
   DDRD = 0xFE;

   /* configure Timer 0 */
   /* prescaler clk/64 -> Interrupt period 256/1000000 * 64 = 16.384 ms */
   TCCR0 = 3 << CS00; 
   TIMSK = 1 << TOIE0;

   SioInit();
   spRxBusMsg = BusMsgBufGet();

   /* Umschaltung der Interruptvektor-Tabelle */
   GICR = (1 << IVCE);
   /* In Bootbereich verschieben */
   GICR = (1 << IVSEL);

   /* Prüfsumme der Applikation berechnen */
   sum = 0;
   for (flashWordAddr = 0; flashWordAddr < (MAX_FIRMWARE_SIZE / 2); flashWordAddr += CHECKSUM_BLOCK_SIZE) {
      sum += FlashSum(flashWordAddr, (uint8_t)CHECKSUM_BLOCK_SIZE);
   }

   if (sum != FLASH_CHECKSUM) {
      /* Fehler */
      sFwuState = WAIT_FOR_UPD_ENTER;      
   }

   sei();
                          
   /* Startup-Msg senden */
   sTxBusMsg.type = eBusDevStartup;  
   sTxBusMsg.senderAddr = MY_ADDR; 
   BusSend(&sTxBusMsg);  
   SioReadFlush();
   
   /* Hauptschleife */
   while (1) {   
      ret = BusCheck();
      ProcessBus(ret);
      /* Mit timeout auf Request zum Firmwareupdate warten  */
      if (sFwuState == WAIT_FOR_UPD_ENTER_TIMEOUT) {
         if (gTimeS8 >= 4) {
            /* Application starten */
            break;
         }
      } 
   } 
            
   cli();
   /* Umschaltung der Interruptvektor-Tabelle */
   GICR = (1 << IVCE);
   /* In Applikationsbereich verschieben */
   GICR = (0 << IVSEL);
   
   /* zur Applikation springen */       
   ApplicationEntry();
   /* hier kommen wir nicht her!!*/
   return 0;
} 

/*-----------------------------------------------------------------------------
*  Verarbeitung der Bustelegramme
*/
static void ProcessBus(uint8_t ret) {
   
   TBusMsgType   msgType;    
   uint16_t        *pData;
   uint16_t        wordAddr;
   bool          rc;

   if ((ret == BUS_MSG_OK) && 
       (spRxBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {
      msgType = spRxBusMsg->type; 
      if (msgType == eBusDevReqReboot) {
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
               if (msgType == eBusDevReqUpdEnter) {
                  /* Applicationbereich des Flash löschen */  
                  FlashErase();
                  /* Antwort senden */
                  SetMsg(eBusDevRespUpdEnter, spRxBusMsg->senderAddr);
                  BusSend(&sTxBusMsg);  
                  sFwuState = WAIT_FOR_UPD_DATA;     
               }           
               break;
            case WAIT_FOR_UPD_DATA:
               if (msgType == eBusDevReqUpdData) {
                  wordAddr = spRxBusMsg->msg.devBus.x.devReq.updData.wordAddr;
                  pData = spRxBusMsg->msg.devBus.x.devReq.updData.data;
                  /* Flash programmieren */
                  rc = FlashProgram(wordAddr, pData, sizeof(spRxBusMsg->msg.devBus.x.devReq.updData.data) / 2);
                  /* Antwort senden */
                  SetMsg(eBusDevRespUpdData, spRxBusMsg->senderAddr);
                  if (rc == true) {
                     /* Falls Programmierung des Block OK: empfangene wordAddr zurücksenden */
                     sTxBusMsg.msg.devBus.x.devResp.updData.wordAddr = wordAddr;
                  } else {                                                                
                     /* Problem bei Programmierung: -1 als wordAddr zurücksenden */
                     sTxBusMsg.msg.devBus.x.devResp.updData.wordAddr = -1;
                  }
                  BusSend(&sTxBusMsg);  
               } else if (msgType == eBusDevReqUpdTerm) {
                  /* programmiervorgang im Flash abschließen (falls erforderlich) */
                  rc = FlashProgramTerminate();
                  /* Antwort senden */
                  SetMsg(eBusDevRespUpdTerm, spRxBusMsg->senderAddr);
                  if (rc == true) {
                     /* Falls Programmierung OK: success auf 1 setzen */
                     sTxBusMsg.msg.devBus.x.devResp.updTerm.success = 1;
                  } else {                                                                
                     /* Problem bei Programmierung: -1 als wordAddr zurücksenden */
                     sTxBusMsg.msg.devBus.x.devResp.updTerm.success = 0;
                  }
                  BusSend(&sTxBusMsg);  
               }
               break;
            default:
               break;
         }
      }   
   }
}

/*-----------------------------------------------------------------------------
*  Sendedaten eintragen
*/
static void SetMsg(TBusMsgType type, uint8_t receiver) {
   sTxBusMsg.type = type;  
   sTxBusMsg.senderAddr = MY_ADDR; 
   sTxBusMsg.msg.devBus.receiverAddr = receiver;
}

/*-----------------------------------------------------------------------------
*  Timer 0 overflow ISR
*  period:  16.384 ms
*/
ISR(TIMER0_OVF_vect) {

  static uint8_t intCnt = 0;

  intCnt++;
  if (intCnt >= 61) { /* 16.384 ms * 61 = 1 s*/
     intCnt = 0;
     gTimeS8++;
  }
}


