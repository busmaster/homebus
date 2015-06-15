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
   bool         shaderFunction;
} TDigoutDesc;

/*-----------------------------------------------------------------------------
*  Functions
*/
#include "digout_l.h"

/*-----------------------------------------------------------------------------
*  Variables
*/
static const TAccessFunc sDigOutFuncs[NUM_DIGOUT] PROGMEM = {
   {On0,  Off0 },
   {On1,  Off1 },
   {On2,  Off2 },
   {On3,  Off3 },
   {On4,  Off4 },
   {On5,  Off5 },
   {On6,  Off6 },
   {On7,  Off7 },
   {On8,  Off8 },
   {On9,  Off9 },
   {On10, Off10},
   {On11, Off11},
   {On12, Off12},
   {On13, Off13},
   {On14, Off14},
   {On15, Off15},
   {On16, Off16},
   {On17, Off17},
   {On18, Off18},
   {On19, Off19},
   {On20, Off20},
   {On21, Off21},
   {On22, Off22},
   {On23, Off23},
   {On24, Off24},
   {On25, Off25},
   {On26, Off26},
   {On27, Off27},
   {On28, Off28},
   {On29, Off29},
   {On30, Off30}
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
      pState->shaderFunction = false;
      pState++;
   }                   
}

/*-----------------------------------------------------------------------------
*  special shader function for digout
*/
bool DigOutGetShaderFunction(TDigOutNumber number) {
   
   if (number < NUM_DIGOUT) {
      return sState[number].shaderFunction;
   } else {
      return false;
   }
}

bool DigOutSetShaderFunction(TDigOutNumber number) {
   
   if (number < NUM_DIGOUT) {
      sState[number].shaderFunction = true;
      return true;
    } else {
      return false;
   }
}

bool DigOutClearShaderFunction(TDigOutNumber number) {

   if (number < NUM_DIGOUT) {
      sState[number].shaderFunction = false;
      return true;
   } else {
      return false;
   }
}

/*-----------------------------------------------------------------------------
*  Ausgang einschalten
*/
void DigOutOn(TDigOutNumber number) {

   TFuncOn fOn = (TFuncOn)pgm_read_word(&sDigOutFuncs[number].fOn);
   fOn();
   sDigOutShadow |= 1 << (uint32_t)number;
}

/*-----------------------------------------------------------------------------
*  Ausgang ausschalten
*/
void DigOutOff(TDigOutNumber number) {

   TFuncOff fOff = (TFuncOff)pgm_read_word(&sDigOutFuncs[number].fOff);
   fOff();
   sDigOutShadow &= ~(1 << (uint32_t)number);
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

   bitMask = 1 << (uint32_t)number;
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

   bitMask = 1 << (uint32_t)number;
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

static void On1(void) {
   DIGOUT_1_ON;
}
static void Off1(void) {
   DIGOUT_1_OFF;
}

static void On2(void) {
   DIGOUT_2_ON;
}
static void Off2(void) {
   DIGOUT_2_OFF;
}

static void On3(void) {
   DIGOUT_3_ON;
}
static void Off3(void) {
   DIGOUT_3_OFF;
}

static void On4(void) {
   DIGOUT_4_ON;
}
static void Off4(void) {
   DIGOUT_4_OFF;
}

static void On5(void) {
   DIGOUT_5_ON;
}
static void Off5(void) {
   DIGOUT_5_OFF;
}

static void On6(void) {
   DIGOUT_6_ON;
}
static void Off6(void) {
   DIGOUT_6_OFF;
}

static void On7(void) {
   DIGOUT_7_ON;
}
static void Off7(void) {
   DIGOUT_7_OFF;
}

static void On8(void) {
   DIGOUT_8_ON;
}
static void Off8(void) {
   DIGOUT_8_OFF;
}

static void On9(void) {
   DIGOUT_9_ON;
}
static void Off9(void) {
   DIGOUT_9_OFF;
}

static void On10(void) {
   DIGOUT_10_ON;
}
static void Off10(void) {
   DIGOUT_10_OFF;
}

static void On11(void) {
   DIGOUT_11_ON;
}
static void Off11(void) {
   DIGOUT_11_OFF;
}

static void On12(void) {
   DIGOUT_12_ON;
}
static void Off12(void) {
   DIGOUT_12_OFF;
}

static void On13(void) {
   DIGOUT_13_ON;
}
static void Off13(void) {
   DIGOUT_13_OFF;
}

static void On14(void) {
   DIGOUT_14_ON;
}
static void Off14(void) {
   DIGOUT_14_OFF;
}

static void On15(void) {
   DIGOUT_15_ON;
}
static void Off15(void) {
   DIGOUT_15_OFF;
}

static void On16(void) {
   DIGOUT_16_ON;
}
static void Off16(void) {
   DIGOUT_16_OFF;
}

static void On17(void) {
   DIGOUT_17_ON;
}
static void Off17(void) {
   DIGOUT_17_OFF;
}

static void On18(void) {
   DIGOUT_18_ON;
}
static void Off18(void) {
   DIGOUT_18_OFF;
}

static void On19(void) {
   DIGOUT_19_ON;
}
static void Off19(void) {
   DIGOUT_19_OFF;
}

static void On20(void) {
   DIGOUT_20_ON;
}
static void Off20(void) {
   DIGOUT_20_OFF;
}

static void On21(void) {
   DIGOUT_21_ON;
}
static void Off21(void) {
   DIGOUT_21_OFF;
}

static void On22(void) {
   DIGOUT_22_ON;
}
static void Off22(void) {
   DIGOUT_22_OFF;
}

static void On23(void) {
   DIGOUT_23_ON;
}
static void Off23(void) {
   DIGOUT_23_OFF;
}

static void On24(void) {
   DIGOUT_24_ON;
}
static void Off24(void) {
   DIGOUT_24_OFF;
}

static void On25(void) {
   DIGOUT_25_ON;
}
static void Off25(void) {
   DIGOUT_25_OFF;
}

static void On26(void) {
   DIGOUT_26_ON;
}
static void Off26(void) {
   DIGOUT_26_OFF;
}

static void On27(void) {
   DIGOUT_27_ON;
}
static void Off27(void) {
   DIGOUT_27_OFF;
}

static void On28(void) {
   DIGOUT_28_ON;
}
static void Off28(void) {
   DIGOUT_28_OFF;
}

static void On29(void) {
   DIGOUT_29_ON;
}
static void Off29(void) {
   DIGOUT_29_OFF;
}

static void On30(void) {
   DIGOUT_30_ON;
}
static void Off30(void) {
   DIGOUT_30_OFF;
}

