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
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
#define SIZE_COMPORT   100

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PrintUsage(void);
static int  Print(const char *fmt, ...);
static void sighandler(int sig);
static int InitBus(const char *comPort);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

    int           i;
    int           j;
    char          comPort[SIZE_COMPORT] = "";
    uint8_t       myAddr;
    bool          myAddrValid = false;
    TBusTelegram  txBusMsg;
    uint8_t       busRet;
    TBusTelegram  *pRxBusMsg;
    uint8_t       mask;
    int           flags;
    int           sioHandle;
    int           sioFd;
    fd_set        rfds;
    int           ret;
    uint8_t       val8;

    signal(SIGPIPE, sighandler);

    flags = fcntl(fileno(stdout), F_GETFL);
    fcntl(fileno(stdout), F_SETFL, flags | O_NONBLOCK);

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

    sioHandle = InitBus(comPort);
    if (sioHandle == -1) {
        printf("cannot open %s\r\n", comPort);
        return -1;
    }
    sioFd = SioGetFd(sioHandle);

    FD_ZERO(&rfds);
    FD_SET(sioFd, &rfds);

    for (;;) {
        ret = select(sioFd + 1, &rfds, 0, 0, 0);
        if ((ret > 0) && FD_ISSET(sioFd, &rfds)) {
            busRet = BusCheck();
            if (busRet == BUS_MSG_OK) {
                pRxBusMsg = BusMsgBufGet();
                if ((pRxBusMsg->type == eBusDevReqActualValueEvent) &&
                    ((pRxBusMsg->msg.devBus.receiverAddr == myAddr))) {
                    Print("event address %d device type ", pRxBusMsg->senderAddr);
                    switch (pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.devType) {
                    case eBusDevTypeDo31:
                        Print("DO31\n");
                        for (i = 0; i < BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE; i++) {
                            for (j = 0, mask = 1; j < 8; j++, mask <<= 1) {
                                if ((i == 3) && (j == 7)) {
                                    // DO31 has 31 outputs, dont display the last bit
                                    break;
                                }
                                if (pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut[i] & mask) {
                                    Print("1");
                                } else {
                                    Print("0");
                                }
                            }
                        }
                        Print("\n");
                        for (i = 0; i < BUS_DO31_SHADER_SIZE_ACTUAL_VALUE; i++) {
                            Print("%02x",
                                pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.shader[i]);
                            if (i < (BUS_DO31_SHADER_SIZE_ACTUAL_VALUE - 1)) {
                                Print(" ");
                            }
                        }
                        memcpy(txBusMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.do31.digOut,
                               pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut,
                               BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE);
                        memcpy(txBusMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.do31.shader,
                               pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.shader,
                               BUS_DO31_SHADER_SIZE_ACTUAL_VALUE);
                        txBusMsg.msg.devBus.x.devResp.actualValueEvent.devType = eBusDevTypeDo31;
                        break;
                    case eBusDevTypeSw8:
                        Print("SW8\n");
                        val8 = pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.sw8.state;
                        for (i = 0, mask = 1; i < 8; i++, mask <<= 1) {
                            if (val8 & mask) {
                                Print("1");
                            } else {
                                Print("0");
                            }
                        }
                        txBusMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.sw8.state = val8;
                        txBusMsg.msg.devBus.x.devResp.actualValueEvent.devType = eBusDevTypeSw8;
                        break;
                    default:
                        break;
                    }
                    Print("\n");
                    fflush(stdout);
                    txBusMsg.type = eBusDevRespActualValueEvent;
                    txBusMsg.senderAddr = pRxBusMsg->msg.devBus.receiverAddr;
                    txBusMsg.msg.devBus.receiverAddr = pRxBusMsg->senderAddr;
                    BusSend(&txBusMsg);
                }
            } else if (busRet == BUS_IF_ERROR) {
                Print("bus interface access error - exiting\n");
                break;
            }
        }
    }

    SioClose(sioHandle);
    return 0;
}

/*-----------------------------------------------------------------------------
*  print till successful (for temporarly unavailable pipes)
*/
static int Print(const char *fmt, ...) {

    int     len;
    va_list args;

    va_start(args, fmt);
    do {
        len = vprintf(fmt, args);
    } while (len < 0);
    va_end(args);

    return len;
}

/*-----------------------------------------------------------------------------
*  sig handler
*/
static void sighandler(int sig) {

    fprintf(stderr, "sig received %d\n", sig);
}

/*-----------------------------------------------------------------------------
*  sio open and bus init
*/
static int InitBus(const char *comPort) {

    uint8_t ch;
    int     handle;

    SioInit();
    handle = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);
    if (handle == -1) {
        return -1;
    }
    while (SioGetNumRxChar(handle) > 0) {
        SioRead(handle, &ch, sizeof(ch));
    }
    BusInit(handle);

    return handle;
}


/*-----------------------------------------------------------------------------
*  show help
*/
static void PrintUsage(void) {

   printf("\r\nUsage:\r\n");
   printf("modulservice -c port -a address\n");
}

