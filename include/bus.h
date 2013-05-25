/*
 * bus.h
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
#ifndef _BUS_H
#define _BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*-----------------------------------------------------------------------------
*  Macros
*/                     
#define BUS_FWU_PACKET_SIZE    32   /* Anzahl der Bytes (geradzahlig)*/
#define BUS_DEV_INFO_VERSION_LEN 16 /* länge des Versionsstrings */

#define BUS_DO31_NUM_SHADER    15   /* max. Anzahl Rollladen-Gruppen */   
#define BUS_DO31_DIGOUT_SIZE_SET   8    /* 8 Byte für 31 DO  (je 2 bit) */
#define BUS_DO31_SHADER_SIZE_SET   4    /* 4 Byte für 15 Rolladen-Gruppen (je 2 bit) */
#define BUS_DO31_DIGOUT_SIZE_GET   4    /* 4 Byte für 31 DO  (je 1 bit) */
#define BUS_DO31_SHADER_SIZE_GET   4    /* 4 Byte für 15 Rolladen-Gruppen (je 2 bit) */

#define BUS_DO31_DIGOUT_SIZE_SET_VALUE     8    /* 8 Byte für 31 DO  (je 2 bit) */
#define BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE  4    /* 4 Byte für 31 DO  (je 1 bit) */
#define BUS_DO31_SHADER_SIZE_SET_VALUE     15   /* 1 Byte je 15 Rollladen */
#define BUS_DO31_SHADER_SIZE_ACTUAL_VALUE  15   /* 1 Byte je Rollladen */

#define BUS_MAX_CLIENT_NUM         16   /* size of list for setting client addresses */
#define BUS_CLIENT_ADDRESS_INVALID 0xff   

/* return codes for function BusCheck */
#define BUS_NO_MSG     0
#define BUS_MSG_OK     1
#define BUS_MSG_RXING  2
#define BUS_MSG_ERROR  3

/*-----------------------------------------------------------------------------
*  typedefs
*/

typedef struct { 
} __attribute__ ((packed)) TBusDevStartup;   /* Type 0xff */

typedef struct {
} __attribute__ ((packed)) TBusButtonPressed;   /* Type 0x01, 0x02, 0x03 */

typedef struct {
} __attribute__ ((packed)) TBusDevReqReboot;   /* Type 0x04 */

typedef struct {
} __attribute__ ((packed)) TBusDevReqUpdEnter;   /* Type  0x05 */

typedef struct {
} __attribute__ ((packed)) TBusDevRespUpdEnter;   /* Type  0x06 */

typedef struct {
   uint16_t wordAddr;
   uint16_t data[BUS_FWU_PACKET_SIZE / 2];
} __attribute__ ((packed)) TBusDevReqUpdData;   /* Type 0x07 */

typedef struct {
   uint16_t wordAddr;
} __attribute__ ((packed)) TBusDevRespUpdData;   /* Type 0x08 */

typedef struct {
} __attribute__ ((packed)) TBusDevReqUpdTerm;   /* Type  0x09 */

typedef struct {
   uint8_t success;
} __attribute__ ((packed)) TBusDevRespUpdTerm;   /* Type 0x0a */
   
typedef struct {        
} __attribute__ ((packed)) TBusDevReqInfo;       /* Type 0x0b */


typedef struct {
   uint8_t dirSwitch[BUS_DO31_NUM_SHADER];
   uint8_t onSwitch[BUS_DO31_NUM_SHADER];
} __attribute__ ((packed)) TBusDevInfoDo31;

typedef struct {
} __attribute__ ((packed)) TBusDevInfoSw8;

typedef enum {
   eBusDevTypeDo31 = 0x00,
   eBusDevTypeSw8 = 0x01
} __attribute__ ((packed)) TBusDevType;

typedef struct {        
   TBusDevType  devType;
   uint8_t  version[BUS_DEV_INFO_VERSION_LEN];
   union {
      TBusDevInfoDo31 do31;
      TBusDevInfoSw8  sw8;
   } devInfo;
} __attribute__ ((packed)) TBusDevRespInfo;     /* Type 0x0c */


typedef struct {
   uint8_t digOut[BUS_DO31_DIGOUT_SIZE_SET];/* je Ausgang 2 Bit:           */
                                            /*                     0*: keine Änderung */
                                            /*                     10: Ausgang auf 0  */
                                            /*                     11: Ausgang auf 1  */
   uint8_t shader[BUS_DO31_SHADER_SIZE_SET];/* je Rollladen 2 Bit: 00: keine Act. */
                                            /*                     01: AUF        */
                                            /*                     10: AB         */
                                            /*                     11: STOP       */
} __attribute__ ((packed)) TBusDevSetStateDo31;

typedef struct {        
   TBusDevType devType;
   union {
      TBusDevSetStateDo31 do31;
   } state;
} __attribute__ ((packed)) TBusDevReqSetState;   /* Type 0x0d */

typedef struct {        
} __attribute__ ((packed)) TBusDevRespSetState;  /* Type 0x0e */


typedef struct {        
} __attribute__ ((packed)) TBusDevReqGetState;   /* Type 0x0f */

typedef struct {
   uint8_t digOut[BUS_DO31_DIGOUT_SIZE_GET];/* je Ausgang 1 Bit      */
   uint8_t shader[BUS_DO31_SHADER_SIZE_GET];/* je Rollladen 2 Bit: 00: nicht konfiguriert */
                                            /*                     01: AUF        */
                                            /*                     10: AB         */
                                            /*                     11: STOP       */
} __attribute__ ((packed)) TBusDevGetStateDo31;


typedef struct {
   uint8_t switchState; 
} __attribute__ ((packed)) TBusDevGetStateSw8;

typedef struct {        
   TBusDevType devType;
   union {
      TBusDevGetStateDo31 do31;
      TBusDevGetStateSw8  sw8;
   } state;
} __attribute__ ((packed)) TBusDevRespGetState;  /* Type 0x10 */

typedef struct {
   uint8_t  switchState;  /* bitfield for 8 switch states; 0 off, 1 on */        
} __attribute__ ((packed)) TBusDevReqSwitchState;        /* type 0x11 */

typedef struct {  
   uint8_t  switchState;   /* same as request */
} __attribute__ ((packed)) TBusDevRespSwitchState;       /* type 0x12 */

typedef struct {        
   uint8_t clientAddr[BUS_MAX_CLIENT_NUM];  /* address 0xff means 'not used' */
} __attribute__ ((packed)) TBusDevReqSetClientAddr;      /* type 0x13 */

typedef struct {        
} __attribute__ ((packed)) TBusDevRespSetClientAddr;     /* type 0x14 */

typedef struct {        
} __attribute__ ((packed)) TBusDevReqGetClientAddr;      /* type 0x15 */

typedef struct {   
   uint8_t clientAddr[BUS_MAX_CLIENT_NUM];  /* address 0xff means 'not used' */
} __attribute__ ((packed)) TBusDevRespGetClientAddr;     /* type 0x16 */

typedef struct {        
   uint8_t addr;
} __attribute__ ((packed)) TBusDevReqSetAddr;            /* type 0x17 */

typedef struct {   
} __attribute__ ((packed)) TBusDevRespSetAddr;           /* type 0x18 */

typedef struct {        
   uint16_t addr;
} __attribute__ ((packed)) TBusDevReqEepromRead;         /* type 0x19 */

typedef struct {   
   uint8_t data;
} __attribute__ ((packed)) TBusDevRespEepromRead;        /* type 0x1a */

typedef struct {        
   uint16_t addr;
   uint8_t  data;
} __attribute__ ((packed)) TBusDevReqEepromWrite;        /* type 0x1b */

typedef struct {   
} __attribute__ ((packed)) TBusDevRespEepromWrite;       /* type 0x1c */


typedef struct {
   uint8_t digOut[BUS_DO31_DIGOUT_SIZE_SET_VALUE];/* 2 bits per output:                      */
                                                  /*                     00: no change       */
                                                  /*                     01: trigger pulse   */
                                                  /*                     10: set output to 0 */
                                                  /*                     11: set output to 1 */
   uint8_t shader[BUS_DO31_SHADER_SIZE_SET_VALUE];/* 1 byte per shader:  0 .. 100: new set value */
                                                  /*                     255:      stop                    */
                                                  /*                     254:      no change               */
} __attribute__ ((packed)) TBusDevSetValueDo31;

typedef struct {        
   TBusDevType devType;
   union {
      TBusDevSetValueDo31 do31;
   } setValue;
} __attribute__ ((packed)) TBusDevReqSetValue;   /* Type 0x1d */

typedef struct {        
} __attribute__ ((packed)) TBusDevRespSetValue;  /* Type 0x1e */


typedef struct {        
} __attribute__ ((packed)) TBusDevReqActualValue;   /* Type 0x1f */

typedef struct {
   uint8_t digOut[BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE];/* 1 bit per output                                       */
   uint8_t shader[BUS_DO31_SHADER_SIZE_ACTUAL_VALUE];/* 1 byte per shader: current position 0 .. 100 (stopped) */
                                                     /*                    252:     not configured             */
                                                     /*                    253:     opening                    */
                                                     /*                    254:     closing                    */
                                                     /*                    255:     error                      */
} __attribute__ ((packed)) TBusDevActualValueDo31;

typedef struct {
   uint8_t state;
} __attribute__ ((packed)) TBusDevActualValueSw8;

typedef struct {        
   TBusDevType devType;
   union {
      TBusDevActualValueDo31 do31;
      TBusDevActualValueSw8  sw8;
   } actualValue;
} __attribute__ ((packed)) TBusDevRespActualValue;  /* Type 0x20 */


typedef union {
   TBusDevReqReboot         reboot;
   TBusDevReqUpdEnter       updEnter;
   TBusDevReqUpdData        updData;
   TBusDevReqUpdTerm        updTerm;
   TBusDevReqInfo           info;
   TBusDevReqSetState       setState;
   TBusDevReqGetState       getState;
   TBusDevReqSetValue       setValue;
   TBusDevReqActualValue    actualValue;
   TBusDevReqSwitchState    switchState;
   TBusDevReqSetClientAddr  setClientAddr;
   TBusDevReqGetClientAddr  getClientAddr;
   TBusDevReqSetAddr        setAddr;
   TBusDevReqEepromRead     readEeprom;
   TBusDevReqEepromWrite    writeEeprom;
} __attribute__ ((packed)) TUniDevReq;

typedef union {
   TBusDevRespUpdEnter       updEnter;
   TBusDevRespUpdData        updData;
   TBusDevRespUpdTerm        updTerm;
   TBusDevRespInfo           info;
   TBusDevRespSetState       setState;
   TBusDevRespGetState       getState;
   TBusDevRespSetValue       setValue;
   TBusDevRespActualValue    actualValue;
   TBusDevRespSwitchState    switchState;
   TBusDevRespSetClientAddr  setClientAddr;
   TBusDevRespGetClientAddr  getClientAddr;
   TBusDevRespSetAddr        setAddr;
   TBusDevRespEepromRead     readEeprom;
   TBusDevRespEepromWrite    writeEeprom;
} __attribute__ ((packed)) TUniDevResp;

typedef struct {
   uint8_t        receiverAddr;
   union {
      TUniDevResp devResp;
      TUniDevReq  devReq;
   } x;            
} __attribute__ ((packed)) TBusDev;

typedef union {              
   TBusDevStartup       devStartup;
   TBusButtonPressed    buttonPressed;
   TBusDev              devBus;
} __attribute__ ((packed)) TBusUniTelegram; 
  
/* telegram types */
typedef enum {
   eBusButtonPressed1 =                  0x01,
   eBusButtonPressed2 =                  0x02,
   eBusButtonPressed1_2 =                0x03,
   eBusDevReqReboot =                    0x04,
   eBusDevReqUpdEnter =                  0x05,
   eBusDevRespUpdEnter =                 0x06,
   eBusDevReqUpdData =                   0x07,
   eBusDevRespUpdData =                  0x08,
   eBusDevReqUpdTerm =                   0x09,
   eBusDevRespUpdTerm =                  0x0a,
   eBusDevReqInfo =                      0x0b,
   eBusDevRespInfo =                     0x0c,
   eBusDevReqSetState =                  0x0d,
   eBusDevRespSetState =                 0x0e,
   eBusDevReqGetState =                  0x0f,
   eBusDevRespGetState =                 0x10,
   eBusDevReqSwitchState =               0x11,
   eBusDevRespSwitchState =              0x12,
   eBusDevReqSetClientAddr =             0x13,
   eBusDevRespSetClientAddr =            0x14,
   eBusDevReqGetClientAddr =             0x15,
   eBusDevRespGetClientAddr =            0x16,
   eBusDevReqSetAddr =                   0x17,
   eBusDevRespSetAddr =                  0x18,
   eBusDevReqEepromRead =                0x19,
   eBusDevRespEepromRead =               0x1a,
   eBusDevReqEepromWrite =               0x1b,
   eBusDevRespEepromWrite =              0x1c,
   eBusDevReqSetValue =                  0x1d,
   eBusDevRespSetValue =                 0x1e,
   eBusDevReqActualValue =               0x1f,
   eBusDevRespActualValue =              0x20,
   eBusDevStartup =                      0xff
} __attribute__ ((packed)) TBusMsgType;

typedef struct {
   uint8_t          senderAddr;                    
   TBusMsgType      type;
   TBusUniTelegram  msg;             
} __attribute__ ((packed)) TBusTelegram;


/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/
void           BusInit(int sioHandle);
uint8_t        BusCheck(void);
TBusTelegram   *BusMsgBufGet(void);
void           BusSend(TBusTelegram *pMsg);

#ifdef __cplusplus
}
#endif
  
#endif
