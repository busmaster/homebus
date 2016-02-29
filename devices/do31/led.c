/*
 * led.c
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

#include "sysdef.h"
#include "board.h"
#include "led.h"

/*-----------------------------------------------------------------------------
*  Macros
*/                     
#define FOREVER      65535     
#define FLASH_TIME   1
#define SHORT_CYCLE  250
#define LONG_CYCLE   1500
#define SLOW_BLINK   1500
#define FAST_BLINK   500

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef struct {
   bool   cyclic;
   uint16_t onTime;  /* FOREVER: dauernd EIN, sonst EIN-Zeit in ms */
   uint16_t offTime; /* FOREVER: dauernd AUS, sonst AUS-Zeit in ms */
   uint16_t switchTime;
   bool   state;
} TState;

/*-----------------------------------------------------------------------------
*  Variables
*/   
static TState sLedState[NUM_LED];   /* 0: rote Led, 1: grüne Led */

/*-----------------------------------------------------------------------------
*  Functions
*/   
static void LedOn(uint8_t num);
static void LedOff(uint8_t num);

/*-----------------------------------------------------------------------------
*  Led Initialisierung
*/
void LedInit(void) {

   LedSet(eLedRedOff);
   LedSet(eLedGreenOff);
}

/*-----------------------------------------------------------------------------
*  Led Statemachine
*/
void LedCheck(void) {

   uint16_t currTimeMs;     
   uint8_t  i;                  
   TState *pState;

   GET_TIME_MS16(currTimeMs);

   pState = &sLedState[0];
   for (i = 0; i < NUM_LED; i++) { 
      if ((pState->offTime != FOREVER) &&
          (pState->onTime != FOREVER)) {
         if (pState->state == LED_ON) {
            if (((uint16_t)(currTimeMs - pState->switchTime)) >= pState->onTime) {
               pState->switchTime = currTimeMs;
               LedOff(i);
               if (pState->cyclic == false) {
                  pState->offTime = FOREVER;
               }
            }
         } else {   
            if (((uint16_t)(currTimeMs - pState->switchTime)) >= pState->offTime) {
               pState->switchTime = currTimeMs;
               LedOn(i);
            }
         }
      }
      pState++;
   }
}
            
/*-----------------------------------------------------------------------------
*  Ledzustand ändern
*/               
void LedSet(TLedState state) {   
  
   uint16_t currTimeMs;    
   TState *pState;
   
   GET_TIME_MS16(currTimeMs);
     
   switch (state) {
      case eLedRedOff:
         LedOff(0);
         sLedState[0].offTime = FOREVER;
         break;
      case eLedRedOn:
         LedOn(0);
         sLedState[0].onTime = FOREVER;
         break;
      case eLedGreenOff:
         LedOff(1);
         sLedState[1].offTime = FOREVER;
         break;         
      case eLedGreenOn:
         LedOn(1);
         sLedState[1].onTime = FOREVER;
         break;         
      case eLedRedFlashSlow:
         pState = &sLedState[0];
         pState->onTime = FLASH_TIME;
         pState->offTime = LONG_CYCLE;     
         pState->cyclic = true;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenFlashSlow:
         pState = &sLedState[1];
         pState->onTime = FLASH_TIME;
         pState->offTime = LONG_CYCLE;   
         pState->cyclic = true;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedRedFlashFast:
         pState = &sLedState[0];
         pState->onTime = FLASH_TIME;
         pState->offTime = SHORT_CYCLE;   
         pState->cyclic = true;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenFlashFast:  
         pState = &sLedState[1];
         pState->onTime = FLASH_TIME;
         pState->offTime = SHORT_CYCLE;   
         pState->cyclic = true;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedRedBlinkSlow:
         pState = &sLedState[0];
         pState->onTime = SLOW_BLINK;
         pState->offTime = SLOW_BLINK;   
         pState->cyclic = true;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenBlinkSlow:
         pState = &sLedState[1];
         pState->onTime = SLOW_BLINK;
         pState->offTime = SLOW_BLINK;   
         pState->cyclic = true;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedRedBlinkFast:
         pState = &sLedState[0];
         pState->onTime = FAST_BLINK;
         pState->offTime = FAST_BLINK;   
         pState->cyclic = true;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenBlinkFast:
         pState = &sLedState[1];
         pState->onTime = FAST_BLINK;
         pState->offTime = FAST_BLINK;   
         pState->cyclic = true;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedRedBlinkOnceShort:
         pState = &sLedState[0];
         pState->onTime = FAST_BLINK;
         pState->offTime = 0;   
         pState->cyclic = false;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedRedBlinkOnceLong:
         pState = &sLedState[0];
         pState->onTime = SLOW_BLINK;
         pState->offTime = 0;   
         pState->cyclic = false;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenBlinkOnceShort:
         pState = &sLedState[1];
         pState->onTime = FAST_BLINK;
         pState->offTime = 0;   
         pState->cyclic = false;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedGreenBlinkOnceLong:
         pState = &sLedState[1];
         pState->onTime = SLOW_BLINK;
         pState->offTime = 0;   
         pState->cyclic = false;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      default:
         break;
   }
}

/*-----------------------------------------------------------------------------
*  Led einschalten
*/               
static void LedOn(uint8_t num) {

   sLedState[num].state = LED_ON;

   if (num == 0) {
      LED1_ON;
   } else {
      LED2_ON;
   }
}

/*-----------------------------------------------------------------------------
*  Led abschalten
*/               
static void LedOff(uint8_t num) {

   sLedState[num].state = LED_OFF;

   if (num == 0) {
      LED1_OFF;
   } else {
      LED2_OFF;
   }
}

