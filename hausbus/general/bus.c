/*-----------------------------------------------------------------------------
*  Bus.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/

#include <stdio.h>
#include "Types.h"
#include "SysDef.h"
#include "Sio.h"
#include "Bus.h"

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
typedef void* (* TProtoFunc)(UINT8 ch);

/*-----------------------------------------------------------------------------
*  Variables
*/                                
/* buffer for bus telegram just receiving/just received */
static TBusTelegramm sRxBuffer; 
static int sSioHandle;
static UINT8 sSioRxBuffer[BUS_SIO_RX_BUF_SIZE];
static UINT8 sCheckSum; 
static UINT8 *spData;

/*-----------------------------------------------------------------------------
*  Functions
*/
static UINT8 BusDecode(UINT8 ch);
static void  TransmitCharProt(UINT8 data);

static TProtoFunc ProtoWaitForStx(UINT8 ch);
static TProtoFunc ProtoWaitForAddr(UINT8 ch);
static TProtoFunc ProtoWaitForType(UINT8 ch);
static TProtoFunc ProtoWaitForChecksum(UINT8 ch);
static TProtoFunc ProtoWaitForReceiverAddr(UINT8 ch);
static TProtoFunc ProtoWaitForReqUpdDataAddrLo(UINT8 ch);
static TProtoFunc ProtoWaitForReqUpdDataAddrHi(UINT8 ch);
static TProtoFunc ProtoWaitForUpdData(UINT8 ch);
static TProtoFunc ProtoWaitForRespUpdDataAddrLo(UINT8 ch);
static TProtoFunc ProtoWaitForRespUpdDataAddrHi(UINT8 ch);
static TProtoFunc ProtoWaitForRespUpdTerm(UINT8 ch);
static TProtoFunc ProtoWaitForRespInfoDevType(UINT8 ch);
static TProtoFunc ProtoWaitForRespInfoVersion(UINT8 ch);
static TProtoFunc ProtoWaitForRespInfoDo31Dirswitch(UINT8 ch);
static TProtoFunc ProtoWaitForRespInfoDo31Onswitch(UINT8 ch);
static TProtoFunc ProtoWaitForReqSetStateDevType(UINT8 ch);
static TProtoFunc ProtoWaitForSetStateDo31Digout(UINT8 ch);
static TProtoFunc ProtoWaitForSetStateDo31Shader(UINT8 ch);
static TProtoFunc ProtoWaitForRespGetStateDevType(UINT8 ch);
static TProtoFunc ProtoWaitForGetStateDo31Digout(UINT8 ch);
static TProtoFunc ProtoWaitForGetStateSw8(UINT8 ch);
static TProtoFunc ProtoWaitForGetStateDo31Shader(UINT8 ch);
static TProtoFunc ProtoWaitForReqSwitchState(UINT8 ch);
static TProtoFunc ProtoWaitForReqSetClientAddr(UINT8 ch);
static TProtoFunc ProtoWaitForRespGetClientAddr(UINT8 ch);
static TProtoFunc ProtoWaitForRespSwitchState(UINT8 ch);
static TProtoFunc ProtoWaitForReqSetAddr(UINT8 ch);
static TProtoFunc ProtoWaitForReqReadEepromAddrLo(UINT8 ch);
static TProtoFunc ProtoWaitForReqReadEepromAddrHi(UINT8 ch);
static TProtoFunc ProtoWaitForRespReadEeprom(UINT8 ch);
static TProtoFunc ProtoWaitForReqWriteEepromAddrLo(UINT8 ch);
static TProtoFunc ProtoWaitForReqWriteEepromAddrHi(UINT8 ch);
static TProtoFunc ProtoWaitForReqWriteEepromAddrData(UINT8 ch);

static TProtoFunc ProtoMsgOK(UINT8 ch);
static TProtoFunc ProtoMsgErr(UINT8 ch);
   
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
UINT8 BusCheck(void) {
                     
   UINT8 numRxChar;
   static UINT8 ret = BUS_NO_MSG;
   UINT8 retTmp;

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
static UINT8 BusDecode(UINT8 numRxChar) {

   static TProtoFunc sProtoState = (TProtoFunc)ProtoWaitForStx;
   static BOOL       sStuffByte = FALSE;
   UINT8             numRead;    
   UINT8             *pBuf = sSioRxBuffer;
   UINT8             ret = BUS_MSG_RXING;
   UINT8             i; 
   UINT8             ch;

   numRead = SioRead(sSioHandle, pBuf, min(sizeof(sSioRxBuffer), numRxChar));
   for (i = 0; i < numRead; i++) {
      ch = *(pBuf + i);
      if (sProtoState != (TProtoFunc)ProtoWaitForStx) {
         if (ch == ESC) {
            sStuffByte = TRUE; 
            continue; 
         }
         if (ch == STX) {
            sStuffByte = FALSE;
            /* unexpected telegram start detected */
            ret = BUS_MSG_ERROR;
            i--;  /* work also when i == 0, cause always 1 is added to i below */
            break;
         }
         if (sStuffByte == TRUE) {
            /* invert character */
            ch = ~ch;
            sStuffByte = FALSE;
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
      if (((UINT8)(i + 1)) < numRead) {
          SioUnRead(sSioHandle, pBuf + i + 1, numRead - (i + 1));
      }
   }
   return ret;
}           

/*-----------------------------------------------------------------------------
* send bus telegram
*/
void BusSend(TBusTelegramm *pMsg) {
  
   UINT8 ch;            
   UINT8 checkSum = CHECKSUM_START;
   UINT8 i;
             
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
         ch = (UINT8)pMsg->msg.devBus.x.devResp.info.devType;
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
         ch = (UINT8)pMsg->msg.devBus.x.devReq.setState.devType;
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
         ch = (UINT8)pMsg->msg.devBus.x.devResp.getState.devType;
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
         ch = (UINT8)pMsg->msg.devBus.x.devReq.switchState.switchState;
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
         ch = (UINT8)pMsg->msg.devBus.x.devResp.switchState.switchState;
         TransmitCharProt(ch);
         checkSum += ch;
         break;
      case eBusDevReqSetAddr:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         /* new address */
         ch = (UINT8)pMsg->msg.devBus.x.devReq.setAddr.addr;
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
         ch = (UINT8)pMsg->msg.devBus.x.devReq.readEeprom.addr & 0xff;
         TransmitCharProt(ch);
         checkSum += ch;   
         /* then high byte */
         ch = (UINT8)(pMsg->msg.devBus.x.devReq.readEeprom.addr >> 8);
         TransmitCharProt(ch);
         checkSum += ch;   
         break;
      case eBusDevRespEepromRead:
         /* receiver address */
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
         ch = (UINT8)pMsg->msg.devBus.x.devResp.readEeprom.data;
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
         ch = (UINT8)pMsg->msg.devBus.x.devReq.writeEeprom.addr & 0xff;
         TransmitCharProt(ch);
         checkSum += ch;   
         /* then high byte */
         ch = (UINT8)(pMsg->msg.devBus.x.devReq.writeEeprom.addr >> 8);
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
static void TransmitCharProt(UINT8 data) {
      
   UINT8 tmp;
   
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
static TProtoFunc ProtoWaitForStx(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForStx;

   if (ch == STX) {
      sCheckSum = CHECKSUM_START + STX;
      ret = (TProtoFunc)ProtoWaitForAddr;
   }
   return ret;
}

static TProtoFunc ProtoWaitForAddr(UINT8 ch) {

   sRxBuffer.senderAddr = ch;
   return (TProtoFunc)ProtoWaitForType;
}

static TProtoFunc ProtoWaitForType(UINT8 ch) {

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

static TProtoFunc ProtoWaitForChecksum(UINT8 ch) {
   
   TProtoFunc ret = (TProtoFunc)ProtoMsgErr;

   sCheckSum -= ch;
   if (ch == sCheckSum) {
      ret = (TProtoFunc)ProtoMsgOK;
   } 
   return ret;
}


static TProtoFunc ProtoWaitForReceiverAddr(UINT8 ch) {

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
         spData = (UINT8 *)sRxBuffer.msg.devBus.x.devReq.setClientAddr.clientAddr;
         break;
      case eBusDevRespGetClientAddr:
         ret = (TProtoFunc)ProtoWaitForRespGetClientAddr;
         spData = (UINT8 *)sRxBuffer.msg.devBus.x.devResp.getClientAddr.clientAddr;
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

static TProtoFunc ProtoWaitForReqUpdDataAddrLo(UINT8 ch) {
   /* save address low byte */ 
   sRxBuffer.msg.devBus.x.devReq.updData.wordAddr = ch;
   return (TProtoFunc)ProtoWaitForReqUpdDataAddrHi;
}

static TProtoFunc ProtoWaitForReqUpdDataAddrHi(UINT8 ch) {
   /* save address high byte */ 
   sRxBuffer.msg.devBus.x.devReq.updData.wordAddr += ch * 256;
   spData = (UINT8 *)sRxBuffer.msg.devBus.x.devReq.updData.data;
   return (TProtoFunc)ProtoWaitForUpdData;
}

static TProtoFunc ProtoWaitForUpdData(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForUpdData;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devReq.updData.data) == BUS_FWU_PACKET_SIZE) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   } 
   return ret;
}

static TProtoFunc ProtoWaitForRespUpdDataAddrLo(UINT8 ch) {
   /* save address low byte */ 
   sRxBuffer.msg.devBus.x.devResp.updData.wordAddr = ch;
   return (TProtoFunc)ProtoWaitForRespUpdDataAddrHi; 
}

static TProtoFunc ProtoWaitForRespUpdDataAddrHi(UINT8 ch) {
   /* save address high byte */ 
   sRxBuffer.msg.devBus.x.devResp.updData.wordAddr += ch * 256;
   return (TProtoFunc)ProtoWaitForChecksum; 
}

static TProtoFunc ProtoWaitForRespUpdTerm(UINT8 ch) {
   /* save return code */ 
   sRxBuffer.msg.devBus.x.devResp.updTerm.success = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}

static TProtoFunc ProtoWaitForRespInfoDevType(UINT8 ch) {

   sRxBuffer.msg.devBus.x.devResp.info.devType = (TBusDevType)ch;
   spData = (UINT8 *)sRxBuffer.msg.devBus.x.devResp.info.version;
   return (TProtoFunc)ProtoWaitForRespInfoVersion; 
}

static TProtoFunc ProtoWaitForRespInfoVersion(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForRespInfoVersion;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devResp.info.version) == BUS_DEV_INFO_VERSION_LEN) {
      /* version complete */
      switch (sRxBuffer.msg.devBus.x.devResp.info.devType) {
         case eBusDevTypeDo31:
            ret = (TProtoFunc)ProtoWaitForRespInfoDo31Dirswitch;
            spData = (UINT8 *)sRxBuffer.msg.devBus.x.devResp.info.devInfo.do31.dirSwitch;
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

static TProtoFunc ProtoWaitForRespInfoDo31Dirswitch(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForRespInfoDo31Dirswitch;
 
   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devResp.info.devInfo.do31.dirSwitch) == BUS_DO31_NUM_SHADER) {
      ret = (TProtoFunc)ProtoWaitForRespInfoDo31Onswitch;
      spData = (UINT8 *)sRxBuffer.msg.devBus.x.devResp.info.devInfo.do31.onSwitch;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForRespInfoDo31Onswitch(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForRespInfoDo31Onswitch;
   
   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devResp.info.devInfo.do31.onSwitch) == BUS_DO31_NUM_SHADER) {
      /* config complete - packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum; 
   }        
   return ret;
}

static TProtoFunc ProtoWaitForReqSetStateDevType(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForStx;

   sRxBuffer.msg.devBus.x.devReq.setState.devType = (TBusDevType)ch;
   switch (sRxBuffer.msg.devBus.x.devReq.setState.devType) {
      case eBusDevTypeDo31:
         ret = (TProtoFunc)ProtoWaitForSetStateDo31Digout;
         spData = (UINT8 *)sRxBuffer.msg.devBus.x.devReq.setState.state.do31.digOut;
         break;
      default:
         /* unknown device type */
         break;
   }
   return ret;
}

static TProtoFunc ProtoWaitForSetStateDo31Digout(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForSetStateDo31Digout;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devReq.setState.state.do31.digOut) == BUS_DO31_DIGOUT_SIZE_SET) {
      /* digOut complete */
      ret = (TProtoFunc)ProtoWaitForSetStateDo31Shader;
      spData = (UINT8 *)sRxBuffer.msg.devBus.x.devReq.setState.state.do31.shader;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForSetStateDo31Shader(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForSetStateDo31Shader;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devReq.setState.state.do31.shader) == BUS_DO31_SHADER_SIZE_SET) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForRespGetStateDevType(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForStx;

   sRxBuffer.msg.devBus.x.devResp.getState.devType = (TBusDevType)ch;
   switch (sRxBuffer.msg.devBus.x.devResp.getState.devType) {
      case eBusDevTypeDo31:
         ret = (TProtoFunc)ProtoWaitForGetStateDo31Digout;
         spData = (UINT8 *)sRxBuffer.msg.devBus.x.devResp.getState.state.do31.digOut;
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

static TProtoFunc ProtoWaitForGetStateDo31Digout(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForGetStateDo31Digout;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devResp.getState.state.do31.digOut) == BUS_DO31_DIGOUT_SIZE_GET) {
      /* digOut complete */
      ret = (TProtoFunc)ProtoWaitForGetStateDo31Shader;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForGetStateDo31Shader(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForGetStateDo31Shader;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devResp.getState.state.do31.shader) == BUS_DO31_SHADER_SIZE_GET) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForGetStateSw8(UINT8 ch) {

   /* save switch state */ 
   sRxBuffer.msg.devBus.x.devResp.getState.state.sw8.switchState = ch;
   return (TProtoFunc)ProtoWaitForChecksum;
}


static TProtoFunc ProtoWaitForReqSwitchState(UINT8 ch) {

   /* save switch state */ 
   sRxBuffer.msg.devBus.x.devReq.switchState.switchState = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}

static TProtoFunc ProtoWaitForRespSwitchState(UINT8 ch) {

   /* save switch state */ 
   sRxBuffer.msg.devBus.x.devResp.switchState.switchState = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}


static TProtoFunc ProtoWaitForReqSetClientAddr(UINT8 ch) {

   TProtoFunc ret = (TProtoFunc)ProtoWaitForReqSetClientAddr;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devReq.setClientAddr.clientAddr) == BUS_MAX_CLIENT_NUM) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   }        
   return ret;
}

static TProtoFunc ProtoWaitForRespGetClientAddr(UINT8 ch) {
       
   TProtoFunc ret = (TProtoFunc)ProtoWaitForRespGetClientAddr;

   /* save character */ 
   *spData = ch;
   spData++;
   if ((spData - (UINT8 *)sRxBuffer.msg.devBus.x.devResp.getClientAddr.clientAddr) == BUS_MAX_CLIENT_NUM) {
      /* packet complete */
      ret = (TProtoFunc)ProtoWaitForChecksum;
   }        
   return ret;
       
}

static TProtoFunc ProtoWaitForReqSetAddr(UINT8 ch) {

   /* save new addess */ 
   sRxBuffer.msg.devBus.x.devReq.setAddr.addr = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 

}

static TProtoFunc ProtoWaitForReqReadEepromAddrLo(UINT8 ch) {

   /* save address low byte */ 
   sRxBuffer.msg.devBus.x.devReq.readEeprom.addr = ch;
   return (TProtoFunc)ProtoWaitForReqReadEepromAddrHi;

}

static TProtoFunc ProtoWaitForReqReadEepromAddrHi(UINT8 ch) {

   /* save address high byte */ 
   sRxBuffer.msg.devBus.x.devReq.readEeprom.addr += ch * 256;
   return (TProtoFunc)ProtoWaitForChecksum; 

}


static TProtoFunc ProtoWaitForRespReadEeprom(UINT8 ch) {

   sRxBuffer.msg.devBus.x.devResp.readEeprom.data = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}

static TProtoFunc ProtoWaitForReqWriteEepromAddrLo(UINT8 ch) {

   /* save address low byte */ 
   sRxBuffer.msg.devBus.x.devReq.writeEeprom.addr = ch;
   return (TProtoFunc)ProtoWaitForReqWriteEepromAddrHi;

}

static TProtoFunc ProtoWaitForReqWriteEepromAddrHi(UINT8 ch) {

   /* save address high byte */ 
   sRxBuffer.msg.devBus.x.devReq.writeEeprom.addr += ch * 256;
   return (TProtoFunc)ProtoWaitForReqWriteEepromAddrData;
}

static TProtoFunc ProtoWaitForReqWriteEepromAddrData(UINT8 ch) {

   sRxBuffer.msg.devBus.x.devReq.writeEeprom.data = ch;
   return (TProtoFunc)ProtoWaitForChecksum; 
}


/*-----------------------------------------------------------------------------
* protocol function for state indication of a correctly received telegram
* function is used as intermediate state
*/
static TProtoFunc ProtoMsgOK(UINT8 ch) {
   return (TProtoFunc)ProtoWaitForStx;
}

/*-----------------------------------------------------------------------------
* protocol function for state indication of a incorrectly received telegram
* function is used as intermediate state
*/
static TProtoFunc ProtoMsgErr(UINT8 ch) {
   return (TProtoFunc)ProtoWaitForStx;
}
