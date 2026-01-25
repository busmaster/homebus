/*-----------------------------------------------------------------------------
*  Application.c
*/

/*-----------------------------------------------------------------------------
*  Includes
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
#include "bus.h"

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
    {ApplicationPressed1_0,  ApplicationPressed1_1,  ApplicationReleased1_0,  ApplicationReleased1_1},
    {ApplicationPressed2_0,  ApplicationPressed2_1,  ApplicationReleased2_0,  ApplicationReleased2_1},
    {ApplicationPressed3_0,  ApplicationPressed3_1,  ApplicationReleased3_0,  ApplicationReleased3_1},
    {ApplicationPressed4_0,  ApplicationPressed4_1,  ApplicationReleased4_0,  ApplicationReleased4_1},
    {ApplicationPressed5_0,  ApplicationPressed5_1,  ApplicationReleased5_0,  ApplicationReleased5_1},
    {ApplicationPressed6_0,  ApplicationPressed6_1,  ApplicationReleased6_0,  ApplicationReleased6_1},
    {ApplicationPressed7_0,  ApplicationPressed7_1,  ApplicationReleased7_0,  ApplicationReleased7_1},
    {ApplicationPressed8_0,  ApplicationPressed8_1,  ApplicationReleased8_0,  ApplicationReleased8_1},
    {ApplicationPressed9_0,  ApplicationPressed9_1,  ApplicationReleased9_0,  ApplicationReleased9_1},
    {ApplicationPressed10_0, ApplicationPressed10_1, ApplicationReleased10_0, ApplicationReleased10_1},
    {ApplicationPressed11_0, ApplicationPressed11_1, ApplicationReleased11_0, ApplicationReleased11_1},
    {ApplicationPressed12_0, ApplicationPressed12_1, ApplicationReleased12_0, ApplicationReleased12_1},
    {ApplicationPressed13_0, ApplicationPressed13_1, ApplicationReleased13_0, ApplicationReleased13_1},
    {ApplicationPressed14_0, ApplicationPressed14_1, ApplicationReleased14_0, ApplicationReleased14_1},
    {ApplicationPressed15_0, ApplicationPressed15_1, ApplicationReleased15_0, ApplicationReleased15_1},
    {ApplicationPressed16_0, ApplicationPressed16_1, ApplicationReleased16_0, ApplicationReleased16_1},
    {ApplicationPressed17_0, ApplicationPressed17_1, ApplicationReleased17_0, ApplicationReleased17_1},
    {ApplicationPressed18_0, ApplicationPressed18_1, ApplicationReleased18_0, ApplicationReleased18_1},
    {ApplicationPressed19_0, ApplicationPressed19_1, ApplicationReleased19_0, ApplicationReleased19_1},
    {ApplicationPressed20_0, ApplicationPressed20_1, ApplicationReleased20_0, ApplicationReleased20_1},
    {ApplicationPressed21_0, ApplicationPressed21_1, ApplicationReleased21_0, ApplicationReleased21_1},
    {ApplicationPressed22_0, ApplicationPressed22_1, ApplicationReleased22_0, ApplicationReleased22_1},
    {ApplicationPressed23_0, ApplicationPressed23_1, ApplicationReleased23_0, ApplicationReleased23_1},
    {ApplicationPressed24_0, ApplicationPressed24_1, ApplicationReleased24_0, ApplicationReleased24_1},
    {ApplicationPressed25_0, ApplicationPressed25_1, ApplicationReleased25_0, ApplicationReleased25_1},
    {ApplicationPressed26_0, ApplicationPressed26_1, ApplicationReleased26_0, ApplicationReleased26_1},
    {ApplicationPressed27_0, ApplicationPressed27_1, ApplicationReleased27_0, ApplicationReleased27_1},
    {ApplicationPressed28_0, ApplicationPressed28_1, ApplicationReleased28_0, ApplicationReleased28_1},
    {ApplicationPressed29_0, ApplicationPressed29_1, ApplicationReleased29_0, ApplicationReleased29_1},
    {ApplicationPressed30_0, ApplicationPressed30_1, ApplicationReleased30_0, ApplicationReleased30_1},
    {ApplicationPressed31_0, ApplicationPressed31_1, ApplicationReleased31_0, ApplicationReleased31_1},
    {ApplicationPressed32_0, ApplicationPressed32_1, ApplicationReleased32_0, ApplicationReleased32_1},
    {ApplicationPressed33_0, ApplicationPressed33_1, ApplicationReleased33_0, ApplicationReleased33_1},
    {ApplicationPressed34_0, ApplicationPressed34_1, ApplicationReleased34_0, ApplicationReleased34_1},
    {ApplicationPressed35_0, ApplicationPressed35_1, ApplicationReleased35_0, ApplicationReleased35_1},
    {ApplicationPressed36_0, ApplicationPressed36_1, ApplicationReleased36_0, ApplicationReleased36_1},
    {ApplicationPressed37_0, ApplicationPressed37_1, ApplicationReleased37_0, ApplicationReleased37_1},
    {ApplicationPressed38_0, ApplicationPressed38_1, ApplicationReleased38_0, ApplicationReleased38_1},
    {ApplicationPressed39_0, ApplicationPressed39_1, ApplicationReleased39_0, ApplicationReleased39_1},
    {ApplicationPressed40_0, ApplicationPressed40_1, ApplicationReleased40_0, ApplicationReleased40_1},
    {ApplicationPressed41_0, ApplicationPressed41_1, ApplicationReleased41_0, ApplicationReleased41_1},
    {ApplicationPressed42_0, ApplicationPressed42_1, ApplicationReleased42_0, ApplicationReleased42_1},
    {ApplicationPressed43_0, ApplicationPressed43_1, ApplicationReleased43_0, ApplicationReleased43_1},
    {ApplicationPressed44_0, ApplicationPressed44_1, ApplicationReleased44_0, ApplicationReleased44_1},
    {ApplicationPressed45_0, ApplicationPressed45_1, ApplicationReleased45_0, ApplicationReleased45_1},
    {ApplicationPressed46_0, ApplicationPressed46_1, ApplicationReleased46_0, ApplicationReleased46_1},
    {ApplicationPressed47_0, ApplicationPressed47_1, ApplicationReleased47_0, ApplicationReleased47_1},
    {ApplicationPressed48_0, ApplicationPressed48_1, ApplicationReleased48_0, ApplicationReleased48_1},
    {ApplicationPressed49_0, ApplicationPressed49_1, ApplicationReleased49_0, ApplicationReleased49_1},
    {ApplicationPressed50_0, ApplicationPressed50_1, ApplicationReleased50_0, ApplicationReleased50_1},
    {ApplicationPressed51_0, ApplicationPressed51_1, ApplicationReleased51_0, ApplicationReleased51_1},
    {ApplicationPressed52_0, ApplicationPressed52_1, ApplicationReleased52_0, ApplicationReleased52_1},
    {ApplicationPressed53_0, ApplicationPressed53_1, ApplicationReleased53_0, ApplicationReleased53_1},
    {ApplicationPressed54_0, ApplicationPressed54_1, ApplicationReleased54_0, ApplicationReleased54_1},
    {ApplicationPressed55_0, ApplicationPressed55_1, ApplicationReleased55_0, ApplicationReleased55_1},
    {ApplicationPressed56_0, ApplicationPressed56_1, ApplicationReleased56_0, ApplicationReleased56_1},
    {ApplicationPressed57_0, ApplicationPressed57_1, ApplicationReleased57_0, ApplicationReleased57_1},
    {ApplicationPressed58_0, ApplicationPressed58_1, ApplicationReleased58_0, ApplicationReleased58_1},
    {ApplicationPressed59_0, ApplicationPressed59_1, ApplicationReleased59_0, ApplicationReleased59_1},
    {ApplicationPressed60_0, ApplicationPressed60_1, ApplicationReleased60_0, ApplicationReleased60_1},
    {ApplicationPressed61_0, ApplicationPressed61_1, ApplicationReleased61_0, ApplicationReleased61_1},
    {ApplicationPressed62_0, ApplicationPressed62_1, ApplicationReleased62_0, ApplicationReleased62_1},
    {ApplicationPressed63_0, ApplicationPressed63_1, ApplicationReleased63_0, ApplicationReleased63_1},
    {ApplicationPressed64_0, ApplicationPressed64_1, ApplicationReleased64_0, ApplicationReleased64_1}
};

/*----------------------------------------------------------------------------- 
* Rückgabe der Versioninfo (max. Länge 15 Zeichen)
*/
const char *ApplicationVersion(void) {
    return "Kurt240_260125";
}

/*----------------------------------------------------------------------------- 
* Benachrichtungung der Application über Tastenereignis
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
 
    if (pButtonEvent->pressed == true) {
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
eDigOut0 
eDigOut1 
eDigOut2 
eDigOut3 
eDigOut4 
eDigOut5 
eDigOut6 
eDigOut7 
eDigOut8 
eDigOut9 
eDigOut10
eDigOut11
eDigOut12
eDigOut13
eDigOut14
eDigOut15
eDigOut16
eDigOut17
eDigOut18
eDigOut19
eDigOut20
eDigOut21
eDigOut22
eDigOut23
eDigOut24
eDigOut25
eDigOut26
eDigOut27
eDigOut28
eDigOut29
eDigOut30
*/


void ApplicationPressed1_0(void) {

    DigOutOn(eDigOut1);  
}
void ApplicationReleased1_0(void) {

    DigOutOff(eDigOut1);  
}
void ApplicationPressed1_1(void) {} 
void ApplicationReleased1_1(void) {}

void ApplicationPressed2_0(void) {

    DigOutToggle(eDigOut28);
    DigOutOff(eDigOut27);
}
void ApplicationReleased2_0(void) {}
void ApplicationPressed2_1(void) {}
void ApplicationReleased2_1(void) {}

void ApplicationPressed3_0(void) {

    DigOutToggle(eDigOut3);  
}
void ApplicationReleased3_0(void) {}
void ApplicationPressed3_1(void) {

    DigOutToggle(eDigOut4);
}
void ApplicationReleased3_1(void) {}
                                     
void ApplicationPressed4_0(void) {}
void ApplicationReleased4_0(void) {}
void ApplicationPressed4_1(void) {}
void ApplicationReleased4_1(void) {}

void ApplicationPressed5_0(void) {

    DigOutToggle(eDigOut1);  
}
void ApplicationReleased5_0(void) {}
void ApplicationPressed5_1(void) {}
void ApplicationReleased5_1(void) {}
                                     
void ApplicationPressed6_0(void) {

    DigOutToggle(eDigOut25);  
}
void ApplicationReleased6_0(void) {}
void ApplicationPressed6_1(void) {}
void ApplicationReleased6_1(void) {}
                                     
void ApplicationPressed7_0(void) {

    DigOutToggle(eDigOut12);  
}
void ApplicationReleased7_0(void) {}
void ApplicationPressed7_1(void) {

    DigOutToggle(eDigOut16);
}
void ApplicationReleased7_1(void) {}

void ApplicationPressed8_0(void) {

    DigOutToggle(eDigOut26);  
}
void ApplicationReleased8_0(void) {}
void ApplicationPressed8_1(void) {}
void ApplicationReleased8_1(void) {}

void ApplicationPressed9_0(void) {

    DigOutToggle(eDigOut0);  
}
void ApplicationReleased9_0(void) {}
void ApplicationPressed9_1(void) {}
void ApplicationReleased9_1(void) {}

void ApplicationPressed10_0(void) {

    DigOutToggle(eDigOut21);
} 
void ApplicationReleased10_0(void) {}
void ApplicationPressed10_1(void) {} 
void ApplicationReleased10_1(void) {}
    
void ApplicationPressed11_0(void) {

    DigOutToggle(eDigOut9);  
} 
void ApplicationReleased11_0(void) {}
void ApplicationPressed11_1(void) {} 
void ApplicationReleased11_1(void) {}
                                     
void ApplicationPressed12_0(void) {} 
void ApplicationReleased12_0(void) {}
void ApplicationPressed12_1(void) { 

    DigOutToggle(eDigOut24);
} 
void ApplicationReleased12_1(void) {}
                                      
void ApplicationPressed13_0(void) {

    DigOutToggle(eDigOut11);  
}
void ApplicationReleased13_0(void) {}
void ApplicationPressed13_1(void) {

    DigOutToggle(eDigOut1); 
}
void ApplicationReleased13_1(void) {}
                                      
void ApplicationPressed14_0(void) {}
void ApplicationReleased14_0(void) {}
void ApplicationPressed14_1(void) {}
void ApplicationReleased14_1(void) {}
                                      
void ApplicationPressed15_0(void) {
   DigOutToggle(eDigOut11);  
}
void ApplicationReleased15_0(void) {}
void ApplicationPressed15_1(void) {

    DigOutToggle(eDigOut28);
    DigOutOff(eDigOut27);
}
void ApplicationReleased15_1(void) {}
                                      
void ApplicationPressed16_0(void) {

    DigOutToggle(eDigOut3);  
}
void ApplicationReleased16_0(void) {}
void ApplicationPressed16_1(void) {

    DigOutToggle(eDigOut4);
}
void ApplicationReleased16_1(void) {}

void ApplicationPressed17_0(void) {

    DigOutToggle(eDigOut11);  
}
void ApplicationReleased17_0(void) {}
void ApplicationPressed17_1(void) {}
void ApplicationReleased17_1(void) {}
                                      
void ApplicationPressed18_0(void) {

    DigOutToggle(eDigOut5);  
}
void ApplicationReleased18_0(void) {}
void ApplicationPressed18_1(void) {

   DigOutToggle(eDigOut2);
}
void ApplicationReleased18_1(void) {}
                                      
void ApplicationPressed19_0(void) {

    DigOutOff(0);
    DigOutOff(eDigOut1);
    DigOutOff(eDigOut2);
    DigOutOff(eDigOut3);
    DigOutOff(eDigOut4);
    DigOutOff(eDigOut5);
    DigOutOff(eDigOut6);
    DigOutOff(eDigOut7);
    DigOutOff(eDigOut8);
    DigOutOff(eDigOut9);
    DigOutOff(eDigOut10);
    DigOutOff(eDigOut11);
    DigOutOff(eDigOut12);
    DigOutOff(eDigOut13);
    DigOutOff(eDigOut14);
    DigOutOff(eDigOut15);
    DigOutOff(eDigOut16);
    DigOutOff(eDigOut17);
    //DigOutOff(eDigOut18);
    //DigOutOff(eDigOut19);
    DigOutOff(eDigOut20);
    DigOutOff(eDigOut21);
    DigOutOff(eDigOut22);
    DigOutOff(eDigOut23);
    DigOutOff(eDigOut24);
    DigOutOff(eDigOut25);
    DigOutOff(eDigOut26);
    DigOutOff(eDigOut27);
    DigOutOff(eDigOut28);
    DigOutOff(eDigOut29);
    DigOutOff(eDigOut30);
}
void ApplicationReleased19_0(void) {}
void ApplicationPressed19_1(void) {

    DigOutOff(eDigOut0);
    DigOutOff(eDigOut1);
    DigOutOff(eDigOut2);
    DigOutOff(eDigOut3);
    DigOutOff(eDigOut4);
    DigOutOff(eDigOut5);
    DigOutOff(eDigOut6);
    DigOutOff(eDigOut7);
    DigOutOff(eDigOut8);
    DigOutOff(eDigOut9);
    DigOutOff(eDigOut10);
    DigOutOff(eDigOut11);
    DigOutOff(eDigOut12);
    DigOutOff(eDigOut13);
    DigOutOff(eDigOut14);
    DigOutOff(eDigOut15);
    DigOutOff(eDigOut16);
    DigOutOff(eDigOut17);
    //DigOutOff(eDigOut18);
    //DigOutOff(eDigOut19);
    DigOutOff(eDigOut20);
    DigOutOff(eDigOut21);
    DigOutOff(eDigOut22);
    DigOutOff(eDigOut23);
    DigOutOff(eDigOut24);
    DigOutOff(eDigOut25);
    DigOutOff(eDigOut26);
    DigOutOff(eDigOut27);
    DigOutOff(eDigOut28);
    DigOutOff(eDigOut29);
    DigOutOff(eDigOut30);
}
void ApplicationReleased19_1(void) {}
                                      
void ApplicationPressed20_0(void) {

    DigOutToggle(eDigOut16);  
}
void ApplicationReleased20_0(void) {}
void ApplicationPressed20_1(void) {

    DigOutToggle(eDigOut12);
}
void ApplicationReleased20_1(void) {}

void ApplicationPressed21_0(void) {}
void ApplicationReleased21_0(void) {}
void ApplicationPressed21_1(void) {}
void ApplicationReleased21_1(void) {}

void ApplicationPressed22_0(void) {

    DigOutToggle(eDigOut11);  
}
void ApplicationReleased22_0(void) {}
void ApplicationPressed22_1(void) {

    DigOutToggle(eDigOut1);
}
void ApplicationReleased22_1(void) {}

void ApplicationPressed23_0(void) {

    DigOutToggle(eDigOut27);
}
void ApplicationReleased23_0(void) {}
void ApplicationPressed23_1(void) {

    DigOutToggle(eDigOut28);
}
void ApplicationReleased23_1(void) {}
                                      
void ApplicationPressed24_0(void) {

    DigOutToggle(eDigOut1);  
}
void ApplicationReleased24_0(void) {}
void ApplicationPressed24_1(void) {}
void ApplicationReleased24_1(void) {}
                                      
void ApplicationPressed25_0(void) {

    DigOutToggle(eDigOut23);  
}
void ApplicationReleased25_0(void) {}
void ApplicationPressed25_1(void) {}
void ApplicationReleased25_1(void) {}
                                      
void ApplicationPressed26_0(void) {

    DigOutToggle(eDigOut22);  
}
void ApplicationReleased26_0(void) {}
void ApplicationPressed26_1(void) {}
void ApplicationReleased26_1(void) {}
                                      
void ApplicationPressed27_0(void) {}
void ApplicationReleased27_0(void) {}
void ApplicationPressed27_1(void) {}
void ApplicationReleased27_1(void) {}
                                      
void ApplicationPressed28_0(void) {

    DigOutToggle(eDigOut10);  
}
void ApplicationReleased28_0(void) {}
void ApplicationPressed28_1(void) {}
void ApplicationReleased28_1(void) {}
                                      
void ApplicationPressed29_0(void) {

    if (DigOutState(eDigOut15) == true) {
        DigOutToggle(eDigOut15);
    } else {
        DigOutDelayedOff(eDigOut15, 432000000);
    }
}
void ApplicationReleased29_0(void) {}
void ApplicationPressed29_1(void) {

    DigOutToggle(eDigOut12);
}
void ApplicationReleased29_1(void) {}
                                      
void ApplicationPressed30_0(void) {

    DigOutToggle(eDigOut21);  
}
void ApplicationReleased30_0(void) {}
void ApplicationPressed30_1(void) {

    DigOutToggle(eDigOut22);
}
void ApplicationReleased30_1(void) {}
                                      
void ApplicationPressed31_0(void) {

    DigOutToggle(eDigOut13);  
}
void ApplicationReleased31_0(void) {}
void ApplicationPressed31_1(void) {}
void ApplicationReleased31_1(void) {}
                                      
void ApplicationPressed32_0(void) {}
void ApplicationReleased32_0(void) {}
void ApplicationPressed32_1(void) {}
void ApplicationReleased32_1(void) {}
    
void ApplicationPressed33_0(void) {

    DigOutToggle(eDigOut28);
}
void ApplicationReleased33_0(void) {}
void ApplicationPressed33_1(void) {

    DigOutToggle(eDigOut27);
}
void ApplicationReleased33_1(void) {}
                                      
void ApplicationPressed34_0(void) {}
void ApplicationReleased34_0(void) {}
void ApplicationPressed34_1(void) {}
void ApplicationReleased34_1(void) {}
                                      
void ApplicationPressed35_0(void) {}
void ApplicationReleased35_0(void) {}
void ApplicationPressed35_1(void) {}
void ApplicationReleased35_1(void) {}
                                      
void ApplicationPressed36_0(void) {}
void ApplicationReleased36_0(void) {}
void ApplicationPressed36_1(void) {}
void ApplicationReleased36_1(void) {}
                                      
void ApplicationPressed37_0(void) {

    DigOutToggle(eDigOut30);
}
void ApplicationReleased37_0(void) {}
void ApplicationPressed37_1(void) {

    DigOutToggle(eDigOut29);
}
void ApplicationReleased37_1(void) {}
                                      
void ApplicationPressed38_0(void) {}
void ApplicationReleased38_0(void) {}
void ApplicationPressed38_1(void) {}
void ApplicationReleased38_1(void) {}
                                      
void ApplicationPressed39_0(void) {

    DigOutToggle(eDigOut17);
}
void ApplicationReleased39_0(void) {}
void ApplicationPressed39_1(void) {}
void ApplicationReleased39_1(void) {}
                                      
void ApplicationPressed40_0(void) {

    DigOutOff(eDigOut0);
    DigOutOff(eDigOut1);
    DigOutOff(eDigOut2);
    DigOutOff(eDigOut3);
    DigOutOff(eDigOut4);
    DigOutOff(eDigOut5);
    DigOutOff(eDigOut6);
    DigOutOff(eDigOut7);
    DigOutOff(eDigOut8);
    DigOutOff(eDigOut9);
    DigOutOff(eDigOut10);
    DigOutOff(eDigOut11);
    DigOutOff(eDigOut12);
    DigOutOff(eDigOut13);
    DigOutOff(eDigOut14);
    DigOutOff(eDigOut15);
    DigOutOff(eDigOut16);
    DigOutOff(eDigOut17);
    //DigOutOff(eDigOut18);
    //DigOutOff(eDigOut19);
    DigOutOff(eDigOut20);
    DigOutOff(eDigOut21);
    DigOutOff(eDigOut22);
    DigOutOff(eDigOut23);
    DigOutOff(eDigOut24);
    DigOutOff(eDigOut25);
    DigOutOff(eDigOut26);
    DigOutOff(eDigOut27);
    DigOutOff(eDigOut28);
    DigOutOff(eDigOut29);
    DigOutOff(eDigOut30);
}

void ApplicationReleased40_0(void) {}
void ApplicationPressed40_1(void) {}
void ApplicationReleased40_1(void) {}
                                      
void ApplicationPressed41_0(void) {

    DigOutToggle(eDigOut27);
    DigOutOff(eDigOut28);
}
void ApplicationReleased41_0(void) {}
void ApplicationPressed41_1(void) {}
void ApplicationReleased41_1(void) {}
                                      
void ApplicationPressed42_0(void) {

    DigOutToggle(eDigOut28);
    DigOutOff(eDigOut27);
}
void ApplicationReleased42_0(void) {}
void ApplicationPressed42_1(void) {

    DigOutToggle(eDigOut28);
    DigOutOff(eDigOut27);
}
void ApplicationReleased42_1(void) {}
                                      
void ApplicationPressed43_0(void) {}
void ApplicationReleased43_0(void) {}
void ApplicationPressed43_1(void) {}
void ApplicationReleased43_1(void) {}
                                      
void ApplicationPressed44_0(void) {}
void ApplicationReleased44_0(void) {}
void ApplicationPressed44_1(void) {}
void ApplicationReleased44_1(void) {}
                                      
void ApplicationPressed45_0(void) {

    if ((DigOutState(eDigOut30) == true) ||
        (DigOutState(eDigOut29) == true)) {
        DigOutOff(eDigOut29);
        DigOutOff(eDigOut30);
    } else {
        DigOutOn(eDigOut29);
    }
}

void ApplicationReleased45_0(void) {}
void ApplicationPressed45_1(void) {}
void ApplicationReleased45_1(void) {}
                                      
void ApplicationPressed46_0(void) {}
void ApplicationReleased46_0(void) {}
void ApplicationPressed46_1(void) {}
void ApplicationReleased46_1(void) {}
                                      
void ApplicationPressed47_0(void) {

    DigOutToggle(eDigOut28);
    DigOutOff(eDigOut27);
}

void ApplicationReleased47_0(void) {}
void ApplicationPressed47_1(void) {

    DigOutToggle(eDigOut11);
}
void ApplicationReleased47_1(void) {}
                                      
void ApplicationPressed48_0(void) {

    if (DigOutState(eDigOut20) == true) {
        DigOutToggle(eDigOut20);
    } else {
        DigOutDelayedOff(eDigOut20, 864000000);
    }
}
void ApplicationReleased48_0(void) {}
void ApplicationPressed48_1(void) {

    if ((DigOutState(eDigOut30) == true) ||
        (DigOutState(eDigOut29) == true)) {
        DigOutOff(eDigOut29);
        DigOutOff(eDigOut30);
    } else {
        DigOutOn(eDigOut29);
    }
}
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
void ApplicationPressed54_1(void) {

    if (DigOutState(eDigOut6) == true) {
        DigOutToggle(eDigOut6);
    } else {
        DigOutDelayedOff(eDigOut6, 864000000);
    }
}
void ApplicationReleased54_1(void) {}
                                      
void ApplicationPressed55_0(void) {}
void ApplicationReleased55_0(void) {}
void ApplicationPressed55_1(void) {}
void ApplicationReleased55_1(void) {}
                                      
void ApplicationPressed56_0(void) {}
void ApplicationReleased56_0(void) {}
void ApplicationPressed56_1(void) {}
void ApplicationReleased56_1(void) {}
                                      
void ApplicationPressed57_0(void) {

    if (DigOutState(eDigOut7) == true) {
        DigOutToggle(eDigOut7);
    } else {
        DigOutDelayedOff(eDigOut7, 18000000);
    }
}

void ApplicationReleased57_0(void) {}
void ApplicationPressed57_1(void) {

    if (DigOutState(eDigOut8) == true) {
        DigOutToggle(eDigOut8);
    } else {
        DigOutDelayedOff(eDigOut8, 7200000);
    }
}

void ApplicationReleased57_1(void) {}

/*Garage*/
void ApplicationPressed58_0(void) {

    DigOutOn(eDigOut19);
}
void ApplicationReleased58_0(void) {

    DigOutOff(eDigOut19);
}
void ApplicationPressed58_1(void) {

    DigOutOn(eDigOut18);
}
void ApplicationReleased58_1(void) {

    DigOutOff(eDigOut18);
}

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
