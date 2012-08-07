/*-----------------------------------------------------------------------------
*  Flash.h
*/

#ifndef _FLASH_H
#define _FLASH_H

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

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/
void  FlashInit(void);
void  FlashErase(void);
BOOL  FlashProgram(UINT16 wordAddr, UINT16 *pBuf, UINT16 bufWordSize);
BOOL  FlashProgramTerminate(void);
UINT16 FlashSum(UINT16 wordAddr, UINT8 numWords);
  
#endif


