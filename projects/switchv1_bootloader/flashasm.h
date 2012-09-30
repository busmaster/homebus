/*-----------------------------------------------------------------------------
*  Flash.h
*/

#ifndef _FLASHASM_H
#define _FLASHASM_H

#ifdef __cplusplus
extern "C" {
#endif
/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdint.h>

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
void FlashFillPagePuffer(uint16_t offset, uint16_t *pBuf, uint8_t numWords);
void FlashProgramPagePuffer(uint16_t pageWordAddr);
void FlashPageErase(uint16_t pageWordAddr);

#ifdef __cplusplus
}
#endif
#endif
