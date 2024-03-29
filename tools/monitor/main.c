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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

#define SIZE_COMPORT   100
#define MAX_NAME_LEN   255

#define STX 0x02
#define ETX 0x03
#define ESC 0x1B
/* Startwert fuer checksumberechung */
#define CHECKSUM_START   0x55

#define SPACE "                              "

/*-----------------------------------------------------------------------------
*  Typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/
static FILE  *spOutput;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PrintUsage(void);
static void BusMonDecoded(int sioHandle);
static void BusMonRaw(int sioHandle);

/*-----------------------------------------------------------------------------
*  Programstart
*/
int main(int argc, char *argv[]) {

    int  handle;
    int  i;
    FILE *pLogFile = 0;
    char comPort[SIZE_COMPORT] = "";
    char logFile[MAX_NAME_LEN] = "";
    bool raw = false;
    uint8_t len;
    uint8_t val8;

    /* COM-Port ermitteln */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            if (argc > i) {
                strncpy(comPort, argv[i + 1], sizeof(comPort) - 1);
                comPort[sizeof(comPort) - 1] = 0;
            }
            break;
        }
    }
    if (strlen(comPort) == 0) {
        PrintUsage();
        return 0;
    }

    /* Name des Logfile ermittlen */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            if (argc > i) {
                strncpy(logFile, argv[i + 1], sizeof(logFile) - 1);
                logFile[sizeof(logFile) - 1] = 0;
            }
            break;
        }
    }

    /* raw-Modus? */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-raw") == 0) {
            raw = true;
            break;
        }
    }

    if (strlen(logFile) != 0) {
        pLogFile = fopen(logFile, "wb");
        if (pLogFile == 0) {
            printf("cannot open %s\r\n", logFile);
            return 0;
        } else {
            printf("logging to %s\r\n", logFile);
        }
        spOutput = pLogFile;
    } else {
        printf("logging to console\r\n");
        spOutput = stdout;
    }

    SioInit();
    handle = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);

    if (handle == -1) {
        printf("cannot open %s\r\n", comPort);
        if (pLogFile != 0) {
            fclose(pLogFile);
        }
        return 0;
    }

    // wait for sio input to settle and the flush
    usleep(100000);
    while ((len = SioGetNumRxChar(handle)) != 0) {
        SioRead(handle, &val8, sizeof(val8));
    }

    if (raw) {
        BusMonRaw(handle);
    } else {
        BusMonDecoded(handle);
    }

    if (handle != -1) {
        SioClose(handle);
    }

    if (pLogFile != 0) {
        fclose(pLogFile);
    }
    return 0;
}

/*-----------------------------------------------------------------------------
*  print help
*/
static void PrintUsage(void) {

    printf("\r\nUsage:");
    printf("monitor -c port [-f file] [-raw]\r\n");
    printf("port: com1 com2 ..\r\n");
    printf("file, if no logfile: log to console\r\n");
    printf("-raw: log hex data\r\n");
}

/*-----------------------------------------------------------------------------
*  print as hex dump
*/
static void BusMonRaw(int sioHandle) {

    uint8_t         ch;
    uint8_t         lastCh = 0;
    uint8_t         checkSum = 0;
    bool            charIsInverted = false;
    int             sioFd;
    int             maxFd;
    fd_set          fds;
    int             result;
    struct timespec ts;
    struct tm       *ptm;

    sioFd = SioGetFd(sioHandle);
    maxFd = sioFd;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(sioFd, &fds);
        result = select(maxFd + 1, &fds, 0, 0, 0);

        if ((result > 0) && SioRead(sioHandle, &ch, 1) == 1) {
            /* received char */
            if (ch == STX) {
                /* check of old checksum */
                checkSum -= lastCh;
                if (checkSum != lastCh) {
                    fprintf(spOutput, "checksum error");
                }
                /* start of new telegram -> line feed */
                fprintf(spOutput, "\r\n");
                checkSum = CHECKSUM_START;
                clock_gettime(CLOCK_REALTIME, &ts);
                ptm = localtime(&ts.tv_sec);
                fprintf(spOutput, "%d-%02d-%02d %2d:%02d:%02d.%03d  ",
                        ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
                        ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
                        (int)ts.tv_nsec / 1000000);
            }
            lastCh = ch;

            if (charIsInverted) {
                lastCh = ~lastCh;
                charIsInverted = false;
                /* Korrektur f�r folgende checksum-Berechnung */
                checkSum = checkSum - ch + ~ch;
            }

            if (ch == ESC) {
                charIsInverted = true;
                /* ESC-Zeichen wird nicht addiert, bei unten folgender Berechnung */
                /* aber nicht unterschieden -> ESC subtrahieren */
                checkSum -= ESC;
            }

            checkSum += ch;
            fprintf(spOutput, "%02x ", ch);
            fflush(spOutput);
        }
    }
}

/*-----------------------------------------------------------------------------
*  print decoded telegrams
*/
static void BusMonDecoded(int sioHandle) {

    int             i;
    uint8_t         ret;
    TBusTelegram    *pBusMsg;
    int             sioFd;
    int             maxFd;
    fd_set          fds;
    int             result;
    struct timespec ts;
    struct tm       *ptm;
    bool            skipError;

    BusInit(sioHandle);
    pBusMsg = BusMsgBufGet();

    sioFd = SioGetFd(sioHandle);
    maxFd = sioFd;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(sioFd, &fds);
        result = select(maxFd + 1, &fds, 0, 0, 0);
        if (result <= 0) {
            continue;
        }
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            skipError = false;
            clock_gettime(CLOCK_REALTIME, &ts);
            ptm = localtime(&ts.tv_sec);
            fprintf(spOutput, "%d-%02d-%02d %2d:%02d:%02d.%03d  ",
                    ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
                    ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
                    (int)ts.tv_nsec / 1000000);

            fprintf(spOutput, "%4d ", pBusMsg->senderAddr);

            switch (pBusMsg->type) {
            case eBusButtonPressed1:
                fprintf(spOutput, "button 1 pressed ");
                break;
            case eBusButtonPressed2:
                fprintf(spOutput, "button 2 pressed ");
                break;
            case eBusButtonPressed1_2:
                fprintf(spOutput, "buttons 1 and 2 pressed ");
                break;
            case eBusDevReqReboot:
                fprintf(spOutput, "request reboot ");
                fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevReqUpdEnter:
                fprintf(spOutput, "request update enter ");
                fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevRespUpdEnter:
                fprintf(spOutput, "response update enter ");
                fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevReqUpdData:
                fprintf(spOutput, "request update data ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "wordaddr: %04x\r\n", pBusMsg->msg.devBus.x.devReq.updData.wordAddr);
                fprintf(spOutput, SPACE "data: ");
                for (i = 0; i < BUS_FWU_PACKET_SIZE / 2; i++) {
                    fprintf(spOutput, "%04x ", pBusMsg->msg.devBus.x.devReq.updData.data[i]);
                }
                break;
            case eBusDevRespUpdData:
                fprintf(spOutput, "response update data ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "wordaddr: %x ", pBusMsg->msg.devBus.x.devResp.updData.wordAddr);
                break;
            case eBusDevReqUpdTerm:
                fprintf(spOutput, "request update terminate ");
                fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevRespUpdTerm:
                fprintf(spOutput, "response update terminate ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "success: %d ", pBusMsg->msg.devBus.x.devResp.updTerm.success);
                break;
            case eBusDevReqDiag:
                fprintf(spOutput, "request diag ");
                fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevRespDiag:
                fprintf(spOutput, "response diag ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                switch (pBusMsg->msg.devBus.x.devResp.diag.devType) {
                case eBusDevTypeSmIf:
                    fprintf(spOutput, SPACE "device: SMIF\r\n");
                    break;
                case eBusDevTypeSg:
                    fprintf(spOutput, SPACE "device: SG\r\n");
                    break;
                default:
                    break;
                }
                fprintf(spOutput, SPACE "data: ");
                for (i = 0; i < sizeof(pBusMsg->msg.devBus.x.devResp.diag.data); i++) {
                    fprintf(spOutput, "%02x ", pBusMsg->msg.devBus.x.devResp.diag.data[i]);
                }
                break;
            case eBusDevReqInfo:
                fprintf(spOutput, "request info ");
                fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevRespInfo:
                fprintf(spOutput, "response info ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                switch (pBusMsg->msg.devBus.x.devResp.info.devType) {
                case eBusDevTypeDo31:
                    fprintf(spOutput, SPACE "device: DO31\r\n");
                    fprintf(spOutput, SPACE "shader configuration:\r\n");
                    for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
                        uint8_t onSw = pBusMsg->msg.devBus.x.devResp.info.devInfo.do31.onSwitch[i];
                        uint8_t dirSw = pBusMsg->msg.devBus.x.devResp.info.devInfo.do31.dirSwitch[i];
                        if ((dirSw == 0xff) &&
                            (onSw == 0xff)) {
                            continue;
                        }
                        fprintf(spOutput, SPACE "   shader %d:\r\n", i);
                        if (dirSw != 0xff) {
                            fprintf(spOutput, SPACE "      onSwitch: %d\r\n", onSw);
                        }
                        if (onSw != 0xff) {
                            fprintf(spOutput, SPACE "      dirSwitch: %d\r\n", dirSw);
                        }
                    }
                    break;
                case eBusDevTypeSw8:
                    fprintf(spOutput, SPACE "device: SW8\r\n");
                    break;
                case eBusDevTypeSw16:
                    fprintf(spOutput, SPACE "device: SW16\r\n");
                    break;
                case eBusDevTypeLum:
                    fprintf(spOutput, SPACE "device: LUM\r\n");
                    break;
                case eBusDevTypeLed:
                    fprintf(spOutput, SPACE "device: LED\r\n");
                    break;
                case eBusDevTypeWind:
                    fprintf(spOutput, SPACE "device: WIND\r\n");
                    break;
                case eBusDevTypeSw8Cal:
                    fprintf(spOutput, SPACE "device: SW8CAL\r\n");
                    break;
                case eBusDevTypeRs485If:
                    fprintf(spOutput, SPACE "device: RS485IF\r\n");
                    break;
                case eBusDevTypePwm4:
                    fprintf(spOutput, SPACE "device: PWM4\r\n");
                    break;
                case eBusDevTypeSmIf:
                    fprintf(spOutput, SPACE "device: SMIF\r\n");
                    break;
                case eBusDevTypeKeyb:
                    fprintf(spOutput, SPACE "device: KEYB\r\n");
                    break;
                case eBusDevTypeKeyRc:
                    fprintf(spOutput, SPACE "device: KEYRC\r\n");
                    break;
                case eBusDevTypeSg:
                    fprintf(spOutput, SPACE "device: SG\r\n");
                    break;
                default:
                    fprintf(spOutput, SPACE "device: unknown\r\n");
                    break;
                }
                fprintf(spOutput, SPACE "version: %s", pBusMsg->msg.devBus.x.devResp.info.version);
                break;
            case eBusDevReqSetState:
                fprintf(spOutput, "request set state ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                switch (pBusMsg->msg.devBus.x.devReq.setState.devType) {
                case eBusDevTypeDo31:
                    fprintf(spOutput, SPACE "device: DO31\r\n");
                    fprintf(spOutput, SPACE "output state:\r\n");
                    for (i = 0; i < 31 /* 31 DO's */; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devReq.setState.state.do31.digOut[i / 4] >> ((i % 4) * 2)) & 0x3;
                        if (state != 0) {
                            fprintf(spOutput, SPACE "   DO%d: ", i);
                            switch (state) {
                            case 0x02:
                                fprintf(spOutput, "OFF");
                                break;
                            case 0x03:
                                fprintf(spOutput, "ON");
                                break;
                            default:
                                fprintf(spOutput, "invalid state");
                                break;
                            }
                            fprintf(spOutput, "\r\n");
                        } else {
                            continue;
                        }
                    }
                    fprintf(spOutput, SPACE "shader state:\r\n");
                    for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devReq.setState.state.do31.shader[i / 4] >> ((i % 4) * 2)) & 0x3;
                        if (state != 0) {
                            fprintf(spOutput, SPACE "   SHADER%d: ", i);
                            switch (state) {
                            case 0x01:
                                fprintf(spOutput, "OPEN");
                                break;
                            case 0x02:
                                fprintf(spOutput, "CLOSE");
                                break;
                            case 0x03:
                                fprintf(spOutput, "STOP");
                                break;
                            default:
                                fprintf(spOutput, "invalid state");
                                break;
                            }
                            if (i != (BUS_DO31_NUM_SHADER - 1)) {                            
                                fprintf(spOutput, "\r\n");
                            }
                        } else {
                            continue;
                        }
                    }
                    break;
                default:
                    fprintf(spOutput, SPACE "device: unknown");
                    break;
                }
                break;
            case eBusDevRespSetState:
                fprintf(spOutput, "response set state ");
                fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevReqGetState:
                fprintf(spOutput, "request get state ");
                fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevRespGetState:
                fprintf(spOutput, "response get state ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                switch (pBusMsg->msg.devBus.x.devResp.getState.devType) {
                case eBusDevTypeDo31:
                    fprintf(spOutput, SPACE "device: DO31\r\n");
                    fprintf(spOutput, SPACE "output state: ");
                    for (i = 0; i < 31 /* 31 DO's */; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devResp.getState.state.do31.digOut[i / 8] >> (i % 8)) & 0x1;
                        if (state == 0) {
                            fprintf(spOutput, "0");
                        } else {
                            fprintf(spOutput, "1");
                        }
                    }
                    fprintf(spOutput, "\r\n");
                    fprintf(spOutput, SPACE "shader state:\r\n");
                    for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devResp.getState.state.do31.shader[i / 4] >> ((i % 4) * 2)) & 0x3;
                        if (state != 0) {
                            fprintf(spOutput, SPACE "   SHADER%d: ", i);
                            switch (state) {
                            case 0x01:
                                fprintf(spOutput, "OPENING");
                                break;
                            case 0x02:
                                fprintf(spOutput, "CLOSING");
                                break;
                            case 0x03:
                                fprintf(spOutput, "STOPPED");
                                break;
                            default:
                                fprintf(spOutput, "invalid state");
                                break;
                            }
                            if (i != (BUS_DO31_NUM_SHADER - 1)) {
                                fprintf(spOutput, "\r\n");
                            }
                        } else {
                            continue;
                        }
                    }
                    break;
                case eBusDevTypeSw8:
                    fprintf(spOutput, SPACE "device: SW8\r\n");
                    fprintf(spOutput, SPACE "switch state: ");
                    for (i = 0; i < 8 /* 8 Switches */; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devResp.getState.state.sw8.switchState >> i) & 0x1;
                        if (state == 0) {
                            fprintf(spOutput, "0");
                        } else {
                            fprintf(spOutput, "1");
                        }
                    }
                    break;
                default:
                    fprintf(spOutput, SPACE "device: unknown");
                    break;
                }
                break;
            case eBusDevReqSwitchState:
                fprintf(spOutput, "request switch state ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "switch state: ");
                for (i = 0; i < 8 /* 8 Switches */; i++) {
                    uint8_t state = (pBusMsg->msg.devBus.x.devReq.switchState.switchState >> i) & 0x1;
                    if (state == 0) {
                        fprintf(spOutput, "0");
                    } else {
                        fprintf(spOutput, "1");
                    }
                }
                break;
            case eBusDevRespSwitchState:
                fprintf(spOutput, "response switch state ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "switch state: ");
                for (i = 0; i < 8 /* 8 Switches */; i++) {
                    uint8_t state = (pBusMsg->msg.devBus.x.devResp.switchState.switchState >> i) & 0x1;
                    if (state == 0) {
                        fprintf(spOutput, "0");
                    } else {
                        fprintf(spOutput, "1");
                    }
                }
                break;
            case eBusDevReqSetClientAddr:
                fprintf(spOutput, "request set client address ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "client addresses: ");
                for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
                    uint8_t address = pBusMsg->msg.devBus.x.devReq.setClientAddr.clientAddr[i];
                    fprintf(spOutput, "%02x ", address);
                }
                break;
            case eBusDevRespSetClientAddr:
                fprintf(spOutput, "response set client address ");
                fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevReqGetClientAddr:
                fprintf(spOutput, "request get client address ");
                fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevRespGetClientAddr:
                fprintf(spOutput, "response get client address ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "client addresses: ");
                for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
                    uint8_t address = pBusMsg->msg.devBus.x.devResp.getClientAddr.clientAddr[i];
                    fprintf(spOutput, "%02x ", address);
                }
                break;
            case eBusDevReqSetAddr:
                fprintf(spOutput, "request set address ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "address: ");
                {
                    uint8_t address = pBusMsg->msg.devBus.x.devReq.setAddr.addr;
                    fprintf(spOutput, "%02x", address);
                }
                break;
            case eBusDevRespSetAddr:
                fprintf(spOutput, "response set address ");
                fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevReqEepromRead:
                fprintf(spOutput, "request read eeprom ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "address: %04x", pBusMsg->msg.devBus.x.devReq.readEeprom.addr);
                break;
            case eBusDevRespEepromRead:
                fprintf(spOutput, "request read eeprom ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "data: %02x", pBusMsg->msg.devBus.x.devResp.readEeprom.data);
                break;
            case eBusDevReqEepromWrite:
                fprintf(spOutput, "request write eeprom ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "address: %04x data: %02x",
                        pBusMsg->msg.devBus.x.devReq.writeEeprom.addr,
                        pBusMsg->msg.devBus.x.devReq.writeEeprom.data);
                break;
            case eBusDevRespEepromWrite:
                fprintf(spOutput, "response write eeprom ");
                fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevReqSetValue:
                fprintf(spOutput, "request set value ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                switch (pBusMsg->msg.devBus.x.devReq.setValue.devType) {
                case eBusDevTypeDo31:
                    fprintf(spOutput, SPACE "device: DO31\r\n");
                    fprintf(spOutput, SPACE "DO: ");
                    for (i = 0; i < BUS_DO31_DIGOUT_SIZE_SET_VALUE; i++) {
                        fprintf(spOutput, "%02x ", pBusMsg->msg.devBus.x.devReq.setValue.setValue.do31.digOut[i]);
                    }
                    fprintf(spOutput, "\r\n");
                    fprintf(spOutput, SPACE "SH: ");
                    for (i = 0; i < BUS_DO31_SHADER_SIZE_SET_VALUE; i++) {
                        fprintf(spOutput, "%02x ", pBusMsg->msg.devBus.x.devReq.setValue.setValue.do31.shader[i]);
                    }
                    break;
                case eBusDevTypeSw8:
                    fprintf(spOutput, SPACE "device: SW8\r\n");
                    fprintf(spOutput, SPACE "DO: ");
                    for (i = 0; i < BUS_SW8_DIGOUT_SIZE_SET_VALUE; i++) {
                        fprintf(spOutput, "%02x ", pBusMsg->msg.devBus.x.devReq.setValue.setValue.sw8.digOut[i]);
                    }
                    break;
                case eBusDevTypeSw16:
                    fprintf(spOutput, SPACE "device: SW16\r\n");
                    fprintf(spOutput, SPACE "led_state:   ");
                    for (i = 0; i < BUS_SW16_LED_SIZE_SET_VALUE; i++) {
                        fprintf(spOutput, "%02x ",
                                pBusMsg->msg.devBus.x.devReq.setValue.setValue.sw16.led_state[i]);
                    }
                    break;
                case eBusDevTypeRs485If:
                    fprintf(spOutput, SPACE "device: RS485IF\r\n");
                    fprintf(spOutput, SPACE "state: ");
                    for (i = 0; i < BUS_RS485IF_SIZE_SET_VALUE; i++) {
                        fprintf(spOutput, "%02x ", pBusMsg->msg.devBus.x.devReq.setValue.setValue.rs485if.state[i]);
                    }
                    break;
                case eBusDevTypePwm4:
                    fprintf(spOutput, SPACE "device: PWM4\r\n");
                    fprintf(spOutput, SPACE "set: %02x\n", pBusMsg->msg.devBus.x.devReq.setValue.setValue.pwm4.set);
                    fprintf(spOutput, SPACE "pwm: ");
                    for (i = 0; i < BUS_PWM4_PWM_SIZE_SET_VALUE; i++) {
                        fprintf(spOutput, "%04x ", pBusMsg->msg.devBus.x.devReq.setValue.setValue.pwm4.pwm[i]);
                    }
                    break;
                case eBusDevTypeKeyRc:
                    fprintf(spOutput, SPACE "device: KEYRC\r\n");
                    fprintf(spOutput, SPACE "command: ");
                    switch (pBusMsg->msg.devBus.x.devReq.setValue.setValue.keyrc.command) {
                    case eBusLockCmdNoAction: fprintf(spOutput, "no action"); break;
                    case eBusLockCmdLock:     fprintf(spOutput, "lock"); break;
                    case eBusLockCmdUnlock:   fprintf(spOutput, "unlock"); break;
                    case eBusLockCmdEto:      fprintf(spOutput, "eto"); break;
                    default: break;
                    }
                    fprintf(spOutput, " (%d)", pBusMsg->msg.devBus.x.devReq.setValue.setValue.keyrc.command);
                    break;
                  default:
                    fprintf(spOutput, SPACE "device: unknown");
                    break;
                }
                break;
            case eBusDevRespSetValue:
                fprintf(spOutput, "response set value ");
                fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevReqActualValue:
                fprintf(spOutput, "request actual value ");
                fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevRespActualValue:
                fprintf(spOutput, "response actual value ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                switch (pBusMsg->msg.devBus.x.devResp.actualValue.devType) {
                case eBusDevTypeDo31:
                    fprintf(spOutput, SPACE "device: DO31\r\n");
                    fprintf(spOutput, SPACE "DO: ");
                    for (i = 0; i < BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%02x ", pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.do31.digOut[i]);
                    }
                    fprintf(spOutput, "\r\n");
                    fprintf(spOutput, SPACE "SH: ");
                    for (i = 0; i < BUS_DO31_SHADER_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%02x ", pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.do31.shader[i]);
                    }
                    break;
                case eBusDevTypeSw8:
                    fprintf(spOutput, SPACE "device: SW8\r\n");
                    fprintf(spOutput, SPACE "state: %02x",
                            pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.sw8.state);
                    break;
                case eBusDevTypeLum:
                    fprintf(spOutput, SPACE "device: LUM\r\n");
                    fprintf(spOutput, SPACE "state: %02x\r\n",
                            pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.lum.state);
                    fprintf(spOutput, SPACE "adc:   %04x",
                            pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.lum.lum_low +
                            pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.lum.lum_high * 256);
                    break;
                case eBusDevTypeLed:
                    fprintf(spOutput, SPACE "device: LED\r\n");
                    fprintf(spOutput, SPACE "state: %02x",
                            pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.led.state);
                    break;
                case eBusDevTypeSw16:
                    fprintf(spOutput, SPACE "device: SW16\r\n");
                    fprintf(spOutput, SPACE "led_state:   ");
                    for (i = 0; i < BUS_SW16_LED_SIZE_SET_VALUE; i++) {
                        fprintf(spOutput, "%02x ",
                                pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.sw16.led_state[i]);
                    }
                    fprintf(spOutput, "\r\n");
                    fprintf(spOutput, SPACE "input_state: %02x",
                            pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.sw16.input_state);
                    break;
                case eBusDevTypeWind:
                    fprintf(spOutput, SPACE "device: WIND\r\n");
                    fprintf(spOutput, SPACE "state: %02x\r\n",
                            pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.wind.state);
                    fprintf(spOutput, SPACE "wind:  %02x",
                            pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.wind.wind);
                    break;
                case eBusDevTypeRs485If:
                    fprintf(spOutput, SPACE "device: RS485IF\r\n");
                    fprintf(spOutput, SPACE "state: ");
                    for (i = 0; i < BUS_RS485IF_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%02x ",
                                pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.rs485if.state[i]);
                    }
                    break;
                case eBusDevTypePwm4:
                    fprintf(spOutput, SPACE "device: PWM4\r\n");
                    fprintf(spOutput, SPACE "state: %02x\r\n",
                            pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.pwm4.state);
                    fprintf(spOutput, SPACE "pwm: ");
                    for (i = 0; i < BUS_PWM4_PWM_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%04x ",
                                pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.pwm4.pwm[i]);
                    }
                    break;
                case eBusDevTypeSmIf:
                    fprintf(spOutput, SPACE "device: SMIF\r\n");
                    fprintf(spOutput, SPACE "A+: %d Wh\r\n",   pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.smif.countA_plus);
                    fprintf(spOutput, SPACE "A-: %d Wh\r\n",   pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.smif.countA_minus);
                    fprintf(spOutput, SPACE "R+: %d varh\r\n", pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.smif.countR_plus);
                    fprintf(spOutput, SPACE "R-: %d varh\r\n", pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.smif.countR_minus);
                    fprintf(spOutput, SPACE "P+: %d W\r\n",    pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.smif.activePower_plus);
                    fprintf(spOutput, SPACE "P-: %d W\r\n",    pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.smif.activePower_minus);
                    fprintf(spOutput, SPACE "Q+: %d var\r\n",  pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.smif.reactivePower_plus);
                    fprintf(spOutput, SPACE "Q-: %d var",      pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.smif.reactivePower_minus);
                    break;
                case eBusDevTypeKeyb: {
                    uint8_t keyEvent = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.keyb.keyEvent;
                    fprintf(spOutput, SPACE "device: KEYB\r\n");
                    fprintf(spOutput, SPACE "key event: %d %s", keyEvent & ~0x80, (keyEvent & 0x80) ? "pressed": "released");
                    break;
                }
                case eBusDevTypeKeyRc: {
                    TBusLockState state = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.keyrc.state;
                    fprintf(spOutput, SPACE "device: KEYRC\r\n");
                    fprintf(spOutput, SPACE "lock state: ");
                    switch (state) {
                    case eBusLockInternal:     fprintf(spOutput, "internal"); break;
                    case eBusLockInvalid1:     fprintf(spOutput, "invalid1"); break;
                    case eBusLockInvalid2:     fprintf(spOutput, "invalid2"); break;
                    case eBusLockNoResp:       fprintf(spOutput, "no response"); break;
                    case eBusLockNoConnection: fprintf(spOutput, "no connection"); break;
                    case eBusLockUncalib:      fprintf(spOutput, "not calibrated"); break;
                    case eBusLockUnlocked:     fprintf(spOutput, "unlocked"); break;
                    case eBusLockLocked:       fprintf(spOutput, "locked"); break;
                    case eBusLockAgain:        fprintf(spOutput, "again"); break;
                    default:                   fprintf(spOutput, "unsupported state"); break;
                    }
                    fprintf(spOutput, " (%d)", state);
                    break;
                }
                case eBusDevTypeSg: {
                    uint8_t output = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.sg.output;
                    fprintf(spOutput, SPACE "device: SG\r\n");
                    fprintf(spOutput, SPACE "output: %02x", output);
                    break;
                }
                default:
                    fprintf(spOutput, SPACE "device: unknown");
                    break;
                }
                break;
            case eBusDevReqActualValueEvent:
                fprintf(spOutput, "request actual value event ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                switch (pBusMsg->msg.devBus.x.devReq.actualValueEvent.devType) {
                case eBusDevTypeDo31:
                    fprintf(spOutput, SPACE "device: DO31\r\n");
                    fprintf(spOutput, SPACE "DO: ");
                    for (i = 0; i < BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%02x ",
                                pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut[i]);
                    }
                    fprintf(spOutput, "\r\n");
                    fprintf(spOutput, SPACE "SH: ");
                    for (i = 0; i < BUS_DO31_SHADER_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%02x ",
                                pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.shader[i]);
                    }
                    break;
                case eBusDevTypeSw8:
                    fprintf(spOutput, SPACE "device: SW8\r\n");
                    fprintf(spOutput, SPACE "state: %02x",
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.sw8.state);
                    break;
                case eBusDevTypeLum:
                    fprintf(spOutput, SPACE "device: LUM\r\n");
                    fprintf(spOutput, SPACE "state: %02x\r\n",
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.lum.state);
                    fprintf(spOutput, SPACE "adc:   %04x",
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.lum.lum_low +
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.lum.lum_high * 256);
                    break;
                case eBusDevTypeLed:
                    fprintf(spOutput, SPACE "device: LED\r\n");
                    fprintf(spOutput, SPACE "state: %02x",
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.led.state);
                    break;
                case eBusDevTypeSw16:
                    fprintf(spOutput, SPACE "device: SW16\r\n");
                    fprintf(spOutput, SPACE "led_state:   ");
                    for (i = 0; i < BUS_SW16_LED_SIZE_SET_VALUE; i++) {
                        fprintf(spOutput, "%02x ",
                                pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.sw16.led_state[i]);
                    }
                    fprintf(spOutput, "\r\n");
                    fprintf(spOutput, SPACE "input_state: %02x",
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.sw16.input_state);
                    break;
                case eBusDevTypeWind:
                    fprintf(spOutput, SPACE "device: WIND\r\n");
                    fprintf(spOutput, SPACE "state: %02x\r\n",
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.wind.state);
                    fprintf(spOutput, SPACE "wind:  %02x",
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.wind.wind);
                    break;
                case eBusDevTypeRs485If:
                    fprintf(spOutput, SPACE "device: RS485IF\r\n");
                    fprintf(spOutput, SPACE "state: ");
                    for (i = 0; i < BUS_RS485IF_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%02x ",
                                pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.rs485if.state[i]);
                    }
                    break;
                case eBusDevTypePwm4:
                    fprintf(spOutput, SPACE "device: PWM4\r\n");
                    fprintf(spOutput, SPACE "state: %02x\r\n",
                            pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm4.state);
                    fprintf(spOutput, SPACE "pwm: ");
                    for (i = 0; i < BUS_PWM4_PWM_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%04x ",
                                pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm4.pwm[i]);
                    }
                    break;
                case eBusDevTypeSmIf:
                    fprintf(spOutput, SPACE "device: SMIF\r\n");
                    fprintf(spOutput, SPACE "A+: %d Wh\r\n",   pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.smif.countA_plus);
                    fprintf(spOutput, SPACE "A-: %d Wh\r\n",   pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.smif.countA_minus);
                    fprintf(spOutput, SPACE "R+: %d varh\r\n", pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.smif.countR_plus);
                    fprintf(spOutput, SPACE "R-: %d varh\r\n", pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.smif.countR_minus);
                    fprintf(spOutput, SPACE "P+: %d W\r\n",    pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.smif.activePower_plus);
                    fprintf(spOutput, SPACE "P-: %d W\r\n",    pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.smif.activePower_minus);
                    fprintf(spOutput, SPACE "Q+: %d var\r\n",  pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.smif.reactivePower_plus);
                    fprintf(spOutput, SPACE "Q-: %d var",      pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.smif.reactivePower_minus);
                    break;
                case eBusDevTypeKeyb: {
                    uint8_t keyEvent = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.keyb.keyEvent;
                    fprintf(spOutput, SPACE "device: KEYB\r\n");
                    fprintf(spOutput, SPACE "key event: %d %s", keyEvent & ~0x80, (keyEvent & 0x80) ? "pressed": "released");
                    break;
                }
                case eBusDevTypeSg: {
                    uint8_t output = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.sg.output;
                    fprintf(spOutput, SPACE "device: SG\r\n");
                    fprintf(spOutput, SPACE "output: %02x", output);
                    break;
                }
                default:
                    fprintf(spOutput, SPACE "device: unknown");
                    break;
                }
                break;
            case eBusDevRespActualValueEvent:
                fprintf(spOutput, "response actual value event ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                switch (pBusMsg->msg.devBus.x.devResp.actualValueEvent.devType) {
                case eBusDevTypeDo31:
                    fprintf(spOutput, SPACE "device: DO31\r\n");
                    fprintf(spOutput, SPACE "DO: ");
                    for (i = 0; i < BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%02x ", pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.do31.digOut[i]);
                    }
                    fprintf(spOutput, "\r\n");
                    fprintf(spOutput, SPACE "SH: ");
                    for (i = 0; i < BUS_DO31_SHADER_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%02x ", pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.do31.shader[i]);
                    }
                    break;
                case eBusDevTypeSw8:
                    fprintf(spOutput, SPACE "device: SW8\r\n");
                    fprintf(spOutput, SPACE "state: %02x",
                            pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.sw8.state);
                    break;
                case eBusDevTypeLum:
                    fprintf(spOutput, SPACE "device: LUM\r\n");
                    fprintf(spOutput, SPACE "state: %02x\r\n",
                            pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.lum.state);
                    fprintf(spOutput, SPACE "adc:   %04x",
                            pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.lum.lum_low +
                            pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.lum.lum_high * 256);
                    break;
                case eBusDevTypeLed:
                    fprintf(spOutput, SPACE "device: LED\r\n");
                    fprintf(spOutput, SPACE "state: %02x",
                            pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.led.state);
                    break;
                case eBusDevTypeSw16:
                    fprintf(spOutput, SPACE "device: SW16\r\n");
                    fprintf(spOutput, SPACE "led_state:   ");
                    for (i = 0; i < BUS_SW16_LED_SIZE_SET_VALUE; i++) {
                        fprintf(spOutput, SPACE "%02x ",
                                pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.sw16.led_state[i]);
                    }
                    fprintf(spOutput, "\r\n");
                    fprintf(spOutput, SPACE "input_state: %02x",
                            pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.sw16.input_state);
                    break;
                case eBusDevTypeWind:
                    fprintf(spOutput, SPACE "device: WIND\r\n");
                    fprintf(spOutput, SPACE "state: %02x\r\n",
                            pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.wind.state);
                    fprintf(spOutput, SPACE "wind:  %02x",
                            pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.wind.wind);
                    break;
                case eBusDevTypeRs485If:
                    fprintf(spOutput, SPACE "device: RS485IF\r\n");
                    fprintf(spOutput, SPACE "state: ");
                    for (i = 0; i < BUS_RS485IF_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%02x ",
                                pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.rs485if.state[i]);
                    }
                    break;
                case eBusDevTypePwm4:
                    fprintf(spOutput, SPACE "device: PWM4\r\n");
                    fprintf(spOutput, SPACE "state: %02x\r\n",
                            pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.pwm4.state);
                    fprintf(spOutput, SPACE "pwm: ");
                    for (i = 0; i < BUS_PWM4_PWM_SIZE_ACTUAL_VALUE; i++) {
                        fprintf(spOutput, "%04x ",
                                pBusMsg->msg.devBus.x.devResp.actualValueEvent.actualValue.pwm4.pwm[i]);
                    }
                    break;
                case eBusDevTypeKeyb: {
                    uint8_t keyEvent = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.keyb.keyEvent;
                    fprintf(spOutput, SPACE "device: KEYB\r\n");
                    fprintf(spOutput, SPACE "key event: %d %s", keyEvent & ~0x80, (keyEvent & 0x80) ? "pressed": "released");
                    break;
                }
                default:
                    fprintf(spOutput, SPACE "device: unknown");
                    break;
                }
                break;
            case eBusDevReqClockCalib:
                fprintf(spOutput, "request clock calibration ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "command: %d ", pBusMsg->msg.devBus.x.devReq.clockCalib.command);
                fprintf(spOutput, "address: %d ", pBusMsg->msg.devBus.x.devReq.clockCalib.address);
                break;
            case eBusDevRespClockCalib:
                fprintf(spOutput, "response clock calibration ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "state: %d ", pBusMsg->msg.devBus.x.devResp.clockCalib.state);
                fprintf(spOutput, "address: %d ", pBusMsg->msg.devBus.x.devResp.clockCalib.address);
                break;
            case eBusDevReqDoClockCalib:
                fprintf(spOutput, "request do clock calibration ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "state: %d ", pBusMsg->msg.devBus.x.devReq.doClockCalib.command);
                skipError = true;
                break;
            case eBusDevRespDoClockCalib:
                fprintf(spOutput, "response do clock calibration ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "state: %d ", pBusMsg->msg.devBus.x.devResp.doClockCalib.state);
                break;
            case eBusDevReqGetTime:
                fprintf(spOutput, "request get time ");
                fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevRespGetTime:
                fprintf(spOutput, "response get time ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "year:       %d\r\n", pBusMsg->msg.devBus.x.devResp.getTime.time.year);
                fprintf(spOutput, SPACE "month:      %d\r\n", pBusMsg->msg.devBus.x.devResp.getTime.time.month);
                fprintf(spOutput, SPACE "day:        %d\r\n", pBusMsg->msg.devBus.x.devResp.getTime.time.day);
                fprintf(spOutput, SPACE "hour:       %d\r\n", pBusMsg->msg.devBus.x.devResp.getTime.time.hour);
                fprintf(spOutput, SPACE "minute:     %d\r\n", pBusMsg->msg.devBus.x.devResp.getTime.time.minute);
                fprintf(spOutput, SPACE "second:     %d\r\n", pBusMsg->msg.devBus.x.devResp.getTime.time.second);
                fprintf(spOutput, SPACE "zoneHour:   %d\r\n", pBusMsg->msg.devBus.x.devResp.getTime.time.zoneHour);
                fprintf(spOutput, SPACE "zoneMinute: %d", pBusMsg->msg.devBus.x.devResp.getTime.time.zoneMinute);
                break;
            case eBusDevReqSetTime:
                fprintf(spOutput, "request set time ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "year:       %d\r\n", pBusMsg->msg.devBus.x.devReq.setTime.time.year);
                fprintf(spOutput, SPACE "month:      %d\r\n", pBusMsg->msg.devBus.x.devReq.setTime.time.month);
                fprintf(spOutput, SPACE "day:        %d\r\n", pBusMsg->msg.devBus.x.devReq.setTime.time.day);
                fprintf(spOutput, SPACE "hour:       %d\r\n", pBusMsg->msg.devBus.x.devReq.setTime.time.hour);
                fprintf(spOutput, SPACE "minute:     %d\r\n", pBusMsg->msg.devBus.x.devReq.setTime.time.minute);
                fprintf(spOutput, SPACE "second:     %d\r\n", pBusMsg->msg.devBus.x.devReq.setTime.time.second);
                fprintf(spOutput, SPACE "zoneHour:   %d\r\n", pBusMsg->msg.devBus.x.devReq.setTime.time.zoneHour);
                fprintf(spOutput, SPACE "zoneMinute: %d", pBusMsg->msg.devBus.x.devReq.setTime.time.zoneMinute);
                break;
            case eBusDevRespSetTime:
                fprintf(spOutput, "response set time ");
                fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
                break;
            case eBusDevReqGetVar:
                fprintf(spOutput, "request get var ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "index: %d", pBusMsg->msg.devBus.x.devReq.getVar.index);
                break;
            case eBusDevRespGetVar:
                fprintf(spOutput, "response get var ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "index: %d\r\n", pBusMsg->msg.devBus.x.devResp.getVar.index);
                fprintf(spOutput, SPACE "result: ");
                switch (pBusMsg->msg.devBus.x.devResp.getVar.result) {
                case eBusVarSuccess:
                    fprintf(spOutput, "success");
                    break;
                case eBusVarLengthError:
                    fprintf(spOutput, "length error");
                    break;
                case eBusVarIndexError:
                    fprintf(spOutput, "index error");
                    break;
                default:
                    fprintf(spOutput, "unknown error code (%d)", pBusMsg->msg.devBus.x.devResp.getVar.result);
                    break;
                }
                printf("\r\n");
                if (pBusMsg->msg.devBus.x.devResp.getVar.length > 0) {
                    fprintf(spOutput, SPACE "data:");
                    for (i = 0; i < pBusMsg->msg.devBus.x.devResp.getVar.length; i++) {
                        fprintf(spOutput, " %02x", pBusMsg->msg.devBus.x.devResp.getVar.data[i]);
                    }
                }
                break;
            case eBusDevReqSetVar:
                fprintf(spOutput, "request set var ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "index: %d\r\n", pBusMsg->msg.devBus.x.devReq.setVar.index);
                fprintf(spOutput, SPACE "data:");
                for (i = 0; i < pBusMsg->msg.devBus.x.devReq.setVar.length; i++) {
                    fprintf(spOutput, " %02x", pBusMsg->msg.devBus.x.devReq.setVar.data[i]);
                }
                break;
            case eBusDevRespSetVar:
                fprintf(spOutput, "response set var ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "index: %d\r\n", pBusMsg->msg.devBus.x.devResp.setVar.index);
                fprintf(spOutput, SPACE "result: ");
                switch (pBusMsg->msg.devBus.x.devResp.setVar.result) {
                case eBusVarSuccess:
                    fprintf(spOutput, "success");
                    break;
                case eBusVarLengthError:
                    fprintf(spOutput, "length error");
                    break;
                case eBusVarIndexError:
                    fprintf(spOutput, "index error");
                    break;
                default:
                    fprintf(spOutput, "unknown error code (%d)", pBusMsg->msg.devBus.x.devResp.setVar.result);
                    break;
                }
                break;
            case eBusDevReqGetFlashData:
                fprintf(spOutput, "request get flash data ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "addr: %08x", pBusMsg->msg.devBus.x.devReq.getFlashData.addr);
                break;
            case eBusDevRespGetFlashData:
                fprintf(spOutput, "response get flash data ");
                fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
                fprintf(spOutput, SPACE "addr: %08x\r\n", pBusMsg->msg.devBus.x.devResp.getFlashData.addr);
                fprintf(spOutput, SPACE "numValid: %d\r\n", pBusMsg->msg.devBus.x.devResp.getFlashData.numValid);
                fprintf(spOutput, SPACE "data:");
                for (i = 0; i < pBusMsg->msg.devBus.x.devResp.getFlashData.numValid; i++) {
                    fprintf(spOutput, " %02x", pBusMsg->msg.devBus.x.devResp.getFlashData.data[i]);
                }
                break;
            case eBusDevStartup:
                fprintf(spOutput, "device startup");
                break;
            default:
                fprintf(spOutput, "unknown frame type %x", pBusMsg->type);
                break;
            }
            fprintf(spOutput, "\r\n");
        } else if (ret == BUS_MSG_ERROR) {
            if (!skipError) {
                fprintf(spOutput, "frame error\r\n");
            }
        }
        fflush(spOutput);
    }
}
