/*-----------------------------------------------------------------------------
*  flash.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/

#include <stdint.h>
#include <stdbool.h>

#include "sysdef.h"
#include "flash.h"
#include "flashasm.h"

/*-----------------------------------------------------------------------------
*  Macros
*/        
#define FIRMWARE_WORD_SIZE  (6UL * 1024UL / 2)  /* 6 KByte */
#define PAGE_WORD_SIZE      32UL

#define INV_PAGE  0xffff

/*-----------------------------------------------------------------------------
*  typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/                                
static uint16_t sActualProgramingPage = INV_PAGE;

/*-----------------------------------------------------------------------------
*  Functions
*/
   
/*-----------------------------------------------------------------------------
*  Flash löschen (Applikationsbereich)
*/
void FlashErase(void) {
  
   uint16_t pageWordAddr;
   int flags;

   flags = DISABLE_INT;

   for (pageWordAddr = 0; pageWordAddr < FIRMWARE_WORD_SIZE; pageWordAddr += PAGE_WORD_SIZE) {
      FlashPageErase(pageWordAddr); 
   }
   RESTORE_INT(flags);
}

/*-----------------------------------------------------------------------------
*  Flash programmieren
*/               
bool FlashProgram(uint16_t wordAddr, uint16_t *pBuf, uint16_t numWords) {
   
   uint16_t page;
   uint16_t startPage;
   uint16_t endPage; 
   uint16_t wordOffset; /* wordoffset innerhalb page */ 
   uint8_t  numPageWords;
   int flags;
   
   if ((wordAddr + numWords) > FIRMWARE_WORD_SIZE) {
      return false;
   }

   flags = DISABLE_INT;
   
   startPage = wordAddr & ~(PAGE_WORD_SIZE - 1);
   endPage = (wordAddr + numWords - 1) & ~(PAGE_WORD_SIZE - 1);
   wordOffset = wordAddr - startPage;
   
   if ((startPage != sActualProgramingPage) &&
       (sActualProgramingPage != INV_PAGE)) {
      /* zuvor teilgefüllte Page programmieren falls in anderer Page fortgesetzt wird */
      FlashProgramPagePuffer(sActualProgramingPage);
   }

   for (page = startPage; page <= endPage; page += PAGE_WORD_SIZE) {
      numPageWords = min(PAGE_WORD_SIZE - wordOffset, numWords);      
      numWords -= numPageWords;
      FlashFillPagePuffer(wordOffset, pBuf, numPageWords);
      if ((wordOffset +  numPageWords) == PAGE_WORD_SIZE) {     
         /* bei gefülltem Pagepuffer -> Pagepuffer programmieren */
         FlashProgramPagePuffer(page);
         sActualProgramingPage = INV_PAGE;  /* nächste Page ist beliebig */
      } else {
         sActualProgramingPage = page;
      }
      pBuf += numPageWords;
      wordOffset = 0;
   } 

   RESTORE_INT(flags);

   return true;                           
}  

               
/*-----------------------------------------------------------------------------
*  Flashprogrammierung abschließen
*/               
bool FlashProgramTerminate(void) {

   int flags;

   if (sActualProgramingPage != INV_PAGE) {
      /* noch im Puffer befindliche Page programmieren */
      flags = DISABLE_INT;
      FlashProgramPagePuffer(sActualProgramingPage);         
      RESTORE_INT(flags);
   } 
   return true;                           
} 


