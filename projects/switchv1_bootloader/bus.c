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

#include "sysdef.h"
#include "sio.h"
#include "bus.h"

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

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef void* (* TProtoFunc)(uint8_t ch);

/*-----------------------------------------------------------------------------
*  Variables
*/                                
/* buffer for bus telegram just receiving/just received */
static TBusTelegram sRxBuffer; 
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


static TProtoFunc ProtoMsgOK(uint8_t ch);
static TProtoFunc ProtoMsgErr(uint8_t ch);
   

/*-----------------------------------------------------------------------------
*  check for new received bus telegram
*  return codes:
*              BUS_NO_MSG     no telegram in progress
*              BUS_MSG_OK     telegram received completely
*              BUS_MSG_RXING  telegram receiving in progress
*              BUS_MSG_ERROR  errorous telegram received (checksum error)
*/
uint8_t BusCheck(void) {
                     
   static uint8_t ret = BUS_NO_MSG;
   uint8_t        ch;
   uint8_t        retTmp;
   uint8_t        numRxChar;

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
TBusTelegram *BusMsgBufGet(void) {
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
static uint8_t BusDecode(uint8_t ch) {

   static TProtoFunc sProtoState = (TProtoFunc)ProtoWaitForStx;
   static bool       sStuffByte = false;
   uint8_t             ret = BUS_MSG_RXING;

   if (sProtoState != (TProtoFunc)ProtoWaitForStx) {
      if (ch == ESC) {
         sStuffByte = true; 
         return ret;
      } else if (ch == STX) {
         /* unexpected telegram start detected */
         sStuffByte = false;
         sCheckSum = CHECKSUM_START + STX;
         sProtoState = (TProtoFunc)ProtoWaitForAddr;
         return BUS_MSG_ERROR;
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
   } else if (sProtoState == (TProtoFunc)ProtoMsgErr) {
      ret = BUS_MSG_ERROR;
   } 

   return ret;
}           

/*-----------------------------------------------------------------------------
* send bus telegram
*/
void BusSend(TBusTelegram *pMsg) {
  
   uint8_t ch;            
   uint8_t checkSum = CHECKSUM_START;
             
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
   SioReadFlush();
}

/*-----------------------------------------------------------------------------
*  send character with low level protocol translation
*  STX -> ESC + ~STX
*  ESC -> ESC + ~ESC
*/
static void TransmitCharProt(uint8_t data) {
      
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
         /* in this cases just the checksum is missing */                                             
         ret = (TProtoFunc)ProtoWaitForChecksum;
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

