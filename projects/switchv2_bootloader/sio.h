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
#include <stdint.h>
#include <sysdef.h>

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
void  SioWrite(uint8_t ch);    
uint8_t SioRead(uint8_t *pCh);
void  SioReadFlush(void);
void  SioWriteReady(void);

#ifdef __cplusplus
}
#endif
  
#endif
