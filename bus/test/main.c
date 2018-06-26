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

#define RX_TIMEOUT     100

/* telegram base sizes without STX + Checksum */
#define MSG_SIZE1      2
#define MSG_SIZE2      3

/*-----------------------------------------------------------------------------
*  print help
*/
static void PrintUsage(void) {

    printf("\nUsage:");
    printf("bustestr -c por\n");
}

static int TestTelegram(TBusTelegram *pTxMsg, uint8_t msgSize) {

    TBusTelegram    *pRxMsg = BusMsgBufGet();
    int             timeout;
    uint8_t         zerobuf[sizeof(TBusTelegram)];

    memset(zerobuf, 0, sizeof(zerobuf));
    memset(pRxMsg, 0, sizeof(*pRxMsg));
    if (BusSend(pTxMsg) != BUS_SEND_OK) {
        return -1;
    }
    for (timeout = 0; (timeout < RX_TIMEOUT) && (BusCheck() != BUS_MSG_OK); timeout++) {
    	usleep(1000);
    }

    if (timeout < RX_TIMEOUT) {
        if (memcmp(pTxMsg, pRxMsg, msgSize) != 0) {
        	return -1;
        }
        if (memcmp((uint8_t *)pRxMsg + msgSize, zerobuf, sizeof(TBusTelegram) - msgSize) != 0) {
        	return -1;
        }
    } else {
    	return -1;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
*  print decoded telegrams
*/
static int Test(void) {

    int             i;
    TBusTelegram    txMsg;

	txMsg.type = eBusButtonPressed1;
	txMsg.senderAddr = 66;
	if (TestTelegram(&txMsg, MSG_SIZE1) != 0) {
		return -1;
	}

	txMsg.type = eBusButtonPressed2;
    txMsg.senderAddr = 66;
	if (TestTelegram(&txMsg, MSG_SIZE1) != 0) {
		return -1;
	}

	txMsg.type = eBusButtonPressed1_2;
	txMsg.senderAddr = 66;
	if (TestTelegram(&txMsg, MSG_SIZE1) != 0) {
		return -1;
	}

	txMsg.type = eBusDevReqReboot;
	txMsg.senderAddr = 66;
	txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

	txMsg.type = eBusDevReqUpdEnter;
	txMsg.senderAddr = 66;
	txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

	txMsg.type = eBusDevRespUpdEnter;
	txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

	txMsg.type = eBusDevReqUpdData;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.updData.wordAddr = 0x4567;
    for (i = 0; i < (BUS_FWU_PACKET_SIZE / 2); i++) {
        txMsg.msg.devBus.x.devReq.updData.data[i] = i + 0x50;
    }
	if (TestTelegram(&txMsg, MSG_SIZE2 + 2 + BUS_FWU_PACKET_SIZE) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespUpdData;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.updData.wordAddr = 0x789a;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqUpdTerm;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespUpdTerm;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.updTerm.success = 0x78;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespInfo;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.info.devType = eBusDevTypeDo31; // 0
    for (i = 0; i < BUS_DEV_INFO_VERSION_LEN; i++) {
        txMsg.msg.devBus.x.devResp.info.version[i] = 0x10 + i;
    }
    for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
        txMsg.msg.devBus.x.devResp.info.devInfo.do31.dirSwitch[i] = i + 0x50;
        txMsg.msg.devBus.x.devResp.info.devInfo.do31.onSwitch[i] = i + 0x70;
    }
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + BUS_DEV_INFO_VERSION_LEN + BUS_DO31_NUM_SHADER * 2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespSetState;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqSetState;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.setState.devType = eBusDevTypeDo31; // 0
    for (i = 0; i < BUS_DO31_DIGOUT_SIZE_SET; i++) {
        txMsg.msg.devBus.x.devReq.setState.state.do31.digOut[i] = 0x30 + i;
    }
    for (i = 0; i < BUS_DO31_SHADER_SIZE_SET; i++) {
        txMsg.msg.devBus.x.devReq.setState.state.do31.shader[i] = 0x60 + i;
    }
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + BUS_DO31_DIGOUT_SIZE_SET + BUS_DO31_SHADER_SIZE_SET) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqGetState;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespGetState;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.getState.devType = eBusDevTypeDo31; // 0
    for (i = 0; i < BUS_DO31_DIGOUT_SIZE_GET; i++) {
        txMsg.msg.devBus.x.devResp.getState.state.do31.digOut[i] = 0x30 + i;
    }
    for (i = 0; i < BUS_DO31_SHADER_SIZE_GET; i++) {
        txMsg.msg.devBus.x.devResp.getState.state.do31.shader[i] = 0x60 + i;
    }
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + BUS_DO31_DIGOUT_SIZE_GET + BUS_DO31_SHADER_SIZE_GET) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqSwitchState;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.switchState.switchState = 0x21;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespSwitchState;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.switchState.switchState = 0x22;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqGetClientAddr;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespGetClientAddr;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
        txMsg.msg.devBus.x.devResp.getClientAddr.clientAddr[i] = i + 0x20;
    }
	if (TestTelegram(&txMsg, MSG_SIZE2 + BUS_MAX_CLIENT_NUM) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqSetClientAddr;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
        txMsg.msg.devBus.x.devReq.setClientAddr.clientAddr[i] = i + 0x10;
    }
	if (TestTelegram(&txMsg, MSG_SIZE2 + BUS_MAX_CLIENT_NUM) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespSetClientAddr;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

	txMsg.type = eBusDevReqSetAddr;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.setAddr.addr = 0x56;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespSetAddr;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

	txMsg.type = eBusDevReqEepromRead;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.readEeprom.addr = 0xabcd;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespEepromRead;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.readEeprom.data = 5;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1) != 0) {
		return -1;
	}

	txMsg.type = eBusDevReqEepromWrite;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.writeEeprom.addr = 0x7879;
    txMsg.msg.devBus.x.devReq.writeEeprom.data = 5;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 2 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespEepromWrite;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqSetValue;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.setValue.devType = eBusDevTypeSw8; // 1
    txMsg.msg.devBus.x.devReq.setValue.setValue.sw8.digOut[0] = 3;
    txMsg.msg.devBus.x.devReq.setValue.setValue.sw8.digOut[1] = 4;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespSetValue;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqActualValue;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespActualValue;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.actualValue.devType = eBusDevTypePwm4; // 8
    txMsg.msg.devBus.x.devResp.actualValue.actualValue.pwm4.state = 0x12;
    txMsg.msg.devBus.x.devResp.actualValue.actualValue.pwm4.pwm[0] = 0x2345;
    txMsg.msg.devBus.x.devResp.actualValue.actualValue.pwm4.pwm[1] = 0x3456;
    txMsg.msg.devBus.x.devResp.actualValue.actualValue.pwm4.pwm[2] = 0x4567;
    txMsg.msg.devBus.x.devResp.actualValue.actualValue.pwm4.pwm[3] = 0x5678;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 1 + 8) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqActualValueEvent;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.actualValueEvent.devType = eBusDevTypeSw8; // 1
    txMsg.msg.devBus.x.devReq.actualValueEvent.actualValue.sw8.state = 5;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespActualValueEvent;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.actualValueEvent.devType = eBusDevTypePwm4; // 8
    txMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.pwm4.state = 0x12;
    txMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.pwm4.pwm[0] = 0x2345;
    txMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.pwm4.pwm[1] = 0x3456;
    txMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.pwm4.pwm[2] = 0x4567;
    txMsg.msg.devBus.x.devResp.actualValueEvent.actualValue.pwm4.pwm[3] = 0x5678;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 1 + 8) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqClockCalib;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.clockCalib.command = eBusClockCalibCommandGetState; // 2
    txMsg.msg.devBus.x.devReq.clockCalib.address = 3;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespClockCalib;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.clockCalib.state = eBusClockCalibStateError; // 3
    txMsg.msg.devBus.x.devResp.clockCalib.address = 4;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqDoClockCalib;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.doClockCalib.command = eBusDoClockCalibContiune; // 1
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespDoClockCalib;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateContiune; // 1
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqDiag;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespDiag;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.diag.devType = eBusDevTypeSmIf; // 9
    for (i = 0; i < BUS_DIAG_SIZE; i++) {
        txMsg.msg.devBus.x.devResp.diag.data[i] = i;
    }
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + BUS_DIAG_SIZE) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespGetTime;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.getTime.time.year = 0x1234;
    txMsg.msg.devBus.x.devResp.getTime.time.month = 1;
    txMsg.msg.devBus.x.devResp.getTime.time.day = 2;
    txMsg.msg.devBus.x.devResp.getTime.time.hour = 3;
    txMsg.msg.devBus.x.devResp.getTime.time.minute = 4;
    txMsg.msg.devBus.x.devResp.getTime.time.second = 5;
    txMsg.msg.devBus.x.devResp.getTime.time.zoneHour = 6;
    txMsg.msg.devBus.x.devResp.getTime.time.zoneMinute = 7;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 9) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqGetTime;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqSetTime;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.setTime.time.year = 0x1234;
    txMsg.msg.devBus.x.devReq.setTime.time.month = 1;
    txMsg.msg.devBus.x.devReq.setTime.time.day = 2;
    txMsg.msg.devBus.x.devReq.setTime.time.hour = 3;
    txMsg.msg.devBus.x.devReq.setTime.time.minute = 4;
    txMsg.msg.devBus.x.devReq.setTime.time.second = 5;
    txMsg.msg.devBus.x.devReq.setTime.time.zoneHour = 6;
    txMsg.msg.devBus.x.devReq.setTime.time.zoneMinute = 7;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 9) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespSetTime;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevStartup;
    txMsg.senderAddr = 66;
	if (TestTelegram(&txMsg, MSG_SIZE1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqInfo;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
	if (TestTelegram(&txMsg, MSG_SIZE2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqGetVar;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.getVar.index = 0x12;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1) != 0) {
		return -1;
	}

	txMsg.type = eBusDevRespGetVar;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.getVar.index = 0x34;
    txMsg.msg.devBus.x.devResp.getVar.length = 5;
    txMsg.msg.devBus.x.devResp.getVar.result = eBusVarSuccess; // 0
    txMsg.msg.devBus.x.devResp.getVar.data[0] = 0;
    txMsg.msg.devBus.x.devResp.getVar.data[1] = 1;
    txMsg.msg.devBus.x.devResp.getVar.data[2] = 2;
    txMsg.msg.devBus.x.devResp.getVar.data[3] = 3;
    txMsg.msg.devBus.x.devResp.getVar.data[4] = 4;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 1 + 1 + 5) != 0) {
		return -1;
	}

	txMsg.type = eBusDevRespGetVar;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.getVar.index = 0x56;
    txMsg.msg.devBus.x.devResp.getVar.length = 0;
    txMsg.msg.devBus.x.devResp.getVar.result = eBusVarIndexError; // 2
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 1 + 1) != 0) {
		return -1;
	}

    txMsg.type = eBusDevReqSetVar;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devReq.setVar.index = 0x78;
    txMsg.msg.devBus.x.devReq.setVar.length = 2;
    txMsg.msg.devBus.x.devReq.setVar.data[0] = 0;
    txMsg.msg.devBus.x.devReq.setVar.data[1] = 1;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 1 + 2) != 0) {
		return -1;
	}

    txMsg.type = eBusDevRespSetVar;
    txMsg.senderAddr = 66;
    txMsg.msg.devBus.receiverAddr = 67;
    txMsg.msg.devBus.x.devResp.setVar.index = 0xab;
    txMsg.msg.devBus.x.devResp.setVar.result = eBusVarSuccess;
	if (TestTelegram(&txMsg, MSG_SIZE2 + 1 + 1) != 0) {
		return -1;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
*  NV memory for persist bus variables
*/

static uint8_t nvmem[512];

static bool BusVarNv(uint16_t address, void *buf, uint8_t bufSize, TBusVarDir dir) {

	void *mem;

    // range check
    if ((address + bufSize) > sizeof(nvmem)) {
        return false;
    }

    mem = (void *)(nvmem + address);
    if (dir == eBusVarRead) {
        memcpy(buf, mem, bufSize);
    } else {
        memcpy(mem, buf, bufSize);
    }
    return true;
}

/*-----------------------------------------------------------------------------
*  main
*/
int main(int argc, char *argv[]) {

    int  handle;
    int  i;
    char comPort[SIZE_COMPORT] = "";
    uint8_t len;
    uint8_t val8;

    /* get tty device */
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

    SioInit();
    handle = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);

    if (handle == -1) {
        printf("can't open %s\r\n", comPort);
        return 0;
    }

    // wait for sio input to settle and the flush
    usleep(100000);
    while ((len = SioGetNumRxChar(handle)) != 0) {
        SioRead(handle, &val8, sizeof(val8));
    }

    BusInit(handle);
#if 1
    BusVarInit(67, BusVarNv);

    {
    	uint8_t var1 = 1;
    	uint16_t var2 = 0x1234;

    	bool rc;
    	uint8_t len;

    	TBusVarHdl hdl1;
    	TBusVarHdl hdl2;
    	TBusTelegram *msg;

    	TBusVarResult result;
    	TBusVarState  state;


    	BusVarAdd(0, sizeof(var1), true);
    	BusVarAdd(1, sizeof(var2), false);

    	rc = BusVarWrite(1, &var2, sizeof(var2), &result);
    	rc = BusVarWrite(0, &var1, sizeof(var1), &result);

    	var1 = 0;
    	var2 = 0;

    	len = BusVarRead(1, &var2, sizeof(var2), &result);
    	printf("len %d var2 %x\n", len, var2);
    	len = BusVarRead(0, &var1, sizeof(var1), &result);
    	printf("len %d var1 %x\n", len, var1);

    	hdl2 = BusVarTransactionOpen(242, 1, &var2, sizeof(var2), eBusVarRead);
    	hdl1 = BusVarTransactionOpen(242, 0, &var1, sizeof(var1), eBusVarRead);

    	for (i = 0; i < 1000; i++) {
    		BusVarProcess();
    		usleep(10000);
   			if (BusCheck() == BUS_MSG_OK) {
  				msg = BusMsgBufGet();
   				if ((msg->type == eBusDevRespGetVar) &&
   					(msg->msg.devBus.receiverAddr == 67)) {
         	        BusVarRespGet(msg->senderAddr, &msg->msg.devBus.x.devResp.getVar);
   				}
  			}

   			if (hdl1 != BUSVAR_HDL_INVALID) {
   		        state = BusVarTransactionState(hdl1);
   		        if (state & BUSVAR_STATE_FINAL) {
   		            if (state == eBusVarState_Ready) {
   		            	printf("var1 %02x\n", var1);
   		            }
   		            BusVarTransactionClose(hdl1);
   		            hdl1 = BUSVAR_HDL_INVALID;
   		        }
   		    }
   			if (hdl2 != BUSVAR_HDL_INVALID) {
   		        state = BusVarTransactionState(hdl2);
   		        if (state & BUSVAR_STATE_FINAL) {
   		            if (state == eBusVarState_Ready) {
   		            	printf("var2 %04x\n", var2);
   		            }
   		            BusVarTransactionClose(hdl2);
   		            hdl2 = BUSVAR_HDL_INVALID;
   		        }
   		    }
    	}

    }
    return 0;
#endif

    if (Test() == 0) {
    	printf("OK\n");
    } else {
    	printf("ERROR\n");
    }

    if (handle != -1) {
        SioClose(handle);
    }

    return 0;
}

