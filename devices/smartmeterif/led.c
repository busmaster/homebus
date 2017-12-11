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

#define LED_ON       1
#define LED_OFF      0

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef struct {
   bool     cyclic;
   uint16_t onTime;  /* FOREVER: always on, else on time in ms */
   uint16_t offTime; /* FOREVER: always off, else off time in ms */
   uint16_t switchTime;
   bool     state;
} TState;

/*-----------------------------------------------------------------------------
*  Variables
*/
static TState sLedState;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void LedOn(void);
static void LedOff(void);

/*-----------------------------------------------------------------------------
*  LED init
*/
void LedInit(void) {

   LedSet(eLedGreenOff);
}

/*-----------------------------------------------------------------------------
*  LED state machine
*/
void LedCheck(void) {

    uint16_t currTimeMs;
    TState *pState = &sLedState;

    GET_TIME_MS16(currTimeMs);

    if ((pState->offTime != FOREVER) &&
        (pState->onTime != FOREVER)) {
        if (pState->state == LED_ON) {
            if (((uint16_t)(currTimeMs - pState->switchTime)) >= pState->onTime) {
                pState->switchTime = currTimeMs;
                LedOff();
                if (pState->cyclic == false) {
                    pState->offTime = FOREVER;
                }
            }
        } else {
            if (((uint16_t)(currTimeMs - pState->switchTime)) >= pState->offTime) {
                pState->switchTime = currTimeMs;
                LedOn();
            }
        }
    }
}

/*-----------------------------------------------------------------------------
*  set LED state
*/
void LedSet(TLedState state) {

    uint16_t currTimeMs;
    TState *pState = &sLedState;

    GET_TIME_MS16(currTimeMs);

    switch (state) {
    case eLedGreenOff:
        LedOff();
        pState->offTime = FOREVER;
        break;
    case eLedGreenOn:
        LedOn();
        pState->onTime = FOREVER;
        break;
    case eLedGreenFlashSlow:
        pState->onTime = FLASH_TIME;
        pState->offTime = LONG_CYCLE;
        pState->cyclic = true;
        pState->switchTime = currTimeMs;
        LedOn();
        break;
    case eLedGreenFlashFast:
        pState->onTime = FLASH_TIME;
        pState->offTime = SHORT_CYCLE;
        pState->cyclic = true;
        pState->switchTime = currTimeMs;
        LedOn();
        break;
    case eLedGreenBlinkSlow:
        pState->onTime = SLOW_BLINK;
        pState->offTime = SLOW_BLINK;
        pState->cyclic = true;
        pState->switchTime = currTimeMs;
        LedOn();
        break;
    case eLedGreenBlinkFast:
        pState->onTime = FAST_BLINK;
        pState->offTime = FAST_BLINK;
        pState->cyclic = true;
        pState->switchTime = currTimeMs;
        LedOn();
        break;
    case eLedGreenBlinkOnceShort:
        pState->onTime = FAST_BLINK;
        pState->offTime = 0;
        pState->cyclic = false;
        pState->switchTime = currTimeMs;
        LedOn();
        break;
    case eLedGreenBlinkOnceLong:
        pState->onTime = SLOW_BLINK;
        pState->offTime = 0;
        pState->cyclic = false;
        pState->switchTime = currTimeMs;
        LedOn();
        break;
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------
*  LED on
*/
static void LedOn(void) {

    sLedState.state = LED_ON;
    LED1_ON;
}

/*-----------------------------------------------------------------------------
*  LED off
*/
static void LedOff(void) {

    sLedState.state = LED_OFF;
    LED1_OFF;
}
