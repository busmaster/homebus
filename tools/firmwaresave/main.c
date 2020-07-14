/*
 * main.c
 *
 * Copyright 2020 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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
#include <signal.h>
#include <errno.h>

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

/* timeout for repeating */
#define TIMEOUT_MS  100

/* max number of retries */
#define MAX_RETRY  10

/*-----------------------------------------------------------------------------
*  Variables
*/
static uint8_t sMyAddr = 0;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PrintUsage(void);
static unsigned long GetTickCount(void);
static int ReqFlashData(uint8_t address, uint32_t flashOffs, uint8_t *buffer, size_t buffer_size);

/*-----------------------------------------------------------------------------
*  restore cursor
*/
static void signal_callback_handler(int signum) {

    system("setterm -cursor on");
    printf("\n");
    exit(signum);
}

/*-----------------------------------------------------------------------------
*  Program start
*/
int main(int argc, char *argv[]) {

    int            sio;
    int            ret;
    uint8_t        len;
    uint8_t        val8;
    int            i;
    char           comDev[64];
    char           fileName[256];
    uint8_t        moduleAddr;
    bool           moduleAddrValid = false;
    uint8_t        data[BUS_GETFLASH_PACKET_SIZE];
    FILE           *file;
    uint32_t       flashOffs;
    int            retryCnt;

    signal(SIGINT, signal_callback_handler);

    comDev[0] = '\0';
    fileName[0] = '\0';

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            /* get com interface */
            if (argc > i) {
                snprintf(comDev, sizeof(comDev), "%s", argv[i + 1]);
                i++;
            }
        } else if (strcmp(argv[i], "-a") == 0) {
            if (argc > i) {
                moduleAddr = atoi(argv[i + 1]);
                moduleAddrValid = true;
                i++;
            }
        } else if (strcmp(argv[i], "-o") == 0) {
            if (argc > i) {
                sMyAddr = atoi(argv[i + 1]);
                i++;
            }
        } else if (strcmp(argv[i], "-f") == 0) {
            if (argc > i) {
                strncpy(fileName, argv[i + 1], sizeof(fileName) - 1);
                fileName[sizeof(fileName) - 1] = '\0';
                i++;
            }
        }
    }
    if ((comDev[0] == '\0') || (fileName[0] == '\0') || !moduleAddrValid) {
        PrintUsage();
        return -1;
    }

    SioInit();
    sio = SioOpen(comDev, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);
    if (sio == -1) {
        printf("cannot open %s\n", comDev);
        return -1;
    }

    // wait for sio input to settle and the flush
    usleep(100000);
    while ((len = SioGetNumRxChar(sio)) != 0) {
        SioRead(sio, &val8, sizeof(val8));
    }
    BusInit(sio);

    /* create file */
    file = fopen(fileName, "wb+");
    if (file == 0) {
        printf("cannot open %s\r\n", fileName);
        return 0;
    }

    flashOffs = 0;
    system("setterm -cursor off");
    printf("received data (bytes):        ");
    do {
        retryCnt = 5;
        do {
            ret = ReqFlashData(moduleAddr, flashOffs, data, sizeof(data));
            retryCnt--;
        } while ((ret == -1) && retryCnt);
        if (ret == -1) {
            printf("\nerror reading flash data from device %d at address offset 0x%08x", moduleAddr, flashOffs);
            // error
            break;
        }
        printf("\b\b\b\b\b\b%6d", flashOffs);
        fflush(stdout);
        fwrite(data, ret, 1, file);
        if (ret < BUS_FWU_PACKET_SIZE) {
            // last packet
            break;
        }
        flashOffs += BUS_FWU_PACKET_SIZE;
    } while (1);

    SioClose(sio);
    fclose(file);
    system("setterm -cursor on");
    printf("\n");

    return 0;
}


/*-----------------------------------------------------------------------------
* sleep
*/
int msleep(long ms) {

    struct timespec ts;
    int ret;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

    return ret;
}

/*-----------------------------------------------------------------------------
*  request flash packet
*/
static int ReqFlashData(uint8_t address, uint32_t flashOffs, uint8_t *buffer, size_t bufferSize) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;
    int             numBytes = -1;

    txBusMsg.type = eBusDevReqGetFlashData;
    txBusMsg.senderAddr = sMyAddr;
    txBusMsg.msg.devBus.receiverAddr = address;
    txBusMsg.msg.devBus.x.devReq.getFlashData.addr = flashOffs;
    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespGetFlashData)                &&
                (pBusMsg->msg.devBus.receiverAddr == txBusMsg.senderAddr) &&
                (pBusMsg->senderAddr == txBusMsg.msg.devBus.receiverAddr) &&
                (pBusMsg->msg.devBus.x.devResp.getFlashData.addr ==
                 txBusMsg.msg.devBus.x.devReq.getFlashData.addr)) {
                responseOk = true;
                memcpy(buffer, pBusMsg->msg.devBus.x.devResp.getFlashData.data, bufferSize);
                numBytes = pBusMsg->msg.devBus.x.devResp.getFlashData.numValid;
            }
        } else {
            if ((actualTimeMs - startTimeMs) > TIMEOUT_MS) {
                timeOut = true;
            }
        }
        msleep(1);
    } while (!responseOk && !timeOut);

    if (responseOk) {
        return numBytes;
    } else {
        return -1;
    }
}

/*-----------------------------------------------------------------------------
*  get ms timestamp
*/
static unsigned long GetTickCount(void) {

    struct timespec ts;
    unsigned long timeMs;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    timeMs = (unsigned long)((unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)ts.tv_nsec / 1000000ULL);

    return timeMs;
}

/*-----------------------------------------------------------------------------
*  help
*/
static void PrintUsage(void) {

    printf("\nUsage:");
    printf("firmwaresave -c comport -f filename -a target-address [-o own-address]\n");
    printf("comport: tty device\n");
    printf("filename: firmware binary file\n");
    printf("target-address: target bus address\n");
    printf("own-address: default 0\n");
}
