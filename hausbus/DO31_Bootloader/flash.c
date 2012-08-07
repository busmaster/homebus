/*-----------------------------------------------------------------------------
*  Flash.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/

#include "Types.h"
#include "Sysdef.h"
#include "Board.h"
#include "Flash.h"
#include "FlashAsm.h"

/*-----------------------------------------------------------------------------
*  Macros
*/        
#define FIRMWARE_WORD_SIZE  (120UL * 1024UL / 2)
#define PAGE_WORD_SIZE      128UL

/* bits in SPMCSR  */
#define SPMEN     0
#define PGERS     1
#define PGWRT     2
#define BLBSET    3
#define RWWSRE    4
#define RWWSB     6
#define SPMIE     7

#define INV_PAGE  0xffff

/*-----------------------------------------------------------------------------
*  typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/                                
static UINT16 sActualProgramingPage = INV_PAGE;

/*-----------------------------------------------------------------------------
*  Functions
*/
   
/*-----------------------------------------------------------------------------
*  Flash Initialisierung
*/
void FlashInit(void) {
                           
}

/*-----------------------------------------------------------------------------
*  Flash löschen (Applikationsbereich)
*/
void FlashErase(void) {
  
   UINT16 pageWordAddr;
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
BOOL FlashProgram(UINT16 wordAddr, UINT16 *pBuf, UINT16 numWords) {
   
   UINT16 page;
   UINT16 startPage;
   UINT16 endPage; 
   UINT16 wordOffset; /* wordoffset innerhalb page */ 
   UINT8  numPageWords;
   int flags;
   
   if ((wordAddr + numWords) > FIRMWARE_WORD_SIZE) {
      return FALSE;
   }

   flags = DISABLE_INT;
   
   startPage = wordAddr & ~(PAGE_WORD_SIZE -1);
   endPage = (wordAddr + numWords - 1) & ~(PAGE_WORD_SIZE -1);
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

   return TRUE;                           
}  

               
/*-----------------------------------------------------------------------------
*  Flashprogrammierung abschließen
*/               
BOOL FlashProgramTerminate(void) {

   int flags;

   if (sActualProgramingPage != INV_PAGE) {
      /* nooch im Puffer befindliche Page programmieren */
      flags = DISABLE_INT;
      FlashProgramPagePuffer(sActualProgramingPage);         
      RESTORE_INT(flags);
   } 
   return TRUE;                           
} 

