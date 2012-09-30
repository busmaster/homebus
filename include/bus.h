/*-----------------------------------------------------------------------------
*  Bus.h
*/

#ifndef _BUS_H
#define _BUS_H


#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
*  Includes
*/
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
} TBusDevStartup;   /* Type 0xff */

typedef struct {
} TBusButtonPressed;   /* Type 0x01, 0x02, 0x03 */

typedef struct {
} TBusDevReqReboot;   /* Type 0x04 */

typedef struct {
} TBusDevReqUpdEnter;   /* Type  0x05 */

typedef struct {
} TBusDevRespUpdEnter;   /* Type  0x06 */

typedef struct {
   uint16_t wordAddr;
   uint16_t data[BUS_FWU_PACKET_SIZE / 2];
} TBusDevReqUpdData;   /* Type 0x07 */

typedef struct {
   uint16_t wordAddr;
} TBusDevRespUpdData;   /* Type 0x08 */

typedef struct {
} TBusDevReqUpdTerm;   /* Type  0x09 */

typedef struct {
   uint8_t success;
} TBusDevRespUpdTerm;   /* Type 0x0a */
   
typedef struct {        
} TBusDevReqInfo;       /* Type 0x0b */


typedef struct {
   uint8_t dirSwitch[BUS_DO31_NUM_SHADER];
   uint8_t onSwitch[BUS_DO31_NUM_SHADER];
} TBusDevInfoDo31;

typedef struct {
} TBusDevInfoSw8;

typedef enum {
   eBusDevTypeDo31 = 0x00,
   eBusDevTypeSw8 = 0x01
} TBusDevType;

typedef struct {        
   TBusDevType devType;
   uint8_t     version[BUS_DEV_INFO_VERSION_LEN];
   union {
      TBusDevInfoDo31 do31;
      TBusDevInfoSw8  sw8;
   } devInfo;
} TBusDevRespInfo;     /* Type 0x0c */


typedef struct {
   uint8_t digOut[BUS_DO31_DIGOUT_SIZE_SET];/* je Ausgang 2 Bit:           */
                                            /*                     0*: keine Änderung */
                                            /*                     10: Ausgang auf 0  */
                                            /*                     11: Ausgang auf 1  */
   uint8_t shader[BUS_DO31_SHADER_SIZE_SET];/* je Rollladen 2 Bit: 00: keine Act. */
                                            /*                     01: AUF        */
                                            /*                     10: AB         */
                                            /*                     11: STOP       */
} TBusDevSetStateDo31;

typedef struct {
   uint8_t digOut[BUS_DO31_DIGOUT_SIZE_GET];/* je Ausgang 1 Bit      */
   uint8_t shader[BUS_DO31_SHADER_SIZE_GET];/* je Rollladen 2 Bit: 00: nicht konfiguriert */
                                            /*                     01: AUF        */
                                            /*                     10: AB         */
                                            /*                     11: STOP       */
} TBusDevGetStateDo31;

typedef struct {
   uint8_t switchState; 
} TBusDevGetStateSw8;

typedef struct {        
   TBusDevType devType;
   union {
      TBusDevSetStateDo31 do31;
   } state;
} TBusDevReqSetState;   /* Type 0x0d */

typedef struct {        
} TBusDevRespSetState;  /* Type 0x0e */


typedef struct {        
} TBusDevReqGetState;   /* Type 0x0f */

typedef struct {        
   TBusDevType devType;
   union {
      TBusDevGetStateDo31 do31;
      TBusDevGetStateSw8  sw8;
   } state;
} TBusDevRespGetState;  /* Type 0x10 */

typedef struct {
   uint8_t  switchState;  /* bitfield for 8 switch states; 0 off, 1 on */        
} TBusDevReqSwitchState;        /* type 0x11 */

typedef struct {  
   uint8_t  switchState;   /* same as request */
} TBusDevRespSwitchState;       /* type 0x12 */

typedef struct {        
   uint8_t clientAddr[BUS_MAX_CLIENT_NUM];  /* address 0xff means 'not used' */
} TBusDevReqSetClientAddr;      /* type 0x13 */

typedef struct {        
} TBusDevRespSetClientAddr;     /* type 0x14 */

typedef struct {        
} TBusDevReqGetClientAddr;      /* type 0x15 */

typedef struct {   
   uint8_t clientAddr[BUS_MAX_CLIENT_NUM];  /* address 0xff means 'not used' */
} TBusDevRespGetClientAddr;     /* type 0x16 */

typedef struct {        
   uint8_t addr;
} TBusDevReqSetAddr;            /* type 0x17 */

typedef struct {   
} TBusDevRespSetAddr;           /* type 0x18 */

typedef struct {        
   uint16_t addr;
} TBusDevReqEepromRead;         /* type 0x19 */

typedef struct {   
   uint8_t data;
} TBusDevRespEepromRead;        /* type 0x1a */

typedef struct {        
   uint16_t addr;
   uint8_t  data;
} TBusDevReqEepromWrite;        /* type 0x1b */

typedef struct {   
} TBusDevRespEepromWrite;       /* type 0x1c */


typedef union {
   TBusDevReqReboot         reboot;
   TBusDevReqUpdEnter       updEnter;
   TBusDevReqUpdData        updData;
   TBusDevReqUpdTerm        updTerm;
   TBusDevReqInfo           info;
   TBusDevReqSetState       setState;
   TBusDevReqGetState       getState;
   TBusDevReqSwitchState    switchState;
   TBusDevReqSetClientAddr  setClientAddr;
   TBusDevReqGetClientAddr  getClientAddr;
   TBusDevReqSetAddr        setAddr;
   TBusDevReqEepromRead     readEeprom;
   TBusDevReqEepromWrite    writeEeprom;
} TUniDevReq;

typedef union {
   TBusDevRespUpdEnter       updEnter;
   TBusDevRespUpdData        updData;
   TBusDevRespUpdTerm        updTerm;
   TBusDevRespInfo           info;
   TBusDevRespSetState       setState;
   TBusDevRespGetState       getState;
   TBusDevRespSwitchState    switchState;
   TBusDevRespSetClientAddr  setClientAddr;
   TBusDevRespGetClientAddr  getClientAddr;
   TBusDevRespSetAddr        setAddr;
   TBusDevRespEepromRead     readEeprom;
   TBusDevRespEepromWrite    writeEeprom;
} TUniDevResp;

typedef struct {
   uint8_t        receiverAddr;
   union {
      TUniDevResp devResp;
      TUniDevReq  devReq;
   } x;            
} TBusDev;

typedef union {              
   TBusDevStartup       devStartup;
   TBusButtonPressed    buttonPressed;
   TBusDev              devBus;
} TBusUniTelegramm; 
  
/* Telegrammtypen */
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
   eBusDevStartup =                      0xff
} TBusMsgType;

typedef struct {
   TBusMsgType       type;
   uint8_t           senderAddr;                    
   TBusUniTelegramm  msg;             
} TBusTelegramm;

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/
void           BusInit(int sioHandle);
uint8_t        BusCheck(void);
TBusTelegramm *BusMsgBufGet(void);
void           BusSend(TBusTelegramm *pMsg);

#ifdef __cplusplus
}
#endif
  
#endif
