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

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
#define SIZE_COMPORT   100

/* eigene Adresse am Bus */
#define MY_ADDR1    70
#define MY_ADDR2    171

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
*  program start
*/
int main(int argc, char *argv[]) {

    int           handle;
    int           i;
    char          comPort[SIZE_COMPORT] = "";
    uint8_t       myAddr;
    bool          myAddrValid = false;
    TBusTelegram  txBusMsg;
    uint8_t       busRet;
    TBusTelegram  *pBusMsg;
    uint8_t       ch;
 
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
    
    /* our bus address */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            if (argc > i) {
                myAddr = atoi(argv[i + 1]);
                myAddrValid = true;
            }
            break;
        }
    }

    if ((strlen(comPort) == 0) ||
        !myAddrValid) {
        PrintUsage();
        return 0;
    }
    
    SioInit();
    handle = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);
    if (handle == -1) {
        printf("cannot open %s\r\n", comPort);
        return 0;
    }
    while (SioGetNumRxChar(handle) > 0) {
        SioRead(handle, &ch, sizeof(ch));
    }
    BusInit(handle);

    for (;;) {
        busRet = BusCheck();
        if (busRet == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevReqActualValueEvent) &&
                ((pBusMsg->msg.devBus.receiverAddr == myAddr))) {
                printf("event address %d device type ", pBusMsg->senderAddr);
                switch (pBusMsg->msg.devBus.x.devReq.actualValueEvent.devType) {
                case eBusDevTypeDo31:
                    printf("DO31\n");
                    for (i = 0; i < BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE; i++) {
                        printf(" %02x", 
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut[i]);
                    }
                    printf("\n");
                    for (i = 0; i < BUS_DO31_SHADER_SIZE_ACTUAL_VALUE; i++) {
                        printf(" %02x", 
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.shader[i]);
                    }
                    printf("\n");
                    memcpy(txBusMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.do31.digOut,
                           pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut,
                           BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE);
                    memcpy(txBusMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.do31.shader,
                           pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.shader,
                           BUS_DO31_SHADER_SIZE_ACTUAL_VALUE);
                    txBusMsg.msg.devBus.x.devResp.actualValueEvent.devType = eBusDevTypeDo31;
                    break;
                default:
                    break;
                }
                printf("\n");
                txBusMsg.type = eBusDevRespActualValueEvent;
                txBusMsg.senderAddr = pBusMsg->msg.devBus.receiverAddr;
                txBusMsg.msg.devBus.receiverAddr = pBusMsg->senderAddr;
                BusSend(&txBusMsg);
            }
        }
    }

    SioClose(handle);
    return 0;
}


/*-----------------------------------------------------------------------------
*  show help
*/
static void PrintUsage(void) {

   printf("\r\nUsage:\r\n");
   printf("modulservice -c port -a address\n");
}

