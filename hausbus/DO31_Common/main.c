/*-----------------------------------------------------------------------------
*  Main.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdio.h>
#include <string.h> 

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#include "Types.h"
#include "Sio.h"
#include "Sysdef.h"
#include "Board.h"
#include "Bus.h"
#include "Button.h"
#include "Led.h"
#include "Application.h"

#include "DigOut.h"

/*-----------------------------------------------------------------------------
*  Macros  
*/         

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

/* Bits in EECR */
#define EEWE     1

#define EEPROM_SIZE (uint8_t *)4096  /* durch 4 teilbar!! */

/* eigene Adresse am Bus */
#define MY_ADDR    sMyAddr   
/* im letzten Byte im Flash liegt die eigene Adresse */
#define FLASH_ADDR 0x1FFFF                              

#define IDLE_SIO1  0x01

/*-----------------------------------------------------------------------------
*  Typedefs
*/  

/*-----------------------------------------------------------------------------
*  Variables
*/  
volatile UINT8  gTimeMs = 0;    
volatile UINT16 gTimeMs16 = 0;  
volatile UINT32 gTimeMs32 = 0;  
volatile UINT16 gTimeS = 0;    

static TBusTelegramm *spBusMsg;            
static TBusTelegramm  sTxBusMsg;        

static const uint8_t *spNextPtrToEeprom;
static UINT8         sMyAddr;

static UINT8   sIdle = 0;

int gDbgSioHdl = -1;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);   
static void TimerInit(void);
static void CheckButton(void);
static void ButtonEvent(UINT8 address, UINT8 button);
static void SwitchEvent(UINT8 address, UINT8 button, BOOL pressed);
static void ProcessBus(UINT8 ret);
static void RestoreDigOut(void);
static UINT8 ReadFlash(UINT32 address);
static void Idle(void);
static void IdleSio1(BOOL setIdle);
static void BusTransceiverPowerDown(BOOL powerDown);

/*-----------------------------------------------------------------------------
*  Programstart
*/
int main(void) {                      

   UINT8 ret;
   int   sioHdl;

   sMyAddr = ReadFlash(FLASH_ADDR);

   PortInit();  
   LedInit();
   TimerInit();           
   ButtonInit();
   DigOutInit();    
   ApplicationInit();
   
   SioInit();
   /* sio for debug traces */
   gDbgSioHdl = SioOpen("USART0", eSioBaud9600, eSioDataBits8, eSioParityNo, 
                        eSioStopBits1, eSioModeFullDuplex);
                   
   /* sio for bus interface */
   sioHdl = SioOpen("USART1", eSioBaud9600, eSioDataBits8, eSioParityNo, 
                    eSioStopBits1, eSioModeHalfDuplex);

   SioSetIdleFunc(sioHdl, IdleSio1);
   SioSetTransceiverPowerDownFunc(sioHdl, BusTransceiverPowerDown);
   BusTransceiverPowerDown(TRUE);
   
   BusInit(sioHdl);
   spBusMsg = BusMsgBufGet();
   
   /* warten bis Betriebsspannung auf 24 V-Seite volle Höhe erreicht hat */
   while (!POWER_GOOD); 

   /* für Delay wird timer-Interrupt benötigt (DigOutAll() in RestoreDigOut()) */
   ENABLE_INT;
  
   RestoreDigOut();
   /* ext. Int für Powerfail: low level aktiv */              
   EICRA = 0x00;                  
   EIMSK = 0x01;

   LedSet(eLedGreenFlashSlow);

   /* Hauptschleife */  
   while (1) {   
      Idle();
      ret = BusCheck();
      ProcessBus(ret);
      CheckButton();
      DigOutStateCheck(); 
      LedCheck();
      ApplicationCheck();
   } 
   return 0;
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
static void IdleSio1(BOOL setIdle) {
  
   if (setIdle == TRUE) {
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
static void BusTransceiverPowerDown(BOOL powerDown) {
   
   if (powerDown) {
      BUS_TRANSCEIVER_POWER_DOWN;
   } else {
      BUS_TRANSCEIVER_POWER_UP;
   }
}


/*-----------------------------------------------------------------------------
*  Ausgangszustand wiederherstellen
*  im EEPROM liegen jeweils 4 Byte mit den 31 Ausgangszuständen. Das MSB im 
*  letzten Byte zeigt mit dem Wert 0 die Gültigkeit der Daten an
*/
static void RestoreDigOut(void) {

   const   uint8_t *ptrToEeprom;
   uint8_t buf[4];
   UINT8   flags;

   /* zuletzt gespeicherten Zustand der Ausgänge suchen */  
   for (ptrToEeprom = (const uint8_t *)3; ptrToEeprom < EEPROM_SIZE; ptrToEeprom += 4) {
      if ((eeprom_read_byte(ptrToEeprom) & 0x80) == 0x00) {
         break;
      }     
   }
   if (ptrToEeprom >= EEPROM_SIZE) {
      /* nichts im EEPROM gefunden -> Ausgänge bleiben ausgeschaltet */ 
      spNextPtrToEeprom = 0;
      return;
   }
                        
   /* wieder auf durch vier teilbare Adresse zurückrechnen */
   ptrToEeprom -= 3;   
   /* Schreibhäufigkeit im EEPROM auf alle Adressen verteilen (wegen Lebensdauer) */              
   spNextPtrToEeprom = (const uint8_t *)(((int)ptrToEeprom + 4) % (int)EEPROM_SIZE);
                 
   /* Ausgangszustand wiederherstellen */     
   flags = DISABLE_INT;
   buf[0] = eeprom_read_byte(ptrToEeprom);
   buf[1] = eeprom_read_byte(ptrToEeprom + 1);
   buf[2] = eeprom_read_byte(ptrToEeprom + 2);
   buf[3] = eeprom_read_byte(ptrToEeprom + 3);
   RESTORE_INT(flags);

   DigOutAll(buf, 4);   

   /* alte Ausgangszustand löschen */     
   flags = DISABLE_INT;
   eeprom_write_byte((uint8_t *)ptrToEeprom, 0xff);
   eeprom_write_byte((uint8_t *)ptrToEeprom + 1, 0xff);
   eeprom_write_byte((uint8_t *)ptrToEeprom + 2, 0xff);
   eeprom_write_byte((uint8_t *)ptrToEeprom + 3, 0xff);
   RESTORE_INT(flags);
}                         
            
/*-----------------------------------------------------------------------------
*  Erkennung von Tasten-Loslass-Ereignissen
*/
static void CheckButton(void) {
   UINT8        i = 0;
   TButtonEvent buttonEventData;

   while (ButtonReleased(&i) == TRUE) {
      if (ButtonGetReleaseEvent(i, &buttonEventData) == TRUE) {
         ApplicationEventButton(&buttonEventData);
      }
   } 
}

/*-----------------------------------------------------------------------------
*  Verarbeitung der Bustelegramme
*/
static void ProcessBus(UINT8 ret) {
   TBusMsgType      msgType;    
   UINT8            i;
   /* zum Speichersparen werden die Pointer in einer union gespeichert */
   /* (max. nur ein Paket in Bearbeitung) */
   union {
      TBusDevRespInfo        *pInfo;
      TBusDevRespGetState    *pGetState;
   } p;

   if (ret == BUS_MSG_OK) {
      msgType = spBusMsg->type; 
      switch (msgType) {   
         case eBusDevReqReboot:
            if (spBusMsg->msg.devBus.receiverAddr == MY_ADDR) {
               /* Über Watchdog Reset auslösen */    
               /* Watchdogtimeout auf kurzeste Zeit (14 ms) stellen */                     
               cli();
               wdt_enable(WDTO_15MS);
               /* warten auf Reset */
               while (1);
            }
            break;   
         case eBusButtonPressed1:
            ButtonEvent(spBusMsg->senderAddr, 1);
            break;
         case eBusButtonPressed2:
            ButtonEvent(spBusMsg->senderAddr, 2);
            break;
         case eBusButtonPressed1_2:
            ButtonEvent(spBusMsg->senderAddr, 1);
            ButtonEvent(spBusMsg->senderAddr, 2);
            break;
         case eBusDevReqInfo:
            if (spBusMsg->msg.devBus.receiverAddr == MY_ADDR) {
               /* Anwortpaket zusammenstellen */
               p.pInfo = &sTxBusMsg.msg.devBus.x.devResp.info;
               sTxBusMsg.type = eBusDevRespInfo;  
               sTxBusMsg.senderAddr = MY_ADDR; 
               sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
               p.pInfo->devType = eBusDevTypeDo31;
               strncpy((char *)(p.pInfo->version), ApplicationVersion(), sizeof(p.pInfo->version)); 
               p.pInfo->version[sizeof(p.pInfo->version) - 1] = '\0';
               for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
                  DigOutShadeGetConfig(i, &(p.pInfo->devInfo.do31.onSwitch[i]), &(p.pInfo->devInfo.do31.dirSwitch[i]));
               }
               BusSend(&sTxBusMsg);  
            }
            break;
         case eBusDevReqGetState:
            if (spBusMsg->msg.devBus.receiverAddr == MY_ADDR) {
               TDigOutShadeAction action;
               UINT8              state;
                
               /* Anwortpaket zusammenstellen */
               p.pGetState = &sTxBusMsg.msg.devBus.x.devResp.getState;
               sTxBusMsg.type = eBusDevRespGetState;  
               sTxBusMsg.senderAddr = MY_ADDR; 
               sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
               p.pGetState->devType = eBusDevTypeDo31;
               DigOutStateAll(p.pGetState->state.do31.digOut, BUS_DO31_DIGOUT_SIZE_GET); 

               /* Init. des Arrays */
               for (i = 0; i < BUS_DO31_SHADER_SIZE_GET; i++) {
                  p.pGetState->state.do31.shader[i] = 0;
               }
               
               for (i = 0; i < eDigOutShadeNum; i++) {
                  if (DigOutShadeState(i, &action) == TRUE) {
                     switch (action) {
                        case eDigOutShadeClose:
                           state = 0x02;
                           break;
                        case eDigOutShadeOpen:
                           state = 0x01;
                           break;
                        case eDigOutShadeStop:
                           state = 0x03;
                           break;
                        default:
                           state = 0x00;
                           break;
                     }
                  } else {
                     state = 0x00;
                  }
                  if ((i / 4) >= BUS_DO31_SHADER_SIZE_GET) {
                     /* falls im Bustelegram zuwenig Platz -> abbrechen */
                     /* (sollte nicht auftreten) */
                     break;
                  }
                  p.pGetState->state.do31.shader[i / 4] |= (state << ((i % 4) * 2));
               }
               BusSend(&sTxBusMsg);  
            }
            break;
         case eBusDevReqSetState:
            if ((spBusMsg->msg.devBus.receiverAddr == MY_ADDR) && 
                (spBusMsg->msg.devBus.x.devReq.setState.devType == eBusDevTypeDo31)) {

               for (i = 0; i < eDigOutNum; i++) {
                  /* für Rollladenfunktion konfigurierte Ausgänge werden nicht geändert */ 
                  if (!DigOutShadeFunction(i)) {
                     UINT8 action = (spBusMsg->msg.devBus.x.devReq.setState.state.do31.digOut[i / 4] >> 
                                    ((i % 4) * 2)) & 0x03;
                     switch (action) {
                        case 0x00:
                        case 0x01:
                           break;
                        case 0x02:
                           DigOutOff(i);
                           break;
                        case 0x3:
                           DigOutOn(i);
                           break;
                        default:
                           break;
                     }
                  }
               }
               for (i = 0; i < eDigOutShadeNum; i++) {
                  UINT8 action = (spBusMsg->msg.devBus.x.devReq.setState.state.do31.shader[i / 4] >> 
                                 ((i % 4) * 2)) & 0x03;
                  switch (action) {
                      case 0x00:
                         /* keine Aktion */
                         break;
                      case 0x01:
                         DigOutShade(i, eDigOutShadeOpen);
                         break;
                      case 0x02:
                         DigOutShade(i, eDigOutShadeClose);
                         break;
                      case 0x03:
                         DigOutShade(i, eDigOutShadeStop);
                         break;
                      default:
                         break;
                  }
               }
               /* Anwortpaket zusammenstellen */
               sTxBusMsg.type = eBusDevRespSetState;  
               sTxBusMsg.senderAddr = MY_ADDR; 
               sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
               BusSend(&sTxBusMsg);  
            }
            break;
         case eBusDevReqSwitchState:
            if (spBusMsg->msg.devBus.receiverAddr == MY_ADDR) {

               UINT8 switchState = spBusMsg->msg.devBus.x.devReq.switchState.switchState;
               if ((switchState & 0x01) != 0) {
                  SwitchEvent(spBusMsg->senderAddr, 1, TRUE);
               } else {
                  SwitchEvent(spBusMsg->senderAddr, 1, FALSE);
               }               

               /* Anwortpaket zusammenstellen */
               sTxBusMsg.type = eBusDevRespSwitchState;  
               sTxBusMsg.senderAddr = MY_ADDR; 
               sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
               sTxBusMsg.msg.devBus.x.devResp.switchState.switchState = switchState;
               BusSend(&sTxBusMsg);  
            }
            break;
         default:
            break;
      }
   } else if (ret == BUS_MSG_ERROR) {
      LedSet(eLedRedBlinkOnceShort);
//SioWrite(gDbgSioHdl, "msg error\r\n");  
      ButtonTimeStampRefresh();
   }
}

/*-----------------------------------------------------------------------------
*  create button event for application
*/
static void ButtonEvent(UINT8 address, UINT8 button) { 
   TButtonEvent buttonEventData;

   if (ButtonNew(address, button) == TRUE) {
      buttonEventData.address = spBusMsg->senderAddr;
      buttonEventData.pressed = TRUE;   
      buttonEventData.buttonNr = button;
      ApplicationEventButton(&buttonEventData);
   }
} 

/*-----------------------------------------------------------------------------
*  create switch event for application
*/
static void SwitchEvent(UINT8 address, UINT8 button, BOOL pressed) { 
   TButtonEvent buttonEventData;

   buttonEventData.address = spBusMsg->senderAddr;
   buttonEventData.pressed = pressed;   
   buttonEventData.buttonNr = button;
   ApplicationEventButton(&buttonEventData);
} 


/*-----------------------------------------------------------------------------
*  Power-Fail Interrupt (Ext. Int 0)
*/
ISR(INT0_vect) {
   uint8_t *ptrToEeprom;
   uint8_t buf[4];            
   UINT8 i;

   LedSet(eLedGreenOff);
   LedSet(eLedRedOff);

   /* Ausgangszustände lesen */
   DigOutStateAllStandard(buf, sizeof(buf));
   /* zum Stromsparen alle Ausgänge abschalten */
   DigOutOffAll();
   
   /* neue Zustände speichern */
   ptrToEeprom = (uint8_t *)spNextPtrToEeprom;
   /* Kennzeichnungsbit löschen */
   buf[3] &= ~0x80;
   for (i = 0; i < sizeof(buf); i++) {
      eeprom_write_byte(ptrToEeprom, buf[i]);
      ptrToEeprom++;
   }
    
   /* Wait for completion of previous write */
   while (!eeprom_is_ready());
   
   LedSet(eLedRedOn);

   /* auf Powerfail warten */
   /* falls sich die Versorgungsspannung wieder erholt hat und */
   /* daher kein Power-Up Reset passiert, wird ein Reset über */
   /* den Watchdog ausgelöst */
   while (!POWER_GOOD);

   /* Versorgung wieder OK */
   /* Watchdogtimeout auf 2 s stellen */                     
   wdt_enable(WDTO_2S);
   
   /* warten auf Reset */
   while (1);
}           


/*-----------------------------------------------------------------------------
*  Timer-Interrupt für Zeitbasis (Timer 0 Compare)
*/
ISR(TIMER0_COMP_vect)  {
   static UINT16 sCounter = 0; 

   /* Interrupt alle 2ms */   
   gTimeMs += 2;
   gTimeMs16 += 2;
   gTimeMs32 += 2;
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
*  von Adresse im flash lesen
*  Aufruf unter gesperrtem Interrupt
*/
static UINT8 ReadFlash(UINT32 address) {

   return pgm_read_byte_far(address);
}
