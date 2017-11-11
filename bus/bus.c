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

// max number of length options of variable length telegrams
#define MAX_NUM_VAR_LEN  10


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

typedef struct {
   uint8_t offset; // offset of TBusDevType element
   struct {
      uint8_t value;
      uint8_t len;
   } varData[MAX_NUM_VAR_LEN];
} TVarLenMsg;

typedef struct {
   uint8_t size;
   TVarLenMsg *pVarLen;
} TTelegramSize;

/*-----------------------------------------------------------------------------
*  Variables
*/
/* buffer for bus telegram just receiving/just received */
static TBusTelegram sRxBuffer;
static int sSioHandle = -1;;

static TVarLenMsg sRespInfoSize = {
   MSG_BASE_SIZE2,
   {
      {eBusDevTypeDo31, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoDo31)},
      {eBusDevTypeSw8,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoSw8)},
      {eBusDevTypeLum,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoLum)},
      {eBusDevTypeLed,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoLed)},
      {eBusDevTypeSw16, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoSw16)},
      {eBusDevTypeWind, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoWind)},
      {eBusDevTypeSw8Cal, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoSw8Cal)},
      {eBusDevTypeRs485If, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoRs485If)},
      {eBusDevTypePwm4, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoPwm4)},
      {eBusDevTypeSmIf, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespInfo, devType) +
                        member_sizeof(TBusDevRespInfo, version) +
                        sizeof(TBusDevInfoSmIf)}
   }
};

static TVarLenMsg sReqSetStateSize = {
   MSG_BASE_SIZE2,
   {
      {eBusDevTypeDo31, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqSetState, devType) +
                        sizeof(TBusDevSetStateDo31)},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0}
   }
};

static TVarLenMsg sRespGetStateSize = {
   MSG_BASE_SIZE2,
   {
      {eBusDevTypeDo31, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespGetState, devType) +
                        sizeof(TBusDevGetStateDo31)},
      {eBusDevTypeSw8,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespGetState, devType) +
                        sizeof(TBusDevGetStateSw8)},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0}
   }
};

static TVarLenMsg sReqSetValueSize = {
   MSG_BASE_SIZE2,
   {
      {eBusDevTypeDo31, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqSetValue, devType) +
                        sizeof(TBusDevSetValueDo31)},
      {eBusDevTypeSw8,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqSetValue, devType) +
                        sizeof(TBusDevSetValueSw8)},
      {eBusDevTypeSw16, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqSetValue, devType) +
                        sizeof(TBusDevSetValueSw16)},
      {eBusDevTypeRs485If, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqSetValue, devType) +
                        sizeof(TBusDevSetValueRs485if)},
      {eBusDevTypePwm4, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqSetValue, devType) +
                        sizeof(TBusDevSetValuePwm4)},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0},
      {0,               0}
   }
};

static TVarLenMsg sRespActualValueSize = {
   MSG_BASE_SIZE2,
   {
      {eBusDevTypeDo31, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValue, devType) +
                        sizeof(TBusDevActualValueDo31)},
      {eBusDevTypeSw8,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValue, devType) +
                        sizeof(TBusDevActualValueSw8)},
      {eBusDevTypeLum,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValue, devType) +
                        sizeof(TBusDevActualValueLum)},
      {eBusDevTypeLed,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValue, devType) +
                        sizeof(TBusDevActualValueLed)},
      {eBusDevTypeSw16, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValue, devType) +
                        sizeof(TBusDevActualValueSw16)},
      {eBusDevTypeWind, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValue, devType) +
                        sizeof(TBusDevActualValueWind)},
      {eBusDevTypeRs485If, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValue, devType) +
                        sizeof(TBusDevActualValueRs485if)},
      {eBusDevTypePwm4, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValue, devType) +
                        sizeof(TBusDevActualValuePwm4)},
      {eBusDevTypeSmIf, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValue, devType) +
                        sizeof(TBusDevActualValueSmif)},
      {0,               0}
   }
};

static TVarLenMsg sReqActualValueEventSize = {
   MSG_BASE_SIZE2,
   {
      {eBusDevTypeDo31, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqActualValueEvent, devType) +
                        sizeof(TBusDevActualValueDo31)},
      {eBusDevTypeSw8,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqActualValueEvent, devType) +
                        sizeof(TBusDevActualValueSw8)},
      {eBusDevTypeLum,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqActualValueEvent, devType) +
                        sizeof(TBusDevActualValueLum)},
      {eBusDevTypeLed,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqActualValueEvent, devType) +
                        sizeof(TBusDevActualValueLed)},
      {eBusDevTypeSw16, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqActualValueEvent, devType) +
                        sizeof(TBusDevActualValueSw16)},
      {eBusDevTypeWind, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqActualValueEvent, devType) +
                        sizeof(TBusDevActualValueWind)},
      {eBusDevTypeRs485If, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqActualValueEvent, devType) +
                        sizeof(TBusDevActualValueRs485if)},
      {eBusDevTypePwm4, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevReqActualValueEvent, devType) +
                        sizeof(TBusDevActualValuePwm4)},
      {0,               0},
      {0,               0}
   }
};

static TVarLenMsg sRespActualValueEventSize = {
   MSG_BASE_SIZE2,
   {
      {eBusDevTypeDo31, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValueEvent, devType) +
                        sizeof(TBusDevActualValueDo31)},
      {eBusDevTypeSw8,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValueEvent, devType) +
                        sizeof(TBusDevActualValueSw8)},
      {eBusDevTypeLum,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValueEvent, devType) +
                        sizeof(TBusDevActualValueLum)},
      {eBusDevTypeLed,  MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValueEvent, devType) +
                        sizeof(TBusDevActualValueLed)},
      {eBusDevTypeSw16, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValueEvent, devType) +
                        sizeof(TBusDevActualValueSw16)},
      {eBusDevTypeWind, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValueEvent, devType) +
                        sizeof(TBusDevActualValueWind)},
      {eBusDevTypeRs485If, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValueEvent, devType) +
                        sizeof(TBusDevActualValueRs485if)},
      {eBusDevTypePwm4, MSG_BASE_SIZE2 +
                        member_sizeof(TBusDevRespActualValueEvent, devType) +
                        sizeof(TBusDevActualValuePwm4)},
      {0,               0},
      {0,               0}
   }
};

// telegram sizes without STX and checksum
// array index + 1 = telegram type (eBusDevStartup is 255 -> gets index 0
static TTelegramSize sTelegramSize[] = {
   { MSG_BASE_SIZE1,                                    0                         }, // eBusDevStartup
   { 0,                                                 0                         }, // unused
   { MSG_BASE_SIZE1 + sizeof(TBusButtonPressed),        0                         }, // eBusButtonPressed1
   { MSG_BASE_SIZE1 + sizeof(TBusButtonPressed),        0                         }, // eBusButtonPressed2
   { MSG_BASE_SIZE1 + sizeof(TBusButtonPressed),        0                         }, // eBusButtonPressed1_2
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqReboot),         0                         }, // eBusDevReqReboot
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqUpdEnter),       0                         }, // eBusDevReqUpdEnter
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespUpdEnter),      0                         }, // eBusDevRespUpdEnter
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqUpdData),        0                         }, // eBusDevReqUpdData
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespUpdData),       0                         }, // eBusDevRespUpdData
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqUpdTerm),        0                         }, // eBusDevReqUpdTerm
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespUpdTerm),       0                         }, // eBusDevRespUpdTerm
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqInfo),           0                         }, // eBusDevReqInfo
   { 0,                                                 &sRespInfoSize            }, // eBusDevRespInfo
   { 0,                                                 &sReqSetStateSize         }, // eBusDevReqSetState
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespSetState),      0                         }, // eBusDevRespSetState
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqGetState),       0                         }, // eBusDevReqGetState
   { 0,                                                 &sRespGetStateSize        }, // eBusDevRespGetState
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqSwitchState),    0                         }, // eBusDevReqSwitchState
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespSwitchState),   0                         }, // eBusDevRespSwitchState
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqSetClientAddr),  0                         }, // eBusDevReqSetClientAddr
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespSetClientAddr), 0                         }, // eBusDevRespSetClientAddr
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqGetClientAddr),  0                         }, // eBusDevReqGetClientAddr
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespGetClientAddr), 0                         }, // eBusDevRespGetClientAddr
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqSetAddr),        0                         }, // eBusDevReqSetAddr
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespSetAddr),       0                         }, // eBusDevRespSetAddr
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqEepromRead),     0                         }, // eBusDevReqEepromRead
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespEepromRead),    0                         }, // eBusDevRespEepromRead
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqEepromWrite),    0                         }, // eBusDevReqEepromWrite
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespEepromWrite),   0                         }, // eBusDevRespEepromWrite
   { 0,                                                 &sReqSetValueSize         }, // eBusDevReqSetValue
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespSetValue),      0                         }, // eBusDevRespSetValue
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqActualValue),    0                         }, // eBusDevReqActualValue
   { 0,                                                 &sRespActualValueSize     }, // eBusDevRespActualValue
   { 0,                                                 &sReqActualValueEventSize }, // eBusDevReqActualValueEvent
   { 0,                                                 &sRespActualValueEventSize}, // eBusDevRespActualValueEvent
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqClockCalib),     0                         }, // eBusDevReqClockCalib
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespClockCalib),    0                         }, // eBusDevRespClockCalib
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqDoClockCalib),   0                         }, // eBusDevReqDoClockCalib
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespDoClockCalib),  0                         }, // eBusDevRespDoClockCalib
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqDiag),           0                         }, // eBusDevReqDiag
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespDiag),          0                         }, // eBusDevRespDiag
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqGetTime),        0                         }, // eBusDevReqGetTime
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespGetTime),       0                         }, // eBusDevRespGetTime
   { MSG_BASE_SIZE2 + sizeof(TBusDevReqSetTime),        0                         }, // eBusDevReqSetTime
   { MSG_BASE_SIZE2 + sizeof(TBusDevRespSetTime),       0                         }, // eBusDevRespSetTime
};

static struct l2State {
   uint8_t protoState;
   uint8_t lastMsgIdx;
   uint8_t dynMsgTypeIdx;
   uint8_t chIdx;
   TVarLenMsg *pVarSize;
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
   sL2State.pVarSize = 0;
}

/*-----------------------------------------------------------------------------
* L2 Rx state machine
*/
static uint8_t L2StateMachine(uint8_t ch) {

   uint8_t           rc = L2_ERROR;
   uint8_t           numTypes;
   TTelegramSize     *pSize;
   uint8_t           i;
   struct l2State   *pL2State = &sL2State;

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
         if (((uint8_t)(ch + 1)) < numTypes) {
            pSize = &sTelegramSize[(uint8_t)(ch + 1)];
            pL2State->lastMsgIdx = pSize->size - 1;
            if (pL2State->lastMsgIdx == 1) {
               rc = L2_COMPLETE;
               pL2State->protoState = L2_WAIT_FOR_SENDER_ADDR;
            } else if (pL2State->lastMsgIdx == 0xff) {
               // sMsgLen is dynamically
               // sMsgLen can be calculated when character at sDynMsgTypeOffset
               // is read
               pL2State->pVarSize = pSize->pVarLen;
               if (pL2State->pVarSize != 0) {
                 pL2State->dynMsgTypeIdx = pL2State->pVarSize->offset;
                 pL2State->protoState = L2_WAIT_FOR_MSG;
                  rc = L2_IN_PROGRESS;
               }
            } else {
               pL2State->protoState = L2_WAIT_FOR_MSG;
               rc = L2_IN_PROGRESS;
            }
         }
         break;
      case L2_WAIT_FOR_MSG:
         *((uint8_t *)&sRxBuffer + pL2State->chIdx) = ch;
         if (pL2State->chIdx == pL2State->lastMsgIdx) {
            rc = L2_COMPLETE;
            pL2State->protoState = L2_WAIT_FOR_SENDER_ADDR;
         } else if (pL2State->chIdx == pL2State->dynMsgTypeIdx) {
            for (i = 0; i < MAX_NUM_VAR_LEN; i++) {
               if (pL2State->pVarSize->varData[i].value == ch) {
                  pL2State->lastMsgIdx = pL2State->pVarSize->varData[i].len - 1;
                  break;
               }
            }
            if (pL2State->lastMsgIdx != 0xff) {
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

   uint8_t ch;
   uint8_t checkSum = CHECKSUM_START;
   uint8_t i;
   TTelegramSize *pSize;
   TVarLenMsg    *pVarSize;
   uint8_t len = 0;
   uint8_t numTypes;
   bool    rc;

   numTypes = ARRAY_CNT(sTelegramSize);
   if ((uint8_t)(pMsg->type + 1) >= numTypes) {
      return BUS_SEND_BAD_TYPE; // error
   }
   pSize = &sTelegramSize[(uint8_t)(pMsg->type + 1)];
   len = pSize->size;
   if (len == 0) {
      pVarSize = pSize->pVarLen;
      if (pVarSize == 0) {
         return BUS_SEND_BAD_VARLEN; // error
      }
      for (i = 0; i < MAX_NUM_VAR_LEN; i++) {
         if (pVarSize->varData[i].value == *((uint8_t *)pMsg + pVarSize->offset)) {
            len = pVarSize->varData[i].len;
            break;
         }
      }
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
