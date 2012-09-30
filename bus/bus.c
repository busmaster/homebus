/*-----------------------------------------------------------------------------
*  bus.c
*/

/*-----------------------------------------------------------------------------
*  Includes
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
                 
/* state machine states for telegram decoding */
/*
#define WAIT_FOR_STX                           0
#define WAIT_FOR_ADDR                          1
#define WAIT_FOR_TYPE                          2      
#define WAIT_FOR_CHECKSUM                      3        
#define WAIT_FOR_RECEIVER_ADDR                 4  
#define WAIT_FOR_UPDREQ_ADDR_LO                5
#define WAIT_FOR_UPDREQ_ADDR_HI                6
#define WAIT_FOR_UPD_DATA                      7
#define WAIT_FOR_UPDRESP_ADDR_LO               8
#define WAIT_FOR_UPDRESP_ADDR_HI               9
#define WAIT_FOR_UPDRESP_TERM                 10
#define WAIT_FOR_INFORESP_DEVTYPE             11
#define WAIT_FOR_INFORESP_VERSION             12
#define WAIT_FOR_INFORESP_DO31_DIRSWITCH      13
#define WAIT_FOR_INFORESP_DO31_ONSWITCH       14
#define WAIT_FOR_SETSTATEREQ_DEVTYPE          15
#define WAIT_FOR_SETSTATE_DO31_DIGOUT         16
#define WAIT_FOR_SETSTATE_DO31_SHADER         17
#define WAIT_FOR_GETSTATERESP_DEVTYPE         18
#define WAIT_FOR_GETSTATE_DO31_DIGOUT         19
#define WAIT_FOR_GETSTATE_DO31_SHADER         20
*/

#define STX 0x02
#define ESC 0x1B             

/* start value for checksum calculation */
#define CHECKSUM_START   0x55        

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef void* (* TProtoFunc)(uint8_t ch);

/*-----------------------------------------------------------------------------
*  Variables
*/                                
/* buffer for bus telegram just receiving/just received */
static TBusTelegramm sRxBuffer; 
static int sSioHandle;
static uint8_t sSioRxBuffer[BUS_SIO_RX_BUF_SIZE];
static uint8_t sCheckSum; 
static uint8_t *spData;

/*-----------------------------------------------------------------------------
*  Functions
*/
static uint8_t BusDecode(uint8_t ch);
static void  TransmitCharProt(uint8_t data);

static TProtoFunc ProtoWaitForStx(uint8_t ch);
static TProtoFunc ProtoWaitForAddr(uint8_t ch);
static TProtoFunc ProtoWaitForType(uint8_t ch);
static TProtoFunc ProtoWaitForChecksum(uint8_t ch);
static TProtoFunc ProtoWaitForReceiverAddr(uint8_t ch);
static TProtoFunc ProtoWaitForReqUpdDataAddrLo(uint8_t ch);
static TProtoFunc ProtoWaitForReqUpdDataAddrHi(uint8_t ch);
static TProtoFunc ProtoWaitForUpdData(uint8_t ch);
static TProtoFunc ProtoWaitForRespUpdDataAddrLo(uint8_t ch);
static TProtoFunc ProtoWaitForRespUpdDataAddrHi(uint8_t ch);
static TProtoFunc ProtoWaitForRespUpdTerm(uint8_t ch);
static TProtoFunc ProtoWaitForRespInfoDevType(uint8_t ch);
static TProtoFunc ProtoWaitForRespInfoVersion(uint8_t ch);
static TProtoFunc ProtoWaitForRespInfoDo31Dirswitch(uint8_t ch);
static TProtoFunc ProtoWaitForRespInfoDo31Onswitch(uint8_t ch);
static TProtoFunc ProtoWaitForReqSetStateDevType(uint8_t ch);
static TProtoFunc ProtoWaitForSetStateDo31Digout(uint8_t ch);
static TProtoFunc ProtoWaitForSetStateDo31Shader(uint8_t ch);
static TProtoFunc ProtoWaitForRespGetStateDevType(uint8_t ch);
static TProtoFunc ProtoWaitForGetStateDo31Digout(uint8_t ch);
static TProtoFunc ProtoWaitForGetStateSw8(uint8_t ch);
static TProtoFunc ProtoWaitForGetStateDo31Shader(uint8_t ch);
static TProtoFunc ProtoWaitForReqSwitchState(uint8_t ch);
static TProtoFunc ProtoWaitForReqSetClientAddr(uint8_t ch);
static TProtoFunc ProtoWaitForRespGetClientAddr(uint8_t ch);
static TProtoFunc ProtoWaitForRespSwitchState(uint8_t ch);
static TProtoFunc ProtoWaitForReqSetAddr(uint8_t ch);
static TProtoFunc ProtoWaitForReqReadEepromAddrLo(uint8_t ch);
static TProtoFunc ProtoWaitForReqReadEepromAddrHi(uint8_t ch);
static TProtoFunc ProtoWaitForRespReadEeprom(uint8_t ch);
static TProtoFunc ProtoWaitForReqWriteEepromAddrLo(uint8_t ch);
static TProtoFunc ProtoWaitForReqWriteEepromAddrHi(uint8_t ch);
static TProtoFunc ProtoWaitForReqWriteEepromAddrData(uint8_t ch);

static TProtoFunc ProtoMsgOK(uint8_t ch);
static TProtoFunc ProtoMsgErr(uint8_t ch);
   
/*----------------------------------------------------------------------------
*   init	 
*/
void BusInit(int sioHandle) {
   
   sSioHandle = sioHandle;
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
TBusTelegramm *BusMsgBufGet(void) {
   return &sRxBuffer;
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

   static TProtoFunc sProtoState = (TProtoFunc)ProtoWaitForStx;
   static bool       sStuffByte = false;
   uint8_t             numRead;    
   uint8_t             *pBuf = sSioRxBuffer;
   uint8_t             ret = BUS_MSG_RXING;
   uint8_t             i; 
   uint8_t             ch;

   numRead = SioRead(sSioHandle, pBuf, min(sizeof(sSioRxBuffer), numRxChar));
   for (i = 0; i < numRead; i++) {
      ch = *(pBuf + i);
      if (sProtoState != (TProtoFunc)ProtoWaitForStx) {
         if (ch == ESC) {
            sStuffByte = true; 
            continue; 
         }
         if (ch == STX) {
            sStuffByte = false;
            /* unexpected telegram start detected */
            ret = BUS_MSG_ERROR;
            i--;  /* work also when i == 0, cause always 1 is added to i below */
            break;
         }
         if (sStuffByte == true) {
            /* invert character */
            ch = ~ch;
            sStuffByte = false;
         }                  
      } 
      sCheckSum += ch;
      sProtoState = (TProtoFunc)sProtoState(ch);
      if (sProtoState == (TProtoFunc)ProtoMsgOK) {
         ret = BUS_MSG_OK;
         break;
      } else if (sProtoState == (TProtoFunc)ProtoMsgErr) {
         ret = BUS_MSG_ERROR;
         break;
      } 
   }

   if (ret != BUS_MSG_RXING) {
      sProtoState = (TProtoFunc)ProtoWaitForStx;
      if (((uint8_t)(i + 1)) < numRead) {
          SioUnRead(sSioHandle, pBuf + i + 1, numRead - (i + 1));
      }
   }
   return ret;
}           

/*-----------------------------------------------------------------------------
* send bus telegram
*/
void BusSend(TBusTelegramm *pMsg) {
  
   uint8_t ch;            
   uint8_t checkSum = CHECKSUM_START;
   uint8_t i;
             
   ch = STX;   
   SioWrite(sSioHandle, &ch, sizeof(ch));
   checkSum += ch;

   ch = pMsg->senderAddr;
   TransmitCharProt(ch);
   checkSum += ch;

   ch = pMsg->type;
   TransmitCharProt(ch);
   checkSum += ch;
  
   switch(pMsg->type) {
      case eBusDevStartup:    
         break;
      case eBusDevReqReboot:    
      case eBusDevReqUpdTerm:    
      case eBusDevReqUpdEnter: 
      case eBusDevReqInfo:
      case eBusDevReqGetState: 
      case eBusDevReqGetClientAddr:
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         break;
      case eBusDevRespUpdEnter:   
      case eBusDevRespSetState: 
      case eBusDevRespSetClientAddr:
      case eBusDevRespSetAddr:
      case eBusDevRespEepromWrite:
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         break;
      case eBusDevReqUpdData:    
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         /* first the low byte */
         ch = pMsg->msg.devBus.x.devReq.updData.wordAddr & 0xff;
         TransmitCharProt(ch);
         checkSum += ch;   
         /* then high byte */
         ch = pMsg->msg.devBus.x.devReq.updData.wordAddr >> 8;
         TransmitCharProt(ch);
         checkSum += ch;   
         for (i = 0; i < BUS_FWU_PACKET_SIZE / 2; i++) {
            ch = pMsg->msg.devBus.x.devReq.updData.data[i] & 0xff;
            TransmitCharProt(ch);
            checkSum += ch;   
            ch = pMsg->msg.devBus.x.devReq.updData.data[i] >> 8;
            TransmitCharProt(ch);
            checkSum += ch;   
         }
         break;
      case eBusDevRespUpdData:    
         ch = pMsg->msg.devBus.receiverAddr;                
         TransmitCharProt(ch);
         checkSum += ch;   
         /* first the low byte */
         ch = pMsg->msg.devBus.x.devResp.updData.wordAddr & 0xff;
         TransmitCharProt(ch);
         checkSum += ch;   
         /* the high byte */
         ch = pMsg->msg.devBus.x.devResp.updData.wordAddr >> 8;
         TransmitCharProt(ch);
         checkSum += ch;   
         break;
      case eBusDevRespUpdTerm:    
         ch = pMsg->msg.devBus.receiverAddr;                
         TransmitCharProt(ch);
         checkSum += ch;   
         ch = pMsg->msg.devBus.x.devResp.updTerm.success;
         TransmitCharProt(ch);
         checkSum += ch;   
         break;
      case eBusDevRespInfo:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;                
         TransmitCharProt(ch);
         checkSum += ch;   
         /* device type */
         ch = (uint8_t)pMsg->msg.devBus.x.devResp.info.devType;
         TransmitCharProt(ch);
         checkSum += ch;
         /* version info */
         for (i = 0; i < BUS_DEV_INFO_VERSION_LEN; i++) {
             ch = pMsg->msg.devBus.x.devResp.info.version[i];
             TransmitCharProt(ch);
             checkSum += ch;
         }
         switch (pMsg->msg.devBus.x.devResp.info.devType) {
            case eBusDevTypeDo31:
               for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
                  ch = pMsg->msg.devBus.x.devResp.info.devInfo.do31.dirSwitch[i];
                  TransmitCharProt(ch);
                  checkSum += ch;
               }
               for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
                  ch = pMsg->msg.devBus.x.devResp.info.devInfo.do31.onSwitch[i];
                  TransmitCharProt(ch);
                  checkSum += ch;
               }
               break;
            case eBusDevTypeSw8:
               break;
            default:
               break;
         }  
         break;
      case eBusDevReqSetState:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;                
         TransmitCharProt(ch);
         checkSum += ch;   
         /* device type */
         ch = (uint8_t)pMsg->msg.devBus.x.devReq.setState.devType;
         TransmitCharProt(ch);
         checkSum += ch;
         switch (pMsg->msg.devBus.x.devReq.setState.devType) {
            case eBusDevTypeDo31:
               for (i = 0; i < BUS_DO31_DIGOUT_SIZE_SET; i++) {
                  ch = pMsg->msg.devBus.x.devReq.setState.state.do31.digOut[i];
                  TransmitCharProt(ch);
                  checkSum += ch;
               }
               for (i = 0; i < BUS_DO31_SHADER_SIZE_SET; i++) {
                  ch = pMsg->msg.devBus.x.devReq.setState.state.do31.shader[i];
                  TransmitCharProt(ch);
                  checkSum += ch;
               }
               break;
            default:
               break;
         }
         break;
      case eBusDevRespGetState:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;                
         TransmitCharProt(ch);
         checkSum += ch;   
         /* device type */
         ch = (uint8_t)pMsg->msg.devBus.x.devResp.getState.devType;
         TransmitCharProt(ch);
         checkSum += ch;
         switch (pMsg->msg.devBus.x.devResp.getState.devType) {
            case eBusDevTypeDo31:
               for (i = 0; i < BUS_DO31_DIGOUT_SIZE_GET; i++) {
                  ch = pMsg->msg.devBus.x.devResp.getState.state.do31.digOut[i];
                  TransmitCharProt(ch);
                  checkSum += ch;
               }
               for (i = 0; i < BUS_DO31_SHADER_SIZE_GET; i++) {
                  ch = pMsg->msg.devBus.x.devResp.getState.state.do31.shader[i];
                  TransmitCharProt(ch);
                  checkSum += ch;
               }
               break;
            case eBusDevTypeSw8:
               ch = pMsg->msg.devBus.x.devResp.getState.state.sw8.switchState;
               TransmitCharProt(ch);
               checkSum += ch;
               break;
            default:
               break;
         }
         break;
      case eBusDevReqSwitchState:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;                
         TransmitCharProt(ch);
         checkSum += ch;   
         /* switch state */
         ch = (uint8_t)pMsg->msg.devBus.x.devReq.switchState.switchState;
         TransmitCharProt(ch);
         checkSum += ch;
         break;
      case eBusDevReqSetClientAddr:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;                
         TransmitCharProt(ch);
         checkSum += ch;   
         /* array of client addresses */
         for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
             ch = pMsg->msg.devBus.x.devReq.setClientAddr.clientAddr[i];
             TransmitCharProt(ch);
             checkSum += ch;
         }
         break;
      case eBusDevRespGetClientAddr:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;                
         TransmitCharProt(ch);
         checkSum += ch;   
         /* array of client addresses */
         for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
             ch = pMsg->msg.devBus.x.devResp.getClientAddr.clientAddr[i];
             TransmitCharProt(ch);
             checkSum += ch;
         }
         break;
      case eBusDevRespSwitchState:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         /* switch state */
         ch = (uint8_t)pMsg->msg.devBus.x.devResp.switchState.switchState;
         TransmitCharProt(ch);
         checkSum += ch;
         break;
      case eBusDevReqSetAddr:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         /* new address */
         ch = (uint8_t)pMsg->msg.devBus.x.devReq.setAddr.addr;
         TransmitCharProt(ch);
         checkSum += ch;
         break;
      case eBusDevReqEepromRead:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         /* eeprom address */
         /* the low byte first */
         ch = (uint8_t)pMsg->msg.devBus.x.devReq.readEeprom.addr & 0xff;
         TransmitCharProt(ch);
         checkSum += ch;   
         /* then high byte */
         ch = (uint8_t)(pMsg->msg.devBus.x.devReq.readEeprom.addr >> 8);
         TransmitCharProt(ch);
         checkSum += ch;   
         break;
      case eBusDevRespEepromRead:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         ch = (uint8_t)pMsg->msg.devBus.x.devResp.readEeprom.data;
         TransmitCharProt(ch);
         checkSum += ch;
         break;
      case eBusDevReqEepromWrite:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         /* eeprom address */
         /* the low byte first */
         ch = (uint8_t)pMsg->msg.devBus.x.devReq.writeEeprom.addr & 0xff;
         TransmitCharProt(ch);
         checkSum += ch;   
         /* then high byte */
         ch = (uint8_t)(pMsg->msg.devBus.x.devReq.writeEeprom.addr >> 8);
         TransmitCharProt(ch);
         checkSum += ch;   
         /* data to write to eeprom */
         ch = pMsg->msg.devBus.x.devReq.writeEeprom.data;
         TransmitCharProt(ch);
         checkSum += ch;   
         break;
      default: 
         break;   
   }
   TransmitCharProt(checkSum);
}

/*-----------------------------------------------------------------------------
*  send character with low level protocol translation
*  STX -> ESC + ~STX
*  ESC -> ESC + ~ESC
*/
static void TransmitCharProt(uint8_t data) {
      
   uint8_t tmp;
   
   if (data == STX) {  
      tmp = ESC;
      SioWrite(sSioHandle, &tmp, sizeof(tmp));
      tmp = ~STX;
      SioWrite(sSioHandle, &tmp, sizeof(tmp));
   } else if (data == ESC) {
      tmp = ESC;
      SioWrite(sSioHandle, &tmp, sizeof(tmp));
      tmp = ~ESC;
      SioWrite(sSioHandle, &tmp, sizeof(tmp));
   } else {
      SioWrite(sSioHandle, &data, sizeof(data));
   }   
}

/*-----------------------------------------------------------------------------
* protocol functions
*/
static TProtoFunc ProtoWaitForStx(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForStx;

   if (ch == STX) {
      sCheckSum = CHECKSUM_START + STX;
      ret = (TProtoFunc)ProtoWaitForAddr;
   }
   return ret;
}

static TProtoFunc ProtoWaitForAddr(uint8_t ch) {

   sRxBuffer.senderAddr = ch;
   return (TProtoFunc)ProtoWaitForType;
}

static TProtoFunc ProtoWaitForType(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForStx;

   sRxBuffer.type = (TBusMsgType)ch;
   switch (sRxBuffer.type) {
      case eBusButtonPressed1:
      case eBusButtonPressed2:
      case eBusButtonPressed1_2:
      case eBusDevStartup:
         ret = (TProtoFunc)ProtoWaitForChecksum;
         break;   
      case eBusDevReqReboot:
      case eBusDevReqUpdEnter:
      case eBusDevRespUpdEnter:
      case eBusDevReqUpdData:
      case eBusDevRespUpdData:
      case eBusDevReqUpdTerm:
      case eBusDevRespUpdTerm:
      case eBusDevReqInfo:
      case eBusDevRespInfo:
      case eBusDevReqSetState:
      case eBusDevRespSetState:
      case eBusDevReqGetState:
      case eBusDevRespGetState:
      case eBusDevReqSwitchState:
      case eBusDevRespSwitchState:
      case eBusDevReqSetClientAddr:
      case eBusDevRespSetClientAddr:
      case eBusDevReqGetClientAddr:
      case eBusDevRespGetClientAddr:
      case eBusDevReqSetAddr:
      case eBusDevRespSetAddr:
      case eBusDevReqEepromRead:
      case eBusDevRespEepromRead:
      case eBusDevReqEepromWrite:
      case eBusDevRespEepromWrite:
         ret = (TProtoFunc)ProtoWaitForReceiverAddr;
         break;
      default:
         /* unknown telegram type */
         break;
   } 
   return ret;
}

static TProtoFunc ProtoWaitForChecksum(uint8_t ch) {
   
   TProtoFunc ret = (TProtoFunc)ProtoMsgErr;

   sCheckSum -= ch;
   if (ch == sCheckSum) {
      ret = (TProtoFunc)ProtoMsgOK;
   } 
   return ret;
}


static TProtoFunc ProtoWaitForReceiverAddr(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForStx;

   /* save receiver address */
   sRxBuffer.msg.devBus.receiverAddr = ch;
   switch (sRxBuffer.type) {
      case eBusDevReqUpdData:
         /* address low is next */
         ret = (TProtoFunc)ProtoWaitForReqUpdDataAddrLo;
         break;
      case eBusDevReqReboot:
      case eBusDevReqUpdEnter:
      case eBusDevReqUpdTerm:
      case eBusDevReqInfo:
      case eBusDevReqGetState:
      case eBusDevReqGetClientAddr:
         /* in this cases just the checksum is missing */                                             
         ret = (TProtoFunc)ProtoWaitForChecksum;
         break;
      case eBusDevRespSetClientAddr:
      case eBusDevRespUpdEnter:
      case eBusDevRespSetState:
      case eBusDevRespSetAddr:
      case eBusDevRespEepromWrite:
         /* in this casees just the checksum is missing */                                             
         ret = (TProtoFunc)ProtoWaitForChecksum;
         break;
      case eBusDevRespUpdData:
         /* address low is next */
         ret = (TProtoFunc)ProtoWaitForRespUpdDataAddrLo;
         break;
      case eBusDevRespUpdTerm:
         /* return code is next */
         ret = (TProtoFunc)ProtoWaitForRespUpdTerm;
         break;
      case eBusDevRespInfo:
         ret = (TProtoFunc)ProtoWaitForRespInfoDevType;
         break; 
      case eBusDevReqSetState:
         ret = (TProtoFunc)ProtoWaitForReqSetStateDevType;
         break;
      case eBusDevRespGetState:
         ret = (TProtoFunc)ProtoWaitForRespGetStateDevType;
         break;
      case eBusDevReqSwitchState:
         ret = (TProtoFunc)ProtoWaitForReqSwitchState;
         break;
      case eBusDevRespSwitchState:
         ret = (TProtoFunc)ProtoWaitForRespSwitchState;
         break;
      case eBusDevReqSetClientAddr:
         ret = (TProtoFunc)ProtoWaitForReqSetClientAddr;
         spData = (uint8_t *)sRxBuffer.msg.devBus.x.devReq.setClientAddr.clientAddr;
         break;
      case eBusDevRespGetClientAddr:
         ret = (TProtoFunc)ProtoWaitForRespGetClientAddr;
         spData = (uint8_t *)sRxBuffer.msg.devBus.x.devResp.getClientAddr.clientAddr;
         break;
      case eBusDevReqSetAddr:
         ret = (TProtoFunc)ProtoWaitForReqSetAddr;
         break;
      case eBusDevReqEepromRead:
         ret = (TProtoFunc)ProtoWaitForReqReadEepromAddrLo;
         break;
      case eBusDevRespEepromRead:
         ret = (TProtoFunc)ProtoWaitForRespReadEeprom;
         break;
      case eBusDevReqEepromWrite:
         ret = (TProtoFunc)ProtoWaitForReqWriteEepromAddrLo;
         break;
      default:
         /* unknown telegram type */
         break;
   }
   return ret;
}

static TProtoFunc ProtoWaitForReqUpdDataAddrLo(uint8_t ch) {
   /* save address low byte */ 
   sRxBuffer.msg.devBus.x.devReq.updData.wordAddr = ch;
   return (TProtoFunc)ProtoWaitForReqUpdDataAddrHi;
}

static TProtoFunc ProtoWaitForReqUpdDataAddrHi(uint8_t ch) {
   /* save address high byte */ 
   sRxBuffer.msg.devBus.x.devReq.updData.wordAddr += ch * 256;
   spData = (uint8_t *)sRxBuffer.msg.devBus.x.devReq.updData.data;
   return (TProtoFunc)ProtoWaitForUpdData;
}

static TProtoFunc ProtoWaitForUpdData(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForUpdData;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devReq.updData.data) == BUS_FWU_PACKET_SIZE) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   } 
   return ret;
}

static TProtoFunc ProtoWaitForRespUpdDataAddrLo(uint8_t ch) {
   /* save address low byte */ 
   sRxBuffer.msg.devBus.x.devResp.updData.wordAddr = ch;
   return (TProtoFunc)ProtoWaitForRespUpdDataAddrHi; 
}

static TProtoFunc ProtoWaitForRespUpdDataAddrHi(uint8_t ch) {
   /* save address high byte */ 
   sRxBuffer.msg.devBus.x.devResp.updData.wordAddr += ch * 256;
   return (TProtoFunc)ProtoWaitForChecksum; 
}

static TProtoFunc ProtoWaitForRespUpdTerm(uint8_t ch) {
   /* save return code */ 
   sRxBuffer.msg.devBus.x.devResp.updTerm.success = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}

static TProtoFunc ProtoWaitForRespInfoDevType(uint8_t ch) {

   sRxBuffer.msg.devBus.x.devResp.info.devType = (TBusDevType)ch;
   spData = (uint8_t *)sRxBuffer.msg.devBus.x.devResp.info.version;
   return (TProtoFunc)ProtoWaitForRespInfoVersion; 
}

static TProtoFunc ProtoWaitForRespInfoVersion(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForRespInfoVersion;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devResp.info.version) == BUS_DEV_INFO_VERSION_LEN) {
      /* version complete */
      switch (sRxBuffer.msg.devBus.x.devResp.info.devType) {
         case eBusDevTypeDo31:
            ret = (TProtoFunc)ProtoWaitForRespInfoDo31Dirswitch;
            spData = (uint8_t *)sRxBuffer.msg.devBus.x.devResp.info.devInfo.do31.dirSwitch;
            break;
         case eBusDevTypeSw8:
            /* version complete - packet complete */
            ret = (TProtoFunc)ProtoWaitForChecksum; 
            break;
         default:
            /* unknown device type */
            ret = (TProtoFunc)ProtoWaitForStx;
            break;
      }
   }        
   return ret;
}

static TProtoFunc ProtoWaitForRespInfoDo31Dirswitch(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForRespInfoDo31Dirswitch;
 
   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devResp.info.devInfo.do31.dirSwitch) == BUS_DO31_NUM_SHADER) {
      ret = (TProtoFunc)ProtoWaitForRespInfoDo31Onswitch;
      spData = (uint8_t *)sRxBuffer.msg.devBus.x.devResp.info.devInfo.do31.onSwitch;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForRespInfoDo31Onswitch(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForRespInfoDo31Onswitch;
   
   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devResp.info.devInfo.do31.onSwitch) == BUS_DO31_NUM_SHADER) {
      /* config complete - packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum; 
   }        
   return ret;
}

static TProtoFunc ProtoWaitForReqSetStateDevType(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForStx;

   sRxBuffer.msg.devBus.x.devReq.setState.devType = (TBusDevType)ch;
   switch (sRxBuffer.msg.devBus.x.devReq.setState.devType) {
      case eBusDevTypeDo31:
         ret = (TProtoFunc)ProtoWaitForSetStateDo31Digout;
         spData = (uint8_t *)sRxBuffer.msg.devBus.x.devReq.setState.state.do31.digOut;
         break;
      default:
         /* unknown device type */
         break;
   }
   return ret;
}

static TProtoFunc ProtoWaitForSetStateDo31Digout(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForSetStateDo31Digout;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devReq.setState.state.do31.digOut) == BUS_DO31_DIGOUT_SIZE_SET) {
      /* digOut complete */
      ret = (TProtoFunc)ProtoWaitForSetStateDo31Shader;
      spData = (uint8_t *)sRxBuffer.msg.devBus.x.devReq.setState.state.do31.shader;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForSetStateDo31Shader(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForSetStateDo31Shader;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devReq.setState.state.do31.shader) == BUS_DO31_SHADER_SIZE_SET) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForRespGetStateDevType(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForStx;

   sRxBuffer.msg.devBus.x.devResp.getState.devType = (TBusDevType)ch;
   switch (sRxBuffer.msg.devBus.x.devResp.getState.devType) {
      case eBusDevTypeDo31:
         ret = (TProtoFunc)ProtoWaitForGetStateDo31Digout;
         spData = (uint8_t *)sRxBuffer.msg.devBus.x.devResp.getState.state.do31.digOut;
         break;
      case eBusDevTypeSw8:
         ret = (TProtoFunc)ProtoWaitForGetStateSw8;
         break;
      default:
         /* unknown device type */
         break;
   }
   return ret;
}

static TProtoFunc ProtoWaitForGetStateDo31Digout(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForGetStateDo31Digout;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devResp.getState.state.do31.digOut) == BUS_DO31_DIGOUT_SIZE_GET) {
      /* digOut complete */
      ret = (TProtoFunc)ProtoWaitForGetStateDo31Shader;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForGetStateDo31Shader(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForGetStateDo31Shader;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devResp.getState.state.do31.shader) == BUS_DO31_SHADER_SIZE_GET) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForGetStateSw8(uint8_t ch) {

   /* save switch state */ 
   sRxBuffer.msg.devBus.x.devResp.getState.state.sw8.switchState = ch;
   return (TProtoFunc)ProtoWaitForChecksum;
}


static TProtoFunc ProtoWaitForReqSwitchState(uint8_t ch) {

   /* save switch state */ 
   sRxBuffer.msg.devBus.x.devReq.switchState.switchState = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}

static TProtoFunc ProtoWaitForRespSwitchState(uint8_t ch) {

   /* save switch state */ 
   sRxBuffer.msg.devBus.x.devResp.switchState.switchState = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}


static TProtoFunc ProtoWaitForReqSetClientAddr(uint8_t ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForReqSetClientAddr;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devReq.setClientAddr.clientAddr) == BUS_MAX_CLIENT_NUM) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForRespGetClientAddr(uint8_t ch) {
       
   TProtoFunc ret = (TProtoFunc)ProtoWaitForRespGetClientAddr;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (uint8_t *)sRxBuffer.msg.devBus.x.devResp.getClientAddr.clientAddr) == BUS_MAX_CLIENT_NUM) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   }        
   return ret;
       
}

static TProtoFunc ProtoWaitForReqSetAddr(uint8_t ch) {

   /* save new addess */ 
   sRxBuffer.msg.devBus.x.devReq.setAddr.addr = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 

}

static TProtoFunc ProtoWaitForReqReadEepromAddrLo(uint8_t ch) {

   /* save address low byte */ 
   sRxBuffer.msg.devBus.x.devReq.readEeprom.addr = ch;
   return (TProtoFunc)ProtoWaitForReqReadEepromAddrHi;

}

static TProtoFunc ProtoWaitForReqReadEepromAddrHi(uint8_t ch) {

   /* save address high byte */ 
   sRxBuffer.msg.devBus.x.devReq.readEeprom.addr += ch * 256;
   return (TProtoFunc)ProtoWaitForChecksum; 

}


static TProtoFunc ProtoWaitForRespReadEeprom(uint8_t ch) {

   sRxBuffer.msg.devBus.x.devResp.readEeprom.data = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}

static TProtoFunc ProtoWaitForReqWriteEepromAddrLo(uint8_t ch) {

   /* save address low byte */ 
   sRxBuffer.msg.devBus.x.devReq.writeEeprom.addr = ch;
   return (TProtoFunc)ProtoWaitForReqWriteEepromAddrHi;

}

static TProtoFunc ProtoWaitForReqWriteEepromAddrHi(uint8_t ch) {

   /* save address high byte */ 
   sRxBuffer.msg.devBus.x.devReq.writeEeprom.addr += ch * 256;
   return (TProtoFunc)ProtoWaitForReqWriteEepromAddrData;
}

static TProtoFunc ProtoWaitForReqWriteEepromAddrData(uint8_t ch) {

   sRxBuffer.msg.devBus.x.devReq.writeEeprom.data = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}


/*-----------------------------------------------------------------------------
* protocol function for state indication of a correctly received telegram
* function is used as intermediate state
*/
static TProtoFunc ProtoMsgOK(uint8_t ch) {
   return (TProtoFunc)ProtoWaitForStx;
}

/*-----------------------------------------------------------------------------
* protocol function for state indication of a incorrectly received telegram
* function is used as intermediate state
*/
static TProtoFunc ProtoMsgErr(uint8_t ch) {
   return (TProtoFunc)ProtoWaitForStx;
}

