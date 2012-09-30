/*-----------------------------------------------------------------------------
*  Flash.h
*/

#ifndef _FLASH_H
#define _FLASH_H

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

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/
void     FlashInit(void);
void     FlashErase(void);
bool     FlashProgram(uint16_t wordAddr, uint16_t *pBuf, uint16_t bufWordSize);
bool     FlashProgramTerminate(void);
uint16_t FlashSum(uint16_t wordAddr, uint8_t numWords);

#ifdef __cplusplus
}
#endif  
#endif


