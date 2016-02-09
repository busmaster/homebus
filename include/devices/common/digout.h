/*
 * digout.h
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
#ifndef _DIGOUT_H
#define _DIGOUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*-----------------------------------------------------------------------------
*  Macros
*/                     

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef enum {
   eDigOut0 = 0,
   eDigOut1 = 1,
   eDigOut2 = 2,
   eDigOut3 = 3,
   eDigOut4 = 4,
   eDigOut5 = 5,
   eDigOut6 = 6,
   eDigOut7 = 7,
   eDigOut8 = 8,
   eDigOut9 = 9,
   eDigOut10 = 10,
   eDigOut11 = 11,
   eDigOut12 = 12,
   eDigOut13 = 13,
   eDigOut14 = 14,
   eDigOut15 = 15,
   eDigOut16 = 16,
   eDigOut17 = 17,
   eDigOut18 = 18,
   eDigOut19 = 19,
   eDigOut20 = 20,
   eDigOut21 = 21,
   eDigOut22 = 22,
   eDigOut23 = 23,
   eDigOut24 = 24,
   eDigOut25 = 25,
   eDigOut26 = 26,
   eDigOut27 = 27,
   eDigOut28 = 28,
   eDigOut29 = 29,
   eDigOut30 = 30,
   eDigOutNum = 31,
   eDigOutInvalid = 255
} TDigOutNumber;       

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/
void DigOutInit(void);
void DigOutOn(TDigOutNumber number);
void DigOutOff(TDigOutNumber number);
void DigOutOffAll(void);
bool DigOutState(TDigOutNumber number);  
void DigOutStateAll(uint8_t *pBuf, uint8_t bufLen);
void DigOutStateAllStandard(uint8_t *pBuf, uint8_t bufLen);
void DigOutAll(uint8_t *pBuf, uint8_t bufLen);
void DigOutToggle(TDigOutNumber number);
void DigOutDelayedOn(TDigOutNumber number, uint32_t onDelayMs);
void DigOutDelayedOff(TDigOutNumber number, uint32_t offDelayMs);
void DigOutDelayedOnDelayedOff(TDigOutNumber number, uint32_t onDelayMs, uint32_t offDelayMs);
void DigOutDelayCancel(TDigOutNumber number);
bool DigOutIsDelayed(TDigOutNumber number);
void DigOutTrigger(TDigOutNumber number);
bool DigOutGetShaderFunction(TDigOutNumber number);
bool DigOutSetShaderFunction(TDigOutNumber number);
bool DigOutClearShaderFunction(TDigOutNumber number);
void DigOutStateCheck(void);


#ifdef __cplusplus
}
#endif
    
#endif
