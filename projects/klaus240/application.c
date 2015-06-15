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
#include <avr/pgmspace.h>

#include "sysdef.h"
#include "board.h"
#include "button.h"
#include "application.h"
#include "digout.h"
#include "shader.h"

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
   {ApplicationPressed64_0,  ApplicationPressed64_1,  ApplicationReleased64_0,  ApplicationReleased64_1},
   {ApplicationPressed65_0,  ApplicationPressed65_1,  ApplicationReleased65_0,  ApplicationReleased65_1},
   {ApplicationPressed66_0,  ApplicationPressed66_1,  ApplicationReleased66_0,  ApplicationReleased66_1},
   {ApplicationPressed67_0,  ApplicationPressed67_1,  ApplicationReleased67_0,  ApplicationReleased67_1},
   {ApplicationPressed68_0,  ApplicationPressed68_1,  ApplicationReleased68_0,  ApplicationReleased68_1},
   {ApplicationPressed69_0,  ApplicationPressed69_1,  ApplicationReleased69_0,  ApplicationReleased69_1},
   {ApplicationPressed70_0,  ApplicationPressed70_1,  ApplicationReleased70_0,  ApplicationReleased70_1},
   {ApplicationPressed71_0,  ApplicationPressed71_1,  ApplicationReleased71_0,  ApplicationReleased71_1},
   {ApplicationPressed72_0,  ApplicationPressed72_1,  ApplicationReleased72_0,  ApplicationReleased72_1},
   {ApplicationPressed73_0,  ApplicationPressed73_1,  ApplicationReleased73_0,  ApplicationReleased73_1},
   {ApplicationPressed74_0,  ApplicationPressed74_1,  ApplicationReleased74_0,  ApplicationReleased74_1},
   {ApplicationPressed75_0,  ApplicationPressed75_1,  ApplicationReleased75_0,  ApplicationReleased75_1},
   {ApplicationPressed76_0,  ApplicationPressed76_1,  ApplicationReleased76_0,  ApplicationReleased76_1},
   {ApplicationPressed77_0,  ApplicationPressed77_1,  ApplicationReleased77_0,  ApplicationReleased77_1},
   {ApplicationPressed78_0,  ApplicationPressed78_1,  ApplicationReleased78_0,  ApplicationReleased78_1},
   {ApplicationPressed79_0,  ApplicationPressed79_1,  ApplicationReleased79_0,  ApplicationReleased79_1},
   {ApplicationPressed80_0,  ApplicationPressed80_1,  ApplicationReleased80_0,  ApplicationReleased80_1},
   {ApplicationPressed81_0,  ApplicationPressed81_1,  ApplicationReleased81_0,  ApplicationReleased81_1},
   {ApplicationPressed82_0,  ApplicationPressed82_1,  ApplicationReleased82_0,  ApplicationReleased82_1},
   {ApplicationPressed83_0,  ApplicationPressed83_1,  ApplicationReleased83_0,  ApplicationReleased83_1},
   {ApplicationPressed84_0,  ApplicationPressed84_1,  ApplicationReleased84_0,  ApplicationReleased84_1},
   {ApplicationPressed85_0,  ApplicationPressed85_1,  ApplicationReleased85_0,  ApplicationReleased85_1},
   {ApplicationPressed86_0,  ApplicationPressed86_1,  ApplicationReleased86_0,  ApplicationReleased86_1},
   {ApplicationPressed87_0,  ApplicationPressed87_1,  ApplicationReleased87_0,  ApplicationReleased87_1},
   {ApplicationPressed88_0,  ApplicationPressed88_1,  ApplicationReleased88_0,  ApplicationReleased88_1},
   {ApplicationPressed89_0,  ApplicationPressed89_1,  ApplicationReleased89_0,  ApplicationReleased89_1},
   {ApplicationPressed90_0,  ApplicationPressed90_1,  ApplicationReleased90_0,  ApplicationReleased90_1},
   {ApplicationPressed91_0,  ApplicationPressed91_1,  ApplicationReleased91_0,  ApplicationReleased91_1},
   {ApplicationPressed92_0,  ApplicationPressed92_1,  ApplicationReleased92_0,  ApplicationReleased92_1},
   {ApplicationPressed93_0,  ApplicationPressed93_1,  ApplicationReleased93_0,  ApplicationReleased93_1},
   {ApplicationPressed94_0,  ApplicationPressed94_1,  ApplicationReleased94_0,  ApplicationReleased94_1},
   {ApplicationPressed95_0,  ApplicationPressed95_1,  ApplicationReleased95_0,  ApplicationReleased95_1},
   {ApplicationPressed96_0,  ApplicationPressed96_1,  ApplicationReleased96_0,  ApplicationReleased96_1},
   {ApplicationPressed97_0,  ApplicationPressed97_1,  ApplicationReleased97_0,  ApplicationReleased97_1},
   {ApplicationPressed98_0,  ApplicationPressed98_1,  ApplicationReleased98_0,  ApplicationReleased98_1},
   {ApplicationPressed99_0,  ApplicationPressed99_1,  ApplicationReleased99_0,  ApplicationReleased99_1},
   {ApplicationPressed100_0, ApplicationPressed100_1, ApplicationReleased100_0, ApplicationReleased100_1},
   {ApplicationPressed101_0, ApplicationPressed101_1, ApplicationReleased101_0, ApplicationReleased101_1},
   {ApplicationPressed102_0, ApplicationPressed102_1, ApplicationReleased102_0, ApplicationReleased102_1},
   {ApplicationPressed103_0, ApplicationPressed103_1, ApplicationReleased103_0, ApplicationReleased103_1},
   {ApplicationPressed104_0, ApplicationPressed104_1, ApplicationReleased104_0, ApplicationReleased104_1},
   {ApplicationPressed105_0, ApplicationPressed105_1, ApplicationReleased105_0, ApplicationReleased105_1},
   {ApplicationPressed106_0, ApplicationPressed106_1, ApplicationReleased106_0, ApplicationReleased106_1},
   {ApplicationPressed107_0, ApplicationPressed107_1, ApplicationReleased107_0, ApplicationReleased107_1},
   {ApplicationPressed108_0, ApplicationPressed108_1, ApplicationReleased108_0, ApplicationReleased108_1},
   {ApplicationPressed109_0, ApplicationPressed109_1, ApplicationReleased109_0, ApplicationReleased109_1},
   {ApplicationPressed110_0, ApplicationPressed110_1, ApplicationReleased110_0, ApplicationReleased110_1},
   {ApplicationPressed111_0, ApplicationPressed111_1, ApplicationReleased111_0, ApplicationReleased111_1},
   {ApplicationPressed112_0, ApplicationPressed112_1, ApplicationReleased112_0, ApplicationReleased112_1},
   {ApplicationPressed113_0, ApplicationPressed113_1, ApplicationReleased113_0, ApplicationReleased113_1},
   {ApplicationPressed114_0, ApplicationPressed114_1, ApplicationReleased114_0, ApplicationReleased114_1},
   {ApplicationPressed115_0, ApplicationPressed115_1, ApplicationReleased115_0, ApplicationReleased115_1},
   {ApplicationPressed116_0, ApplicationPressed116_1, ApplicationReleased116_0, ApplicationReleased116_1},
   {ApplicationPressed117_0, ApplicationPressed117_1, ApplicationReleased117_0, ApplicationReleased117_1},
   {ApplicationPressed118_0, ApplicationPressed118_1, ApplicationReleased118_0, ApplicationReleased118_1},
   {ApplicationPressed119_0, ApplicationPressed119_1, ApplicationReleased119_0, ApplicationReleased119_1},
   {ApplicationPressed120_0, ApplicationPressed120_1, ApplicationReleased120_0, ApplicationReleased120_1},
   {ApplicationPressed121_0, ApplicationPressed121_1, ApplicationReleased121_0, ApplicationReleased121_1},
   {ApplicationPressed122_0, ApplicationPressed122_1, ApplicationReleased122_0, ApplicationReleased122_1},
   {ApplicationPressed123_0, ApplicationPressed123_1, ApplicationReleased123_0, ApplicationReleased123_1},
   {ApplicationPressed124_0, ApplicationPressed124_1, ApplicationReleased124_0, ApplicationReleased124_1},
   {ApplicationPressed125_0, ApplicationPressed125_1, ApplicationReleased125_0, ApplicationReleased125_1},
   {ApplicationPressed126_0, ApplicationPressed126_1, ApplicationReleased126_0, ApplicationReleased126_1},
   {ApplicationPressed127_0, ApplicationPressed127_1, ApplicationReleased127_0, ApplicationReleased127_1},
   {ApplicationPressed128_0, ApplicationPressed128_1, ApplicationReleased128_0, ApplicationReleased128_1}
};

/*-----------------------------------------------------------------------------
*  Functions
*/


/*-----------------------------------------------------------------------------
* returns version string (max length is 15 chars)
*/
const char *ApplicationVersion(void) {
   return "Klaus2_0.04";
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

}


/*

eDigOut0   Netzteil Stiege
eDigOut1   Stiege Leuchte 1 (1. Lampe von unten)
eDigOut2   Stiege Leuchte 2
eDigOut3   Stiege Leuchte 3
eDigOut4   Stiege Leuchte 4
eDigOut5   Stiege Leuchte 5
eDigOut6   Stiege Leuchte 6 (letze Lampe, ganu oben)
eDigOut7   Schrankraum
eDigOut8   Schlafzimmer
eDigOut9   Garage
eDigOut10  Bad Spiegel
eDigOut11  Vorzimmer OG
eDigOut12  Bad Haengelampe
eDigOut13  Wohn
eDigOut14  Gang EG
eDigOut15  Ess
eDigOut16  Vorraum EG
eDigOut17  WC EG
eDigOut18  Glocke
eDigOut19  Kueche Haengelampe
eDigOut20  Terrasse
eDigOut21  Lagerraum
eDigOut22  Stiege UG
eDigOut23  Arbeit UG
eDigOut24  Fitness
eDigOut25  Vorraum UG
eDigOut26  Technik

*/

void ApplicationPressed1_0(void) {}
void ApplicationReleased1_0(void) {}
void ApplicationPressed1_1(void) {}
void ApplicationReleased1_1(void) {}

void ApplicationPressed2_0(void) {}
void ApplicationReleased2_0(void) {}
void ApplicationPressed2_1(void) {}
void ApplicationReleased2_1(void) {}

void ApplicationPressed3_0(void) {}
void ApplicationReleased3_0(void) {}
void ApplicationPressed3_1(void) {}
void ApplicationReleased3_1(void) {}

void ApplicationPressed4_0(void) {}
void ApplicationReleased4_0(void) {}
void ApplicationPressed4_1(void) {}
void ApplicationReleased4_1(void) {}

void ApplicationPressed5_0(void) {}
void ApplicationReleased5_0(void) {}
void ApplicationPressed5_1(void) {}
void ApplicationReleased5_1(void) {}

void ApplicationPressed6_0(void) {}
void ApplicationReleased6_0(void) {}
void ApplicationPressed6_1(void) {}
void ApplicationReleased6_1(void) {}

void ApplicationPressed7_0(void) {}
void ApplicationReleased7_0(void) {}
void ApplicationPressed7_1(void) {}
void ApplicationReleased7_1(void) {}

void ApplicationPressed8_0(void) {}
void ApplicationReleased8_0(void) {}
void ApplicationPressed8_1(void) {}
void ApplicationReleased8_1(void) {}

void ApplicationPressed9_0(void) {}
void ApplicationReleased9_0(void) {}
void ApplicationPressed9_1(void) {}
void ApplicationReleased9_1(void) {}

void ApplicationPressed10_0(void) {}
void ApplicationReleased10_0(void) {}
void ApplicationPressed10_1(void) {}
void ApplicationReleased10_1(void) {}

void ApplicationPressed11_0(void) {}
void ApplicationReleased11_0(void) {}
void ApplicationPressed11_1(void) {}
void ApplicationReleased11_1(void) {}

void ApplicationPressed12_0(void) {}
void ApplicationReleased12_0(void) {}
void ApplicationPressed12_1(void) {}
void ApplicationReleased12_1(void) {}

void ApplicationPressed13_0(void) {}
void ApplicationReleased13_0(void) {}
void ApplicationPressed13_1(void) {}
void ApplicationReleased13_1(void) {}

void ApplicationPressed14_0(void) {}
void ApplicationReleased14_0(void) {}
void ApplicationPressed14_1(void) {}
void ApplicationReleased14_1(void) {}

void ApplicationPressed15_0(void) {}
void ApplicationReleased15_0(void) {}
void ApplicationPressed15_1(void) {}
void ApplicationReleased15_1(void) {}

void ApplicationPressed16_0(void) {}
void ApplicationReleased16_0(void) {}
void ApplicationPressed16_1(void) {}
void ApplicationReleased16_1(void) {}

void ApplicationPressed17_0(void) {}
void ApplicationReleased17_0(void) {}
void ApplicationPressed17_1(void) {}
void ApplicationReleased17_1(void) {}


#define DELAY 500
void ApplicationPressed18_0(void) {
   /* Stiege, Taster EG */

   uint32_t delay = 0;
   bool delayed1 = DigOutIsDelayed(eDigOut1);
   bool delayed2 = DigOutIsDelayed(eDigOut2);
   bool delayed3 = DigOutIsDelayed(eDigOut3);
   bool delayed4 = DigOutIsDelayed(eDigOut4);
   bool delayed5 = DigOutIsDelayed(eDigOut5);
   bool delayed6 = DigOutIsDelayed(eDigOut6);

   if (delayed1 ||
       delayed2 ||
       delayed3 ||
       delayed4 ||
       delayed5 ||
       delayed6) {
      DigOutDelayCancel(eDigOut0);
      DigOutDelayCancel(eDigOut1);
      DigOutDelayCancel(eDigOut2);
      DigOutDelayCancel(eDigOut3);
      DigOutDelayCancel(eDigOut4);
      DigOutDelayCancel(eDigOut5);
      DigOutDelayCancel(eDigOut6);
   } else if (DigOutState(eDigOut0)) {
      if (DigOutState(eDigOut6)) {
         DigOutDelayedOff(eDigOut6, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut5)) {
         DigOutDelayedOff(eDigOut5, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut4)) {
         DigOutDelayedOff(eDigOut4, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut3)) {
         DigOutDelayedOff(eDigOut3, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut2)) {
         DigOutDelayedOff(eDigOut2, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut1)) {
         DigOutDelayedOff(eDigOut1, delay);
      }
      DigOutDelayedOff(eDigOut0, delay);
   } else {
      DigOutOn(eDigOut0);
      DigOutOn(eDigOut1);
      delay += DELAY;
      DigOutDelayedOn(eDigOut2, delay);
      delay += DELAY;
      DigOutDelayedOn(eDigOut3, delay);
      delay += DELAY;
      DigOutDelayedOn(eDigOut4, delay);
      delay += DELAY;
      DigOutDelayedOn(eDigOut5, delay);
      delay += DELAY;
      DigOutDelayedOn(eDigOut6, delay);
   }
}
void ApplicationReleased18_0(void) {}
void ApplicationPressed18_1(void) {}
void ApplicationReleased18_1(void) {}

static void StiegeOben(void) {
   uint32_t delay = 0;
   bool delayed1 = DigOutIsDelayed(eDigOut1);
   bool delayed2 = DigOutIsDelayed(eDigOut2);
   bool delayed3 = DigOutIsDelayed(eDigOut3);
   bool delayed4 = DigOutIsDelayed(eDigOut4);
   bool delayed5 = DigOutIsDelayed(eDigOut5);
   bool delayed6 = DigOutIsDelayed(eDigOut6);

   if (delayed1 ||
       delayed2 ||
       delayed3 ||
       delayed4 ||
       delayed5 ||
       delayed6) {
      DigOutDelayCancel(eDigOut0);
      DigOutDelayCancel(eDigOut1);
      DigOutDelayCancel(eDigOut2);
      DigOutDelayCancel(eDigOut3);
      DigOutDelayCancel(eDigOut4);
      DigOutDelayCancel(eDigOut5);
      DigOutDelayCancel(eDigOut6);
   } else if (DigOutState(eDigOut0)) {
      if (DigOutState(eDigOut1)) {
         DigOutDelayedOff(eDigOut1, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut2)) {
         DigOutDelayedOff(eDigOut2, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut3)) {
         DigOutDelayedOff(eDigOut3, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut4)) {
         DigOutDelayedOff(eDigOut4, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut5)) {
         DigOutDelayedOff(eDigOut5, delay);
         delay += DELAY;
      }
      if (DigOutState(eDigOut6)) {
         DigOutDelayedOff(eDigOut6, delay);
      }
      DigOutDelayedOff(eDigOut0, delay);
   } else {
      DigOutOn(eDigOut0);
      DigOutOn(eDigOut6);
      delay += DELAY;
      DigOutDelayedOn(eDigOut5, delay);
      delay += DELAY;
      DigOutDelayedOn(eDigOut4, delay);
      delay += DELAY;
      DigOutDelayedOn(eDigOut3, delay);
      delay += DELAY;
      DigOutDelayedOn(eDigOut2, delay);
      delay += DELAY;
      DigOutDelayedOn(eDigOut1, delay);
   }
}

void ApplicationPressed19_0(void) {
   /* Stiege, Taster Schlafzimmer */
   StiegeOben();
}
void ApplicationReleased19_0(void) {}
void ApplicationPressed19_1(void) {
   DigOutToggle(eDigOut11);
}
void ApplicationReleased19_1(void) {}

void ApplicationPressed20_0(void) {
   /* Stiege, Taster Kinderzimmer */
   StiegeOben();
}
void ApplicationReleased20_0(void) {}
void ApplicationPressed20_1(void) {
   /* Vorraum OG */
   DigOutToggle(eDigOut11);
}
void ApplicationReleased20_1(void) {}

void ApplicationPressed21_0(void) {
   /* Schlafzimmer */
   DigOutToggle(eDigOut8);
}
void ApplicationReleased21_0(void) {}
void ApplicationPressed21_1(void) {}
void ApplicationReleased21_1(void) {}

void ApplicationPressed22_0(void) {
   /* Schrankraum */
   if (DigOutState(eDigOut7)) {
      DigOutOff(eDigOut7);
   } else {
      DigOutDelayedOff(eDigOut7, 600000 /* 10 min */);
   }

}
void ApplicationReleased22_0(void) {}
void ApplicationPressed22_1(void) {}
void ApplicationReleased22_1(void) {}

void ApplicationPressed23_0(void) {
   /* Bad Spiegel */
   DigOutToggle(eDigOut10);
}
void ApplicationReleased23_0(void) {}
void ApplicationPressed23_1(void) {
   /* Bad Haengelampe */
   DigOutToggle(eDigOut12);
}
void ApplicationReleased23_1(void) {}

void ApplicationPressed24_0(void) {}
void ApplicationReleased24_0(void) {}
void ApplicationPressed24_1(void) {
   /* Kueche Haengelampe */
   DigOutToggle(eDigOut19);
}
void ApplicationReleased24_1(void) {}

void ApplicationPressed25_0(void) {
   /* Ess */
   DigOutToggle(eDigOut15);
}
void ApplicationReleased25_0(void) {}
void ApplicationPressed25_1(void) {
   /* Wohn */
   DigOutToggle(eDigOut13);
}
void ApplicationReleased25_1(void) {}

void ApplicationPressed26_0(void) {
   /* Gang EG */
   DigOutToggle(eDigOut14);
}
void ApplicationReleased26_0(void) {}
void ApplicationPressed26_1(void) {
   /* Ess */
   DigOutToggle(eDigOut15);
}
void ApplicationReleased26_1(void) {}

void ApplicationPressed27_0(void) {
   /* Vorraum EG */
   DigOutToggle(eDigOut16);
}
void ApplicationReleased27_0(void) {}
void ApplicationPressed27_1(void) {
   /* Gang EG */
   DigOutToggle(eDigOut14);
}
void ApplicationReleased27_1(void) {}

void ApplicationPressed28_0(void) {}
void ApplicationReleased28_0(void) {}
void ApplicationPressed28_1(void) {
   /* Vorraum EG */
   DigOutToggle(eDigOut16);
}
void ApplicationReleased28_1(void) {}

void ApplicationPressed29_0(void) {
   /* WC EG */
   DigOutOn(eDigOut17);
}
void ApplicationReleased29_0(void) {
   /* WC EG */
   DigOutOff(eDigOut17);
}
void ApplicationPressed29_1(void) {}
void ApplicationReleased29_1(void) {}

void ApplicationPressed30_0(void) {
   /* Glocke */
   if (!DigOutIsDelayed(eDigOut18)) {
      DigOutDelayedOff(eDigOut18, 250);
   }

   /* Licht in Ess-, Wohnzimmer, Fitness, Arbeit UG kurz umschalten */
   DigOutToggle(eDigOut13);
   DigOutToggle(eDigOut15);
   DigOutToggle(eDigOut23);
   DigOutToggle(eDigOut24);
}
void ApplicationReleased30_0(void) {
   /* Licht in Ess-, Wohnzimmer, Fitness, Arbeit UG kurz umschalten */
   DigOutToggle(eDigOut13);
   DigOutToggle(eDigOut15);
   DigOutToggle(eDigOut23);
   DigOutToggle(eDigOut24);
}
void ApplicationPressed30_1(void) {}
void ApplicationReleased30_1(void) {}

void ApplicationPressed31_0(void) {
   /* Kueche Haengelampe */
   DigOutToggle(eDigOut19);
}
void ApplicationReleased31_0(void) {}
void ApplicationPressed31_1(void) {}
void ApplicationReleased31_1(void) {}

void ApplicationPressed32_0(void) {}
void ApplicationReleased32_0(void) {}
void ApplicationPressed32_1(void) {}
void ApplicationReleased32_1(void) {}

void ApplicationPressed33_0(void) {
   /* Kueche Haengelampe */
   DigOutToggle(eDigOut19);
}
void ApplicationReleased33_0(void) {}
void ApplicationPressed33_1(void) {
   /* Terrasse */
   DigOutToggle(eDigOut20);
}
void ApplicationReleased33_1(void) {}

void ApplicationPressed34_0(void) {
   /* Garage */
   DigOutToggle(eDigOut9);
}
void ApplicationReleased34_0(void) {}
void ApplicationPressed34_1(void) {}
void ApplicationReleased34_1(void) {}

void ApplicationPressed35_0(void) {
   /* Garage */
   DigOutToggle(eDigOut9);
}
void ApplicationReleased35_0(void) {}
void ApplicationPressed35_1(void) {}
void ApplicationReleased35_1(void) {}

void ApplicationPressed36_0(void) {}
void ApplicationReleased36_0(void) {}
void ApplicationPressed36_1(void) {}
void ApplicationReleased36_1(void) {}

void ApplicationPressed37_0(void) {}
void ApplicationReleased37_0(void) {}
void ApplicationPressed37_1(void) {}
void ApplicationReleased37_1(void) {}

void ApplicationPressed38_0(void) {
   /* Keller Stiege */
   DigOutToggle(eDigOut22);
}
void ApplicationReleased38_0(void) {}
void ApplicationPressed38_1(void) {}
void ApplicationReleased38_1(void) {}

void ApplicationPressed39_0(void) {
   /* Keller Lager */
   DigOutToggle(eDigOut21);
}
void ApplicationReleased39_0(void) {}
void ApplicationPressed39_1(void) {}
void ApplicationReleased39_1(void) {}

void ApplicationPressed40_0(void) {
   /* Keller Fitness */
   DigOutToggle(eDigOut24);
}
void ApplicationReleased40_0(void) {}
void ApplicationPressed40_1(void) {}
void ApplicationReleased40_1(void) {}

void ApplicationPressed41_0(void) {
   /* Keller Technik */
   DigOutToggle(eDigOut26);
}
void ApplicationReleased41_0(void) {}
void ApplicationPressed41_1(void) {}
void ApplicationReleased41_1(void) {}

void ApplicationPressed42_0(void) {
   /* Keller Arbeit */
   DigOutToggle(eDigOut23);
}
void ApplicationReleased42_0(void) {}
void ApplicationPressed42_1(void) {}
void ApplicationReleased42_1(void) {}

void ApplicationPressed43_0(void) {
   /* Keller Stiege */
   DigOutToggle(eDigOut22);
}
void ApplicationReleased43_0(void) {}
void ApplicationPressed43_1(void) {
   /* Keller Vorraum */
   DigOutToggle(eDigOut25);
}
void ApplicationReleased43_1(void) {}

void ApplicationPressed44_0(void) {}
void ApplicationReleased44_0(void) {}
void ApplicationPressed44_1(void) {}
void ApplicationReleased44_1(void) {}

void ApplicationPressed45_0(void) {}
void ApplicationReleased45_0(void) {}
void ApplicationPressed45_1(void) {}
void ApplicationReleased45_1(void) {}

void ApplicationPressed46_0(void) {}
void ApplicationReleased46_0(void) {}
void ApplicationPressed46_1(void) {}
void ApplicationReleased46_1(void) {}

void ApplicationPressed47_0(void) {}
void ApplicationReleased47_0(void) {}
void ApplicationPressed47_1(void) {}
void ApplicationReleased47_1(void) {}

void ApplicationPressed48_0(void) {}
void ApplicationReleased48_0(void) {}
void ApplicationPressed48_1(void) {}
void ApplicationReleased48_1(void) {}

void ApplicationPressed49_0(void) {}
void ApplicationReleased49_0(void) {}
void ApplicationPressed49_1(void) {}
void ApplicationReleased49_1(void) {}

void ApplicationPressed50_0(void) {}
void ApplicationReleased50_0(void) {}
void ApplicationPressed50_1(void) {}
void ApplicationReleased50_1(void) {}

void ApplicationPressed51_0(void) {}
void ApplicationReleased51_0(void) {}
void ApplicationPressed51_1(void) {}
void ApplicationReleased51_1(void) {}

void ApplicationPressed52_0(void) {}
void ApplicationReleased52_0(void) {}
void ApplicationPressed52_1(void) {}
void ApplicationReleased52_1(void) {}

void ApplicationPressed53_0(void) {}
void ApplicationReleased53_0(void) {}
void ApplicationPressed53_1(void) {}
void ApplicationReleased53_1(void) {}

void ApplicationPressed54_0(void) {}
void ApplicationReleased54_0(void) {}
void ApplicationPressed54_1(void) {}
void ApplicationReleased54_1(void) {}

void ApplicationPressed55_0(void) {}
void ApplicationReleased55_0(void) {}
void ApplicationPressed55_1(void) {}
void ApplicationReleased55_1(void) {}

void ApplicationPressed56_0(void) {}
void ApplicationReleased56_0(void) {}
void ApplicationPressed56_1(void) {}
void ApplicationReleased56_1(void) {}

void ApplicationPressed57_0(void) {}
void ApplicationReleased57_0(void) {}
void ApplicationPressed57_1(void) {}
void ApplicationReleased57_1(void) {}

void ApplicationPressed58_0(void) {}
void ApplicationReleased58_0(void) {}
void ApplicationPressed58_1(void) {}
void ApplicationReleased58_1(void) {}

void ApplicationPressed59_0(void) {}
void ApplicationReleased59_0(void) {}
void ApplicationPressed59_1(void) {}
void ApplicationReleased59_1(void) {}

void ApplicationPressed60_0(void) {}
void ApplicationReleased60_0(void) {}
void ApplicationPressed60_1(void) {}
void ApplicationReleased60_1(void) {}

void ApplicationPressed61_0(void) {}
void ApplicationReleased61_0(void) {}
void ApplicationPressed61_1(void) {}
void ApplicationReleased61_1(void) {}

void ApplicationPressed62_0(void) {}
void ApplicationReleased62_0(void) {}
void ApplicationPressed62_1(void) {}
void ApplicationReleased62_1(void) {}

void ApplicationPressed63_0(void) {}
void ApplicationReleased63_0(void) {}
void ApplicationPressed63_1(void) {}
void ApplicationReleased63_1(void) {}

void ApplicationPressed64_0(void) {}
void ApplicationReleased64_0(void) {}
void ApplicationPressed64_1(void) {}
void ApplicationReleased64_1(void) {}

void ApplicationPressed65_0(void) {}
void ApplicationReleased65_0(void) {}
void ApplicationPressed65_1(void) {}
void ApplicationReleased65_1(void) {}

void ApplicationPressed66_0(void) {}
void ApplicationReleased66_0(void) {}
void ApplicationPressed66_1(void) {}
void ApplicationReleased66_1(void) {}

void ApplicationPressed67_0(void) {}
void ApplicationReleased67_0(void) {}
void ApplicationPressed67_1(void) {}
void ApplicationReleased67_1(void) {}

void ApplicationPressed68_0(void) {}
void ApplicationReleased68_0(void) {}
void ApplicationPressed68_1(void) {}
void ApplicationReleased68_1(void) {}

void ApplicationPressed69_0(void) {}
void ApplicationReleased69_0(void) {}
void ApplicationPressed69_1(void) {}
void ApplicationReleased69_1(void) {}

void ApplicationPressed70_0(void) {}
void ApplicationReleased70_0(void) {}
void ApplicationPressed70_1(void) {}
void ApplicationReleased70_1(void) {}

void ApplicationPressed71_0(void) {}
void ApplicationReleased71_0(void) {}
void ApplicationPressed71_1(void) {}
void ApplicationReleased71_1(void) {}

void ApplicationPressed72_0(void) {}
void ApplicationReleased72_0(void) {}
void ApplicationPressed72_1(void) {}
void ApplicationReleased72_1(void) {}

void ApplicationPressed73_0(void) {}
void ApplicationReleased73_0(void) {}
void ApplicationPressed73_1(void) {}
void ApplicationReleased73_1(void) {}

void ApplicationPressed74_0(void) {}
void ApplicationReleased74_0(void) {}
void ApplicationPressed74_1(void) {}
void ApplicationReleased74_1(void) {}

void ApplicationPressed75_0(void) {}
void ApplicationReleased75_0(void) {}
void ApplicationPressed75_1(void) {}
void ApplicationReleased75_1(void) {}

void ApplicationPressed76_0(void) {}
void ApplicationReleased76_0(void) {}
void ApplicationPressed76_1(void) {}
void ApplicationReleased76_1(void) {}

void ApplicationPressed77_0(void) {}
void ApplicationReleased77_0(void) {}
void ApplicationPressed77_1(void) {}
void ApplicationReleased77_1(void) {}

void ApplicationPressed78_0(void) {}
void ApplicationReleased78_0(void) {}
void ApplicationPressed78_1(void) {}
void ApplicationReleased78_1(void) {}

void ApplicationPressed79_0(void) {}
void ApplicationReleased79_0(void) {}
void ApplicationPressed79_1(void) {}
void ApplicationReleased79_1(void) {}

void ApplicationPressed80_0(void) {}
void ApplicationReleased80_0(void) {}
void ApplicationPressed80_1(void) {}
void ApplicationReleased80_1(void) {}

void ApplicationPressed81_0(void) {}
void ApplicationReleased81_0(void) {}
void ApplicationPressed81_1(void) {}
void ApplicationReleased81_1(void) {}

void ApplicationPressed82_0(void) {}
void ApplicationReleased82_0(void) {}
void ApplicationPressed82_1(void) {}
void ApplicationReleased82_1(void) {}

void ApplicationPressed83_0(void) {}
void ApplicationReleased83_0(void) {}
void ApplicationPressed83_1(void) {}
void ApplicationReleased83_1(void) {}

void ApplicationPressed84_0(void) {}
void ApplicationReleased84_0(void) {}
void ApplicationPressed84_1(void) {}
void ApplicationReleased84_1(void) {}

void ApplicationPressed85_0(void) {}
void ApplicationReleased85_0(void) {}
void ApplicationPressed85_1(void) {}
void ApplicationReleased85_1(void) {}

void ApplicationPressed86_0(void) {}
void ApplicationReleased86_0(void) {}
void ApplicationPressed86_1(void) {}
void ApplicationReleased86_1(void) {}

void ApplicationPressed87_0(void) {}
void ApplicationReleased87_0(void) {}
void ApplicationPressed87_1(void) {}
void ApplicationReleased87_1(void) {}

void ApplicationPressed88_0(void) {}
void ApplicationReleased88_0(void) {}
void ApplicationPressed88_1(void) {}
void ApplicationReleased88_1(void) {}

void ApplicationPressed89_0(void) {}
void ApplicationReleased89_0(void) {}
void ApplicationPressed89_1(void) {}
void ApplicationReleased89_1(void) {}

void ApplicationPressed90_0(void) {}
void ApplicationReleased90_0(void) {}
void ApplicationPressed90_1(void) {}
void ApplicationReleased90_1(void) {}

void ApplicationPressed91_0(void) {}
void ApplicationReleased91_0(void) {}
void ApplicationPressed91_1(void) {}
void ApplicationReleased91_1(void) {}

void ApplicationPressed92_0(void) {}
void ApplicationReleased92_0(void) {}
void ApplicationPressed92_1(void) {}
void ApplicationReleased92_1(void) {}

void ApplicationPressed93_0(void) {}
void ApplicationReleased93_0(void) {}
void ApplicationPressed93_1(void) {}
void ApplicationReleased93_1(void) {}

void ApplicationPressed94_0(void) {}
void ApplicationReleased94_0(void) {}
void ApplicationPressed94_1(void) {}
void ApplicationReleased94_1(void) {}

void ApplicationPressed95_0(void) {}
void ApplicationReleased95_0(void) {}
void ApplicationPressed95_1(void) {}
void ApplicationReleased95_1(void) {}

void ApplicationPressed96_0(void) {}
void ApplicationReleased96_0(void) {}
void ApplicationPressed96_1(void) {}
void ApplicationReleased96_1(void) {}

void ApplicationPressed97_0(void) {}
void ApplicationReleased97_0(void) {}
void ApplicationPressed97_1(void) {}
void ApplicationReleased97_1(void) {}

void ApplicationPressed98_0(void) {}
void ApplicationReleased98_0(void) {}
void ApplicationPressed98_1(void) {}
void ApplicationReleased98_1(void) {}

void ApplicationPressed99_0(void) {}
void ApplicationReleased99_0(void) {}
void ApplicationPressed99_1(void) {}
void ApplicationReleased99_1(void) {}

void ApplicationPressed100_0(void) {}
void ApplicationReleased100_0(void) {}
void ApplicationPressed100_1(void) {}
void ApplicationReleased100_1(void) {}

void ApplicationPressed101_0(void) {}
void ApplicationReleased101_0(void) {}
void ApplicationPressed101_1(void) {}
void ApplicationReleased101_1(void) {}

void ApplicationPressed102_0(void) {}
void ApplicationReleased102_0(void) {}
void ApplicationPressed102_1(void) {}
void ApplicationReleased102_1(void) {}

void ApplicationPressed103_0(void) {}
void ApplicationReleased103_0(void) {}
void ApplicationPressed103_1(void) {}
void ApplicationReleased103_1(void) {}

void ApplicationPressed104_0(void) {}
void ApplicationReleased104_0(void) {}
void ApplicationPressed104_1(void) {}
void ApplicationReleased104_1(void) {}

void ApplicationPressed105_0(void) {}
void ApplicationReleased105_0(void) {}
void ApplicationPressed105_1(void) {}
void ApplicationReleased105_1(void) {}

void ApplicationPressed106_0(void) {}
void ApplicationReleased106_0(void) {}
void ApplicationPressed106_1(void) {}
void ApplicationReleased106_1(void) {}

void ApplicationPressed107_0(void) {}
void ApplicationReleased107_0(void) {}
void ApplicationPressed107_1(void) {}
void ApplicationReleased107_1(void) {}

void ApplicationPressed108_0(void) {}
void ApplicationReleased108_0(void) {}
void ApplicationPressed108_1(void) {}
void ApplicationReleased108_1(void) {}

void ApplicationPressed109_0(void) {}
void ApplicationReleased109_0(void) {}
void ApplicationPressed109_1(void) {}
void ApplicationReleased109_1(void) {}

void ApplicationPressed110_0(void) {}
void ApplicationReleased110_0(void) {}
void ApplicationPressed110_1(void) {}
void ApplicationReleased110_1(void) {}

void ApplicationPressed111_0(void) {}
void ApplicationReleased111_0(void) {}
void ApplicationPressed111_1(void) {}
void ApplicationReleased111_1(void) {}

void ApplicationPressed112_0(void) {}
void ApplicationReleased112_0(void) {}
void ApplicationPressed112_1(void) {}
void ApplicationReleased112_1(void) {}

void ApplicationPressed113_0(void) {}
void ApplicationReleased113_0(void) {}
void ApplicationPressed113_1(void) {}
void ApplicationReleased113_1(void) {}

void ApplicationPressed114_0(void) {}
void ApplicationReleased114_0(void) {}
void ApplicationPressed114_1(void) {}
void ApplicationReleased114_1(void) {}

void ApplicationPressed115_0(void) {}
void ApplicationReleased115_0(void) {}
void ApplicationPressed115_1(void) {}
void ApplicationReleased115_1(void) {}

void ApplicationPressed116_0(void) {}
void ApplicationReleased116_0(void) {}
void ApplicationPressed116_1(void) {}
void ApplicationReleased116_1(void) {}

void ApplicationPressed117_0(void) {}
void ApplicationReleased117_0(void) {}
void ApplicationPressed117_1(void) {}
void ApplicationReleased117_1(void) {}

void ApplicationPressed118_0(void) {}
void ApplicationReleased118_0(void) {}
void ApplicationPressed118_1(void) {}
void ApplicationReleased118_1(void) {}

void ApplicationPressed119_0(void) {}
void ApplicationReleased119_0(void) {}
void ApplicationPressed119_1(void) {}
void ApplicationReleased119_1(void) {}

void ApplicationPressed120_0(void) {}
void ApplicationReleased120_0(void) {}
void ApplicationPressed120_1(void) {}
void ApplicationReleased120_1(void) {}

void ApplicationPressed121_0(void) {}
void ApplicationReleased121_0(void) {}
void ApplicationPressed121_1(void) {}
void ApplicationReleased121_1(void) {}

void ApplicationPressed122_0(void) {}
void ApplicationReleased122_0(void) {}
void ApplicationPressed122_1(void) {}
void ApplicationReleased122_1(void) {}

void ApplicationPressed123_0(void) {}
void ApplicationReleased123_0(void) {}
void ApplicationPressed123_1(void) {}
void ApplicationReleased123_1(void) {}

void ApplicationPressed124_0(void) {}
void ApplicationReleased124_0(void) {}
void ApplicationPressed124_1(void) {}
void ApplicationReleased124_1(void) {}

void ApplicationPressed125_0(void) {}
void ApplicationReleased125_0(void) {}
void ApplicationPressed125_1(void) {}
void ApplicationReleased125_1(void) {}

void ApplicationPressed126_0(void) {}
void ApplicationReleased126_0(void) {}
void ApplicationPressed126_1(void) {}
void ApplicationReleased126_1(void) {}

void ApplicationPressed127_0(void) {}
void ApplicationReleased127_0(void) {}
void ApplicationPressed127_1(void) {}
void ApplicationReleased127_1(void) {}

void ApplicationPressed128_0(void) {}
void ApplicationReleased128_0(void) {}
void ApplicationPressed128_1(void) {}
void ApplicationReleased128_1(void) {}
