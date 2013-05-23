/*
 * flash.c for ATmega8, ATmega88
 * 
 * Copyright 2013 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
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
*  erase flash (application section)
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
*  program flash
*/               
bool FlashProgram(uint16_t wordAddr, uint16_t *pBuf, uint16_t numWords) {
   
   uint16_t page;
   uint16_t startPage;
   uint16_t endPage; 
   uint16_t wordOffset; /* word offset within page */ 
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
      /* if continue in other page, program current buffer */
      FlashProgramPagePuffer(sActualProgramingPage);
   }

   for (page = startPage; page <= endPage; page += PAGE_WORD_SIZE) {
      numPageWords = min(PAGE_WORD_SIZE - wordOffset, numWords);      
      numWords -= numPageWords;
      FlashFillPagePuffer(wordOffset, pBuf, numPageWords);
      if ((wordOffset +  numPageWords) == PAGE_WORD_SIZE) {     
         /* buffer is full -> program page */
         FlashProgramPagePuffer(page);
         sActualProgramingPage = INV_PAGE;  /* next page not yet known */
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
*  terminate flash programing
*/               
bool FlashProgramTerminate(void) {

   int flags;

   if (sActualProgramingPage != INV_PAGE) {
      /* program current page */
      flags = DISABLE_INT;
      FlashProgramPagePuffer(sActualProgramingPage);         
      RESTORE_INT(flags);
   } 
   return true;                           
} 


