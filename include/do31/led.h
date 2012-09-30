/*-----------------------------------------------------------------------------
*  Led.h
*/

#ifndef _LED_H
#define _LED_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
*  Includes
*/

/*-----------------------------------------------------------------------------
*  Macros
*/                     

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef enum {
   eLedRedOff, 
   eLedRedOn,
   eLedGreenOff,
   eLedGreenOn,
   
   eLedRedFlashSlow,
   eLedGreenFlashSlow,
   eLedRedFlashFast,
   eLedGreenFlashFast,
   
   eLedRedBlinkSlow,
   eLedGreenBlinkSlow,
   eLedRedBlinkFast,
   eLedGreenBlinkFast,

   eLedRedBlinkOnceShort,
   eLedRedBlinkOnceLong,
   eLedGreenBlinkOnceShort,
   eLedGreenBlinkOnceLong
} TLedState;       

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/
void  LedInit(void);
void  LedCheck(void);
void  LedSet(TLedState state);
  
#endif


