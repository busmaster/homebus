/*
 * application.c
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
#include <string.h>
#include <avr/pgmspace.h>

#include "sysdef.h"
#include "board.h"
#include "button.h"
#include "application.h"
#include "application_l.h"
#include "pwm.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef void (* TFuncPressed0)(void);
typedef void (* TFuncPressed1)(void);
typedef void (* TFuncReleased0)(void);
typedef void (* TFuncReleased1)(void);

typedef struct {
   TFuncPressed0  fPressed0;
   TFuncPressed1  fPressed1;
   TFuncReleased0 fReleased0;
   TFuncReleased1 fReleased1;
} TUserFunc;

/*-----------------------------------------------------------------------------
*  Variables
*/
static const TUserFunc sApplicationFuncs[] PROGMEM = {
   {ApplicationPressed1_0,   ApplicationPressed1_1,   ApplicationReleased1_0,   ApplicationReleased1_1},
   {ApplicationPressed2_0,   ApplicationPressed2_1,   ApplicationReleased2_0,   ApplicationReleased2_1},
   {ApplicationPressed3_0,   ApplicationPressed3_1,   ApplicationReleased3_0,   ApplicationReleased3_1},
   {ApplicationPressed4_0,   ApplicationPressed4_1,   ApplicationReleased4_0,   ApplicationReleased4_1},
   {ApplicationPressed5_0,   ApplicationPressed5_1,   ApplicationReleased5_0,   ApplicationReleased5_1},
   {ApplicationPressed6_0,   ApplicationPressed6_1,   ApplicationReleased6_0,   ApplicationReleased6_1},
   {ApplicationPressed7_0,   ApplicationPressed7_1,   ApplicationReleased7_0,   ApplicationReleased7_1},
   {ApplicationPressed8_0,   ApplicationPressed8_1,   ApplicationReleased8_0,   ApplicationReleased8_1},
   {ApplicationPressed9_0,   ApplicationPressed9_1,   ApplicationReleased9_0,   ApplicationReleased9_1},
   {ApplicationPressed10_0,  ApplicationPressed10_1,  ApplicationReleased10_0,  ApplicationReleased10_1},
   {ApplicationPressed11_0,  ApplicationPressed11_1,  ApplicationReleased11_0,  ApplicationReleased11_1},
   {ApplicationPressed12_0,  ApplicationPressed12_1,  ApplicationReleased12_0,  ApplicationReleased12_1},
   {ApplicationPressed13_0,  ApplicationPressed13_1,  ApplicationReleased13_0,  ApplicationReleased13_1},
   {ApplicationPressed14_0,  ApplicationPressed14_1,  ApplicationReleased14_0,  ApplicationReleased14_1},
   {ApplicationPressed15_0,  ApplicationPressed15_1,  ApplicationReleased15_0,  ApplicationReleased15_1},
   {ApplicationPressed16_0,  ApplicationPressed16_1,  ApplicationReleased16_0,  ApplicationReleased16_1},
   {ApplicationPressed17_0,  ApplicationPressed17_1,  ApplicationReleased17_0,  ApplicationReleased17_1},
   {ApplicationPressed18_0,  ApplicationPressed18_1,  ApplicationReleased18_0,  ApplicationReleased18_1},
   {ApplicationPressed19_0,  ApplicationPressed19_1,  ApplicationReleased19_0,  ApplicationReleased19_1},
   {ApplicationPressed20_0,  ApplicationPressed20_1,  ApplicationReleased20_0,  ApplicationReleased20_1},
   {ApplicationPressed21_0,  ApplicationPressed21_1,  ApplicationReleased21_0,  ApplicationReleased21_1},
   {ApplicationPressed22_0,  ApplicationPressed22_1,  ApplicationReleased22_0,  ApplicationReleased22_1},
   {ApplicationPressed23_0,  ApplicationPressed23_1,  ApplicationReleased23_0,  ApplicationReleased23_1},
   {ApplicationPressed24_0,  ApplicationPressed24_1,  ApplicationReleased24_0,  ApplicationReleased24_1},
   {ApplicationPressed25_0,  ApplicationPressed25_1,  ApplicationReleased25_0,  ApplicationReleased25_1},
   {ApplicationPressed26_0,  ApplicationPressed26_1,  ApplicationReleased26_0,  ApplicationReleased26_1},
   {ApplicationPressed27_0,  ApplicationPressed27_1,  ApplicationReleased27_0,  ApplicationReleased27_1},
   {ApplicationPressed28_0,  ApplicationPressed28_1,  ApplicationReleased28_0,  ApplicationReleased28_1},
   {ApplicationPressed29_0,  ApplicationPressed29_1,  ApplicationReleased29_0,  ApplicationReleased29_1},
   {ApplicationPressed30_0,  ApplicationPressed30_1,  ApplicationReleased30_0,  ApplicationReleased30_1},
   {ApplicationPressed31_0,  ApplicationPressed31_1,  ApplicationReleased31_0,  ApplicationReleased31_1},
   {ApplicationPressed32_0,  ApplicationPressed32_1,  ApplicationReleased32_0,  ApplicationReleased32_1},
   {ApplicationPressed33_0,  ApplicationPressed33_1,  ApplicationReleased33_0,  ApplicationReleased33_1},
   {ApplicationPressed34_0,  ApplicationPressed34_1,  ApplicationReleased34_0,  ApplicationReleased34_1},
   {ApplicationPressed35_0,  ApplicationPressed35_1,  ApplicationReleased35_0,  ApplicationReleased35_1},
   {ApplicationPressed36_0,  ApplicationPressed36_1,  ApplicationReleased36_0,  ApplicationReleased36_1},
   {ApplicationPressed37_0,  ApplicationPressed37_1,  ApplicationReleased37_0,  ApplicationReleased37_1},
   {ApplicationPressed38_0,  ApplicationPressed38_1,  ApplicationReleased38_0,  ApplicationReleased38_1},
   {ApplicationPressed39_0,  ApplicationPressed39_1,  ApplicationReleased39_0,  ApplicationReleased39_1},
   {ApplicationPressed40_0,  ApplicationPressed40_1,  ApplicationReleased40_0,  ApplicationReleased40_1},
   {ApplicationPressed41_0,  ApplicationPressed41_1,  ApplicationReleased41_0,  ApplicationReleased41_1},
   {ApplicationPressed42_0,  ApplicationPressed42_1,  ApplicationReleased42_0,  ApplicationReleased42_1},
   {ApplicationPressed43_0,  ApplicationPressed43_1,  ApplicationReleased43_0,  ApplicationReleased43_1},
   {ApplicationPressed44_0,  ApplicationPressed44_1,  ApplicationReleased44_0,  ApplicationReleased44_1},
   {ApplicationPressed45_0,  ApplicationPressed45_1,  ApplicationReleased45_0,  ApplicationReleased45_1},
   {ApplicationPressed46_0,  ApplicationPressed46_1,  ApplicationReleased46_0,  ApplicationReleased46_1},
   {ApplicationPressed47_0,  ApplicationPressed47_1,  ApplicationReleased47_0,  ApplicationReleased47_1},
   {ApplicationPressed48_0,  ApplicationPressed48_1,  ApplicationReleased48_0,  ApplicationReleased48_1},
   {ApplicationPressed49_0,  ApplicationPressed49_1,  ApplicationReleased49_0,  ApplicationReleased49_1},
   {ApplicationPressed50_0,  ApplicationPressed50_1,  ApplicationReleased50_0,  ApplicationReleased50_1},
   {ApplicationPressed51_0,  ApplicationPressed51_1,  ApplicationReleased51_0,  ApplicationReleased51_1},
   {ApplicationPressed52_0,  ApplicationPressed52_1,  ApplicationReleased52_0,  ApplicationReleased52_1},
   {ApplicationPressed53_0,  ApplicationPressed53_1,  ApplicationReleased53_0,  ApplicationReleased53_1},
   {ApplicationPressed54_0,  ApplicationPressed54_1,  ApplicationReleased54_0,  ApplicationReleased54_1},
   {ApplicationPressed55_0,  ApplicationPressed55_1,  ApplicationReleased55_0,  ApplicationReleased55_1},
   {ApplicationPressed56_0,  ApplicationPressed56_1,  ApplicationReleased56_0,  ApplicationReleased56_1},
   {ApplicationPressed57_0,  ApplicationPressed57_1,  ApplicationReleased57_0,  ApplicationReleased57_1},
   {ApplicationPressed58_0,  ApplicationPressed58_1,  ApplicationReleased58_0,  ApplicationReleased58_1},
   {ApplicationPressed59_0,  ApplicationPressed59_1,  ApplicationReleased59_0,  ApplicationReleased59_1},
   {ApplicationPressed60_0,  ApplicationPressed60_1,  ApplicationReleased60_0,  ApplicationReleased60_1},
   {ApplicationPressed61_0,  ApplicationPressed61_1,  ApplicationReleased61_0,  ApplicationReleased61_1},
   {ApplicationPressed62_0,  ApplicationPressed62_1,  ApplicationReleased62_0,  ApplicationReleased62_1},
   {ApplicationPressed63_0,  ApplicationPressed63_1,  ApplicationReleased63_0,  ApplicationReleased63_1},
   {ApplicationPressed64_0,  ApplicationPressed64_1,  ApplicationReleased64_0,  ApplicationReleased64_1}
};

static TBusTelegram sTxBusMsg;
static bool         sTxRetry = false;

/*-----------------------------------------------------------------------------
*  Functions
*/


/*-----------------------------------------------------------------------------
* returns version string (max length is 15 chars)
*/
const char *ApplicationVersion(void) {
   return "pwm_0.01";
}

/*-----------------------------------------------------------------------------
* ButtonEvent for Application
*/
void ApplicationEventButton(TButtonEvent *pButtonEvent) {

   uint8_t        index;
   TFuncPressed0  fPressed0;
   TFuncPressed1  fPressed1;
   TFuncReleased0 fReleased0;
   TFuncReleased1 fReleased1;

   index = pButtonEvent->address - 1;

   if (index >= (sizeof(sApplicationFuncs) / sizeof(TUserFunc))) {
      return;
   }

   if (pButtonEvent->pressed) {
      if (pButtonEvent->buttonNr == 1) {
          fPressed0 = (TFuncPressed0)pgm_read_word(&sApplicationFuncs[index].fPressed0);
          fPressed0();
      } else {
          fPressed1 = (TFuncPressed1)pgm_read_word(&sApplicationFuncs[index].fPressed1);
          fPressed1();
      }
   } else {
      if (pButtonEvent->buttonNr == 1) {
          fReleased0 = (TFuncReleased0)pgm_read_word(&sApplicationFuncs[index].fReleased0);
          fReleased0();
      } else {
          fReleased1 = (TFuncReleased1)pgm_read_word(&sApplicationFuncs[index].fReleased1);
          fReleased1();
      }
   }
}

void ApplicationInit(void) {

}

void ApplicationStart(void) {

}

void ApplicationCheck(void) {

    if (sTxRetry) {
        sTxRetry = BusSend(&sTxBusMsg) != BUS_SEND_OK;
    }
}


static void ApplicationPressed1_0(void) {}
static void ApplicationReleased1_0(void) {}
static void ApplicationPressed1_1(void) {}
static void ApplicationReleased1_1(void) {}

static void ApplicationPressed2_0(void) {}
static void ApplicationReleased2_0(void) {}
static void ApplicationPressed2_1(void) {}
static void ApplicationReleased2_1(void) {}

static void ApplicationPressed3_0(void) {}
static void ApplicationReleased3_0(void) {}
static void ApplicationPressed3_1(void) {}
static void ApplicationReleased3_1(void) {}

static void ApplicationPressed4_0(void) {}
static void ApplicationReleased4_0(void) {}
static void ApplicationPressed4_1(void) {}
static void ApplicationReleased4_1(void) {}

static void ApplicationPressed5_0(void) {}
static void ApplicationReleased5_0(void) {}
static void ApplicationPressed5_1(void) {}
static void ApplicationReleased5_1(void) {}

static void ApplicationPressed6_0(void) {}
static void ApplicationReleased6_0(void) {}
static void ApplicationPressed6_1(void) {}
static void ApplicationReleased6_1(void) {}

static void ApplicationPressed7_0(void) {}
static void ApplicationReleased7_0(void) {}
static void ApplicationPressed7_1(void) {}
static void ApplicationReleased7_1(void) {}

static void ApplicationPressed8_0(void) {}
static void ApplicationReleased8_0(void) {}
static void ApplicationPressed8_1(void) {}
static void ApplicationReleased8_1(void) {}

static void ApplicationPressed9_0(void) {}
static void ApplicationReleased9_0(void) {}
static void ApplicationPressed9_1(void) {}
static void ApplicationReleased9_1(void) {}

static void ApplicationPressed10_0(void) {}
static void ApplicationReleased10_0(void) {}
static void ApplicationPressed10_1(void) {}
static void ApplicationReleased10_1(void) {}

static void ApplicationPressed11_0(void) {}
static void ApplicationReleased11_0(void) {}
static void ApplicationPressed11_1(void) {}
static void ApplicationReleased11_1(void) {}

static void ApplicationPressed12_0(void) {}
static void ApplicationReleased12_0(void) {}
static void ApplicationPressed12_1(void) {}
static void ApplicationReleased12_1(void) {}

static void ApplicationPressed13_0(void) {}
static void ApplicationReleased13_0(void) {}
static void ApplicationPressed13_1(void) {}
static void ApplicationReleased13_1(void) {}

static void ApplicationPressed14_0(void) {}
static void ApplicationReleased14_0(void) {}
static void ApplicationPressed14_1(void) {}
static void ApplicationReleased14_1(void) {}

static void ApplicationPressed15_0(void) {}
static void ApplicationReleased15_0(void) {}
static void ApplicationPressed15_1(void) {}
static void ApplicationReleased15_1(void) {}

static void ApplicationPressed16_0(void) {}
static void ApplicationReleased16_0(void) {}
static void ApplicationPressed16_1(void) {}
static void ApplicationReleased16_1(void) {}

static void ApplicationPressed17_0(void) {}
static void ApplicationReleased17_0(void) {}
static void ApplicationPressed17_1(void) {}
static void ApplicationReleased17_1(void) {}

static void ApplicationPressed18_0(void) {}
static void ApplicationReleased18_0(void) {}
static void ApplicationPressed18_1(void) {}
static void ApplicationReleased18_1(void) {}

static void ApplicationPressed19_0(void) {}
static void ApplicationReleased19_0(void) {}
static void ApplicationPressed19_1(void) {}
static void ApplicationReleased19_1(void) {}

static void ApplicationPressed20_0(void) {}
static void ApplicationReleased20_0(void) {}
static void ApplicationPressed20_1(void) {}
static void ApplicationReleased20_1(void) {}

static void ApplicationPressed21_0(void) {}
static void ApplicationReleased21_0(void) {}
static void ApplicationPressed21_1(void) {}
static void ApplicationReleased21_1(void) {}

static void ApplicationPressed22_0(void) {}
static void ApplicationReleased22_0(void) {}
static void ApplicationPressed22_1(void) {}
static void ApplicationReleased22_1(void) {}

static void ApplicationPressed23_0(void) {}
static void ApplicationReleased23_0(void) {}
static void ApplicationPressed23_1(void) {}
static void ApplicationReleased23_1(void) {}

static void ApplicationPressed24_0(void) {}
static void ApplicationReleased24_0(void) {}
static void ApplicationPressed24_1(void) {}
static void ApplicationReleased24_1(void) {}

static void ApplicationPressed25_0(void) {}
static void ApplicationReleased25_0(void) {}
static void ApplicationPressed25_1(void) {}
static void ApplicationReleased25_1(void) {}

static void ApplicationPressed26_0(void) {}
static void ApplicationReleased26_0(void) {}
static void ApplicationPressed26_1(void) {}
static void ApplicationReleased26_1(void) {}

static void ApplicationPressed27_0(void) {}
static void ApplicationReleased27_0(void) {}
static void ApplicationPressed27_1(void) {}
static void ApplicationReleased27_1(void) {}

static void ApplicationPressed28_0(void) {}
static void ApplicationReleased28_0(void) {}
static void ApplicationPressed28_1(void) {}
static void ApplicationReleased28_1(void) {}

static void ApplicationPressed29_0(void) {}
static void ApplicationReleased29_0(void) {}
static void ApplicationPressed29_1(void) {}
static void ApplicationReleased29_1(void) {}

static void ApplicationPressed30_0(void) {}
static void ApplicationReleased30_0(void) {}
static void ApplicationPressed30_1(void) {}
static void ApplicationReleased30_1(void) {}

static void ApplicationPressed31_0(void) {}
static void ApplicationReleased31_0(void) {}
static void ApplicationPressed31_1(void) {}
static void ApplicationReleased31_1(void) {}

static void ApplicationPressed32_0(void) {}
static void ApplicationReleased32_0(void) {}
static void ApplicationPressed32_1(void) {}
static void ApplicationReleased32_1(void) {}

static void ApplicationPressed33_0(void) {}
static void ApplicationReleased33_0(void) {}
static void ApplicationPressed33_1(void) {}
static void ApplicationReleased33_1(void) {}

static void ApplicationPressed34_0(void) {}
static void ApplicationReleased34_0(void) {}
static void ApplicationPressed34_1(void) {}
static void ApplicationReleased34_1(void) {}

static void ApplicationPressed35_0(void) {}
static void ApplicationReleased35_0(void) {}
static void ApplicationPressed35_1(void) {}
static void ApplicationReleased35_1(void) {}

static void ApplicationPressed36_0(void) {}
static void ApplicationReleased36_0(void) {}
static void ApplicationPressed36_1(void) {}
static void ApplicationReleased36_1(void) {}

static void ApplicationPressed37_0(void) {}
static void ApplicationReleased37_0(void) {}
static void ApplicationPressed37_1(void) {}
static void ApplicationReleased37_1(void) {}

static void ApplicationPressed38_0(void) {}
static void ApplicationReleased38_0(void) {}
static void ApplicationPressed38_1(void) {}
static void ApplicationReleased38_1(void) {}

static void ApplicationPressed39_0(void) {}
static void ApplicationReleased39_0(void) {}
static void ApplicationPressed39_1(void) {}
static void ApplicationReleased39_1(void) {}

static void ApplicationPressed40_0(void) {}
static void ApplicationReleased40_0(void) {}
static void ApplicationPressed40_1(void) {}
static void ApplicationReleased40_1(void) {}

static void ApplicationPressed41_0(void) {}
static void ApplicationReleased41_0(void) {}
static void ApplicationPressed41_1(void) {}
static void ApplicationReleased41_1(void) {}

static void ApplicationPressed42_0(void) {}
static void ApplicationReleased42_0(void) {}
static void ApplicationPressed42_1(void) {}
static void ApplicationReleased42_1(void) {}

static void ApplicationPressed43_0(void) {}
static void ApplicationReleased43_0(void) {}
static void ApplicationPressed43_1(void) {}
static void ApplicationReleased43_1(void) {}

static void ApplicationPressed44_0(void) {}
static void ApplicationReleased44_0(void) {}
static void ApplicationPressed44_1(void) {}
static void ApplicationReleased44_1(void) {}

static void ApplicationPressed45_0(void) {}
static void ApplicationReleased45_0(void) {}
static void ApplicationPressed45_1(void) {}
static void ApplicationReleased45_1(void) {}


static void SetSupply(void) {
    uint16_t val;
    uint8_t  i;
    bool     do_On;
    
    /* digout29/do31 240 on/off */
    sTxBusMsg.type = eBusDevReqSetValue;
    sTxBusMsg.senderAddr = 239;
    sTxBusMsg.msg.devBus.receiverAddr = 240;
    sTxBusMsg.msg.devBus.x.devReq.setValue.devType = eBusDevTypeDo31;

    memset(sTxBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.digOut, 0, 
           sizeof(sTxBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.digOut)); // default: no change
    memset(sTxBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.shader, 254, 
           sizeof(sTxBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.shader)); // default: no change

    do_On = false;
    for (i = 0; i < 4; i++) {
        PwmGet(i, &val);
        if (val != 0) {
            do_On = true;
            break;
        }
    }
    if (do_On) {
        /* on */
        sTxBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.digOut[29 / 4] = 0x3 << ((29 % 4) * 2);
    } else {
        /* off */
        sTxBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.digOut[29 / 4] = 0x2 << ((29 % 4) * 2);
    }
    sTxRetry = BusSend(&sTxBusMsg) != BUS_SEND_OK;
}

void ApplicationPressed46_0(void) {
    uint16_t val;

    /* Kaffeemschine */ 
    if (PwmGet(2, &val)) {
        if (val == 0) {
            val = 65535;
        } else {
            val = 0;
        }
        PwmSet(2, val);
    }
    SetSupply();
}
void ApplicationReleased46_0(void) {}

static void ApplicationPressed46_1(void) {
    uint16_t val;
    
    /* Geschirrspüler */
    if (PwmGet(0, &val)) {
        if (val == 0) {
            val = 65535;
        } else {
            val = 0;
        }
        PwmSet(0, val);
    }
    SetSupply();
}
static void ApplicationReleased46_1(void) {}
static void ApplicationPressed47_0(void) {
    uint16_t val;

    /* Dunstabzug */ 
    if (PwmGet(3, &val)) {
        if (val == 0) {
            val = 65535;
        } else {
            val = 0;
        }
        PwmSet(3, val);
    }
    SetSupply();
}
static void ApplicationReleased47_0(void) {}
static void ApplicationPressed47_1(void) {
    uint16_t val;

    /* Abwasch */ 
    if (PwmGet(1, &val)) {
        if (val == 0) {
            val = 65535;
        } else {
            val = 0;
        }
        PwmSet(1, val);
    }

    SetSupply();
}
static void ApplicationReleased47_1(void) {}

static void ApplicationPressed48_0(void) {}
static void ApplicationReleased48_0(void) {}
static void ApplicationPressed48_1(void) {}
static void ApplicationReleased48_1(void) {}

static void ApplicationPressed49_0(void) {}
static void ApplicationReleased49_0(void) {}
static void ApplicationPressed49_1(void) {}
static void ApplicationReleased49_1(void) {}

static void ApplicationPressed50_0(void) {}
static void ApplicationReleased50_0(void) {}
static void ApplicationPressed50_1(void) {}
static void ApplicationReleased50_1(void) {}

static void ApplicationPressed51_0(void) {}
static void ApplicationReleased51_0(void) {}
static void ApplicationPressed51_1(void) {}
static void ApplicationReleased51_1(void) {}

static void ApplicationPressed52_0(void) {}
static void ApplicationReleased52_0(void) {}
static void ApplicationPressed52_1(void) {}
static void ApplicationReleased52_1(void) {}

static void ApplicationPressed53_0(void) {}
static void ApplicationReleased53_0(void) {}
static void ApplicationPressed53_1(void) {}
static void ApplicationReleased53_1(void) {}

static void ApplicationPressed54_0(void) {}
static void ApplicationReleased54_0(void) {}
static void ApplicationPressed54_1(void) {}
static void ApplicationReleased54_1(void) {}

static void ApplicationPressed55_0(void) {}
static void ApplicationReleased55_0(void) {}
static void ApplicationPressed55_1(void) {}
static void ApplicationReleased55_1(void) {}

static void ApplicationPressed56_0(void) {}
static void ApplicationReleased56_0(void) {}
static void ApplicationPressed56_1(void) {}
static void ApplicationReleased56_1(void) {}

static void ApplicationPressed57_0(void) {}
static void ApplicationReleased57_0(void) {}
static void ApplicationPressed57_1(void) {}
static void ApplicationReleased57_1(void) {}

static void ApplicationPressed58_0(void) {}
static void ApplicationReleased58_0(void) {}
static void ApplicationPressed58_1(void) {}
static void ApplicationReleased58_1(void) {}

static void ApplicationPressed59_0(void) {}
static void ApplicationReleased59_0(void) {}
static void ApplicationPressed59_1(void) {}
static void ApplicationReleased59_1(void) {}

static void ApplicationPressed60_0(void) {}
static void ApplicationReleased60_0(void) {}
static void ApplicationPressed60_1(void) {}
static void ApplicationReleased60_1(void) {}

static void ApplicationPressed61_0(void) {}
static void ApplicationReleased61_0(void) {}
static void ApplicationPressed61_1(void) {}
static void ApplicationReleased61_1(void) {}

static void ApplicationPressed62_0(void) {}
static void ApplicationReleased62_0(void) {}
static void ApplicationPressed62_1(void) {}
static void ApplicationReleased62_1(void) {}

static void ApplicationPressed63_0(void) {}
static void ApplicationReleased63_0(void) {}
static void ApplicationPressed63_1(void) {}
static void ApplicationReleased63_1(void) {}

static void ApplicationPressed64_0(void) {}
static void ApplicationReleased64_0(void) {}
static void ApplicationPressed64_1(void) {}
static void ApplicationReleased64_1(void) {}
