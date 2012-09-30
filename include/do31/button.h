/*-----------------------------------------------------------------------------
*  Button.h
*/
#ifndef _BUTTON_H
#define _BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif
/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdint.h>
#include <stdbool.h>

/*-----------------------------------------------------------------------------
*  Macros
*/                     

/*-----------------------------------------------------------------------------
*  typedefs
*/

typedef struct {
   uint8_t  address;
   uint8_t  buttonNr;                    
   bool   pressed;             
} TButtonEvent;

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/
bool ButtonReleased(uint8_t *pIndex);
bool ButtonNew(uint8_t addr, uint8_t buttonNr);
bool ButtonGetReleaseEvent(uint8_t index, TButtonEvent *pEvent);
void ButtonTimeStampRefresh(void);  
void ButtonInit(void);

#ifdef __cplusplus
}
#endif  
#endif
