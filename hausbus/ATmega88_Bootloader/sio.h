/*-----------------------------------------------------------------------------
*  Sio.h
*/
#ifndef _SIO_H
#define _SIO_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
*  Includes
*/
#include "Types.h"
#include "sysDef.h"

/*-----------------------------------------------------------------------------
*  Macros
*/                     

/*-----------------------------------------------------------------------------
*  typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/

void  SioInit(void);
void  SioWrite(UINT8 ch);    
UINT8 SioRead(UINT8 *pCh);
void  SioReadFlush(void);
void  SioWriteReady(void);

#ifdef __cplusplus
}
#endif
  
#endif
