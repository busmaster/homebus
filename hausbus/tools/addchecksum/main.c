/*-----------------------------------------------------------------------------
*  Main.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*-----------------------------------------------------------------------------
*  Macros
*/

#define PAD_WORD       0xffff

#define SRC_FILE       argv[1]
#define DST_FILE       argv[2]
#define BIN_SIZE       binFileSize

#define CHECKSUM      0x1234  /* Summe, die bei Checksum herauskommen muss */

/*-----------------------------------------------------------------------------
*  Typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PrintUsage(void);

/*-----------------------------------------------------------------------------
*  Programstart
*/
int main(int argc, char *argv[]) {

   FILE *pSrcFile;
   FILE *pDestFile;
   int  ret;
   unsigned int addr;
   unsigned int i;
   uint8_t data;
   unsigned int binFileSize;
   uint16_t sum16 = 0;
   uint16_t dataPad = PAD_WORD;
   uint16_t readBuf;

   if (argc != 4) {
      PrintUsage();
      return 0;
   }

   binFileSize = atoi(argv[3]);

   /* source-File öffnen */
   pSrcFile = fopen(SRC_FILE, "rb");
   if (pSrcFile == 0) {
      printf("cannot open %s\r\n", SRC_FILE);
      return 0;
   }

   /* Destination-File erzeugen */
   pDestFile = fopen(DST_FILE, "wb");
   if (pDestFile == 0) {
      printf("cannot create %s\r\n", DST_FILE);
      return 0;
   }
  
   sum16 = 0;
   addr = 0;
   while (1) {
      ret = fread(&readBuf, sizeof(readBuf), 1, pSrcFile);  
      if (!feof(pSrcFile)) {
          fwrite(&readBuf, sizeof(readBuf), 1, pDestFile);
          sum16 += readBuf; 
          addr += 2;
      } else {
         break;
      }
   } 

   /* als Summe soll CHECKSUM herauskommen */
   for (i = 0; i < (BIN_SIZE - addr - 2); i += 2) {
       sum16 += dataPad; 
   } 
   
   sum16 = CHECKSUM - sum16;
   data = (uint8_t)sum16;
   fwrite(&data, sizeof(data), 1, pDestFile);
   data = (uint8_t)(sum16 >> 8);
   fwrite(&data, sizeof(data), 1, pDestFile);
 
   fclose(pSrcFile); 
   fclose(pDestFile); 
  
   return 0;
}


/*-----------------------------------------------------------------------------
*  Hilfe anzeigen
*/
static void PrintUsage(void) {

   printf("\r\nUsage:");
   printf("addchecksum source destination binsize\r\n");
   printf("source: binary source file\r\n");
   printf("destination: destination binary file\r\n");
   printf("binsize: destination binary file size (for sum calc)\r\n");
}
