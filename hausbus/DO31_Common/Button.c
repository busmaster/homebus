/*-----------------------------------------------------------------------------
*  Button.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include "Types.h"
#include "Sysdef.h"
#include "Board.h"
#include "Button.h"

/*-----------------------------------------------------------------------------
*  Macros  
*/         
/* Anzahl der gleichzeitig gedrückten Taster */
#define BUTTON_MAX_NUM_ACTIVE  5            

/* nach 100ms ohne Bustelegramm wird Taster als geöffnet gewertet */  
#define BUTTON_TIMEOUT         60  /* ms */

/*-----------------------------------------------------------------------------
*  typedefs
*/  
typedef struct {
   UINT8 addr;
   UINT8 buttonNr;
   UINT8 timeStamp;
} TButtonActive;

/*-----------------------------------------------------------------------------
*  Variables
*/      
static BOOL sActiveButtons = FALSE;   
static TButtonActive sActiveList[BUTTON_MAX_NUM_ACTIVE];

/*-----------------------------------------------------------------------------
*  Functions
*/

void ButtonInit(void) {

   UINT8 i;    
   TButtonActive *pButton;
   
   pButton = &sActiveList[0];
   for (i = 0; i < BUTTON_MAX_NUM_ACTIVE; i++) {
      pButton->addr = 0;
      pButton++;
   }           
}


/*----------------------------------------------------------------------------- 
* Timeoutprüfung der Buttonliste
* retourniert TRUE bei der ab *pIndex gesuchte Taste, bei der ein Timeout 
* aufgetreten ist.
* Über pIndex wird der Index der gefundenen aktiven Taste retourniert.
*/
BOOL ButtonReleased(UINT8 *pIndex) {
  
   UINT8         i;  
   TButtonActive *pButton;
   BOOL          ret = FALSE;
   BOOL          activeFound = FALSE;
 
   if (sActiveButtons == FALSE) {
      return FALSE;
   }    
   i = *pIndex;
   if (i >= BUTTON_MAX_NUM_ACTIVE) {           
      return FALSE;
   }
   pButton = &sActiveList[i];
   for (; i < BUTTON_MAX_NUM_ACTIVE; i++) {
      if (pButton->addr == 0) {
         pButton++;
      } else if (((UINT8)(GET_TIME_MS - pButton->timeStamp)) > BUTTON_TIMEOUT) { 
         /* Taster losgelassen */
         ret = TRUE;
         break;
      } else { 
         activeFound = TRUE;
         pButton++;
      }
   }           
   
   /* falls die Suche bei 0 begonnen hat und keine aktiven Tasten gefunden wurde */
   /* wird sActiveButtons auf FALSE gesetzt, wegen schnellerem Durchlauf */
   if ((i == BUTTON_MAX_NUM_ACTIVE) &&
       (*pIndex == 0)               &&
       (activeFound == FALSE)) {
      sActiveButtons = FALSE;
   } 
   
   if (ret == TRUE) {
      *pIndex = i;
   }
     
   return ret;
}
     
/*----------------------------------------------------------------------------- 
* Prüfung auf neuen Tastendruck
* retourniert TRUE bei neuem Tastendruck
*/
BOOL ButtonNew(UINT8 addr, UINT8 buttonNr) {
  
   UINT8         i;  
   TButtonActive *pButton;
   UINT8         emptyIndex = 0xff;
   BOOL          found = FALSE;
   
   sActiveButtons = TRUE;
 
   pButton = &sActiveList[0];
   for (i = 0; i < BUTTON_MAX_NUM_ACTIVE; i++) {     
      if ((emptyIndex == 0xff) &&
          (pButton->addr == 0)) {
         emptyIndex = i;
      } else if ((pButton->addr == addr) &&
                 (pButton->buttonNr == buttonNr)) { 
         /* Tastenereignis ist schon bekannt */
         found = TRUE;
         break;
      }
      pButton++;
   }           

   if ((found == FALSE) &&
       (emptyIndex != 0xff)) {
      pButton = &sActiveList[emptyIndex];
      pButton->addr = addr;
      pButton->buttonNr = buttonNr;
   } 

   /* Timestamp eintragen/aktualisieren */
   pButton->timeStamp = GET_TIME_MS;

   return (found == TRUE) ? FALSE : TRUE;
}

/*----------------------------------------------------------------------------- 
* Event auslesen und aus Liste quittieren.
*/
BOOL ButtonGetReleaseEvent(UINT8 index, TButtonEvent *pEvent) {

   TButtonActive *pButton;

   if (index >= BUTTON_MAX_NUM_ACTIVE) {           
      return FALSE;
   } 
   pButton = &sActiveList[index];
   pEvent->address = pButton->addr;
   pEvent->buttonNr = pButton->buttonNr;
   pEvent->pressed = FALSE;

   /* Quittierung */   
   pButton->addr = 0;
   return TRUE;
}

/*----------------------------------------------------------------------------- 
* alle aktiven timestamps auf aktuelle Zeit setzen
* bei Busempfangsfehler wird angenommen, dass alle gedrückten Taster gedrückt
* geblieben sind
*/
void ButtonTimeStampRefresh(void) {
  
   TButtonActive *pButton;       
   UINT8 i;

   if (sActiveButtons == FALSE) {
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

