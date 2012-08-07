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

#define STX 0x02
#define ESC 0x1B             

/* start value for checksum calculation */
#define CHECKSUM_START   0x55        

/* Port D bit 5 controls bus transceiver power down */
#define BUS_TRANSCEIVER_POWER_DOWN \
   PORTD |= (1 << 5)
   
#define BUS_TRANSCEIVER_POWER_UP \
   PORTD &= ~(1 << 5)
 

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef void* (* TProtoFunc)(UINT8 ch);

/*-----------------------------------------------------------------------------
*  Variables
*/                                
/* buffer for bus telegram just receiving/just received */
static TBusTelegramm sRxBuffer; 
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


static TProtoFunc ProtoMsgOK(UINT8 ch);
static TProtoFunc ProtoMsgErr(UINT8 ch);
   

/*-----------------------------------------------------------------------------
*  check for new received bus telegram
*  return codes:
*              BUS_NO_MSG     no telegram in progress
*              BUS_MSG_OK     telegram received completely
*              BUS_MSG_RXING  telegram receiving in progress
*              BUS_MSG_ERROR  errorous telegram received (checksum error)
*/
UINT8 BusCheck(void) {
                     
   static UINT8 ret = BUS_NO_MSG;
   UINT8        ch;
   UINT8        retTmp;
   UINT8        numRxChar;

   numRxChar = SioRead(&ch);
   if (numRxChar != 0) {
      ret = BusDecode(ch);
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
static UINT8 BusDecode(UINT8 ch) {

   static TProtoFunc sProtoState = (TProtoFunc)ProtoWaitForStx;
   static BOOL       sStuffByte = FALSE;
   UINT8             ret = BUS_MSG_RXING;

   if (sProtoState != (TProtoFunc)ProtoWaitForStx) {
      if (ch == ESC) {
         sStuffByte = TRUE; 
         return ret;
      } else if (ch == STX) {
         /* unexpected telegram start detected */
         sStuffByte = FALSE;
         sCheckSum = CHECKSUM_START + STX;
         sProtoState = (TProtoFunc)ProtoWaitForAddr;
         return BUS_MSG_ERROR;
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
   } else if (sProtoState == (TProtoFunc)ProtoMsgErr) {
      ret = BUS_MSG_ERROR;
   } 

   return ret;
}           

/*-----------------------------------------------------------------------------
* send bus telegram
*/
void BusSend(TBusTelegramm *pMsg) {
  
   UINT8 ch;            
   UINT8 checkSum = CHECKSUM_START;
         
   /* enable bus transmitter */      
   BUS_TRANSCEIVER_POWER_UP;
   
   ch = STX;
   SioWrite(ch);
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
      case eBusDevRespUpdEnter: 
         ch = pMsg->msg.devBus.receiverAddr;
         TransmitCharProt(ch);
         checkSum += ch;
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
      default: 
         break;   
   }
   TransmitCharProt(checkSum);
   SioWriteReady();
   SioReadFlush();
   BUS_TRANSCEIVER_POWER_DOWN;
}

/*-----------------------------------------------------------------------------
*  send character with low level protocol translation
*  STX -> ESC + ~STX
*  ESC -> ESC + ~ESC
*/
static void TransmitCharProt(UINT8 data) {
      
   if (data == STX) {  
      SioWrite(ESC);
      SioWrite(~STX);
   } else if (data == ESC) {
      SioWrite(ESC);
      SioWrite(~ESC);
   } else {
      SioWrite(data);
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
      case eBusDevReqReboot:
      case eBusDevReqUpdEnter:
      case eBusDevReqUpdData:
      case eBusDevReqUpdTerm:
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
         /* in this cases just the checksum is missing */                                             
         ret = (TProtoFunc)ProtoWaitForChecksum;
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
