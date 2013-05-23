/*
 * main.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
#define NAME_LEN_COMPORT       255
#define NAME_LEN_COMMANDFILE   255
#define MAX_LINE_LEN           300
#define LEN_ACTION             10

/* our address */
#define MY_ADDR                0

/* default timeout for receiption of response telegram */
#define RESPONSE_TIMEOUT   5000 /* ms */

#define SHADER_OPEN            0
#define SHADER_CLOSE           1
#define SHADER_STOP            2

#define DO31_NUM_SHUTTER       15

/*-----------------------------------------------------------------------------
*  Typedefs
*/
typedef enum {
   eActionOpen,
   eActionClose,
   eActionStop,
   eActionNone
} TAction;


/*-----------------------------------------------------------------------------
*  Variables
*/

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PrintUsage(void);
#ifndef WIN32
static unsigned long GetTickCount(void);
#endif
static bool BusSetShaderState(uint8_t do31Address, TAction *pShutter, int numShutter);

int main(int argc, char *argv[]) {

   int      i;
   char     comPort[NAME_LEN_COMPORT] = "";
   char     commandFile[NAME_LEN_COMMANDFILE] = "";
   char     lineBuf[MAX_LINE_LEN];
   TAction  shutterAction[BUS_DO31_NUM_SHADER];
   uint8_t  do31Address = 0;
   uint8_t  shutter = 0;
   uint16_t delayMs = 0;
   char     *word;
   int      sioHdl;
   FILE     *pCmd;

   /* get com interface */
   for (i = 1; i < argc; i++) {
     if (strcmp(argv[i], "-c") == 0) {
       if (argc > i) {
         strncpy(comPort, argv[i + 1], sizeof(comPort) - 1);
         comPort[sizeof(comPort) - 1] = '\0';
       }
       break;
     }
   }

   if (strlen(comPort) == 0) {
     PrintUsage();
     return 0;
   }

   /* get command file name */
   for (i = 1; i < argc; i++) {
     if (strcmp(argv[i], "-x") == 0) {
       if (argc > i) {
         strncpy(commandFile, argv[i + 1], sizeof(commandFile) - 1);
         commandFile[sizeof(commandFile) - 1] = '\0';
       }
       break;
     }
   }

   if (strlen(commandFile) == 0) {
     PrintUsage();
     return 0;
   }

   pCmd = fopen(commandFile, "r");
   if (pCmd == 0) {
      printf("can't open %s\n", commandFile);
   }

    SioInit();
    sioHdl = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);
    if (sioHdl == -1) {
       printf("cannot open %s\r\n", comPort);
       return 0;
   }
    uint8_t len;
    while ((len = SioGetNumRxChar(sioHdl)) != 0) {
       uint8_t rxBuf[255];
       SioRead(sioHdl, rxBuf, len);
   }
    BusInit(sioHdl);

    while (fgets(lineBuf, sizeof(lineBuf), pCmd) != 0) {
       word = strtok(lineBuf, ":");
       if (strcmp(word, "DO31") == 0) {
          do31Address = atoi(strtok(0, " "));
          for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
             shutterAction[i] = eActionNone;
          }
            for (;;) {
            word = strtok(0, ":");
            if (word == 0) {
               break;
            }
            if (word[0] == 'S') {
               shutter = atoi(&word[1]);
            }
            if (shutter > BUS_DO31_NUM_SHADER) {
               break;
            }
            word = strtok(0, " \n");
            if (word == 0) {
               break;
            }
            if (strcmp(word, "open") == 0) {
               shutterAction[shutter] = eActionOpen;
            } else if (strcmp(word, "close") == 0) {
               shutterAction[shutter] = eActionClose;
            } else if (strcmp(word, "stop") == 0) {
               shutterAction[shutter] = eActionStop;
            }
            }
            BusSetShaderState(do31Address, shutterAction, BUS_DO31_NUM_SHADER);
       } else if (strcmp(word, "DELAY") == 0) {
          delayMs = atoi(strtok(0, " "));
            usleep(delayMs * 1000);
       }
    }

    fclose(pCmd);
    SioClose(sioHdl);

    return 0;
}

/*-----------------------------------------------------------------------------
*  set shader state
*/
static bool BusSetShaderState(uint8_t address, TAction *pShutterAction, int numShutter) {

   TBusTelegram    txBusMsg;
   uint8_t         ret;
   unsigned long   startTimeMs;
   unsigned long   actualTimeMs;
   TBusTelegram    *pBusMsg;
   bool            responseOk = false;
   bool            timeOut = false;
   uint8_t         shaderBytePos;
   uint8_t         shaderBitPos;
   int             i;

   txBusMsg.type = eBusDevReqSetState;
   txBusMsg.senderAddr = MY_ADDR;
   txBusMsg.msg.devBus.receiverAddr = address;
   txBusMsg.msg.devBus.x.devReq.setState.devType = eBusDevTypeDo31;
   memset(txBusMsg.msg.devBus.x.devReq.setState.state.do31.digOut, 0, BUS_DO31_DIGOUT_SIZE_SET);
   memset(txBusMsg.msg.devBus.x.devReq.setState.state.do31.shader, 0, BUS_DO31_SHADER_SIZE_SET);

   for (i = 0; i < numShutter; i++) {
      shaderBytePos = i / 4;
      shaderBitPos = i % 4 * 2;
      switch (*(pShutterAction + i)) {
         case eActionOpen:
            txBusMsg.msg.devBus.x.devReq.setState.state.do31.shader[shaderBytePos] |= 1 << shaderBitPos;
            break;
         case eActionClose:
            txBusMsg.msg.devBus.x.devReq.setState.state.do31.shader[shaderBytePos] |= 2 << shaderBitPos;
            break;
         case eActionStop:
            txBusMsg.msg.devBus.x.devReq.setState.state.do31.shader[shaderBytePos] |= 3 << shaderBitPos;
            break;
         default:
            break;
      }
   }
   BusSend(&txBusMsg);
   startTimeMs = GetTickCount();
   do {
      actualTimeMs = GetTickCount();
      ret = BusCheck();
      if (ret == BUS_MSG_OK) {
         pBusMsg = BusMsgBufGet();
         if ((pBusMsg->type == eBusDevRespSetState)         &&
             (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
             (pBusMsg->senderAddr == address)) {
            responseOk = true;
         }
      } else {
         if ((actualTimeMs - startTimeMs) > RESPONSE_TIMEOUT) {
            timeOut = true;
         }
      }
   } while (!responseOk && !timeOut);

   if (responseOk) {
      return true;
   } else {
      return false;
   }
}

#ifndef WIN32
static unsigned long GetTickCount(void) {

   struct timespec ts;
   unsigned long timeMs;

   clock_gettime(CLOCK_MONOTONIC, &ts);
   timeMs = (unsigned long)((unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)ts.tv_nsec / 1000000ULL);

   return timeMs;

}
#endif
/*-----------------------------------------------------------------------------
*  show help
*/
static void PrintUsage(void) {

   printf("\nUsage:\n");
   printf("shuttercontrol -c port -x commandfile\n");
   printf("-c port: comX /dev/ttyX\n");
   printf("-x command file\n");
}

