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
#include "digout.h"

#define NUM_DIGOUT 31

typedef struct {
   bool digoutState; 
   bool shaderFunction;
} TDigoutDesc;

static TDigoutDesc sState[eDigOutNum];


/*-----------------------------------------------------------------------------
*  init
*/
void DigOutInit(void) {
 
   uint8_t i;
   TDigoutDesc *pState;

   pState = sState;
   for (i = 0; i < NUM_DIGOUT; i++) {
      pState->digoutState = false;
      pState->shaderFunction = false;
      pState++;
   }                   
}

/*-----------------------------------------------------------------------------
*  special shader function for digout
*/
bool DigOutSetShaderFunction(TDigOutNumber number) {

   if (number < NUM_DIGOUT) {
      sState[number].shaderFunction = true;
      return true;
    } else {
      return false;
   }
}

/*-----------------------------------------------------------------------------
*  Ausgang einschalten
*/
void DigOutOn(TDigOutNumber number) {

   if (number < NUM_DIGOUT) {
      sState[number].digoutState = true;
   }
}

/*-----------------------------------------------------------------------------
*  Ausgang ausschalten
*/
void DigOutOff(TDigOutNumber number) {

   if (number < NUM_DIGOUT) {
      sState[number].digoutState = false;
   }
}

