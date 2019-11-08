/*
 * button.c
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
#include "button.h"

/*-----------------------------------------------------------------------------
*  Macros  
*/         
/* Anzahl der gleichzeitig gedrückten Taster */
#define BUTTON_MAX_NUM_ACTIVE  5            

/* nach 100ms ohne Bustelegramm wird Taster als geöffnet gewertet */  
#define BUTTON_TIMEOUT         100  /* ms */

/*-----------------------------------------------------------------------------
*  typedefs
*/  
typedef struct {
   uint8_t addr;
   uint8_t buttonNr;
   uint8_t timeStamp;
} TButtonActive;

/*-----------------------------------------------------------------------------
*  Variables
*/      
static bool sActiveButtons = false;   
static TButtonActive sActiveList[BUTTON_MAX_NUM_ACTIVE];

/*-----------------------------------------------------------------------------
*  Functions
*/

void ButtonInit(void) {

   uint8_t i;    
   TButtonActive *pButton;
   
   pButton = &sActiveList[0];
   for (i = 0; i < BUTTON_MAX_NUM_ACTIVE; i++) {
      pButton->addr = 0;
      pButton++;
   }           
}
/*----------------------------------------------------------------------------- 
* Timeoutprüfung der Buttonliste
* retourniert true bei der ab *pIndex gesuchte Taste, bei der ein Timeout 
* aufgetreten ist.
* Über pIndex wird der Index der gefundenen aktiven Taste retourniert.
*/
bool ButtonReleased(uint8_t *pIndex) {
  
   uint8_t       i;  
   TButtonActive *pButton;
   bool          ret = false;
   bool          activeFound = false;
 
   if (sActiveButtons == false) {
      return false;
   }
   i = *pIndex;
   if (i >= BUTTON_MAX_NUM_ACTIVE) {           
      return false;
   }
   pButton = &sActiveList[i];
   for (; i < BUTTON_MAX_NUM_ACTIVE; i++) {
      if (pButton->addr == 0) {
         pButton++;
      } else if (((uint8_t)(GET_TIME_MS - pButton->timeStamp)) > BUTTON_TIMEOUT) { 
         /* Taster losgelassen */
         ret = true;
         break;
      } else { 
         activeFound = true;
         pButton++;
      }
   }           
   
   /* falls die Suche bei 0 begonnen hat und keine aktiven Tasten gefunden wurde */
   /* wird sActiveButtons auf false gesetzt, wegen schnellerem Durchlauf */
   if ((i == BUTTON_MAX_NUM_ACTIVE) &&
       (*pIndex == 0) &&
       (activeFound == false)) {
      sActiveButtons = false;
   }

   if (ret == true) {
      *pIndex = i;    
   }   
     
   return ret;
}
     
/*----------------------------------------------------------------------------- 
* Prüfung auf neuen Tastendruck
* retourniert true bei neuem Tastendruck
*/
bool ButtonNew(uint8_t addr, uint8_t buttonNr) {

   uint8_t       i;  
   TButtonActive *pButton;
   uint8_t       emptyIndex = 0xff;
   bool          found = false;
   
   sActiveButtons = true;
 
   pButton = &sActiveList[0];
   for (i = 0; i < BUTTON_MAX_NUM_ACTIVE; i++) {     
      if ((emptyIndex == 0xff) &&
          (pButton->addr == 0)) {
         emptyIndex = i;
      } else if ((pButton->addr == addr) &&
                 (pButton->buttonNr == buttonNr)) { 
         /* Tastenereignis ist schon bekannt */
         found = true;
         break;
      }
      pButton++;
   }           

   if ((found == false) && 
       (emptyIndex != 0xff)) {
      pButton = &sActiveList[emptyIndex];
      pButton->addr = addr;
      pButton->buttonNr = buttonNr;
   } 

   /* Timestamp eintragen/aktualisieren */
   pButton->timeStamp = GET_TIME_MS;

   return (found == true) ? false : true;
}

/*----------------------------------------------------------------------------- 
* Event auslesen und aus Liste quittieren.
*/
bool ButtonGetReleaseEvent(uint8_t index, TButtonEvent *pEvent) {

   TButtonActive *pButton;

   if (index >= BUTTON_MAX_NUM_ACTIVE) {           
      return false;
   } 
   pButton = &sActiveList[index];
   pEvent->address = pButton->addr;
   pEvent->buttonNr = pButton->buttonNr;
   pEvent->pressed = false;

   /* Quittierung */   
   pButton->addr = 0;
   return true;
}

/*----------------------------------------------------------------------------- 
* alle aktiven timestamps auf aktuelle Zeit setzen
* bei Busempfangsfehler wird angenommen, dass alle gedrückten Taster gedrückt
* geblieben sind
*/
void ButtonTimeStampRefresh(void) {
  
   TButtonActive *pButton;       
   uint8_t i;

   if (sActiveButtons == false) {
      return;
   }
 
   pButton = &sActiveList[0];
   for (i = 0; i < BUTTON_MAX_NUM_ACTIVE; i++) {     
      if (pButton->addr != 0) {
         pButton->timeStamp = GET_TIME_MS;
      }
      pButton++;
   }           
}

