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
#define BUS_DEV_INFO_VERSION_LEN 16 /* length of version string */

#define BUS_DO31_NUM_SHADER    15   /* max. Anzahl Rollladen-Gruppen */
#define BUS_DO31_DIGOUT_SIZE_SET   8    /* 8 bytes for 31 dig outs (2 bit for each) */
#define BUS_DO31_SHADER_SIZE_SET   4    /* 4 bytes for 15 shader groups (2 bit for each) */
#define BUS_DO31_DIGOUT_SIZE_GET   4    /* 4 bytes for 31 dig outs (1 bit for each) */
#define BUS_DO31_SHADER_SIZE_GET   4    /* 4 bytes for 15 shader groups (2 bit for each) */

#define BUS_DO31_DIGOUT_SIZE_SET_VALUE     8    /* 8 bytes for 31 dig outs (2 bit for each) */
#define BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE  4    /* 4 bytes for 31 dig outs (1 bit for each) */
#define BUS_DO31_SHADER_SIZE_SET_VALUE     15   /* 1 byte for each shader */
#define BUS_DO31_SHADER_SIZE_ACTUAL_VALUE  15   /* 1 byte for each shader */

#define BUS_SW8_DIGOUT_SIZE_SET_VALUE      2    /* 2 byte for 8 DO (2 bit each) */
#define BUS_SW16_LED_SIZE_SET_VALUE        4    /* 8 leds, 4 bit each -> 4 byte for 8 Leds */

#define BUS_RS485IF_SIZE_SET_VALUE         32
#define BUS_RS485IF_SIZE_ACTUAL_VALUE      32

#define BUS_PWM4_PWM_SIZE_SET_VALUE        4    /* 4 uint16 for pwm outs */
#define BUS_PWM4_PWM_SIZE_ACTUAL_VALUE     4    /* 4 uint16 for pwm outs */

#define BUS_PWM16_PWM_SIZE_SET_VALUE       16   /* 16 uint16 for pwm outs */
#define BUS_PWM16_PWM_SIZE_ACTUAL_VALUE    16   /* 16 uint16 for pwm outs */

#define BUS_DIAG_SIZE                      32

#define BUS_MAX_CLIENT_NUM         16   /* size of list for setting client addresses */
#define BUS_CLIENT_ADDRESS_INVALID 0xff

#define BUS_MAX_VAR_SIZE                   32

/* return codes for function BusCheck */
#define BUS_NO_MSG     0
#define BUS_MSG_OK     1
#define BUS_MSG_RXING  2
#define BUS_MSG_ERROR  3
#define BUS_IF_ERROR   4

/* return codes for function BusSend */
#define BUS_SEND_OK            0
#define BUS_SEND_TX_ERROR      1
#define BUS_SEND_BAD_TYPE      2
#define BUS_SEND_BAD_VARLEN    3
#define BUS_SEND_BAD_LEN       4

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

typedef struct {
} __attribute__ ((packed)) TBusDevInfoLum;

typedef struct {
} __attribute__ ((packed)) TBusDevInfoLed;

typedef struct {
} __attribute__ ((packed)) TBusDevInfoSw16;

typedef struct {
} __attribute__ ((packed)) TBusDevInfoWind;

typedef struct {
} __attribute__ ((packed)) TBusDevInfoSw8Cal;

typedef struct {
} __attribute__ ((packed)) TBusDevInfoRs485If;

typedef struct {
} __attribute__ ((packed)) TBusDevInfoPwm4;

typedef struct {
} __attribute__ ((packed)) TBusDevInfoPwm16;

typedef struct {
} __attribute__ ((packed)) TBusDevInfoSmIf;

typedef enum {
   eBusDevTypeDo31    = 0x00,
   eBusDevTypeSw8     = 0x01,
   eBusDevTypeLum     = 0x02,
   eBusDevTypeLed     = 0x03,
   eBusDevTypeSw16    = 0x04,
   eBusDevTypeWind    = 0x05,
   eBusDevTypeSw8Cal  = 0x06,
   eBusDevTypeRs485If = 0x07,
   eBusDevTypePwm4    = 0x08,
   eBusDevTypeSmIf    = 0x09,
   eBusDevTypePwm16   = 0x0a,   
   eBusDevTypeInv     = 0xff
} __attribute__ ((packed)) TBusDevType;

typedef struct {
   TBusDevType  devType;
   uint8_t  version[BUS_DEV_INFO_VERSION_LEN];
   union {
      TBusDevInfoDo31    do31;
      TBusDevInfoSw8     sw8;
      TBusDevInfoLum     lum;
      TBusDevInfoLed     led;
      TBusDevInfoSw16    sw16;
      TBusDevInfoWind    wind;
      TBusDevInfoSw8Cal  sw8Cal;
      TBusDevInfoRs485If rs485if;
      TBusDevInfoPwm4    pwm4;
      TBusDevInfoSmIf    smif;
      TBusDevInfoPwm16   pwm16;	  
   } devInfo;
} __attribute__ ((packed)) TBusDevRespInfo;     /* Type 0x0c */


typedef struct {
   uint8_t digOut[BUS_DO31_DIGOUT_SIZE_SET];/* je Ausgang 2 Bit:           */
                                            /*                     0*: no change        */
                                            /*                     10: set dig out to 0 */
                                            /*                     11: set dig out to 1 */
   uint8_t shader[BUS_DO31_SHADER_SIZE_SET];/* je Rollladen 2 Bit: 00: no change        */
                                            /*                     01: up               */
                                            /*                     10: down             */
                                            /*                     11: stop             */
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
   uint8_t digOut[BUS_SW8_DIGOUT_SIZE_SET_VALUE]; /* 2 bits per output:                      */
                                                  /*                     00: no change       */
                                                  /*                     01: trigger pulse   */
                                                  /*                     10: set output to 0 */
                                                  /*                     11: set output to 1 */
} __attribute__ ((packed)) TBusDevSetValueSw8;

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
   uint8_t led_state[BUS_SW16_LED_SIZE_SET_VALUE];
} __attribute__ ((packed)) TBusDevSetValueSw16;

typedef struct {
   uint8_t state[BUS_RS485IF_SIZE_SET_VALUE];
} __attribute__ ((packed)) TBusDevSetValueRs485if;

typedef struct {
    uint8_t  set; /* 2 bits per output                                        */
                  /* 00: no change, ignore pwm[] field                        */
                  /* 01: on: set to current pwm value, ignore pwm[] field     */
                  /* 10: on: set to value from pwm[] field                    */
                  /* 11: off, ignore pwm[] field                              */
    uint16_t pwm[BUS_PWM4_PWM_SIZE_SET_VALUE];
} __attribute__ ((packed)) TBusDevSetValuePwm4;

typedef struct {
    uint32_t set; /* 2 bits per output                                        */
                  /* 00: no change, ignore pwm[] field                        */
                  /* 01: on: set to current pwm value, ignore pwm[] field     */
                  /* 10: on: set to value from pwm[] field                    */
                  /* 11: off, ignore pwm[] field                              */
    uint16_t pwm[BUS_PWM16_PWM_SIZE_SET_VALUE];
} __attribute__ ((packed)) TBusDevSetValuePwm16;

typedef struct {
   TBusDevType devType;
   union {
      TBusDevSetValueSw8     sw8;
      TBusDevSetValueDo31    do31;
      TBusDevSetValueSw16    sw16;
      TBusDevSetValueRs485if rs485if;
      TBusDevSetValuePwm4    pwm4;
      TBusDevSetValuePwm16   pwm16;	  
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
                                                     /*                    253:     closing                    */
                                                     /*                    254:     opening                    */
                                                     /*                    255:     error                      */
} __attribute__ ((packed)) TBusDevActualValueDo31;

typedef struct {
   uint8_t state;
} __attribute__ ((packed)) TBusDevActualValueSw8;

typedef struct {
   uint8_t state;              /* state of luminance switches       */
   uint8_t lum_low;            /* actual luminance (ADC low byte)   */
   uint8_t lum_high;           /* actual luminance (ADC high byte)  */
} __attribute__ ((packed)) TBusDevActualValueLum;

typedef struct {
   uint8_t state;
} __attribute__ ((packed)) TBusDevActualValueLed;

typedef struct {
   uint8_t led_state[BUS_SW16_LED_SIZE_SET_VALUE];
   uint8_t input_state;
} __attribute__ ((packed)) TBusDevActualValueSw16;

typedef struct {
   uint8_t state;              /* state of luminance switches       */
   uint8_t wind;               /* actual wind                       */
} __attribute__ ((packed)) TBusDevActualValueWind;

typedef struct {
   uint8_t state[BUS_RS485IF_SIZE_ACTUAL_VALUE];
} __attribute__ ((packed)) TBusDevActualValueRs485if;

typedef struct {
    uint8_t  state; /* 1 bit per output: 0 off, 1 on */
    uint16_t pwm[BUS_PWM4_PWM_SIZE_ACTUAL_VALUE];
} __attribute__ ((packed)) TBusDevActualValuePwm4;

typedef struct {
    uint16_t state; /* 1 bit per output: 0 off, 1 on */
    uint16_t pwm[BUS_PWM16_PWM_SIZE_ACTUAL_VALUE];
} __attribute__ ((packed)) TBusDevActualValuePwm16;

typedef struct {
   uint32_t countA_plus;
   uint32_t countA_minus;
   uint32_t countR_plus;
   uint32_t countR_minus;
   uint32_t activePower_plus;
   uint32_t activePower_minus;
   uint32_t reactivePower_plus;
   uint32_t reactivePower_minus;
} __attribute__ ((packed)) TBusDevActualValueSmif;

typedef struct {
   TBusDevType devType;
   union {
      TBusDevActualValueDo31    do31;
      TBusDevActualValueSw8     sw8;
      TBusDevActualValueLum     lum;
      TBusDevActualValueLed     led;
      TBusDevActualValueSw16    sw16;
      TBusDevActualValueWind    wind;
      TBusDevActualValueRs485if rs485if;
      TBusDevActualValuePwm4    pwm4;
      TBusDevActualValueSmif    smif;
      TBusDevActualValuePwm16   pwm16;
   } actualValue;
} __attribute__ ((packed)) TBusDevRespActualValue;  /* Type 0x20 */


typedef struct {
   TBusDevType devType;
   union {
      TBusDevActualValueDo31    do31;
      TBusDevActualValueSw8     sw8;
      TBusDevActualValueLum     lum;
      TBusDevActualValueLed     led;
      TBusDevActualValueSw16    sw16;
      TBusDevActualValueWind    wind;
      TBusDevActualValueRs485if rs485if;
      TBusDevActualValuePwm4    pwm4;
      TBusDevActualValueSmif    smif;
      TBusDevActualValuePwm16   pwm16;
   } actualValue;
} __attribute__ ((packed)) TBusDevReqActualValueEvent;  /* Type 0x21 */

typedef struct {
   TBusDevType devType;
   union {
      TBusDevActualValueDo31    do31;
      TBusDevActualValueSw8     sw8;
      TBusDevActualValueLum     lum;
      TBusDevActualValueLed     led;
      TBusDevActualValueSw16    sw16;
      TBusDevActualValueWind    wind;
      TBusDevActualValueRs485if rs485if;
      TBusDevActualValuePwm4    pwm4;
      TBusDevActualValueSmif    smif;
      TBusDevActualValuePwm16   pwm16;
   } actualValue; /* same as request */
} __attribute__ ((packed)) TBusDevRespActualValueEvent;  /* Type 0x22 */

typedef enum {
   eBusClockCalibCommandCalibrate = 0,
   eBusClockCalibCommandIdle = 1,
   eBusClockCalibCommandGetState = 2
} __attribute__ ((packed)) TClockCalibCommand;

typedef struct {
    TClockCalibCommand command;
    uint8_t address; /* address of device to calibrate */
} __attribute__ ((packed)) TBusDevReqClockCalib;        /* type 0x23 */

typedef enum {
    eBusClockCalibStateIdle = 0,
    eBusClockCalibStateSuccess = 1,
    eBusClockCalibStateBusy = 2,
    eBusClockCalibStateError = 3,
    eBusClockCalibStateInternalError = 4,
    eBusClockCalibStateInvalidCommand = 5
} __attribute__ ((packed)) TClockCalibState;

typedef struct {
    TClockCalibState state;
    uint8_t          address;
} __attribute__ ((packed)) TBusDevRespClockCalib;       /* type 0x24 */

typedef enum {
   eBusDoClockCalibInit = 0,
   eBusDoClockCalibContiune = 1
} __attribute__ ((packed)) TDoClockCalibCommand;

typedef struct {
    TDoClockCalibCommand command;
} __attribute__ ((packed)) TBusDevReqDoClockCalib;        /* type 0x25 */

typedef enum {
    eBusDoClockCalibStateSuccess = 0,
    eBusDoClockCalibStateContiune = 1,
    eBusDoClockCalibStateError = 2
} __attribute__ ((packed)) TDoClockCalibState;

typedef struct {
    TDoClockCalibState state;
} __attribute__ ((packed)) TBusDevRespDoClockCalib;       /* type 0x26 */

typedef struct {
} __attribute__ ((packed)) TBusDevReqDiag;                /* type 0x27 */

typedef struct {
    TBusDevType devType;
    uint8_t     data[BUS_DIAG_SIZE];
} __attribute__ ((packed)) TBusDevRespDiag;               /* type 0x28 */

typedef struct {
    uint16_t    year;
    uint8_t     month;
    uint8_t     day;
    uint8_t     hour;
    uint8_t     minute;
    uint8_t     second;
    int8_t      zoneHour;
    uint8_t     zoneMinute;
} __attribute__ ((packed)) TBusTime;

typedef struct {
} __attribute__ ((packed)) TBusDevReqGetTime;             /* type 0x29 */

typedef struct {
    TBusTime    time;
} __attribute__ ((packed)) TBusDevRespGetTime;            /* type 0x2a */

typedef struct {
    TBusTime    time;
} __attribute__ ((packed)) TBusDevReqSetTime;             /* type 0x2b */

typedef struct {
} __attribute__ ((packed)) TBusDevRespSetTime;            /* type 0x2c */

typedef struct {
    uint8_t index;
} __attribute__ ((packed)) TBusDevReqGetVar;              /* type 0x2d */

typedef enum {
    eBusVarSuccess = 0,
    eBusVarLengthError = 1,
    eBusVarIndexError = 2,
    eBusVarNvError = 3
} __attribute__ ((packed)) TBusVarResult;

typedef struct {
    TBusVarResult result;
    uint8_t       index;
    uint8_t       length;
    uint8_t       data[BUS_MAX_VAR_SIZE];
} __attribute__ ((packed)) TBusDevRespGetVar;             /* type 0x2e */

typedef struct {
    uint8_t  index;
    uint8_t  length;
    uint8_t  data[BUS_MAX_VAR_SIZE];
} __attribute__ ((packed)) TBusDevReqSetVar;              /* type 0x2f */

typedef struct {
    TBusVarResult result;
    uint8_t       index;
} __attribute__ ((packed)) TBusDevRespSetVar;             /* type 0x30 */

typedef union {
   TBusDevReqReboot           reboot;
   TBusDevReqUpdEnter         updEnter;
   TBusDevReqUpdData          updData;
   TBusDevReqUpdTerm          updTerm;
   TBusDevReqInfo             info;
   TBusDevReqSetState         setState;
   TBusDevReqGetState         getState;
   TBusDevReqSetValue         setValue;
   TBusDevReqActualValue      actualValue;
   TBusDevReqActualValueEvent actualValueEvent;
   TBusDevReqSwitchState      switchState;
   TBusDevReqSetClientAddr    setClientAddr;
   TBusDevReqGetClientAddr    getClientAddr;
   TBusDevReqSetAddr          setAddr;
   TBusDevReqEepromRead       readEeprom;
   TBusDevReqEepromWrite      writeEeprom;
   TBusDevReqClockCalib       clockCalib;
   TBusDevReqDoClockCalib     doClockCalib;
   TBusDevReqDiag             diag;
   TBusDevReqGetTime          getTime;
   TBusDevReqSetTime          setTime;
   TBusDevReqGetVar           getVar;
   TBusDevReqSetVar           setVar;
} __attribute__ ((packed)) TUniDevReq;

typedef union {
   TBusDevRespUpdEnter         updEnter;
   TBusDevRespUpdData          updData;
   TBusDevRespUpdTerm          updTerm;
   TBusDevRespInfo             info;
   TBusDevRespSetState         setState;
   TBusDevRespGetState         getState;
   TBusDevRespSetValue         setValue;
   TBusDevRespActualValue      actualValue;
   TBusDevRespActualValueEvent actualValueEvent;
   TBusDevRespSwitchState      switchState;
   TBusDevRespSetClientAddr    setClientAddr;
   TBusDevRespGetClientAddr    getClientAddr;
   TBusDevRespSetAddr          setAddr;
   TBusDevRespEepromRead       readEeprom;
   TBusDevRespEepromWrite      writeEeprom;
   TBusDevRespClockCalib       clockCalib;
   TBusDevRespDoClockCalib     doClockCalib;
   TBusDevRespDiag             diag;
   TBusDevRespGetTime          getTime;
   TBusDevRespSetTime          setTime;
   TBusDevRespGetVar           getVar;
   TBusDevRespSetVar           setVar;
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
   eBusDevReqActualValueEvent =          0x21,
   eBusDevRespActualValueEvent =         0x22,
   eBusDevReqClockCalib =                0x23,
   eBusDevRespClockCalib =               0x24,
   eBusDevReqDoClockCalib =              0x25,
   eBusDevRespDoClockCalib =             0x26,
   eBusDevReqDiag =                      0x27,
   eBusDevRespDiag =                     0x28,
   eBusDevReqGetTime =                   0x29,
   eBusDevRespGetTime =                  0x2a,
   eBusDevReqSetTime =                   0x2b,
   eBusDevRespSetTime =                  0x2c,
   eBusDevReqGetVar =                    0x2d,
   eBusDevRespGetVar =                   0x2e,
   eBusDevReqSetVar =                    0x2f,
   eBusDevRespSetVar =                   0x30,
   eBusDevStartup =                      0xff
} __attribute__ ((packed)) TBusMsgType;

typedef struct {
   uint8_t          senderAddr;
   TBusMsgType      type;
   TBusUniTelegram  msg;
} __attribute__ ((packed)) TBusTelegram;

/*-----------------------------------------------------------------------------
*  Functions
*/
void           BusInit(int sioHandle);
void           BusExit(int sioHandle);
uint8_t        BusCheck(void);
TBusTelegram   *BusMsgBufGet(void);
uint8_t        BusSend(TBusTelegram *pMsg);
uint8_t        BusSendToBuf(TBusTelegram *pMsg);
uint8_t        BusSendToBufRaw(uint8_t *pRawData, uint8_t len);
uint8_t        BusSendBuf(void);

/*
*  BusVar
*/
typedef enum {
    /* final states */
    eBusVarState_Error     = 0x01,
    eBusVarState_Timeout   = 0x02,
    eBusVarState_Ready     = 0x04,
    eBusVarState_TxError   = 0x08,
    eBusVarState_Invalid   = 0x10,
    /* transient states */
    eBusVarState_Scheduled = 0x20,
    eBusVarState_Waiting   = 0x40,
    eBusVarState_TxRetry   = 0x80,
} __attribute__ ((packed)) TBusVarState;

#define BUSVAR_STATE_FINAL (eBusVarState_Error   | \
                            eBusVarState_Timeout | \
                            eBusVarState_Ready   | \
                            eBusVarState_TxError | \
                            eBusVarState_Invalid)

typedef enum {
    eBusVarType_uint8    = 0,
    eBusVarType_uint16   = 1,
    eBusVarType_uint32   = 2,
    eBusVarType_uint64   = 3,
    eBusVarType_int8     = 4,
    eBusVarType_int16    = 5,
    eBusVarType_int32    = 6,
    eBusVarType_int64    = 7,
    eBusVarType_string   = 8,
    eBusVarType_invalid  = 9
} __attribute__ ((packed)) TBusVarType;

static inline int BusVarTypeToSize(TBusVarType type) {

    int size;

    switch(type) {
    case eBusVarType_uint8:
    case eBusVarType_int8:
        size = 1;
        break;
    case eBusVarType_uint16:
    case eBusVarType_int16:
        size = 2;
        break;
    case eBusVarType_uint32:
    case eBusVarType_int32:
        size = 4;
        break;
    case eBusVarType_uint64:
    case eBusVarType_int64:
        size = 8;
        break;
    case eBusVarType_string:
        size = 0;
        break;
    default:
        size = -1;
        break;
    }
    return size;
}

typedef enum {
    eBusVarMode_ro,
    eBusVarMode_rw,
    eBusVarMode_const,
    eBusVarMode_invalid
} __attribute__ ((packed)) TBusVarMode;

typedef void *TBusVarHdl;
#define BUSVAR_HDL_INVALID     (TBusVarHdl)-1


typedef enum {
    eBusVarRead,
    eBusVarWrite
} __attribute__ ((packed)) TBusVarDir;

typedef bool (* TBusBarNvFunc)(uint16_t address, void *buf, uint8_t bufSize, TBusVarDir dir);

void    BusVarInit(uint8_t myAddr, TBusBarNvFunc func);
bool    BusVarAdd(uint8_t idx, uint8_t size, bool persistent);
bool    BusVarSetInfo(uint8_t idx, const char *name, TBusVarType type, TBusVarMode mode);
uint8_t BusVarRead(uint8_t idx, void *buf, uint8_t bufSize, TBusVarResult *result);
bool    BusVarWrite(uint8_t idx, void *buf, uint8_t bufSize, TBusVarResult *result);

TBusVarHdl BusVarTransactionOpen(uint8_t busAddr, uint8_t varIdx, void *buf, uint8_t bufSize, TBusVarDir dir);
TBusVarState BusVarTransactionState(TBusVarHdl varHdl);
void BusVarTransactionClose(TBusVarHdl varHdl);
void BusVarRespGet(uint8_t addr, TBusDevRespGetVar *respGet);
void BusVarRespSet(uint8_t addr, TBusDevRespSetVar *respSet);
void BusVarProcess(void);

#ifdef __cplusplus
}
#endif

#endif
