/*
 * sio.c
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

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "sio.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

#define MAX_NUM_SIO     4
#define UNREAD_BUF_SIZE 512  // 2er-Potenz!!
#define TX_BUF_SIZE     255  // max 255

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef struct {
    bool   used;
    HANDLE hCom;
    struct {
      uint8_t buf[TX_BUF_SIZE];
      uint8_t pos;
    } bufferedTx;
    struct {
        uint8_t      buf[UNREAD_BUF_SIZE];
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
static void CommErrorClear(int handle);
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
*  Schnittstelle öffnen
*/
int SioOpen(const char *pPortName,
            TSioBaud baud,
            TSioDataBits dataBits,
            TSioParity parity,
            TSioStopBits stopBits,
            TSioMode mode
            ) {
   HANDLE       hCom;
   DCB          dcb;
   bool         fSuccess;
   COMMTIMEOUTS timeouts;
   int          i;

   // freien descriptor suchen
   for (i = 0; (i < MAX_NUM_SIO) && (sSio[i].used == true); i++);

   if (i == MAX_NUM_SIO) {
      // kein Platz
      printf("no handle for %s\r\n", pPortName);
      return -1;
   }

   hCom = CreateFileA(pPortName, GENERIC_WRITE | GENERIC_READ,
                          0, NULL, OPEN_EXISTING, 0, NULL );

   if (hCom == INVALID_HANDLE_VALUE) {
      return -1;
   }

   sSio[i].used = true;
   sSio[i].hCom = hCom;
   sSio[i].unRead.bufIdxWr = 0;
   sSio[i].unRead.bufIdxRd = 0;

   fSuccess = GetCommState(hCom, &dcb);
   if (!fSuccess) {
      printf("GetCommState failed\r\n");
      return -1;
   }

   switch (baud) {
      case eSioBaud9600:
         dcb.BaudRate = 9600;
         break;
      default:
         dcb.BaudRate = 9600;
         break;
   }

   switch (dataBits) {
      case eSioDataBits8:
         dcb.ByteSize = DATABITS_8;
         break;
      default:
         dcb.ByteSize = DATABITS_8;
         break;
   }

   switch (parity) {
      case eSioParityNo:
         dcb.Parity = NOPARITY;
         break;
      default:
         dcb.Parity = NOPARITY;
         break;
   }

   switch (stopBits) {
      case eSioStopBits1:
         dcb.StopBits = ONESTOPBIT;
         break;
      default:
         dcb.StopBits = ONESTOPBIT;
         break;
   }

   fSuccess = SetCommState(hCom, &dcb);
   if (!fSuccess) {
      printf("SetCommState failed\r\n");
      return -1;
   }


  fSuccess = GetCommTimeouts(hCom, &timeouts);


  /* Set timeout to 0 to force that:
     If a character is in the buffer, the character is read,
     If no character is in the buffer, the function do not wait and returns immediatly
  */
   timeouts.ReadIntervalTimeout = MAXDWORD;
   timeouts.ReadTotalTimeoutMultiplier = 0;
   timeouts.ReadTotalTimeoutConstant = 0;

   fSuccess = SetCommTimeouts(hCom, &timeouts);

//   SetCommMask(hCom, EV_RXCHAR);

   return i;
}

/*-----------------------------------------------------------------------------
*  Schnittstelle schließen
*/
int SioClose(int handle) {

   if (!HandleValid(handle)) {
      return -1;
   }

   sSio[handle].used = false;
   CloseHandle(sSio[handle].hCom);

   return 0;
}

/*-----------------------------------------------------------------------------
*  Sio Sendepuffer schreiben
*/
uint8_t SioWrite(int handle, uint8_t *pBuf, uint8_t bufSize) {

   bool fSuccess;
   unsigned long bytesWritten;

   if (!HandleValid(handle)) {
      return 0;
   }

   fSuccess = WriteFile(sSio[handle].hCom, pBuf, bufSize, &bytesWritten ,NULL);
   if (!fSuccess) {
      CommErrorClear(handle);
   }
   return (uint8_t)bytesWritten;
}

/*-----------------------------------------------------------------------------
*  write data to tx buffer - do not yet start with tx
*/
uint8_t SioWriteBuffered(int handle, uint8_t *pBuf, uint8_t bufSize) {

   uint8_t   len;
   TSioDesc  *pSio;

   if (!HandleValid(handle)) {
      return 0;
   }
   pSio = &sSio[handle];

   len = sizeof(pSio->bufferedTx.buf) - pSio->bufferedTx.pos;
   len = min(len, bufSize);
   memcpy(&pSio->bufferedTx.buf[pSio->bufferedTx.pos], pBuf, len);
   pSio->bufferedTx.pos += len;

   return (uint8_t)len;
}

/*-----------------------------------------------------------------------------
*  trigger tx of buffer
*/
bool SioSendBuffer(int handle) {

    bool fSuccess;
   unsigned long bytesWritten = 123;
   TSioDesc      *pSio;
   bool          rc = false;

    if (!HandleValid(handle)) {
        return 0;
    }
    pSio = &sSio[handle];

    fSuccess = WriteFile(pSio->hCom, pSio->bufferedTx.buf, pSio->bufferedTx.pos, &bytesWritten ,NULL);
    if (!fSuccess) {
        printf("comm err\n");
        CommErrorClear(handle);
    } else {
        if (bytesWritten == pSio->bufferedTx.pos) {
            rc = true;
        }
    }
    pSio->bufferedTx.pos = 0;

    return rc;
}

/*-----------------------------------------------------------------------------
*  Sio Empfangspuffer lesen
*/
uint8_t SioRead(int handle, uint8_t *pBuf, uint8_t bufSize) {

   bool fSuccess;
   unsigned long bytesRead = 0;
   uint8_t readUnread;

   if (!HandleValid(handle)) {
      return 0;
   }

   readUnread = ReadUnRead(handle, pBuf, bufSize);
   if (readUnread < bufSize) {
      // noch Platz im Buffer
      fSuccess = ReadFile(sSio[handle].hCom, pBuf + readUnread,
                          bufSize - readUnread, &bytesRead ,NULL);
      if (!fSuccess) {
         CommErrorClear(handle);
      }
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
*  Zeichen in Empfangspuffer zurückschreiben
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
      // falls alte Daten im unread-buf überschrieben wurden: rdIdx korr.
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

   DWORD inLen;
   DWORD numRxChar = 0;

   if (HandleValid(handle)) {
      COMSTAT  comStat;
      DWORD    dwErrorFlags;
      ClearCommError(sSio[handle].hCom, &dwErrorFlags, &comStat);
      inLen = comStat.cbInQue;
      numRxChar = inLen + UnReadBufLen(handle);
      if (numRxChar > 255) {
         numRxChar = 255;
      }
   }
   return numRxChar;
}

/*-----------------------------------------------------------------------------
*  Zeichen in Empfangspuffer zurückschreiben
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
*  Handle prüfen
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


/*-----------------------------------------------------------------------------
*  Schnittstelle zurücksetzen
*/
static void CommErrorClear(int handle) {

   if (HandleValid(handle)) {
      COMSTAT  comStat;
      DWORD    dwErrorFlags;
      ClearCommError(sSio[handle].hCom, &dwErrorFlags, &comStat);
   }
}

/*-----------------------------------------------------------------------------
*  check handle
*/
bool SioHandleValid(int handle) {

    return true;
}
