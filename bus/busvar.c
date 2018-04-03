/*
 * busvar.c
 *
 * Copyright 2018 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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
#include <string.h>
#include <sysdef.h>

#include "bus.h"


#define TX_RETRY_TIMEOUT 50  /* ms */
#define RESPONSE_TIMEOUT 100 /* ms */

#define TRANSACTION_FIFO_LEN    8 /* must be power of 2 */

#define NUM_FIFO_FREE     ((sVarFifoRdIdx - sVarFifoWrIdx - 1) & (TRANSACTION_FIFO_LEN - 1))
#define IDX_INC(__idx__)   __idx__++; __idx__ &= (TRANSACTION_FIFO_LEN - 1)


typedef struct {
    uint8_t     size;
    void        *mem;
} TVarTab;

typedef struct {
    uint8_t addr;
    uint8_t idx;
    void    *buf;     // caller supplied mem
    uint8_t size;
    TBusVarDir dir;
    TBusVarState state;
    uint8_t txRetryCnt;
    uint16_t txTime;
} TVarTransactionDesc;


static TVarTab sVarTable[BUSVAR_NUMVAR];

static uint8_t  sHeap[BUSVAR_MEMSIZE];
static uint16_t sHeapCurr = 0;
static uint8_t  sVarIdx = 0;

static TVarTransactionDesc sVarTransactionFifo[TRANSACTION_FIFO_LEN];
static uint8_t sVarFifoWrIdx;
static uint8_t sVarFifoRdIdx;
static uint8_t sMyAddr;

void BusVarInit(uint8_t addr) {
    
	TVarTransactionDesc *vtd = sVarTransactionFifo;
	uint8_t i;

	sMyAddr = addr;
	sVarFifoWrIdx = 0;
	sVarFifoRdIdx = 0;

	for (i = 0; i < TRANSACTION_FIFO_LEN; i++) {
	    vtd->state = eBusVarState_Invalid;
	    vtd++;
	}
}

/* local access */

bool BusVarAdd(uint8_t size, uint8_t *idx) {
    
    if (sVarIdx >= (BUSVAR_NUMVAR - 1)) {
        return false;
    }
    if ((sHeapCurr + size) > BUSVAR_MEMSIZE) {
        return false;
    }
    sVarTable[sVarIdx].size = size;
    sVarTable[sVarIdx].mem = &sHeap[sHeapCurr];
    
    *idx = sVarIdx;

    sVarIdx++;
    sHeapCurr += size;
    
    return true;
}

uint8_t BusVarRead(uint8_t idx, void *buf, uint8_t bufSize, TBusVarResult *result) {

    TVarTab *tab;

    if (idx >= sVarIdx) {
        *result = eBusVarIndexError;
        return 0;
    }
    tab = &sVarTable[idx];
    if (bufSize < tab->size) {
        *result = eBusVarLengthError;
        return 0;
    }
    memcpy(buf, tab->mem, tab->size);
    *result = eBusVarSuccess;

    return tab->size;
}

bool BusVarWrite(uint8_t idx, void *buf, uint8_t bufSize, TBusVarResult *result) {

    TVarTab *tab;

    if (idx >= sVarIdx) {
        *result = eBusVarIndexError;
        return false;
    }
    tab = &sVarTable[idx];
    if (bufSize != tab->size) {
        *result = eBusVarLengthError;
        return false;
    }
    memcpy(tab->mem, buf, tab->size);
    *result = eBusVarSuccess;

    return true;
}

/* remote access */

TBusVarHdl BusVarTransactionOpen(uint8_t addr, uint8_t idx, void *buf, uint8_t size, TBusVarDir dir) {

	TVarTransactionDesc *vtd;

	if (NUM_FIFO_FREE == 0) {
		return BUSVAR_HDL_INVALID;
	}

	vtd = &sVarTransactionFifo[sVarFifoWrIdx];
	vtd->addr = addr;
	vtd->idx = idx;
	vtd->dir = dir;
	vtd->buf = buf;
	vtd->size = size;
	vtd->state = eBusVarState_Scheduled;
	vtd->txRetryCnt = 5;

	IDX_INC(sVarFifoWrIdx);

    return (TBusVarHdl)vtd;
}

TBusVarState BusVarTransactionState(TBusVarHdl varHdl) {
    
    TVarTransactionDesc *vtd = (TVarTransactionDesc *)varHdl;

    return vtd->state;
}
void BusVarTransactionClose(TBusVarHdl varHdl) {

	uint8_t rdIdx = sVarFifoRdIdx;
    TVarTransactionDesc *vtd = (TVarTransactionDesc *)varHdl;

    vtd->state = eBusVarState_Invalid;

    do {
    	IDX_INC(rdIdx);
        vtd = &sVarTransactionFifo[rdIdx];
    } while (vtd->state == eBusVarState_Invalid);

    sVarFifoRdIdx = rdIdx;
}

void BusVarRespSet(uint8_t addr, TBusDevRespSetVar *respSet) {

	TVarTransactionDesc *vtd;

	if (sVarFifoRdIdx == sVarFifoWrIdx) {
		// no response expected
		return;
	}
	vtd = &sVarTransactionFifo[sVarFifoRdIdx];
	if (respSet->index == vtd->idx) {
		vtd->state = eBusVarState_Ready;
	} else {
		vtd->state = eBusVarState_Error;
	}
}

void BusVarRespGet(uint8_t addr, TBusDevRespGetVar *respGet) {

	TVarTransactionDesc *vtd;
	uint8_t i;

	if (sVarFifoRdIdx == sVarFifoWrIdx) {
		// no response expected
		return;
	}
	vtd = &sVarTransactionFifo[sVarFifoRdIdx];
	if ((respGet->index == vtd->idx) &&
		(respGet->length <= vtd->size)) {
		vtd->size = respGet->length;
		for (i = 0; i < vtd->size; i++) {
			*((uint8_t *)vtd->buf + i) = respGet->data[i];
		}
		vtd->state = eBusVarState_Ready;
	} else {
		vtd->state = eBusVarState_Error;
	}
}

void BusVarProcess(void) {

	TVarTransactionDesc *vtd;
	uint8_t rdIdx = sVarFifoRdIdx;
    static TBusTelegram  sTxMsg;
    uint16_t actualTime16;
    uint8_t i;

	if (sVarFifoRdIdx == sVarFifoWrIdx) {
		return;
	}
	vtd = &sVarTransactionFifo[rdIdx];

    GET_TIME_MS16(actualTime16);

	switch (vtd->state) {
	case eBusVarState_Scheduled:
	    sTxMsg.senderAddr = sMyAddr;
	    sTxMsg.msg.devBus.receiverAddr = vtd->addr;
	    if (vtd->dir == eBusVarRead) {
	        sTxMsg.type = eBusDevReqGetVar;
	        sTxMsg.msg.devBus.x.devReq.getVar.index = vtd->idx;
	    } else {
	        sTxMsg.type = eBusDevReqSetVar;
	        sTxMsg.msg.devBus.x.devReq.setVar.index = vtd->idx;
	        sTxMsg.msg.devBus.x.devReq.setVar.length = vtd->size;
	        for (i = 0; i < vtd->size; i++) {
	        	sTxMsg.msg.devBus.x.devReq.setVar.data[i] = *((uint8_t *)vtd->buf + i);
	        }
	    }
	    if (BusSend(&sTxMsg) == BUS_SEND_OK) {
	    	vtd->state = eBusVarState_Waiting;
	    } else {
	    	vtd->state = eBusVarState_TxRetry;
	    }
    	vtd->txTime = actualTime16;
		break;
	case eBusVarState_Waiting:
		if ((vtd->txTime - actualTime16) > RESPONSE_TIMEOUT) {
			vtd->state = eBusVarState_Timeout;
		}
		break;
	case eBusVarState_TxRetry:
		if ((vtd->txTime - actualTime16) > TX_RETRY_TIMEOUT) {
			if (vtd->txRetryCnt > 0) {
				if (BusSend(&sTxMsg) == BUS_SEND_OK) {
					vtd->state = eBusVarState_Waiting;
				} else {
					vtd->state = eBusVarState_TxRetry;
				}
				vtd->txTime = actualTime16;
				vtd->txRetryCnt--;
			} else {
				vtd->state = eBusVarState_TxError;
			}
		}
		break;
	case eBusVarState_Timeout:
	case eBusVarState_TxError:
	case eBusVarState_Ready:
		// do nothing - transaction shall be closed by user
		break;
	default:
		break;
	}
}
