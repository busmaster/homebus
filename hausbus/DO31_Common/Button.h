/*-----------------------------------------------------------------------------
*  Button.h
*/
#ifndef _BUTTON_H
#define _BUTTON_H

/*-----------------------------------------------------------------------------
*  Includes
*/
#include "Types.h"

/*-----------------------------------------------------------------------------
*  Macros
*/                     

/*-----------------------------------------------------------------------------
*  typedefs
*/

typedef struct {
   UINT8  address;
   UINT8  buttonNr;                    
   BOOL   pressed;             
} TButtonEvent;

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/
BOOL ButtonReleased(UINT8 *pIndex);
BOOL ButtonNew(UINT8 addr, UINT8 buttonNr);
BOOL ButtonGetReleaseEvent(UINT8 index, TButtonEvent *pEvent);
void ButtonTimeStampRefresh(void);  
void ButtonInit(void);
  
#endif
