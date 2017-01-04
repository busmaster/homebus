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
#ifdef WIN32
#include <conio.h>
#include <windows.h>
#endif

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

#define SIZE_COMPORT   100

/* eigene Adresse am Bus */
#define MY_ADDR    myAddr

/* default timeout for receiption of response telegram */
#define RESPONSE_TIMEOUT   1000 /* ms */

#define OP_INVALID                          0
#define OP_SET_NEW_ADDRESS                  1
#define OP_SET_CLIENT_ADDRESS_LIST          2
#define OP_GET_CLIENT_ADDRESS_LIST          3
#define OP_RESET                            4
#define OP_EEPROM_READ                      5
#define OP_EEPROM_WRITE                     6
#define OP_GET_ACTUAL_VALUE                 7
#define OP_SET_VALUE_DO31_DO                8
#define OP_SET_VALUE_DO31_SH                9
#define OP_SET_VALUE_SW8                    10
#define OP_SET_VALUE_SW16                   11
#define OP_SET_VALUE_RS485IF                13
#define OP_SET_VALUE_PWM4                   14
#define OP_INFO                             15
#define OP_EXIT                             16
#define OP_CLOCK_CALIB                      17
#define OP_SWITCH_STATE                     18
#define OP_HELP                             19
#define OP_GET_INFO_RANGE                   20

#define SIZE_CLIENT_LIST                    BUS_MAX_CLIENT_NUM

#define SIZE_EEPROM_BUF                     100

#define CMD_SIZE                            300

/*-----------------------------------------------------------------------------
*  Typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/
static uint8_t myAddr = 0;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PrintUsage(void);
static bool ModulReset(uint8_t address);
static bool ModulNewAddress(uint8_t address, uint8_t newAddress);
static bool ModulReadClientAddress(uint8_t address, uint8_t *pList, uint8_t listLen);
static bool ModulSetClientAddress(uint8_t address, uint8_t *pList, uint8_t listLen);
static bool ModulReadEeprom(uint8_t address, uint8_t *pBuf, unsigned int bufLen, unsigned int eepromAddress);
static bool ModulWriteEeprom(uint8_t address, uint8_t *pBuf, unsigned int bufLen, unsigned int eepromAddress);
static bool ModulGetActualValue(uint8_t address, TBusDevRespActualValue *pBuf);
static bool ModulSetValue(uint8_t address, TBusDevReqSetValue *pBuf);
static bool ModulInfo(uint8_t address, TBusDevRespInfo *pBuf, uint16_t resp_timeout);
static bool ModuleClockCalib(uint8_t address, uint8_t calibAddress);
static bool SwitchEvent(uint8_t clientAddr, uint8_t state);
static int  GetOperation(int argc, char *argv[], int *pArgi);

#ifndef WIN32
static unsigned long GetTickCount(void);
#endif
/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

    int                    handle;
    int                    i;
    int                    j;
    int                    k;
    char                   comPort[SIZE_COMPORT] = "";
    int                    operation;
    uint8_t                clientList[SIZE_CLIENT_LIST];
    uint8_t                eepromData[SIZE_EEPROM_BUF];
    uint8_t                moduleAddr;
    uint8_t                newModuleAddr;
    unsigned int           eepromAddress;
    unsigned int           eepromLength;
    uint8_t                *pBuf;
    bool                   ret = false;
    uint8_t                mask;
    TBusDevRespActualValue actVal;
    TBusDevReqSetValue     setVal;
    TBusDevRespInfo        info;
    bool                   server_run = false;
    uint8_t                len;
    int                    argi;
    char                   buffer[CMD_SIZE];
    char                   *p;
    uint8_t                val8;
    uint8_t                defaultMyAddr = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            /* get com interface */
            if (argc > i) {
                strncpy(comPort, argv[i + 1], sizeof(comPort) - 1);
                comPort[sizeof(comPort) - 1] = '\0';
            }
        } else if (strcmp(argv[i], "-a") == 0) {
            if (argc > i) {
                moduleAddr = atoi(argv[i + 1]);
            }
        } else if (strcmp(argv[i], "-o") == 0) {
            if (argc > i) {
                myAddr = atoi(argv[i + 1]);
                defaultMyAddr = myAddr;
            }
        } else if (strcmp(argv[i], "-s") == 0) {
            server_run = true;
            break;
        }
    }
    if (strlen(comPort) == 0) {
        PrintUsage();
        return -1;
    }

    SioInit();
    handle = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);
    if (handle == -1) {
        printf("cannot open %s\r\n", comPort);
        return -1;
    }

    // wait for sio input to settle and the flush
    usleep(100000);
    while ((len = SioGetNumRxChar(handle)) != 0) {
        SioRead(handle, &val8, sizeof(val8));
    }
    BusInit(handle);

    do {
        if (server_run) {
            p = fgets(buffer, sizeof(buffer), stdin);
            if (p == 0) {
                printf("error/EOF on reading from stdin. exiting\n");
                ret = 0;
                break;
            }
            /* remove the terminating newline */
            buffer[strlen(buffer) - 1] = '\0';
            p = strtok(buffer, " ");
            argc = 1;
            while (p != 0) {
                argv[argc] = p;
                p = strtok(0, " ");
                argc++;
            }

            myAddr = defaultMyAddr;

            for (i = 0; i < argc; i++) {
                if ((strcmp(argv[i], "-a") == 0) &&
                    (argc > i)) {
                    moduleAddr = atoi(argv[i + 1]);
                } else  if ((strcmp(argv[i], "-o") == 0) &&
                    (argc > i)) {
                    myAddr = atoi(argv[i + 1]);
                }
            }
        }
        operation = GetOperation(argc, argv, &argi);
        switch (operation) {
        case OP_SET_NEW_ADDRESS:
            newModuleAddr = atoi(argv[argi]);
            ret = ModulNewAddress(moduleAddr, newModuleAddr);
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_SET_CLIENT_ADDRESS_LIST:
            memset(clientList, 0xff, sizeof(clientList));
            for (j = argi, k = 0; (j < argc) && (k < (int)sizeof(clientList)); j++, k++) {
                clientList[k] = atoi(argv[j]);
            }
            ret = ModulSetClientAddress(moduleAddr, clientList, sizeof(clientList));
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_GET_CLIENT_ADDRESS_LIST:
            ret = ModulReadClientAddress(moduleAddr, clientList, sizeof(clientList));
            if (ret) {
                printf("client list: ");
                for (i = 0; i < (int)sizeof(clientList); i++) {
                    printf("%02x ", clientList[i]);
                }
                printf("\r\n");
            }
            break;
        case OP_RESET:
            ret = ModulReset(moduleAddr);
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_EEPROM_READ:
            eepromAddress = atoi(argv[argi]);
            if (argc > (argi + 1)) {
                eepromLength = atoi(argv[argi + 1]);
                pBuf = (uint8_t *)malloc(eepromLength);
                if (pBuf != 0) {
                    ret = ModulReadEeprom(moduleAddr, pBuf, eepromLength, eepromAddress);
                    if (ret) {
                        eepromLength += eepromAddress % 16;
                        printf("eeprom dump:");
                        for (i = 0; i < (int)eepromLength; i++) {
                            if ((i % 16) == 0) {
                                printf("\r\n%04x: ", eepromAddress + i - eepromAddress % 16);
                            }
                            if (i < ((int)eepromAddress % 16)) {
                                printf("   ");
                            } else {
                                printf("%02x ", *(pBuf + i - eepromAddress % 16));
                            }
                        }
                        printf("\r\n");
                    }
                    free(pBuf);
                } else {
                    printf("cannot allocate memory for command execution\r\n");
                }
            }
            break;
        case OP_EEPROM_WRITE:
            eepromAddress = atoi(argv[argi]);
            eepromLength = 0;
            for (j = argi + 1, k = 0; (j < argc) && (k < (int)sizeof(eepromData)); j++, k++) {
                eepromData[k] = atoi(argv[j]);
                eepromLength++;
            }
            ret = ModulWriteEeprom(moduleAddr, eepromData, eepromLength, eepromAddress);
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_GET_ACTUAL_VALUE:
            ret = ModulGetActualValue(moduleAddr, &actVal);
            if (ret) {
                printf("devType: ");
                switch (actVal.devType) {
                case eBusDevTypeDo31:
                    printf("DO31");
                    break;
                case eBusDevTypeSw8:
                    printf("SW8");
                    break;
                case eBusDevTypeSw16:
                    printf("SW16");
                    break;
                case eBusDevTypeLum:
                    printf("LUM");
                    break;
                case eBusDevTypeLed:
                    printf("LED");
                    break;
                case eBusDevTypeWind:
                    printf("WIND");
                    break;
                default:
                    break;
                }
                switch (actVal.devType) {
                case eBusDevTypeDo31:
                    printf("\r\ndigout: ");
                    for (i = 0; i < sizeof(actVal.actualValue.do31.digOut); i++) {
                        for (j = 0, mask = 1; j < 8; j++, mask <<= 1) {
                            if ((i == 3) && (j == 7)) {
                                // DO31 has 31 outputs, dont display last bit
                                break;
                            }
                            if (actVal.actualValue.do31.digOut[i] & mask) {
                                printf("1");
                            } else {
                                printf("0");
                            }
                        }
                        printf(" ");
                    }
                    printf("\r\nshader: ");
                    for (i = 0; i < sizeof(actVal.actualValue.do31.shader); i++) {
                        printf("%02x ", actVal.actualValue.do31.shader[i]);
                    }
                    printf("\r\n");
                    break;
                case eBusDevTypeSw8:
                    printf("\r\niostate: ");
                    for (j = 0, mask = 1; j < 8; j++, mask <<= 1) {
                        if (actVal.actualValue.sw8.state & mask) {
                            printf("1");
                        } else {
                            printf("0");
                        }
                    }
                    printf("\r\n");
                    break;
                case eBusDevTypeSw16:
                    printf("\r\ninput state: ");
                    for (j = 0, mask = 1; j < 8; j++, mask <<= 1) {
                        if (actVal.actualValue.sw16.input_state & mask) {
                            printf("1");
                        } else {
                            printf("0");
                        }
                    }
                    printf("\r\nled state: ");
                    for (i = 0; i < sizeof(actVal.actualValue.sw16.led_state); i++) {
                        printf("%x %x ", actVal.actualValue.sw16.led_state[i] & 0x0f, 
                                        (actVal.actualValue.sw16.led_state[i] & 0xf0) >> 4);
                    }
                    printf("\r\n");
                    break;
                case eBusDevTypeLum:
                    printf("\r\nstate: ");
                    for (j = 0, mask = 1; j < 8; j++, mask <<= 1) {
                        if (actVal.actualValue.lum.state & mask) {
                            printf("1");
                        } else {
                            printf("0");
                        }
                    }
                    printf("\r\nADC:   0x%04x\r\n", actVal.actualValue.lum.lum_low | (actVal.actualValue.lum.lum_high << 8));
                    break;
                case eBusDevTypeLed:
                    printf("\r\nLED state: %02x\r\n", actVal.actualValue.led.state);
                    break;
                case eBusDevTypeWind:
                    printf("\r\nstate: ");
                    for (j = 0, mask = 1; j < 8; j++, mask <<= 1) {
                        if (actVal.actualValue.wind.state & mask) {
                            printf("1");
                        } else {
                            printf("0");
                        }
                    }
                    printf("\r\nWIND:  0x%02x\r\n", actVal.actualValue.wind.wind);
                    break;
                case eBusDevTypeRs485If:
                    printf("\r\nstate: ");
                    for (i = 0; i < sizeof(actVal.actualValue.rs485if.state); i++) {
                        printf("%02x ", actVal.actualValue.rs485if.state[i]);
                    }
                    printf("\r\n");
                    break;
                case eBusDevTypePwm4:
                    printf("\r\npwm: ");
                    for (i = 0; i < BUS_PWM4_PWM_SIZE_ACTUAL_VALUE; i++) {
                        printf("%04x ", actVal.actualValue.pwm4.pwm[i]);
                    }
                    printf("\r\n");
                    break;
                default:
                    break;
                }
                printf("OK\r\n");
            }
            break;
        case OP_SET_VALUE_DO31_DO:
            setVal.devType = eBusDevTypeDo31;
            memset(setVal.setValue.do31.digOut, 0, sizeof(setVal.setValue.do31.digOut)); // default: no change
            memset(setVal.setValue.do31.shader, 254, sizeof(setVal.setValue.do31.shader)); // default: no change
            for (j = argi, k = 0; (j < argc) && (k < (int)(sizeof(setVal.setValue.do31.digOut) * 4 - 1)); j++, k++) {
                setVal.setValue.do31.digOut[k / 4] |= ((uint8_t)atoi(argv[j]) & 0x03) << ((k % 4) * 2);
            }
            ret = ModulSetValue(moduleAddr, &setVal);
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_SET_VALUE_DO31_SH:
            setVal.devType = eBusDevTypeDo31;
            memset(setVal.setValue.do31.digOut, 0, sizeof(setVal.setValue.do31.digOut)); // default: no change
            memset(setVal.setValue.do31.shader, 254, sizeof(setVal.setValue.do31.shader)); // default: no change
            for (j = argi, k = 0; (j < argc) && (k < (int)sizeof(setVal.setValue.do31.shader)); j++, k++) {
                setVal.setValue.do31.shader[k] = (uint8_t)atoi(argv[j]);
            }
            ret = ModulSetValue(moduleAddr, &setVal);
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_SET_VALUE_SW8:
            setVal.devType = eBusDevTypeSw8;
            memset(setVal.setValue.sw8.digOut, 0, sizeof(setVal.setValue.sw8.digOut)); // default: no change
            for (j = argi, k = 0; (j < argc) && (k < (int)(sizeof(setVal.setValue.sw8.digOut) * 4)); j++, k++) {
                setVal.setValue.sw8.digOut[k / 4] |= ((uint8_t)atoi(argv[j]) & 0x03) << ((k % 4) * 2);
            }
            ret = ModulSetValue(moduleAddr, &setVal);
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_SET_VALUE_SW16:
            setVal.devType = eBusDevTypeSw16;
            memset(setVal.setValue.sw16.led_state, 0, sizeof(setVal.setValue.sw16.led_state));
            for (j = argi, k = 0; (j < argc) && (k < (int)(sizeof(setVal.setValue.sw16.led_state) * 2)); j++, k++) {
                setVal.setValue.sw16.led_state[k / 2] |= ((uint8_t)atoi(argv[j]) & 0x0F) << ((k % 2) * 4);
            }
            ret = ModulSetValue(moduleAddr, &setVal);
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_SET_VALUE_RS485IF:
            setVal.devType = eBusDevTypeRs485If;
            memset(setVal.setValue.rs485if.state, 0, sizeof(setVal.setValue.rs485if.state));
            for (j = argi, k = 0; (j < argc) && (k < (int)sizeof(setVal.setValue.rs485if.state)); j++, k++) {
                setVal.setValue.rs485if.state[k] = (uint8_t)atoi(argv[j]);
            }
            ret = ModulSetValue(moduleAddr, &setVal);
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_SET_VALUE_PWM4:
            setVal.devType = eBusDevTypePwm4;
            memset(setVal.setValue.pwm4.pwm, 0, sizeof(setVal.setValue.pwm4.pwm));
            setVal.setValue.pwm4.mask = (uint8_t)atoi(argv[argi]);
            for (j = argi + 1, k = 0; (j < argc) && (k < (int)sizeof(setVal.setValue.pwm4.pwm)); j++, k++) {
                setVal.setValue.pwm4.pwm[k] = (uint16_t)atoi(argv[j]);
            }
            ret = ModulSetValue(moduleAddr, &setVal);
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_INFO:
            ret = ModulInfo(moduleAddr, &info, RESPONSE_TIMEOUT);
            if (ret) {
                printf("devType: ");
                switch (info.devType) {
                case eBusDevTypeDo31:
                    printf("DO31");
                    break;
                case eBusDevTypeSw8:
                    printf("SW8");
                    break;
                case eBusDevTypeLum:
                    printf("LUM");
                    break;
                case eBusDevTypeLed:
                    printf("LED");
                    break;
                case eBusDevTypeSw16:
                    printf("SW16");
                    break;
                case eBusDevTypeWind:
                    printf("WIND");
                    break;
                case eBusDevTypeRs485If:
                    printf("RS485IF");
                    break;
                case eBusDevTypePwm4:
                    printf("PWM4");
                    break;
                default:
                    break;
                }
                printf("\r\n");
                printf("version: %s\r\n", info.version);
            } else {
                printf("ERROR\r\n");
            }
            break;
        case OP_CLOCK_CALIB:
            ret = ModuleClockCalib(moduleAddr, atoi(argv[argi]));
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_SWITCH_STATE:
            ret = SwitchEvent(moduleAddr, atoi(argv[argi]));
            if (ret) {
                printf("OK\r\n");
            }
            break;
        case OP_GET_INFO_RANGE:
            if((argc - argi) > 1 ) {
                for (moduleAddr = atoi(argv[argi]); ; moduleAddr++) {
                    ret = ModulInfo(moduleAddr, &info, 100);
                    if (ret) {
                        printf("%-8i",moduleAddr);
                        switch (info.devType) {
                        case eBusDevTypeDo31:
                            printf("%-8s", "DO31");
                            break;
                        case eBusDevTypeSw8:
                            printf("%-8s", "SW8");
                            break;
                        case eBusDevTypeLum:
                            printf("%-8s", "LUM");
                            break;
                        case eBusDevTypeLed:
                            printf("%-8s", "LED");
                            break;
                        case eBusDevTypeSw16:
                            printf("%-8s", "SW16");
                            break;
                        case eBusDevTypeWind:
                            printf("%-8s", "WIND");
                            break;
                        case eBusDevTypeRs485If:
                            printf("%-8s", "RS485IF");
                            break;
                        case eBusDevTypePwm4:
                            printf("%-8s", "PWM4");
                            break;
                        default:
                            break;
                        }
                        printf("%s\r\n", info.version);
                    } else {
                        printf("%i\r\n",moduleAddr);
                        ret = 1;
                    }
                    if (moduleAddr == atoi(argv[argi+1])) {
                        break;
                    }
                }
            }
            break;
        case OP_HELP:
            PrintUsage();
            break;
        case OP_EXIT:
            printf("OK\r\n");
            server_run = false;
            break;
        default:
            break;
        }
    } while (server_run);

    if (handle != -1) {
        SioClose(handle);
    }
    if (ret) {
        return 0;
    } else {
        printf("ERROR\r\n");
        return -1;
    }
}

/*-----------------------------------------------------------------------------
*  set value
*/
static bool ModulSetValue(uint8_t address, TBusDevReqSetValue *pSetVal) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;

    txBusMsg.type = eBusDevReqSetValue;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;
    memcpy(&txBusMsg.msg.devBus.x.devReq.setValue, pSetVal, sizeof(txBusMsg.msg.devBus.x.devReq.setValue));
    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespSetValue) &&
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

/*-----------------------------------------------------------------------------
*  read info
*/
static bool ModulInfo(uint8_t address, TBusDevRespInfo *pBuf, uint16_t resp_timeout) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;

    txBusMsg.type = eBusDevReqInfo;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;
    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            startTimeMs = GetTickCount();
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespInfo) &&
                (pBusMsg->senderAddr == address)) {
                responseOk = true;
            }
        } else {
            if ((actualTimeMs - startTimeMs) > resp_timeout) {
                timeOut = true;
            }
        }
    } while (!responseOk && !timeOut);

    if (responseOk) {
        pBuf->devType = pBusMsg->msg.devBus.x.devResp.info.devType;
        memcpy(pBuf->version , pBusMsg->msg.devBus.x.devResp.info.version, sizeof(pBuf->version));
        switch (pBuf->devType) {
        case eBusDevTypeDo31:
            memcpy(&pBuf->devInfo.do31 , &pBusMsg->msg.devBus.x.devResp.info.devInfo.do31, sizeof(pBuf->devInfo.do31));
            break;
        case eBusDevTypeSw8:
            memcpy(&pBuf->devInfo.sw8 , &pBusMsg->msg.devBus.x.devResp.info.devInfo.sw8, sizeof(pBuf->devInfo.sw8));
            break;
        default:
            break;
        }
        return true;
    } else {
        return false;
    }
}

/*-----------------------------------------------------------------------------
*  read actual value
*/
static bool ModulGetActualValue(uint8_t address, TBusDevRespActualValue *pBuf) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;

    txBusMsg.type = eBusDevReqActualValue;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;
    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespActualValue) &&
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
        pBuf->devType = pBusMsg->msg.devBus.x.devResp.actualValue.devType;
        switch (pBuf->devType) {
        case eBusDevTypeDo31:
            memcpy(pBuf->actualValue.do31.digOut,
                   pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.do31.digOut,
                   sizeof(pBuf->actualValue.do31.digOut));
            memcpy(pBuf->actualValue.do31.shader,
                   pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.do31.shader,
                   sizeof(pBuf->actualValue.do31.shader));
            break;
        case eBusDevTypeSw8:
            pBuf->actualValue.sw8.state = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.sw8.state;
            break;
        case eBusDevTypeSw16:
            memcpy(pBuf->actualValue.sw16.led_state, pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.sw16.led_state, 
                   sizeof(pBuf->actualValue.sw16.led_state));
            pBuf->actualValue.sw16.input_state = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.sw16.input_state;
            break;
        case eBusDevTypeLum:
            pBuf->actualValue.lum.state = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.lum.state;
            pBuf->actualValue.lum.lum_high = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.lum.lum_high;
            pBuf->actualValue.lum.lum_low = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.lum.lum_low;
            break;
        case eBusDevTypeLed:
            pBuf->actualValue.led.state = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.led.state;
            break;
        case eBusDevTypeWind:
            pBuf->actualValue.wind.state = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.wind.state;
            pBuf->actualValue.wind.wind = pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.wind.wind;
            break;
        case eBusDevTypeRs485If:
            memcpy(pBuf->actualValue.rs485if.state,
                   pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.rs485if.state,
                   sizeof(pBuf->actualValue.rs485if.state));
            break;
        case eBusDevTypePwm4:
            memcpy(pBuf->actualValue.pwm4.pwm,
                   pBusMsg->msg.devBus.x.devResp.actualValue.actualValue.pwm4.pwm,
                   sizeof(pBuf->actualValue.pwm4.pwm));
            break;
        default:
            break;
        }
        return true;
    } else {
        return false;
    }
}

/*-----------------------------------------------------------------------------
*  module reset
*/
static bool ModulReset(uint8_t address) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;

    txBusMsg.type = eBusDevReqReboot;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;
    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevStartup) &&
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

/*-----------------------------------------------------------------------------
*  set new bus module address
*/
static bool ModulNewAddress(uint8_t address, uint8_t newAddress) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;

    txBusMsg.type = eBusDevReqSetAddr;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;
    txBusMsg.msg.devBus.x.devReq.setAddr.addr = newAddress;
    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespSetAddr)         &&
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

/*-----------------------------------------------------------------------------
*  read client address list from bus module
*/
static bool ModulReadClientAddress(uint8_t address, uint8_t *pList, uint8_t listLen) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;
    int             i;

    txBusMsg.type = eBusDevReqGetClientAddr;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;
    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespGetClientAddr)   &&
                (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
                (pBusMsg->senderAddr == address)) {
                responseOk = true;
                for (i = 0; (i < BUS_MAX_CLIENT_NUM) && (i < listLen); i++, pList++) {
                    *pList = pBusMsg->msg.devBus.x.devResp.getClientAddr.clientAddr[i];
                }
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

/*-----------------------------------------------------------------------------
*  set new client address list
*/
static bool ModulSetClientAddress(uint8_t address, uint8_t *pList, uint8_t listLen) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;
    int             i;

    txBusMsg.type = eBusDevReqSetClientAddr;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;
    memset(txBusMsg.msg.devBus.x.devReq.setClientAddr.clientAddr, 0xff, BUS_MAX_CLIENT_NUM);
    for (i = 0; i < listLen; i++) {
        txBusMsg.msg.devBus.x.devReq.setClientAddr.clientAddr[i] = pList[i];
    }
    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespSetClientAddr)   &&
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

/*-----------------------------------------------------------------------------
*  read eeprom data from bus module
*/
static bool ModulReadEeprom(
    uint8_t address,
    uint8_t *pBuf,
    unsigned int bufLen,
    unsigned int eepromAddress) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;
    unsigned int    currentAddress = eepromAddress;
    unsigned int    numRead = 0;

    txBusMsg.type = eBusDevReqEepromRead;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;

    while (numRead < bufLen) {
        txBusMsg.msg.devBus.x.devReq.readEeprom.addr = (uint16_t)currentAddress;
        BusSend(&txBusMsg);
        startTimeMs = GetTickCount();
        do {
            actualTimeMs = GetTickCount();
            ret = BusCheck();
            if (ret == BUS_MSG_OK) {
                pBusMsg = BusMsgBufGet();
                if ((pBusMsg->type == eBusDevRespEepromRead) &&
                    (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
                    (pBusMsg->senderAddr == address)) {
                    *pBuf = pBusMsg->msg.devBus.x.devResp.readEeprom.data;
                    numRead++;
                    currentAddress++;
                    pBuf++;
                    responseOk = true;
                }
            } else {
                if ((actualTimeMs - startTimeMs) > RESPONSE_TIMEOUT) {
                    timeOut = true;
                }
            }
        } while (!responseOk && !timeOut);
        if (!responseOk) {
            break;
        }
        responseOk = false;
    }

    if (numRead == bufLen) {
        return true;
    } else {
        return false;
    }
}

/*-----------------------------------------------------------------------------
*  write eeprom data from bus module
*/
static bool ModulWriteEeprom(
    uint8_t address,
    uint8_t *pBuf,
    unsigned int bufLen,
    unsigned int eepromAddress) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;
    unsigned int    currentAddress = eepromAddress;
    unsigned int    numWritten = 0;

    txBusMsg.type = eBusDevReqEepromWrite;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;

    while (numWritten < bufLen) {
        txBusMsg.msg.devBus.x.devReq.writeEeprom.addr = (uint16_t)currentAddress;
        txBusMsg.msg.devBus.x.devReq.writeEeprom.data = *pBuf;
        BusSend(&txBusMsg);
        startTimeMs = GetTickCount();
        do {
            actualTimeMs = GetTickCount();
            ret = BusCheck();
            if (ret == BUS_MSG_OK) {
                pBusMsg = BusMsgBufGet();
                if ((pBusMsg->type == eBusDevRespEepromWrite) &&
                    (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
                    (pBusMsg->senderAddr == address)) {
                    numWritten++;
                    currentAddress++;
                    pBuf++;
                    responseOk = true;
                }
            } else {
                if ((actualTimeMs - startTimeMs) > RESPONSE_TIMEOUT) {
                    timeOut = true;
                }
            }
        } while (!responseOk && !timeOut);
        if (!responseOk) {
            break;
        }
        responseOk = false;
    }

    if (numWritten == bufLen) {
        return true;
    } else {
        return false;
    }
}



/*-----------------------------------------------------------------------------
*  clock calibration
*/
static bool ClockCalib(
    uint8_t address,
    uint8_t calibAddress,
    TClockCalibCommand command,
    TClockCalibState *pState,
    uint8_t *pStateAddress
    ) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;

    txBusMsg.type = eBusDevReqClockCalib;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = address;
    txBusMsg.msg.devBus.x.devReq.clockCalib.address = calibAddress;
    txBusMsg.msg.devBus.x.devReq.clockCalib.command = command;

    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespClockCalib) &&
                (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
                (pBusMsg->senderAddr == address)) {
                *pState = pBusMsg->msg.devBus.x.devResp.clockCalib.state;
                *pStateAddress = pBusMsg->msg.devBus.x.devResp.clockCalib.address;
                responseOk = true;
            }
        } else {
            if ((actualTimeMs - startTimeMs) > 20000) {
                timeOut = true;
            }
        }
    } while (!responseOk && !timeOut);

    return responseOk;
}

static bool ModuleClockCalib(
    uint8_t moduleAddress,
    uint8_t calibAddress
    ) {
    bool rc = false;
    TClockCalibState       clockCalibState;
    uint8_t                stateAddress;

    if (!ClockCalib(moduleAddress, calibAddress, eBusClockCalibCommandGetState, &clockCalibState, &stateAddress)) {
        return false;
    }

    if (clockCalibState == eBusClockCalibStateBusy) {
        printf("calibrating %d\r\n", stateAddress);
        return true;
    } else if (clockCalibState != eBusClockCalibStateIdle) {
        if (!ClockCalib(moduleAddress, calibAddress, eBusClockCalibCommandIdle, &clockCalibState, &stateAddress)) {
            printf("command idle error\r\n");
            return false;
        }
    }
    
    if (!ClockCalib(moduleAddress, calibAddress, eBusClockCalibCommandCalibrate, &clockCalibState, &stateAddress)) {
        printf("command calibrate error\r\n");
        return false;
    }

    do {
        if (clockCalibState == eBusClockCalibStateBusy) {
#ifdef WIN32
            Sleep(300);
#else
            usleep(300000); 
#endif
        } else {
            if (clockCalibState == eBusClockCalibStateSuccess) {
                rc = true;
            } else {
                printf("command calibrate result %d\r\n", clockCalibState);
            }
            break;
        }
    } while (ClockCalib(moduleAddress, calibAddress, eBusClockCalibCommandGetState, &clockCalibState, &stateAddress));

    return rc;
}

/*-----------------------------------------------------------------------------
*  set new bus module address
*/
static bool SwitchEvent(uint8_t clientAddr, uint8_t state) {

    TBusTelegram    txBusMsg;
    uint8_t         ret;
    unsigned long   startTimeMs;
    unsigned long   actualTimeMs;
    TBusTelegram    *pBusMsg;
    bool            responseOk = false;
    bool            timeOut = false;

    txBusMsg.type = eBusDevReqSwitchState;
    txBusMsg.senderAddr = MY_ADDR;
    txBusMsg.msg.devBus.receiverAddr = clientAddr;
    txBusMsg.msg.devBus.x.devReq.switchState.switchState = state;
    BusSend(&txBusMsg);
    startTimeMs = GetTickCount();
    do {
        actualTimeMs = GetTickCount();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespSwitchState)                      &&
                (pBusMsg->msg.devBus.receiverAddr == MY_ADDR)                  &&
                (pBusMsg->senderAddr == clientAddr)                            &&
                (pBusMsg->msg.devBus.x.devResp.switchState.switchState == state)) {
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
*  get requested operation
*/
static int GetOperation(int argc, char *argv[], int *pArgi) {

    int i;
    int operation = OP_INVALID;
    *pArgi = 0;

    /* set new module address */
    for (i = 1; (i < argc) && (operation == OP_INVALID); i++) {
        if (strcmp(argv[i], "-na") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_SET_NEW_ADDRESS;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-setcl") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_SET_CLIENT_ADDRESS_LIST;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-getcl") == 0) {
            operation = OP_GET_CLIENT_ADDRESS_LIST;
        } else if (strcmp(argv[i], "-reset") == 0) {
            operation = OP_RESET;
        } else if (strcmp(argv[i], "-eerd") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_EEPROM_READ;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-eewr") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_EEPROM_WRITE;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-actval") == 0) {
            operation = OP_GET_ACTUAL_VALUE;
        } else if (strcmp(argv[i], "-setvaldo31_do") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_SET_VALUE_DO31_DO;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-setvaldo31_sh") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_SET_VALUE_DO31_SH;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-setvalsw8") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_SET_VALUE_SW8;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-setvalsw16") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_SET_VALUE_SW16;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-setvalrs485if") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_SET_VALUE_RS485IF;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-setvalpwm4") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_SET_VALUE_PWM4;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-info") == 0) {
            operation = OP_INFO;
        } else if (strcmp(argv[i], "-clockcalib") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_CLOCK_CALIB;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-switchstate") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_SWITCH_STATE;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-inforange") == 0) {
            if (argc > i) {
                *pArgi = i + 1;
                operation = OP_GET_INFO_RANGE;
            } else {
                break;
            }
        } else if (strcmp(argv[i], "-help") == 0) {
            operation = OP_HELP;
        } else if (strcmp(argv[i], "-exit") == 0) {
            operation = OP_EXIT;
        }
    }

    return operation;
}

/*-----------------------------------------------------------------------------
*  show help
*/
static void PrintUsage(void) {

    printf("\r\nUsage:\r\n");
    printf("modulservice -c port -a addr [-o ownaddr] [-s]                \r\n");  
    printf("                             (-na addr                       |\r\n");
    printf("                              -setcl addr1 .. addr16         |\r\n");
    printf("                              -getcl                         |\r\n");
    printf("                              -reset                         |\r\n");
    printf("                              -eerd addr len                 |\r\n");
    printf("                              -eewr addr data1 .. dataN      |\r\n");
    printf("                              -actval                        |\r\n");
    printf("                              -setvaldo31_do do0 .. do30     |\r\n");
    printf("                              -setvaldo31_sh sh0 .. sh14     |\r\n");
    printf("                              -setvalsw8 do0 .. do7          |\r\n");
    printf("                              -setvalsw16 led0 .. led7       |\r\n");
    printf("                              -setvalrs485if data0 .. data31 |\r\n");
    printf("                              -setvalpwm4 mask pwm0 .. pwm3  |\r\n");
    printf("                              -info                          |\r\n");
    printf("                              -inforange start stopp         |\r\n");	
    printf("                              -clockcalib addr               |\r\n");
    printf("                              -switchstate data              |\r\n");
    printf("                              -help                          |\r\n");
    printf("                              -exit)                          \r\n");
    printf("-c port: com1 com2 ..\r\n");
    printf("-a addr: addr = address of module\r\n");
    printf("-o addr: addr = our address, default:0\r\n");
    printf("-s server mode, accept command from stdin\r\n");
    printf("-na addr: set new address, addr = new address\r\n");
    printf("-setcl addr1 .. addr16 : set client address list, addr1 = 1st client's address\r\n");
    printf("-getcl: show client address list\r\n");
    printf("-reset: reset module\r\n");
    printf("-eerd addr len: read EEPROM data from offset addr with number len\r\n");
    printf("-eewr addr data1 .. dataN: write EEPROM data from offset\r\n");
    printf("-actval: read actual values from modul\r\n");
    printf("-setvaldo31_do do0 .. do30: set value for dig out\r\n");
    printf("-setvaldo31_sh sh0 .. sh14: set value for shader\r\n");
    printf("-setvalsw8 do0 .. do7: set value for dig out\r\n");
    printf("-setvalsw16 led0 .. led7: set value for led\r\n");
    printf("-setvalrs485if data0 .. data31: set byte value for rs485if\r\n");
    printf("-setvalpwm4 mask pwm0 .. pwm3: mask = bit mask for channel, pwmX = 16 bit value\r\n");
    printf("-info: read type and version string from modul\r\n");
    printf("-inforange start stopp: read type and version string from modul start to stopp address\r\n");	
    printf("-clockcalib: clock calibration\r\n");
    printf("-switchstate: generate a switch pressed or released event (ReqSwitchState, -a addr is the client)\r\n");
    printf("-help: print this help\r\n");
    printf("-exit: nop operation, exit server\r\n");
}
