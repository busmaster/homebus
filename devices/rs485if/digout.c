/*
 * digout.c
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
#include <string.h>
#include <avr/pgmspace.h>

#include "sysdef.h"
#include "board.h"
#include "digout.h"

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef void (* TFuncOn)(void);
typedef void (* TFuncOff)(void);

typedef struct {
   TFuncOn     fOn;
   TFuncOff    fOff;
} TAccessFunc;

typedef enum {
   eDigOutNoDelay,
   eDigOutDelayOn,
   eDigOutDelayOff,
   eDigOutDelayOnOff
} TDelayState;
 
typedef struct {
   uint32_t     startTimeMs;
   uint32_t     onDelayMs; 
   uint32_t     offDelayMs;
   TDelayState  state;
} TDigoutDesc;

/*-----------------------------------------------------------------------------
*  Functions
*/
#include "digout_l.h"

/*-----------------------------------------------------------------------------
*  Variables
*/
static const TAccessFunc sDigOutFuncs[NUM_DIGOUT] PROGMEM = {
   {On0,  Off0 }
};

static TDigoutDesc sState[eDigOutNum];
static uint32_t    sDigOutShadow;
          
/*-----------------------------------------------------------------------------
*  init
*/
void DigOutInit(void) {
   uint8_t i;
   TDigoutDesc *pState;

   pState = sState;
   for (i = 0; i < NUM_DIGOUT; i++) {
      pState->state = eDigOutNoDelay;
      pState++;
   }                   
}

/*-----------------------------------------------------------------------------
*  Ausgang einschalten
*/
void DigOutOn(TDigOutNumber number) {

   TFuncOn fOn = (TFuncOn)pgm_read_word(&sDigOutFuncs[number].fOn);
   fOn();
   sDigOutShadow |= 1UL << (uint32_t)number;
}

/*-----------------------------------------------------------------------------
*  Ausgang ausschalten
*/
void DigOutOff(TDigOutNumber number) {

   TFuncOff fOff = (TFuncOff)pgm_read_word(&sDigOutFuncs[number].fOff);
   fOff();
   sDigOutShadow &= ~(1UL << (uint32_t)number);
}

/*-----------------------------------------------------------------------------
*  alle Ausgänge ausschalten
*/
void DigOutOffAll(void) {     
   uint8_t i;

   for (i = 0; i < NUM_DIGOUT; i++) {
      DigOutOff(i);
   }
}

/*-----------------------------------------------------------------------------
*  Ausgangszustand lesen
*/
bool DigOutState(TDigOutNumber number) {

   uint32_t bitMask;

   bitMask = 1UL << (uint32_t)number;
   return (sDigOutShadow & bitMask) != 0;
}

/*-----------------------------------------------------------------------------
*  alle Ausgangszustände lesen
*/
void DigOutStateAll(uint8_t *pBuf, uint8_t bufLen) {

   memcpy(pBuf, &sDigOutShadow, bufLen);
}

/*-----------------------------------------------------------------------------
*  alle Ausgangszustände lesen
*  Ausgangzustände von Ausgängen mit Sonderfunktion (Shade, Delay-Zustand)
*  werden mit Zustand 0 (= ausgeschaltet) gemeldet
*  Diese Ausgangszustände werden beim Powerfail im EEPROM gespeichert
*/
void DigOutStateAllStandard(uint8_t *pBuf, uint8_t bufLen) {
   uint8_t i;
   TDigoutDesc *pState;  
   uint8_t maxIdx;

   pState = sState;
   memset(pBuf, 0, bufLen);
   maxIdx = min(bufLen * 8, NUM_DIGOUT);
   for (i = 0; i < maxIdx; i++) {   
      if (pState->state == eDigOutNoDelay) {
         /* nur Zustand von Standardausgang ist interessant */
         if (DigOutState(i) == true) {
            *(pBuf + i / 8) |= 1 << (i % 8);
         }
      }
      pState++;
   }
}

/*-----------------------------------------------------------------------------
*  alle Ausgangszustände setzen (mit zeitverzögerung zwischen eingeschalteten
*  Ausgängen zur Verringerung von Einschaltstromspitzen)
*/
void DigOutAll(uint8_t *pBuf, uint8_t bufLen) {
   uint8_t i;
   uint8_t maxIdx;

   maxIdx = min(bufLen * 8, NUM_DIGOUT);
   for (i = 0; i < maxIdx; i++) {
      if ((*(pBuf + i / 8) & (1 << (i % 8))) != 0) { 
         DigOutOn(i);
         /* Verzögerung nur bei eingeschalteten Ausgängen */
         DELAY_MS(200);
      } else {
         DigOutOff(i);
      }      
   }
}


/*-----------------------------------------------------------------------------
*  Ausgang wechseln
*/
void DigOutToggle(TDigOutNumber number) {

   uint32_t bitMask;

   bitMask = 1UL << (uint32_t)number;
   if ((sDigOutShadow & bitMask) == 0) {
      DigOutOn(number);
   } else {
      DigOutOff(number);
   }
}

/*-----------------------------------------------------------------------------
*  Ausgang mit Verzögerung einschalten
*/
void DigOutDelayedOn(TDigOutNumber number, uint32_t onDelayMs) {
   sState[number].state = eDigOutDelayOn;
   sState[number].onDelayMs = onDelayMs;   
   GET_TIME_MS32(sState[number].startTimeMs);
}

/*-----------------------------------------------------------------------------
*  Ausgang mit Verzögerung ausschalten
*/
void DigOutDelayedOff(TDigOutNumber number, uint32_t offDelayMs) {

   DigOutOn(number);
   sState[number].state = eDigOutDelayOff;
   sState[number].offDelayMs = offDelayMs;
   GET_TIME_MS32(sState[number].startTimeMs);
}

/*-----------------------------------------------------------------------------
*  Ausgang mit Verzögerung ein- und ausschalten        
*  Die Ausschaltzeitverzögerung beginnt nach der Einschaltzeitverzögerung zu
*  laufen.
*/
void DigOutDelayedOnDelayedOff(TDigOutNumber number, uint32_t onDelayMs, uint32_t offDelayMs) {
   sState[number].state = eDigOutDelayOnOff;
   sState[number].onDelayMs = onDelayMs;
   sState[number].offDelayMs = offDelayMs;
   GET_TIME_MS32(sState[number].startTimeMs);
   /* startTimeMs für das Ausschalten wird beim Einschalten gesetzt */
}

/*-----------------------------------------------------------------------------

*/
void DigOutDelayCancel(TDigOutNumber number)  {
   sState[number].state = eDigOutNoDelay;
}

/*-----------------------------------------------------------------------------

*/
bool DigOutIsDelayed(TDigOutNumber number)  {
   return (sState[number].state != eDigOutNoDelay);
}

/*-----------------------------------------------------------------------------
*  trigger output pulse
*/
void DigOutTrigger(TDigOutNumber number) {
   DigOutDelayedOff(number, 1000);
}

/*-----------------------------------------------------------------------------
*  Statemachine für zeitgesteuerung Aktionen
*  mit jedem Aufruf wird ein Ausgang bearbeitet
*/
void DigOutStateCheck(void) {
   static uint8_t sDigoutNum = 0;   
   TDigoutDesc    *pState; 
   TDelayState    delayState;
   uint32_t       actualTime;

   GET_TIME_MS32(actualTime);
   pState = &sState[sDigoutNum];
   delayState = pState->state;
   if (delayState == eDigOutNoDelay) {
      /* nichts zu tun */
   } else if (delayState == eDigOutDelayOn) {
      if (((uint32_t)(actualTime - pState->startTimeMs)) >= pState->onDelayMs) {
         DigOutOn(sDigoutNum);
         delayState = eDigOutNoDelay;
      }
   } else if (delayState == eDigOutDelayOff) {
      if (((uint32_t)(actualTime - pState->startTimeMs)) >= pState->offDelayMs) {
         DigOutOff(sDigoutNum);
         delayState = eDigOutNoDelay;
      }                                                   
   } else if (delayState == eDigOutDelayOnOff) {
      if (DigOutState(sDigoutNum) == false) {
         /* einschaltverzögerung aktiv*/
         if (((uint32_t)(actualTime - pState->startTimeMs)) >= pState->onDelayMs) {
             DigOutOn(sDigoutNum);
             /* ab jetzt beginnt die Ausschaltverzögerung */
             pState->startTimeMs = actualTime;
         }
      } else {
         if (((uint32_t)(actualTime - pState->startTimeMs)) >= pState->offDelayMs) {
            DigOutOff(sDigoutNum);
            delayState = eDigOutNoDelay;
         }                                                   
      }
   }
   sDigoutNum++;
   if (sDigoutNum >= NUM_DIGOUT) {
      sDigoutNum = 0;
   }
   pState->state = delayState;
}


/*-----------------------------------------------------------------------------
*  Zugriffsfunktion für die Umsetzung auf die Nummer des Ausgangs
*/
static void On0(void) {
   DIGOUT_0_ON;
}
static void Off0(void) {
   DIGOUT_0_OFF;
}
