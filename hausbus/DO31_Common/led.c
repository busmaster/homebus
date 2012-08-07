/*-----------------------------------------------------------------------------
*  Led.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include "Types.h"
#include "Sysdef.h"
#include "Board.h"
#include "Led.h"

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
   BOOL   cyclic;
   UINT16 onTime;  /* FOREVER: dauernd EIN, sonst EIN-Zeit in ms */
   UINT16 offTime; /* FOREVER: dauernd AUS, sonst AUS-Zeit in ms */
   UINT16 switchTime;
   BOOL   state;
} TState;

/*-----------------------------------------------------------------------------
*  Variables
*/   
static TState sLedState[NUM_LED];   /* 0: rote Led, 1: grüne Led */

/*-----------------------------------------------------------------------------
*  Functions
*/   
static void LedOn(UINT8 num);
static void LedOff(UINT8 num);

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

   UINT16 currTimeMs;     
   UINT8  i;                  
   TState *pState;

   GET_TIME_MS16(currTimeMs);

   pState = &sLedState[0];
   for (i = 0; i < NUM_LED; i++) { 
      if ((pState->offTime != FOREVER) &&
          (pState->onTime != FOREVER)) {
         if (pState->state == LED_ON) {
            if (((UINT16)(currTimeMs - pState->switchTime)) >= pState->onTime) {
               pState->switchTime = currTimeMs;
               LedOff(i);
               if (pState->cyclic == FALSE) {
                  pState->offTime = FOREVER;
               }
            }
         } else {   
            if (((UINT16)(currTimeMs - pState->switchTime)) >= pState->offTime) {
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
  
   UINT16 currTimeMs;    
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
         pState->cyclic = TRUE;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenFlashSlow:
         pState = &sLedState[1];
         pState->onTime = FLASH_TIME;
         pState->offTime = LONG_CYCLE;   
         pState->cyclic = TRUE;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedRedFlashFast:
         pState = &sLedState[0];
         pState->onTime = FLASH_TIME;
         pState->offTime = SHORT_CYCLE;   
         pState->cyclic = TRUE;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenFlashFast:  
         pState = &sLedState[1];
         pState->onTime = FLASH_TIME;
         pState->offTime = SHORT_CYCLE;   
         pState->cyclic = TRUE;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedRedBlinkSlow:
         pState = &sLedState[0];
         pState->onTime = SLOW_BLINK;
         pState->offTime = SLOW_BLINK;   
         pState->cyclic = TRUE;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenBlinkSlow:
         pState = &sLedState[1];
         pState->onTime = SLOW_BLINK;
         pState->offTime = SLOW_BLINK;   
         pState->cyclic = TRUE;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedRedBlinkFast:
         pState = &sLedState[0];
         pState->onTime = FAST_BLINK;
         pState->offTime = FAST_BLINK;   
         pState->cyclic = TRUE;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenBlinkFast:
         pState = &sLedState[1];
         pState->onTime = FAST_BLINK;
         pState->offTime = FAST_BLINK;   
         pState->cyclic = TRUE;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedRedBlinkOnceShort:
         pState = &sLedState[0];
         pState->onTime = FAST_BLINK;
         pState->offTime = 0;   
         pState->cyclic = FALSE;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedRedBlinkOnceLong:
         pState = &sLedState[0];
         pState->onTime = SLOW_BLINK;
         pState->offTime = 0;   
         pState->cyclic = FALSE;
         pState->switchTime = currTimeMs;
         LedOn(0);
         break;
      case eLedGreenBlinkOnceShort:
         pState = &sLedState[1];
         pState->onTime = FAST_BLINK;
         pState->offTime = 0;   
         pState->cyclic = FALSE;
         pState->switchTime = currTimeMs;
         LedOn(1);
         break;
      case eLedGreenBlinkOnceLong:
         pState = &sLedState[1];
         pState->onTime = SLOW_BLINK;
         pState->offTime = 0;   
         pState->cyclic = FALSE;
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
static void LedOn(UINT8 num) {

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
static void LedOff(UINT8 num) {

   sLedState[num].state = LED_OFF;

   if (num == 0) {
      LED1_OFF;
   } else {
      LED2_OFF;
   }
}

