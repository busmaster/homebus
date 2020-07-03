/*
 * bus.c
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

#include "sio.h"
#include "sysdef.h"
#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
/* size of buffer for SIO receiving */
#define BUS_SIO_RX_BUF_SIZE                    10

#define STX 0x02
#define ESC 0x1B

/* start value for checksum calculation */
#define CHECKSUM_START   0x55

#define member_sizeof(T,F) sizeof(((T *)0)->F)

#define MSG_BASE_SIZE1  member_sizeof(TBusTelegram, type) + member_sizeof(TBusTelegram, senderAddr)
#define MSG_BASE_SIZE2  MSG_BASE_SIZE1 + member_sizeof(TBusDev, receiverAddr)

#define L1_WAIT_FOR_STX      0
#define L1_RX_MSG            1
#define L1_WAIT_FOR_CHECKSUM 2

#define L2_IN_PROGRESS          0
#define L2_ERROR                1
#define L2_COMPLETE             2

#define L2_WAIT_FOR_SENDER_ADDR 0
#define L2_WAIT_FOR_TYPE        1
#define L2_WAIT_FOR_MSG         2

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef void* (* TProtoFunc)(uint8_t ch);

typedef enum {
    eBusLenConst,   // constant length telegram
    eBusLenDevType, // TBusDevType dependent lenght of telegram
    eBusLenDirect   // length of variable field is contained in telegram 
} TBusLenType;

typedef struct {
    TBusDevType devType;
    uint8_t     len;
} TBusDevTypeLen;

typedef struct {
    uint8_t offset; // offset of TBusDevType element
    TBusDevTypeLen len[];
} TBusLenDevType;

typedef struct {
    uint8_t offsetLen; // offset of length byte
    uint8_t add;  // add this value to get the total telegram length
} TBusLenDirect;

typedef struct {
    TBusLenType lenType;
    union {
        uint8_t        constant;
        TBusLenDevType *pDevType;
        TBusLenDirect  direct;
    } len;
} TTelegramSize;

/*-----------------------------------------------------------------------------
*  Variables
*/
/* buffer for bus telegram just receiving/just received */
static TBusTelegram sRxBuffer;
static int sSioHandle = -1;

#undef BASE_SIZE
#define BASE_SIZE (MSG_BASE_SIZE2 + member_sizeof(TBusDevRespInfo, devType) + member_sizeof(TBusDevRespInfo, version))
static TBusLenDevType sRespInfoSize = {
    MSG_BASE_SIZE2,
    {
        {eBusDevTypeDo31,    BASE_SIZE + sizeof(TBusDevInfoDo31)},
        {eBusDevTypeSw8,     BASE_SIZE + sizeof(TBusDevInfoSw8)},
        {eBusDevTypeLum,     BASE_SIZE + sizeof(TBusDevInfoLum)},
        {eBusDevTypeLed,     BASE_SIZE + sizeof(TBusDevInfoLed)},
        {eBusDevTypeSw16,    BASE_SIZE + sizeof(TBusDevInfoSw16)},
        {eBusDevTypeWind,    BASE_SIZE + sizeof(TBusDevInfoWind)},
        {eBusDevTypeSw8Cal,  BASE_SIZE + sizeof(TBusDevInfoSw8Cal)},
        {eBusDevTypeRs485If, BASE_SIZE + sizeof(TBusDevInfoRs485If)},
        {eBusDevTypePwm4,    BASE_SIZE + sizeof(TBusDevInfoPwm4)},
        {eBusDevTypeSmIf,    BASE_SIZE + sizeof(TBusDevInfoSmIf)},
        {eBusDevTypePwm16,   BASE_SIZE + sizeof(TBusDevInfoPwm16)},
        {eBusDevTypeInv,     0}
    }
};

#undef BASE_SIZE
#define BASE_SIZE  (MSG_BASE_SIZE2 + member_sizeof(TBusDevReqSetState, devType) + sizeof(TBusDevSetStateDo31))
static TBusLenDevType sReqSetStateSize = {
    MSG_BASE_SIZE2,
    {
        {eBusDevTypeDo31, BASE_SIZE},
        {eBusDevTypeInv,     0}
    }
};

#undef BASE_SIZE
#define BASE_SIZE  (MSG_BASE_SIZE2 + member_sizeof(TBusDevRespGetState, devType))
static TBusLenDevType sRespGetStateSize = {
    MSG_BASE_SIZE2,
    {
        {eBusDevTypeDo31, BASE_SIZE + sizeof(TBusDevGetStateDo31)},
        {eBusDevTypeSw8,  BASE_SIZE + sizeof(TBusDevGetStateSw8)},
        {eBusDevTypeInv,     0}
    }
};

#undef BASE_SIZE
#define BASE_SIZE  (MSG_BASE_SIZE2 + member_sizeof(TBusDevReqSetValue, devType))
static TBusLenDevType sReqSetValueSize = {
    MSG_BASE_SIZE2,
    {
        {eBusDevTypeDo31,    BASE_SIZE + sizeof(TBusDevSetValueDo31)},
        {eBusDevTypeSw8,     BASE_SIZE + sizeof(TBusDevSetValueSw8)},
        {eBusDevTypeSw16,    BASE_SIZE + sizeof(TBusDevSetValueSw16)},
        {eBusDevTypeRs485If, BASE_SIZE + sizeof(TBusDevSetValueRs485if)},
        {eBusDevTypePwm4,    BASE_SIZE + sizeof(TBusDevSetValuePwm4)},
        {eBusDevTypePwm16,   BASE_SIZE + sizeof(TBusDevSetValuePwm16)},
        {eBusDevTypeInv,     0}
    }
};

#undef BASE_SIZE
#define BASE_SIZE (MSG_BASE_SIZE2 + member_sizeof(TBusDevRespActualValue, devType))
static TBusLenDevType sRespActualValueSize = {
    MSG_BASE_SIZE2,
    {
        {eBusDevTypeDo31,    BASE_SIZE + sizeof(TBusDevActualValueDo31)},
        {eBusDevTypeSw8,     BASE_SIZE + sizeof(TBusDevActualValueSw8)},
        {eBusDevTypeLum,     BASE_SIZE + sizeof(TBusDevActualValueLum)},
        {eBusDevTypeLed,     BASE_SIZE + sizeof(TBusDevActualValueLed)},
        {eBusDevTypeSw16,    BASE_SIZE + sizeof(TBusDevActualValueSw16)},
        {eBusDevTypeWind,    BASE_SIZE + sizeof(TBusDevActualValueWind)},
        {eBusDevTypeRs485If, BASE_SIZE + sizeof(TBusDevActualValueRs485if)},
        {eBusDevTypePwm4,    BASE_SIZE + sizeof(TBusDevActualValuePwm4)},
        {eBusDevTypeSmIf,    BASE_SIZE + sizeof(TBusDevActualValueSmif)},
        {eBusDevTypePwm16,   BASE_SIZE + sizeof(TBusDevActualValuePwm16)},
        {eBusDevTypeInv,     0}
    }
};

#undef BASE_SIZE
#define BASE_SIZE (MSG_BASE_SIZE2 + member_sizeof(TBusDevReqActualValueEvent, devType))
static TBusLenDevType sReqActualValueEventSize = {
    MSG_BASE_SIZE2,
    {
        {eBusDevTypeDo31,    BASE_SIZE + sizeof(TBusDevActualValueDo31)},
        {eBusDevTypeSw8,     BASE_SIZE + sizeof(TBusDevActualValueSw8)},
        {eBusDevTypeLum,     BASE_SIZE + sizeof(TBusDevActualValueLum)},
        {eBusDevTypeLed,     BASE_SIZE + sizeof(TBusDevActualValueLed)},
        {eBusDevTypeSw16,    BASE_SIZE + sizeof(TBusDevActualValueSw16)},
        {eBusDevTypeWind,    BASE_SIZE + sizeof(TBusDevActualValueWind)},
        {eBusDevTypeRs485If, BASE_SIZE + sizeof(TBusDevActualValueRs485if)},
        {eBusDevTypePwm4,    BASE_SIZE + sizeof(TBusDevActualValuePwm4)},
        {eBusDevTypeSmIf,    BASE_SIZE + sizeof(TBusDevActualValueSmif)},
        {eBusDevTypePwm16,   BASE_SIZE + sizeof(TBusDevActualValuePwm16)},
        {eBusDevTypeInv,     0}
    }
};

#undef BASE_SIZE
#define BASE_SIZE (MSG_BASE_SIZE2 + member_sizeof(TBusDevRespActualValueEvent, devType))
static TBusLenDevType sRespActualValueEventSize = {
    MSG_BASE_SIZE2,
    {
        {eBusDevTypeDo31,    BASE_SIZE + sizeof(TBusDevActualValueDo31)},
        {eBusDevTypeSw8,     BASE_SIZE + sizeof(TBusDevActualValueSw8)},
        {eBusDevTypeLum,     BASE_SIZE + sizeof(TBusDevActualValueLum)},
        {eBusDevTypeLed,     BASE_SIZE + sizeof(TBusDevActualValueLed)},
        {eBusDevTypeSw16,    BASE_SIZE + sizeof(TBusDevActualValueSw16)},
        {eBusDevTypeWind,    BASE_SIZE + sizeof(TBusDevActualValueWind)},
        {eBusDevTypeRs485If, BASE_SIZE + sizeof(TBusDevActualValueRs485if)},
        {eBusDevTypePwm4,    BASE_SIZE + sizeof(TBusDevActualValuePwm4)},
        {eBusDevTypeSmIf,    BASE_SIZE + sizeof(TBusDevActualValueSmif)},
        {eBusDevTypePwm16,   BASE_SIZE + sizeof(TBusDevActualValuePwm16)},
        {eBusDevTypeInv,     0}
    }
};

#define LC len.constant
#define LD len.direct
#define LT len.pDevType

#define LEN_RESP_GET_VAR_OFFS   LD.offsetLen = MSG_BASE_SIZE2 +                \
                                member_sizeof(TBusDevRespGetVar, index) +      \
                                member_sizeof(TBusDevRespGetVar, result)
#define LEN_RESP_GET_VAR_ADD    LD.add = MSG_BASE_SIZE2 +                      \
                                member_sizeof(TBusDevRespGetVar, index) +      \
                                member_sizeof(TBusDevRespGetVar, length) +     \
                                member_sizeof(TBusDevRespGetVar, result)

#define LEN_REQ_SET_VAR_OFFS    LD.offsetLen = MSG_BASE_SIZE2 +                \
                                member_sizeof(TBusDevReqSetVar, index)
#define LEN_REQ_SET_VAR_ADD     LD.add = MSG_BASE_SIZE2 +                      \
                                member_sizeof(TBusDevReqSetVar, index) +       \
                                member_sizeof(TBusDevReqSetVar, length)

// telegram sizes without STX and checksum
// array index = telegram type (eBusDevStartup is 255 -> set to index 0)
static TTelegramSize sTelegramSize[] = {
    { eBusLenConst,   .LC = MSG_BASE_SIZE1                                    }, // eBusDevStartup
    { eBusLenConst,   .LC = MSG_BASE_SIZE1 + sizeof(TBusButtonPressed)        }, // eBusButtonPressed1
    { eBusLenConst,   .LC = MSG_BASE_SIZE1 + sizeof(TBusButtonPressed)        }, // eBusButtonPressed2
    { eBusLenConst,   .LC = MSG_BASE_SIZE1 + sizeof(TBusButtonPressed)        }, // eBusButtonPressed1_2
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqReboot)         }, // eBusDevReqReboot
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqUpdEnter)       }, // eBusDevReqUpdEnter
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespUpdEnter)      }, // eBusDevRespUpdEnter
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqUpdData)        }, // eBusDevReqUpdData
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespUpdData)       }, // eBusDevRespUpdData
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqUpdTerm)        }, // eBusDevReqUpdTerm
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespUpdTerm)       }, // eBusDevRespUpdTerm
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqInfo)           }, // eBusDevReqInfo
    { eBusLenDevType, .LT = &sRespInfoSize                                    }, // eBusDevRespInfo
    { eBusLenDevType, .LT = &sReqSetStateSize                                 }, // eBusDevReqSetState
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespSetState)      }, // eBusDevRespSetState
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqGetState)       }, // eBusDevReqGetState
    { eBusLenDevType, .LT = &sRespGetStateSize                                }, // eBusDevRespGetState
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqSwitchState)    }, // eBusDevReqSwitchState
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespSwitchState)   }, // eBusDevRespSwitchState
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqSetClientAddr)  }, // eBusDevReqSetClientAddr
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespSetClientAddr) }, // eBusDevRespSetClientAddr
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqGetClientAddr)  }, // eBusDevReqGetClientAddr
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespGetClientAddr) }, // eBusDevRespGetClientAddr
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqSetAddr)        }, // eBusDevReqSetAddr
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespSetAddr)       }, // eBusDevRespSetAddr
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqEepromRead)     }, // eBusDevReqEepromRead
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespEepromRead)    }, // eBusDevRespEepromRead
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqEepromWrite)    }, // eBusDevReqEepromWrite
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespEepromWrite)   }, // eBusDevRespEepromWrite
    { eBusLenDevType, .LT = &sReqSetValueSize                                 }, // eBusDevReqSetValue
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespSetValue)      }, // eBusDevRespSetValue
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqActualValue)    }, // eBusDevReqActualValue
    { eBusLenDevType, .LT = &sRespActualValueSize                             }, // eBusDevRespActualValue
    { eBusLenDevType, .LT = &sReqActualValueEventSize                         }, // eBusDevReqActualValueEvent
    { eBusLenDevType, .LT = &sRespActualValueEventSize                        }, // eBusDevRespActualValueEvent
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqClockCalib)     }, // eBusDevReqClockCalib
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespClockCalib)    }, // eBusDevRespClockCalib
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqDoClockCalib)   }, // eBusDevReqDoClockCalib
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespDoClockCalib)  }, // eBusDevRespDoClockCalib
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqDiag)           }, // eBusDevReqDiag
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespDiag)          }, // eBusDevRespDiag
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqGetTime)        }, // eBusDevReqGetTime
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespGetTime)       }, // eBusDevRespGetTime
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqSetTime)        }, // eBusDevReqSetTime
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespSetTime)       }, // eBusDevRespSetTime
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevReqGetVar)         }, // eBusDevReqGetVar
    { eBusLenDirect,  .LEN_RESP_GET_VAR_OFFS, .LEN_RESP_GET_VAR_ADD           }, // eBusDevRespGetVar
    { eBusLenDirect,  .LEN_REQ_SET_VAR_OFFS, .LEN_REQ_SET_VAR_ADD             }, // eBusDevReqSetVar
    { eBusLenConst,   .LC = MSG_BASE_SIZE2 + sizeof(TBusDevRespSetVar)        }  // eBusDevRespSetVar
};

static struct l2State {
   uint8_t        protoState;
   uint8_t        lastMsgIdx;
   uint8_t        dynMsgTypeIdx;
   uint8_t        lengthIdx;
   uint8_t        lengthAdd;
   uint8_t        chIdx;
   TBusLenDevType *pLDT;
} sL2State;

/*-----------------------------------------------------------------------------
*  Functions
*/
static uint8_t BusDecode(uint8_t ch);
static bool    TransmitCharProt(uint8_t data);
static void    L2StateInit(uint8_t protoState);

/*----------------------------------------------------------------------------
*   init
*/
void BusInit(int sioHandle) {

   sSioHandle = sioHandle;
   L2StateInit(L2_WAIT_FOR_SENDER_ADDR);
}

/*----------------------------------------------------------------------------
*   exit
*/
void BusExit(int sioHandle) {

   sSioHandle = -1;
}

/*-----------------------------------------------------------------------------
*  check for new received bus telegram
*  return codes:
*              BUS_NO_MSG     no telegram in progress
*              BUS_MSG_OK     telegram received completely
*              BUS_MSG_RXING  telegram receiving in progress
*              BUS_MSG_ERROR  errorous telegram received (checksum error)
*/
uint8_t BusCheck(void) {

   uint8_t numRxChar;
   static uint8_t ret = BUS_NO_MSG;
   uint8_t retTmp;

    if (!SioHandleValid(sSioHandle)) {
        return BUS_IF_ERROR;
    }

   numRxChar = SioGetNumRxChar(sSioHandle);
   if (numRxChar != 0) {
      ret = BusDecode(numRxChar);
      if ((ret == BUS_MSG_ERROR) ||
          (ret == BUS_MSG_OK)) {
         retTmp = ret;
         ret = BUS_NO_MSG;
         return retTmp;
      }
   }
   return ret;
}

/*-----------------------------------------------------------------------------
* get buffer for telegram
* buffer is valid when BUS_MSG_OK is returned by BusCheck til next call
* of BusCheck
*/
TBusTelegram *BusMsgBufGet(void) {
   return &sRxBuffer;
}

/*-----------------------------------------------------------------------------
* L2 init
*/
static void L2StateInit(uint8_t protoState) {

   sL2State.protoState = protoState;
   sL2State.chIdx = 1;
   sL2State.lastMsgIdx = 0xff;
   sL2State.dynMsgTypeIdx = 0;
   sL2State.lengthIdx = 0;
   sL2State.pLDT = 0;
}

/*-----------------------------------------------------------------------------
* L2 Rx state machine
*/
static uint8_t L2StateMachine(uint8_t ch) {

    uint8_t           rc = L2_ERROR;
    uint8_t           numTypes;
    TTelegramSize     *pSize;
    uint8_t           i;
    struct l2State    *pL2State = &sL2State;
    TBusDevTypeLen    *pLen;

    switch (pL2State->protoState) {
    case L2_WAIT_FOR_SENDER_ADDR:
        sRxBuffer.senderAddr = ch;
        L2StateInit(L2_WAIT_FOR_TYPE);
        rc = L2_IN_PROGRESS;
        break;
    case L2_WAIT_FOR_TYPE:
        sRxBuffer.type = (TBusMsgType)ch;
        pL2State->chIdx = 2;
        // find expected length of message
        numTypes = ARRAY_CNT(sTelegramSize);
        if (ch == eBusDevStartup) {
            ch = 0; // DevStartup is index 0 in sTelegramSize
        }
        if ((uint8_t)ch < numTypes) {
            pSize = &sTelegramSize[(uint8_t)ch];
            switch (pSize->lenType) {
            case eBusLenConst:
                pL2State->lastMsgIdx = pSize->len.constant - 1;
                if (pL2State->lastMsgIdx == 1) {
                    rc = L2_COMPLETE;
                    pL2State->protoState = L2_WAIT_FOR_SENDER_ADDR;
                } else {
                    pL2State->protoState = L2_WAIT_FOR_MSG;
                    rc = L2_IN_PROGRESS;
                }
                break;
            case eBusLenDevType:
                // sMsgLen is dynamically
                // sMsgLen can be calculated when character at dynMsgTypeIdx
                // is read
                pL2State->lastMsgIdx = 0;
                pL2State->pLDT = pSize->len.pDevType;
                pL2State->dynMsgTypeIdx = pL2State->pLDT->offset;
                pL2State->lengthIdx = 0;
                pL2State->protoState = L2_WAIT_FOR_MSG;
                rc = L2_IN_PROGRESS;
                break;
            case eBusLenDirect:
                // sMsgLen is dynamically
                // sMsgLen can be calculated when character at lengthIdx
                // is read
                pL2State->lastMsgIdx = 0;
                pL2State->lengthIdx = pSize->len.direct.offsetLen;
                pL2State->lengthAdd = pSize->len.direct.add;
                pL2State->dynMsgTypeIdx = 0;
                pL2State->protoState = L2_WAIT_FOR_MSG;
                rc = L2_IN_PROGRESS;
                break;
            default: 
                break;
            }
         }
        break;
    case L2_WAIT_FOR_MSG:
        *((uint8_t *)&sRxBuffer + pL2State->chIdx) = ch;
        if (pL2State->chIdx == pL2State->lastMsgIdx) {
            rc = L2_COMPLETE;
            pL2State->protoState = L2_WAIT_FOR_SENDER_ADDR;
        } else if (pL2State->chIdx == pL2State->dynMsgTypeIdx) {
            for (i = 0, pLen = pL2State->pLDT->len; pLen->devType != eBusDevTypeInv; i++, pLen++) {
                if (pLen->devType == ch) {
                    pL2State->lastMsgIdx = pLen->len - 1;
                    break;
                }
            }
            if (pL2State->lastMsgIdx != 0) {
                rc = L2_IN_PROGRESS;
            }
        } else if (pL2State->chIdx == pL2State->lengthIdx) {
            pL2State->lastMsgIdx = ch + pL2State->lengthAdd - 1;
            if (pL2State->chIdx == pL2State->lastMsgIdx) {
                rc = L2_COMPLETE;
                pL2State->protoState = L2_WAIT_FOR_SENDER_ADDR;
            } else {
                rc = L2_IN_PROGRESS;
            }
        } else {
            rc = L2_IN_PROGRESS;
        }
        pL2State->chIdx++;
        break;
    default:
        break;
   }
   if (rc == L2_ERROR) {
       pL2State->protoState = L2_WAIT_FOR_SENDER_ADDR;
   }
   return rc;
}

/*-----------------------------------------------------------------------------
*  state machine for telegram decoding
*  return codes:
*              BUS_MSG_OK     telegram received completely
*              BUS_MSG_RXING  telegram receiving in progress
*              BUS_MSG_ERROR  errorous telegram received (checksum error)
*
*  unkown telegram types are ignored
*/
static uint8_t BusDecode(uint8_t numRxChar) {

    static uint8_t    sL1ProtoState = L1_WAIT_FOR_STX;
    static uint8_t    sSioRxBuffer[BUS_SIO_RX_BUF_SIZE];
    static uint8_t    sCheckSum;
    static bool       sStuffByte;
    uint8_t           *pBuf = sSioRxBuffer;
    uint8_t           numRead;
    uint8_t           rc = BUS_MSG_RXING;
    uint8_t           i;
    uint8_t           ch;
    uint8_t           l2State;

    numRead = SioRead(sSioHandle, pBuf, min(sizeof(sSioRxBuffer), numRxChar));
    for (i = 0; i < numRead; i++) {
        if (rc != BUS_MSG_RXING) {
            break;
        }
        ch = *(pBuf + i);
        switch (sL1ProtoState) {
        case L1_WAIT_FOR_STX:
            if (ch == STX) {
                sL1ProtoState = L1_RX_MSG;
                sCheckSum = CHECKSUM_START + ch;
                sStuffByte = false;
            } else {
                // no STX at start
                rc = BUS_MSG_ERROR;
                // skip all chars till next STX
                i++;
                while (i < numRead) {
                    ch = *(pBuf + i);
                    if (ch == STX) {
                        break;
                    }
                    i++;
                }
                i--;
            }
            break;
        case L1_RX_MSG:
        case L1_WAIT_FOR_CHECKSUM:
            if (ch == STX) {
                // unexpected STX
                rc = BUS_MSG_ERROR;
                i--;
            } else if (ch == ESC) {
                sStuffByte = true;
            } else {
                if (sStuffByte == true) {
                    /* invert character */
                    ch = ~ch;
                    sStuffByte = false;
                }
                if (sL1ProtoState == L1_RX_MSG) {
                    l2State = L2StateMachine(ch);
                    sCheckSum += ch;
                    if (l2State == L2_COMPLETE) {
                        sL1ProtoState = L1_WAIT_FOR_CHECKSUM;
                    } else if (l2State == L2_ERROR) {
                        rc = BUS_MSG_ERROR;
                    }
                } else {
                    if (ch == sCheckSum) {
                        rc = BUS_MSG_OK;
                    } else {
                        rc = BUS_MSG_ERROR;
                    }
                }
            }
            break;
        default:
            break;
        }
    }

    if (rc != BUS_MSG_RXING) {
        sL1ProtoState = L1_WAIT_FOR_STX;
        if (i < numRead) {
            SioUnRead(sSioHandle, pBuf + i, numRead - i);
        }
    }
    if (rc == BUS_MSG_ERROR) {
        L2StateInit(L2_WAIT_FOR_SENDER_ADDR);
    }
    return rc;
}

/*-----------------------------------------------------------------------------
* send bus telegram
*/
uint8_t BusSendToBuf(TBusTelegram *pMsg) {

    uint8_t         ch;
    uint8_t         checkSum = CHECKSUM_START;
    uint8_t         i;
    TTelegramSize   *pSize;
    TBusDevTypeLen  *pLen;    
    uint8_t         len = 0;
    uint8_t         numTypes;
    bool            rc;
    uint8_t         sizeIdx;

    if (pMsg == 0) {
        return BUS_SEND_TX_ERROR;
    }
    if (pMsg->type == eBusDevStartup) {
        sizeIdx = 0;
    } else {
        numTypes = ARRAY_CNT(sTelegramSize);
    	if (((uint8_t)pMsg->type < numTypes)) {
            sizeIdx = (uint8_t)pMsg->type;
    	} else {
            return BUS_SEND_BAD_TYPE;
        }
    }
    pSize = &sTelegramSize[sizeIdx];
    switch (pSize->lenType) {
    case eBusLenConst:
        len = pSize->len.constant;
        break;
    case eBusLenDevType:
        for (i = 0, pLen = pSize->len.pDevType->len; pLen->devType != eBusDevTypeInv; i++, pLen++) {
            if (pLen->devType == *((uint8_t *)pMsg + pSize->len.pDevType->offset)) {
                len = pLen->len;
                break;
            }
        }
        break;
    case eBusLenDirect:
        len = *((uint8_t *)pMsg + pSize->len.direct.offsetLen) + pSize->len.direct.add;
        break;
    default:
        break;
    }

    if (len == 0) {
        return BUS_SEND_BAD_LEN; // error
    }
    ch = STX;
    rc = SioWriteBuffered(sSioHandle, &ch, sizeof(ch)) == sizeof(ch) ? true : false;
    checkSum += ch;
    for (i = 0; rc && (i < len); i++) {
        ch = *((uint8_t *)pMsg + i);
        rc = TransmitCharProt(ch);
        checkSum += ch;
    }
    rc = rc && TransmitCharProt(checkSum);
    if (rc) {
        return BUS_SEND_OK;
    } else {
        return BUS_SEND_TX_ERROR;
    }
}

uint8_t BusSendToBufRaw(uint8_t *pBuf, uint8_t len) {
    
    return SioWriteBuffered(sSioHandle, pBuf, len);
}

uint8_t BusSendBuf(void) {
    
    if (SioSendBuffer(sSioHandle)) {
        return BUS_SEND_OK;
    } else {
        return BUS_SEND_TX_ERROR;
    }
}

uint8_t BusSend(TBusTelegram *pMsg) {

    uint8_t rc;

    rc = BusSendToBuf(pMsg);
    if (rc == BUS_SEND_OK) {
        rc = BusSendBuf();
    }
    return rc;
}

/*-----------------------------------------------------------------------------
*  send character with low level protocol translation
*  STX -> ESC + ~STX
*  ESC -> ESC + ~ESC
*/
static bool TransmitCharProt(uint8_t data) {

    uint8_t tmp;
    bool    rc;

    if (data == STX) {
        tmp = ESC;
        rc = SioWriteBuffered(sSioHandle, &tmp, sizeof(tmp)) == sizeof(tmp) ? true : false;
        tmp = ~STX;
        rc = rc && SioWriteBuffered(sSioHandle, &tmp, sizeof(tmp)) == sizeof(tmp) ? true : false;
    } else if (data == ESC) {
        tmp = ESC;
        rc = SioWriteBuffered(sSioHandle, &tmp, sizeof(tmp)) == sizeof(tmp) ? true : false;
        tmp = ~ESC;
        rc = rc && SioWriteBuffered(sSioHandle, &tmp, sizeof(tmp)) == sizeof(tmp) ? true : false;
    } else {
        rc = SioWriteBuffered(sSioHandle, &data, sizeof(data)) == sizeof(tmp) ? true : false;
    }
    return rc;
}
