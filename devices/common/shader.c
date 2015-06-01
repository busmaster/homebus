/*
 * shader.c
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

#include "sysdef.h"
#include "digout.h"
#include "shader.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
/* Zeitdauer, um die dirSwitch und onSwitch verzögert geschaltet werden */
/* (dirSwitch und onSwitch schalten um diese Diff verzögert) */
#define SHADER_DELAY_DIRCHANGE     20  /* 10 ms */

/* kein gleichzeitiger schalten der Relais */
#define SHADER_DELAY_RELAY         2  /* 10 ms */

// positions for SetAction
#define SHADER_OPEN_COMPLETELY     100
#define SHADER_CLOSE_COMPLETELY    0

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef enum {
   eStateStopped,  
   eStateExit,
   eStateOpenInit,  
   eStateOpening,
   eStateCloseInit,
   eStateClosing,
   eStateDirectionChangeOpenInit,
   eStateDirectionChangeCloseInit
} TShaderInternalState;

typedef enum {
   eCmdStop,
   eCmdSetPosition,
   eCmdNone
} TShaderCmd;

typedef struct {
   TDigOutNumber           onSwitch;
   TDigOutNumber           dirSwitch;
   uint16_t                openDuration;
   uint16_t                closeDuration;
   uint16_t                actionTimestamp;
   uint8_t                 setPosition;    /* sollwert 0..100 */
   uint8_t                 actualPosition; /* istwert 0..100 */
   uint8_t                 startingPosition;
   TShaderInternalState    state;
   TShaderCmd              cmd;
} TShaderDesc;

/*-----------------------------------------------------------------------------
*  Variables
*/
static TShaderDesc sShader[eShaderNum];

/*-----------------------------------------------------------------------------
* Init
*/
void ShaderInit(void) {

   uint8_t i;
   TShaderDesc  *pShader = sShader;

   for (i = 0; i < ARRAY_CNT(sShader); i++) {
      pShader->onSwitch = eDigOutInvalid;
      pShader->dirSwitch = eDigOutInvalid;
      pShader++;
   }
}

/*-----------------------------------------------------------------------------
*  Zuordnung der Ausgänge für einen Rollladen
*  onSwitch schaltet das Stromversorgungsrelais
*  dirSwitch schaltet das Richtungsrelais
*/
void ShaderSetConfig(TShaderNumber number,
                     TDigOutNumber onSwitch,
                     TDigOutNumber dirSwitch,
                     uint32_t      openDurationMs,
                     uint32_t      closeDurationMs
   ) {
      
   TShaderDesc  *pShader;

   if (number >= eShaderNum) {
      return;
   }

   if (((openDurationMs / 10) > UINT16_MAX) ||
       ((closeDurationMs / 10) > UINT16_MAX)) {
      return;
   }


   DigOutSetShaderFunction(onSwitch);
   DigOutSetShaderFunction(dirSwitch);

   pShader = &sShader[number];
   pShader->onSwitch = onSwitch;
   pShader->dirSwitch = dirSwitch;
   pShader->actualPosition = 50; /* default: middle position */
   pShader->startingPosition = 50;
   pShader->openDuration = openDurationMs / 10; /* in 10 ms */
   pShader->closeDuration = closeDurationMs / 10;  /* in 10 ms */
   pShader->setPosition = 50; /* default: shader is completely open */
}

/*-----------------------------------------------------------------------------
*  Zuordnung der Ausgänge für einen Rollladen lesen
*/
void ShaderGetConfig(TShaderNumber number, TDigOutNumber *pOnSwitch, TDigOutNumber *pDirSwitch) {

   *pOnSwitch = sShader[number].onSwitch;
   *pDirSwitch = sShader[number].dirSwitch;
}


/*-----------------------------------------------------------------------------
*  Ausgang für Rollladen einschalten
*  Schaltet sich nach 30 s automatisch ab
*/
void ShaderSetAction(TShaderNumber number, TShaderAction action) {

   TShaderDesc *pShader;

   if (number >= eShaderNum) {
      return;
   }
   pShader = &sShader[number];
   switch (action) {
      case eShaderOpen:
         ShaderSetPosition(number, SHADER_OPEN_COMPLETELY);
         break;
      case eShaderClose:
         ShaderSetPosition(number, SHADER_CLOSE_COMPLETELY);
         break;
      case eShaderStop:
         pShader->cmd = eCmdStop;
         break;
      default:
         break;
   }
}


/*-----------------------------------------------------------------------------
*  Ermittlung, ob Rollladenausgang aktiv (fährt gerade AUF oder ZU)
*  Wird verwendet, um bei Tastenbetätigung zwischen AUF/ZU und STOP unter-
*  scheiden zu können
*/
bool ShaderGetState(TShaderNumber number, TShaderState *pState) {

   TShaderDesc *pShader;

   if (number >= eShaderNum) {
      return false;
   }
   pShader = &sShader[number];
   if ((pShader->onSwitch == eDigOutInvalid) ||
       (pShader->dirSwitch == eDigOutInvalid)) {
      return false;
   }
   
   switch (pShader->state) {
   case eStateOpening:
      *pState = eShaderOpening;
      break;
   case eStateClosing:
      *pState = eShaderClosing;
      break;
   default:
      *pState = eShaderStopped;
      break;
   }   
   return true;
}


/*-----------------------------------------------------------------------------
*  set shutter position
*/
bool ShaderSetPosition(TShaderNumber number, uint8_t position) {

   TShaderDesc *pShader;

   if (number >= eShaderNum) {
      return false;
   }
   pShader = &sShader[number];
   if ((pShader->onSwitch == eDigOutInvalid) ||
       (pShader->dirSwitch == eDigOutInvalid)) {
      return false;
   }
   if (position > 100) {
      return false;
   }
   pShader->cmd = eCmdSetPosition;
   pShader->setPosition = position;

   return true;
}

/*-----------------------------------------------------------------------------
*  Get position of shader
*/
bool ShaderGetPosition(TShaderNumber number, uint8_t *pPosition) {

   TShaderDesc *pShader;
   
   if (number >= eShaderNum) {
      return false;
   }
   pShader = &sShader[number];
   if ((pShader->onSwitch == eDigOutInvalid) ||
       (pShader->dirSwitch == eDigOutInvalid)) {
      return false;
   }
   pShader = &sShader[number];
   *pPosition = pShader->actualPosition;

   return true;
}

void ShaderCheck(void) {

   static uint8_t       sShaderNum = 0;   
   TShaderDesc          *pShader;
   TShaderCmd           cmd;
   TShaderCmd           nextCmd;
   TShaderInternalState nextState;
   uint16_t             currentTime;
   uint32_t             actionPeriod;

   sShaderNum++;
   sShaderNum %= eShaderNum;
   pShader = &sShader[sShaderNum];
   if ((pShader->onSwitch == eDigOutInvalid) ||
       (pShader->dirSwitch == eDigOutInvalid)) {
      return;
   }

   cmd = pShader->cmd;
   nextState = pShader->state; // default: the next state is the current one
   nextCmd = cmd;

   GET_TIME_10MS16(currentTime);
   actionPeriod = currentTime - pShader->actionTimestamp;
   switch (pShader->state) {
   case eStateStopped:
      switch (cmd) {
      case eCmdSetPosition:
         if (pShader->setPosition < pShader->actualPosition) {
            DigOutOff(pShader->dirSwitch);            
            nextState = eStateCloseInit;
         } else if (pShader->setPosition > pShader->actualPosition) {
            DigOutOn(pShader->dirSwitch);            
            nextState = eStateOpenInit;
         }
         GET_TIME_10MS16(pShader->actionTimestamp);
         break;
      default:
         break;
      }
      // eCmdStop and eCmdNone are ignored
      nextCmd = eCmdNone;
      break;
   case eStateExit:
      if (actionPeriod >= SHADER_DELAY_RELAY) {
         DigOutOff(pShader->dirSwitch);
         nextState = eStateStopped;
         GET_TIME_10MS16(pShader->actionTimestamp);            
      }
      break;
   case eStateOpenInit:
      if (actionPeriod >= SHADER_DELAY_RELAY) {
         DigOutOn(pShader->onSwitch);
         nextState = eStateOpening;
         pShader->startingPosition = pShader->actualPosition;
         GET_TIME_10MS16(pShader->actionTimestamp);            
      }
      break;
   case eStateOpening:
      if ((actionPeriod * 100 / pShader->openDuration) > (100 - pShader->startingPosition)) {
         pShader->actualPosition = 100;
      } else {
         pShader->actualPosition = pShader->startingPosition + actionPeriod * 100 / pShader->openDuration;
      }
      switch (cmd) {
      case eCmdNone:
         if (pShader->setPosition == SHADER_OPEN_COMPLETELY) {
            if (actionPeriod >= ((uint32_t)pShader->openDuration * 110 / 100 /*  duration + 10% */)) {
               DigOutOff(pShader->onSwitch);
               nextState = eStateExit;
               GET_TIME_10MS16(pShader->actionTimestamp);
            }
         } else if (pShader->actualPosition >= pShader->setPosition) {
            DigOutOff(pShader->onSwitch);
            nextState = eStateExit;
            GET_TIME_10MS16(pShader->actionTimestamp);
         }
         break;
     case eCmdStop:
         DigOutOff(pShader->onSwitch);
         nextState = eStateExit;
         GET_TIME_10MS16(pShader->actionTimestamp);
         break;
     case eCmdSetPosition:
         if (pShader->setPosition < pShader->actualPosition) {
            DigOutOff(pShader->onSwitch);
            nextState = eStateDirectionChangeCloseInit;
            GET_TIME_10MS16(pShader->actionTimestamp);
         }
         break;
     default:
       break;
      }
      nextCmd = eCmdNone;
      break;
   case eStateCloseInit:
      if (actionPeriod >= SHADER_DELAY_RELAY) {
         DigOutOn(pShader->onSwitch);
         nextState = eStateClosing;
         pShader->startingPosition = pShader->actualPosition;
         GET_TIME_10MS16(pShader->actionTimestamp);            
      }
      break;
   case eStateClosing:
      if ((actionPeriod * 100 / pShader->closeDuration) > (pShader->startingPosition)) {
         pShader->actualPosition = 0;
      } else {
         pShader->actualPosition = pShader->startingPosition - actionPeriod * 100 / pShader->closeDuration;
      }
      switch (cmd) {
      case eCmdNone:
         if (pShader->setPosition == SHADER_CLOSE_COMPLETELY) {
            if (actionPeriod >= ((uint32_t)pShader->closeDuration * 110 / 100 /*  duration + 10% */)) {
               DigOutOff(pShader->onSwitch);
               nextState = eStateExit;
               GET_TIME_10MS16(pShader->actionTimestamp);
            }
         } else if (pShader->actualPosition <= pShader->setPosition) {
            DigOutOff(pShader->onSwitch);
            nextState = eStateExit;
            GET_TIME_10MS16(pShader->actionTimestamp);
         }
         break;
      case eCmdStop:
         DigOutOff(pShader->onSwitch);
         nextState = eStateExit;
         GET_TIME_10MS16(pShader->actionTimestamp);
         break;
      case eCmdSetPosition:
         if (pShader->setPosition > pShader->actualPosition) {
            DigOutOff(pShader->onSwitch);
            nextState = eStateDirectionChangeOpenInit;
            GET_TIME_10MS16(pShader->actionTimestamp);
         }
         break;
      default:
        break;
      }
      nextCmd = eCmdNone;
      break;
   case eStateDirectionChangeCloseInit:
     switch (cmd) {
     case eCmdNone:
         if (actionPeriod >= SHADER_DELAY_DIRCHANGE) {
            DigOutOff(pShader->dirSwitch);            
            nextState = eStateCloseInit;
            GET_TIME_10MS16(pShader->actionTimestamp);  
         }
         break;
     case eCmdStop:
         DigOutOff(pShader->onSwitch);
         nextState = eStateExit;
         GET_TIME_10MS16(pShader->actionTimestamp);
         break;
     case eCmdSetPosition:
         if (pShader->setPosition > pShader->actualPosition) {
            // no direction change, continue in same direction
            DigOutOn(pShader->dirSwitch);
            nextState = eStateOpenInit;
            GET_TIME_10MS16(pShader->actionTimestamp);
         }
         break;
     default:
       break;
      }
      nextCmd = eCmdNone;
      break;
   case eStateDirectionChangeOpenInit:
     switch (cmd) {
     case eCmdNone:
         if (actionPeriod >= SHADER_DELAY_DIRCHANGE) {
            DigOutOn(pShader->dirSwitch);            
            nextState = eStateOpenInit;
            GET_TIME_10MS16(pShader->actionTimestamp);           
         }
         break;
     case eCmdStop:
         DigOutOff(pShader->onSwitch);
         nextState = eStateExit;
         GET_TIME_10MS16(pShader->actionTimestamp);
         break;
     case eCmdSetPosition:
         if (pShader->setPosition < pShader->actualPosition) {
            // no direction change, continue in same direction
            DigOutOff(pShader->dirSwitch);
            nextState = eStateCloseInit;
            GET_TIME_10MS16(pShader->actionTimestamp);
         }
         break;
     default:
       break;
      }
      nextCmd = eCmdNone; 
      break;
   default:
      break;
   }
   pShader->cmd = nextCmd;
   pShader->state = nextState;
}

