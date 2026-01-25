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
    return "Kurt241_260125";
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

#if 0
//Rolladen Ausgängen zuordnen
DigOutShadeConfig(eDigOutShade0, eDigOut10, eDigOut9); //Rollo Esszimmer Links
DigOutShadeConfig(eDigOutShade1, eDigOut8, eDigOut7);  //Rollo Esszimmer Mitte
DigOutShadeConfig(eDigOutShade2, eDigOut6, eDigOut5);  //Rollo Esszimmer Rechts
DigOutShadeConfig(eDigOutShade3, eDigOut12, eDigOut11);//Rollo Schalfzimmer
DigOutShadeConfig(eDigOutShade4, eDigOut14, eDigOut13);//Rollo Küche
DigOutShadeConfig(eDigOutShade5, eDigOut16, eDigOut15);//Rollo Wohnzimmer Tür
DigOutShadeConfig(eDigOutShade6, eDigOut18, eDigOut17);//Rollo Wohnzimmer Mitte
DigOutShadeConfig(eDigOutShade7, eDigOut20, eDigOut19);//Rollo Wohnzimmer Fenster links
#endif

   // Rollo Esszimmer links
   ShaderSetConfig(eShader0, eDigOut10, eDigOut9, 31000, 30000);

   // Rollo Esszimmer mitte
   ShaderSetConfig(eShader1, eDigOut8, eDigOut7, 31000, 30000);

   // Rollo Esszimmer rechts
   ShaderSetConfig(eShader2, eDigOut6, eDigOut5, 31000, 30000);

   /* Rollo Schlafzimmer  */
   ShaderSetConfig(eShader3, eDigOut12, eDigOut11, 31000, 30000);

   /* Rollo Kueche */
   ShaderSetConfig(eShader4, eDigOut14, eDigOut13, 31000, 30000);

   /* Rollo Wohnzimmer Tür */
   ShaderSetConfig(eShader5, eDigOut16, eDigOut15, 41000, 40000);

   /* Rollo Wohnzimmer mitte */
   ShaderSetConfig(eShader6, eDigOut18, eDigOut17, 31000, 30000);

   /* Rollo Wohnzimmer Fenster links */
   ShaderSetConfig(eShader7, eDigOut20, eDigOut19, 31000, 30000);
}

void ApplicationStart(void) {

}

void ApplicationCheck(void) {

}

void ApplicationPressed1_0(void) {}
void ApplicationReleased1_0(void) {}
void ApplicationPressed1_1(void) {} 
void ApplicationReleased1_1(void) {}

void ApplicationPressed2_0(void) {}
void ApplicationReleased2_0(void) {}
void ApplicationPressed2_1(void) {

    DigOutToggle(eDigOut21);
}
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
void ApplicationPressed6_1(void) {

    DigOutToggle(eDigOut4);
}
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
                                      
void ApplicationPressed14_0(void) {

    DigOutToggle(eDigOut0);  
}
void ApplicationReleased14_0(void) {}
void ApplicationPressed14_1(void) {

    DigOutToggle(eDigOut1);  
}
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
                                      
void ApplicationPressed18_0(void) {}
void ApplicationReleased18_0(void) {}
void ApplicationPressed18_1(void) {}
void ApplicationReleased18_1(void) {}
                                      
void ApplicationPressed19_0(void) {

    DigOutOff(eDigOut0);
    DigOutOff(eDigOut1);
    //DigOutOff(eDigOut2);
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
    //DigOutOff(eDigOut2);
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

void ApplicationPressed20_0(void) {}
void ApplicationReleased20_0(void) {}
void ApplicationPressed20_1(void) {}
void ApplicationReleased20_1(void) {}

void ApplicationPressed21_0(void) {

    DigOutToggle(eDigOut22);  
}
void ApplicationReleased21_0(void) {}
void ApplicationPressed21_1(void) {

    DigOutToggle(eDigOut22);  
}
void ApplicationReleased21_1(void) {}
                                      
void ApplicationPressed22_0(void) {}
void ApplicationReleased22_0(void) {}
void ApplicationPressed22_1(void) {}
void ApplicationReleased22_1(void) {}
                                      
void ApplicationPressed23_0(void) {}
void ApplicationReleased23_0(void) {}
void ApplicationPressed23_1(void) {}
void ApplicationReleased23_1(void) {}
                                      
void ApplicationPressed24_0(void) {}
void ApplicationReleased24_0(void) {}
void ApplicationPressed24_1(void) {}
void ApplicationReleased24_1(void) {}
                                      
void ApplicationPressed25_0(void) {}
void ApplicationReleased25_0(void) {}
void ApplicationPressed25_1(void) {}
void ApplicationReleased25_1(void) {}
                                      
void ApplicationPressed26_0(void) {}
void ApplicationReleased26_0(void) {}
void ApplicationPressed26_1(void) {}
void ApplicationReleased26_1(void) {}
                                      
//Rolladen Küche
void ApplicationPressed27_0(void) {

    TShaderState state;

    ShaderGetState(eShader4, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader4, eShaderOpen);
    } else {
        ShaderSetAction(eShader4, eShaderStop);
    }
}
void ApplicationReleased27_0(void) {}
void ApplicationPressed27_1(void) {

    TShaderState state;

    ShaderGetState(eShader4, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader4, eShaderClose);
    } else {
        ShaderSetAction(eShader4, eShaderStop);
    } 
}

void ApplicationReleased27_1(void) {}
                                      
void ApplicationPressed28_0(void) {}
void ApplicationReleased28_0(void) {}
void ApplicationPressed28_1(void) {}
void ApplicationReleased28_1(void) {}
                                      
void ApplicationPressed29_0(void) {}
void ApplicationReleased29_0(void) {}
void ApplicationPressed29_1(void) {}
void ApplicationReleased29_1(void) {}
                                      
void ApplicationPressed30_0(void) {}
void ApplicationReleased30_0(void) {}
void ApplicationPressed30_1(void) {}
void ApplicationReleased30_1(void) {}
                                      
void ApplicationPressed31_0(void) {}
void ApplicationReleased31_0(void) {}
void ApplicationPressed31_1(void) {}
void ApplicationReleased31_1(void) {}
                                      
void ApplicationPressed32_0(void) {}
void ApplicationReleased32_0(void) {}
void ApplicationPressed32_1(void) {}
void ApplicationReleased32_1(void) {}
    
void ApplicationPressed33_0(void) {}
void ApplicationReleased33_0(void) {}
void ApplicationPressed33_1(void) {}
void ApplicationReleased33_1(void) {}
                                      
void ApplicationPressed34_0(void) {

    DigOutToggle(eDigOut0);  
}
void ApplicationReleased34_0(void) {}
void ApplicationPressed34_1(void) {
    DigOutToggle(eDigOut1);  
}
void ApplicationReleased34_1(void) {}
                                      
void ApplicationPressed35_0(void) {

    TShaderState state;

    ShaderGetState(eShader3, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader3, eShaderOpen);
    } else {
        ShaderSetAction(eShader3, eShaderStop);
    }
}
void ApplicationReleased35_0(void) {}
//Rollo Schlafzimmer
void ApplicationPressed35_1(void) {
    TShaderState state;

    ShaderGetState(eShader3, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader3, eShaderClose);
    } else {
        ShaderSetAction(eShader3, eShaderStop);
    } 
}
void ApplicationReleased35_1(void) {}

void ApplicationPressed36_0(void) {

    DigOutToggle(eDigOut3);  
}
void ApplicationReleased36_0(void) {}
void ApplicationPressed36_1(void) {}
void ApplicationReleased36_1(void) {}
                                      
void ApplicationPressed37_0(void) {}
void ApplicationReleased37_0(void) {}
void ApplicationPressed37_1(void) {}
void ApplicationReleased37_1(void) {}
                                      
void ApplicationPressed38_0(void) {}
void ApplicationReleased38_0(void) {}
void ApplicationPressed38_1(void) {}
void ApplicationReleased38_1(void) {}
                                      
void ApplicationPressed39_0(void) {}
void ApplicationReleased39_0(void) {}
void ApplicationPressed39_1(void) {}
void ApplicationReleased39_1(void) {}
                                      
void ApplicationPressed40_0(void) {

    DigOutOff(eDigOut0);
    DigOutOff(eDigOut1);
    //DigOutOff(eDigOut2);
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
                                      
void ApplicationPressed41_0(void) {}
void ApplicationReleased41_0(void) {}

//Haustür Aussen
void ApplicationPressed41_1(void) {

    if (DigOutState(2) == true) {
        DigOutToggle(eDigOut2);
    } else {
        DigOutDelayedOff(eDigOut2, 60000);
    }
}

void ApplicationReleased41_1(void) {}
                                      
void ApplicationPressed42_0(void) {}
void ApplicationReleased42_0(void) {}
void ApplicationPressed42_1(void) {}
void ApplicationReleased42_1(void) {}
                                      
void ApplicationPressed43_0(void) {

    DigOutToggle(eDigOut30);  
}
void ApplicationReleased43_0(void) {}
void ApplicationPressed43_1(void) {}
void ApplicationReleased43_1(void) {}
    
//Rollo Esszimmer alle gleichzeitig	                                  
void ApplicationPressed44_0(void) {

    TShaderState state;

    ShaderGetState(eShader0, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader0, eShaderOpen);
    } else {
        ShaderSetAction(eShader0, eShaderStop);
    }

    ShaderGetState(eShader1, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader1, eShaderOpen);
    } else {
        ShaderSetAction(eShader1, eShaderStop);
    }

    ShaderGetState(eShader2, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader2, eShaderOpen);
    } else {
        ShaderSetAction(eShader2, eShaderStop);
    }
}
void ApplicationReleased44_0(void) {}
void ApplicationPressed44_1(void) {

    TShaderState state;

    ShaderGetState(eShader0, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader0, eShaderClose);
    } else {
        ShaderSetAction(eShader0, eShaderStop);
    } 

    ShaderGetState(eShader1, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader1, eShaderClose);
    } else {
        ShaderSetAction(eShader1, eShaderStop);
    }
    
    ShaderGetState(eShader2, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader2, eShaderClose);
    } else {
        ShaderSetAction(eShader2, eShaderStop);
    } 
}
void ApplicationReleased44_1(void) {}
                                      
void ApplicationPressed45_0(void) {}
void ApplicationReleased45_0(void) {}
void ApplicationPressed45_1(void) {}
void ApplicationReleased45_1(void) {}
                                      


// Rollo Wohnzimmer Alle
void ApplicationPressed46_0(void) {

    TShaderState state;

    ShaderGetState(eShader5, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader5, eShaderOpen);
    } else {
        ShaderSetAction(eShader5, eShaderStop);
    }

    ShaderGetState(eShader6, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader6, eShaderOpen);
    } else {
        ShaderSetAction(eShader6, eShaderStop);
    }

    ShaderGetState(eShader7, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader7, eShaderOpen);
    } else {
        ShaderSetAction(eShader7, eShaderStop);
    }
}

void ApplicationReleased46_0(void) {}
void ApplicationPressed46_1(void) {

    TShaderState state;

    ShaderGetState(eShader5, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader5, eShaderClose);
    } else {
        ShaderSetAction(eShader5, eShaderStop);
    } 

    ShaderGetState(eShader6, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader6, eShaderClose);
    } else {
        ShaderSetAction(eShader6, eShaderStop);
    }

    ShaderGetState(eShader7, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader7, eShaderClose);
    } else {
        ShaderSetAction(eShader7, eShaderStop);
    }
}
void ApplicationReleased46_1(void) {}

void ApplicationPressed47_0(void) {}
void ApplicationReleased47_0(void) {}
void ApplicationPressed47_1(void) {}
void ApplicationReleased47_1(void) {}
                                      
void ApplicationPressed48_0(void) {}
void ApplicationReleased48_0(void) {}
void ApplicationPressed48_1(void) {}
void ApplicationReleased48_1(void) {}

void ApplicationPressed49_0(void) {

    TShaderState state;

    ShaderGetState(eShader0, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader0, eShaderOpen);
    } else {
        ShaderSetAction(eShader0, eShaderStop);
    }
}
void ApplicationReleased49_0(void) {}
void ApplicationPressed49_1(void) {

    TShaderState state;

    ShaderGetState(eShader0, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader0, eShaderClose);
    } else {
        ShaderSetAction(eShader0, eShaderStop);
    } 
}
void ApplicationReleased49_1(void) {}
                                      
void ApplicationPressed50_0(void) {

    TShaderState state;

    ShaderGetState(eShader3, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader3, eShaderOpen);
    } else {
        ShaderSetAction(eShader3, eShaderStop);
    }
}
void ApplicationReleased50_0(void) {}
void ApplicationPressed50_1(void) {}
void ApplicationReleased50_1(void) {

    TShaderState state;

    ShaderGetState(eShader3, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader3, eShaderClose);
    } else {
        ShaderSetAction(eShader3, eShaderStop);
    } 
}
                                      
void ApplicationPressed51_0(void) {

    TShaderState state;

    ShaderGetState(eShader7, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader7, eShaderOpen);
    } else {
        ShaderSetAction(eShader7, eShaderStop);
    }
}
void ApplicationReleased51_0(void) {}
void ApplicationPressed51_1(void) {

    TShaderState state;

    ShaderGetState(eShader7, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader7, eShaderClose);
    } else {
        ShaderSetAction(eShader7, eShaderStop);
    }
}
void ApplicationReleased51_1(void) {}
                                      
void ApplicationPressed52_0(void) {

    TShaderState state;

    ShaderGetState(eShader5, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader5, eShaderOpen);
    } else {
        ShaderSetAction(eShader5, eShaderStop);
    }
}
void ApplicationReleased52_0(void) {}
void ApplicationPressed52_1(void) {

    TShaderState state;

    ShaderGetState(eShader5, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader5, eShaderClose);
    } else {
       ShaderSetAction(eShader5, eShaderStop);
    }
}
void ApplicationReleased52_1(void) {}
                                      
void ApplicationPressed53_0(void) {

    TShaderState state;

    ShaderGetState(eShader6, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader6, eShaderOpen);
    } else {
        ShaderSetAction(eShader6, eShaderStop);
    }
}
void ApplicationReleased53_0(void) {}
void ApplicationPressed53_1(void) {

    TShaderState state;

    ShaderGetState(eShader6, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader6, eShaderClose);
    } else {
        ShaderSetAction(eShader6, eShaderStop);
    }
}
void ApplicationReleased53_1(void) {}
                                      
void ApplicationPressed54_0(void) {}
void ApplicationReleased54_0(void) {}
void ApplicationPressed54_1(void) {}
void ApplicationReleased54_1(void) {}
                                      
void ApplicationPressed55_0(void) {

   TShaderState state;

   ShaderGetState(eShader1, &state);
   if (state == eShaderStopped) {
      ShaderSetAction(eShader1, eShaderOpen);
   } else {
      ShaderSetAction(eShader1, eShaderStop);
   } 
}
void ApplicationReleased55_0(void) {}
void ApplicationPressed55_1(void) {

    TShaderState state;

    ShaderGetState(eShader1, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader1, eShaderClose);
    } else {
        ShaderSetAction(eShader1, eShaderStop);
    } 
}
void ApplicationReleased55_1(void) {}
// Rollo Esszimmer Rechts
void ApplicationPressed56_0(void) {

    TShaderState state;

    ShaderGetState(eShader2, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader2, eShaderOpen);
    } else {
        ShaderSetAction(eShader2, eShaderStop);
    }
}

void ApplicationReleased56_0(void) {}
void ApplicationPressed56_1(void) {

    TShaderState state;

    ShaderGetState(eShader2, &state);
    if (state == eShaderStopped) {
        ShaderSetAction(eShader2, eShaderClose);
    } else {
        ShaderSetAction(eShader2, eShaderStop);
    } 
}

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
