/*
* shader.h
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
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
* MA 02110-1301, USA.
*
*
*/
#ifndef _SHADER_H
#define _SHADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "digout.h"

/*-----------------------------------------------------------------------------
* Macros
*/

/*-----------------------------------------------------------------------------
* typedefs
*/
typedef enum {
   eShader0 = 0,
   eShader1 = 1,
   eShader2 = 2,
   eShader3 = 3,
   eShader4 = 4,
   eShader5 = 5,
   eShader6 = 6,
   eShader7 = 7,
   eShader8 = 8,
   eShader9 = 9,
   eShader10 = 10,
   eShader11 = 11,
   eShader12 = 12,
   eShader13 = 13,
   eShader14 = 14,
   eShaderNum = 15,
   eShaderInvalid = 255
} TShaderNumber;

typedef enum {
   eShaderClose,
   eShaderOpen,
   eShaderStop,
   eShaderNone
} TShaderAction;

typedef enum {
   eShaderClosing,
   eShaderOpening,
   eShaderStopped
} TShaderState;

typedef enum {
   eStateOpen = 0,
   eStateClose = 1,
} TShaderLastAction;
/*-----------------------------------------------------------------------------
* Variables
*/

/*-----------------------------------------------------------------------------
* Functions
*/
void ShaderInit(void);
void ShaderSetConfig(TShaderNumber number, TDigOutNumber onSwitch,
                       TDigOutNumber dirSwitch,
                       uint16_t openDurationMs, uint16_t closeDurationMs);
void ShaderGetConfig(TShaderNumber number, TDigOutNumber *pOnSwitch, TDigOutNumber *pDirSwitch);
void ShaderSetAction(TShaderNumber number, TShaderAction action);
bool ShaderGetState(TShaderNumber number, TShaderState *pState);
bool ShaderSetPosition(TShaderNumber number, uint8_t targetPosition);
bool ShaderSetHandPosition(TShaderNumber number, uint8_t targetPosition);
bool ShaderGetPosition(TShaderNumber number, uint8_t *pPosition);
void ShaderCheck(void);
bool ShaderGetLastAction(TShaderNumber number, TShaderLastAction *pState);


#ifdef __cplusplus
}
#endif
#endif