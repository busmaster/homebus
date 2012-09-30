/*-----------------------------------------------------------------------------
*  Sio.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "sysdef.h"
#include "sio.h"

/*-----------------------------------------------------------------------------
*  Macros
*/                     

#define MAX_NUM_SIO     4
#define UNREAD_BUF_SIZE 512  // 2er-Potenz!!

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef struct {
   bool   used;
   int    fd; 
   struct {
      uint8_t        buf[UNREAD_BUF_SIZE];
      unsigned int bufIdxWr;
      unsigned int bufIdxRd;
   } unRead;
} TSioDesc;

/*-----------------------------------------------------------------------------
*  Variables
*/                                
static TSioDesc sSio[MAX_NUM_SIO];

/*-----------------------------------------------------------------------------
*  Functions
*/
static bool HandleValid(int handle);
static unsigned int UnReadBufLen(int handle);
static uint8_t ReadUnRead(int handle, uint8_t *pBuf, uint8_t bufSize);


/*-----------------------------------------------------------------------------
*  Sio Initialisierung
*/
void SioInit(void) {

   int i;   

   for (i = 0; i < MAX_NUM_SIO; i++) {
      sSio[i].used = false;
   }
}

/*-----------------------------------------------------------------------------
*  Schnittstelle �ffnen
*/
int SioOpen(const char *pPortName, 
            TSioBaud baud, 
            TSioDataBits dataBits, 
            TSioParity parity, 
            TSioStopBits stopBits,
            TSioMode mode
            ) {
   int            fd; 
   int            i;
   struct termios settings;

   // freien descriptor suchen
   for (i = 0; (i < MAX_NUM_SIO) && (sSio[i].used == true); i++);
   
   if (i == MAX_NUM_SIO) {
      // kein Platz
      printf("no handle for %s\r\n", pPortName);
      return -1;
   }
               
   fd = open(pPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);
                          
   if (fd == -1) {                          
      return -1;
   }

   sSio[i].used = true;
   sSio[i].fd = fd;
   sSio[i].unRead.bufIdxWr = 0;
   sSio[i].unRead.bufIdxRd = 0;
   
   memset(&settings, 0, sizeof(settings));

   tcgetattr(fd, &settings);

   settings.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
   settings.c_oflag &= ~OPOST;
   settings.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
   settings.c_cflag &= ~(CSIZE|PARENB);

   settings.c_cc[VMIN] = 0;
   settings.c_cc[VTIME] = 1;

   /* Set controlflags */
   settings.c_cflag =   CREAD |           /* characters may be read */
                        CLOCAL;          /* ignore modem state, local connection */

   switch (baud) {
      case eSioBaud9600:
         cfsetispeed(&settings, B9600);
         cfsetospeed(&settings, B9600);
         break;
      default:
         cfsetispeed(&settings, B9600);
         cfsetospeed(&settings, B9600);
         break;
   }
      
   switch (dataBits) {
      case eSioDataBits8:
         settings.c_cflag |= CS8;  
         break;
      default:
         settings.c_cflag |= CS8;  
         break;
   }         

   switch (parity) {
      case eSioParityNo:
         break;
      default:
         break;
   }         
   
   switch (stopBits) {
      case eSioStopBits1:
         break;
      default:
         break;
   }         
   
   /* transfer setup to interface */
   if (tcsetattr(fd, TCSANOW, &settings) < 0) {
      fprintf(stderr, "Error setting terminal attributes for %s (errno: %s)!\n",
              pPortName, strerror (errno));
      return -1;
   }
   return i;
}

/*-----------------------------------------------------------------------------
*  Schnittstelle schlie�en
*/
int SioClose(int handle) {
   
   if (!HandleValid(handle)) {
      return -1;
   }

   sSio[handle].used = false;
   close(sSio[handle].fd);
  
   return 0;         
}

/*-----------------------------------------------------------------------------
*  Sio Sendepuffer schreiben
*/
uint8_t SioWrite(int handle, uint8_t *pBuf, uint8_t bufSize) {

   unsigned long bytesWritten;

   if (!HandleValid(handle)) {
      return 0;
   }
                        
   bytesWritten = write(sSio[handle].fd, pBuf, bufSize); 

   return (uint8_t)bytesWritten;           
}

/*-----------------------------------------------------------------------------
*  Sio Empfangspuffer lesen
*/
uint8_t SioRead(int handle, uint8_t *pBuf, uint8_t bufSize) {

   unsigned long bytesRead = 0;
   uint8_t readUnread;

   if (!HandleValid(handle)) {
      return 0;
   }
      
   readUnread = ReadUnRead(handle, pBuf, bufSize);
   if (readUnread < bufSize) {                 
      // noch Platz im Buffer                 
      bytesRead = read(sSio[handle].fd, pBuf + readUnread, bufSize - readUnread); 
   }

#if 0
if (bytesRead != 0) {
   printf("sioread");
   for (int i = 0; i < bytesRead; i++) {
      printf(" %02x", *(pBuf + i));
   }
   printf("\r\n");
}
#endif

   return (uint8_t)(bytesRead + readUnread);           
}

/*-----------------------------------------------------------------------------
*  Zeichen in Empfangspuffer zur�ckschreiben
*/
uint8_t SioUnRead(int handle, uint8_t *pBuf, uint8_t bufSize) {

   unsigned int free;
   unsigned int used;
   unsigned int i;
   unsigned int rdIdx;
   unsigned int wrIdx;

   if (HandleValid(handle)) {
      wrIdx = sSio[handle].unRead.bufIdxWr;
      used = UnReadBufLen(handle);
      free = UNREAD_BUF_SIZE - 1 - used;

      for (i = 0; i < bufSize; i++) {
         sSio[handle].unRead.buf[wrIdx] = *(pBuf + i); 
         wrIdx++;                    
         wrIdx &= (UNREAD_BUF_SIZE - 1);
      }
      // falls alte Daten im unread-buf �berschrieben wurden: rdIdx korr.
      if (free < bufSize) {
         rdIdx = (wrIdx + 1) & (UNREAD_BUF_SIZE - 1);
         sSio[handle].unRead.bufIdxRd = rdIdx;
      }   
      sSio[handle].unRead.bufIdxWr = wrIdx;
   }
   return bufSize;    
}


/*-----------------------------------------------------------------------------
*  Anzahl der Zeichen im Empfangspuffer
*/
uint8_t SioGetNumRxChar(int handle) {

   uint32_t inLen;
   uint32_t numRxChar = 0;

   if (HandleValid(handle)) {
      ioctl(sSio[handle].fd, FIONREAD, &inLen);
      numRxChar = inLen + UnReadBufLen(handle);
      if (numRxChar > 255) {
         numRxChar = 255;
      }
   } 
   return numRxChar;
}
      
/*-----------------------------------------------------------------------------
*  Zeichen in Empfangspuffer zur�ckschreiben
*/
static uint8_t ReadUnRead(int handle, uint8_t *pBuf, uint8_t bufSize) {

   unsigned int len = 0;
   unsigned int used;
   unsigned int i;
   unsigned int rdIdx;

   if (HandleValid(handle)) {
      rdIdx = sSio[handle].unRead.bufIdxRd;
      used = UnReadBufLen(handle);
      len = min(bufSize, used);
      for (i = 0; i < len; i++) {
         *(pBuf + i) = sSio[handle].unRead.buf[rdIdx]; 
         rdIdx++;                    
         rdIdx &= (UNREAD_BUF_SIZE - 1);
      }
      sSio[handle].unRead.bufIdxRd = rdIdx; 
   }  
   return (uint8_t)len;
}


/*-----------------------------------------------------------------------------
*  Belegung des unread-buf
*/
static unsigned int UnReadBufLen(int handle) {

   unsigned int used = UNREAD_BUF_SIZE;

   if (HandleValid(handle)) {
      used = (sSio[handle].unRead.bufIdxWr - sSio[handle].unRead.bufIdxRd) & 
             (UNREAD_BUF_SIZE - 1);
   } 
   return used;
}

/*-----------------------------------------------------------------------------
*  Handle pr�fen
*/
static bool HandleValid(int handle) {
   
   if ((handle >= MAX_NUM_SIO) ||
       (handle < 0)) {
      printf("invalid handle %i\r\n", handle);
      return false;
   }
   
   if (sSio[handle].used == false) {
      printf("invalid handle %i\r\n", handle);
      return false;
   }
   
   return true;
}


