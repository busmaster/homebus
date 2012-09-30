/*-----------------------------------------------------------------------------
*  digout.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <avr/pgmspace.h>

#include "sysdef.h"
#include "board.h"
#include "digout.h"

/*-----------------------------------------------------------------------------
*  Macros
*/                     
/* Zeitdauer für Rollladenansteuerung */
#define SHADE_DELAY        30000 /* ms */       

/* Zeitdauer, um die dirSwitch und onSwitch verzögert geschaltet werden */
/* (dirSwitch und onSwitch schalten um diese Diff verzögert) */
#define SHADE_DELAY_DIFF   200  /* ms */


/*-----------------------------------------------------------------------------
*  typedefs
*/

typedef void (* TFuncOn)(void);
typedef void (* TFuncOff)(void);
typedef bool (* TFuncState)(void);
typedef void (* TFuncToggle)(void);

typedef struct {
   TFuncOn     fOn;
   TFuncOff    fOff;
   TFuncState  fState;
   TFuncToggle fToggle;
} TAccessFunc;

typedef enum {
   eDigOutNoDelay,
   eDigOutDelayOn,
   eDigOutDelayOff,
   eDigOutDelayOnOff
} TDelayState;
 
typedef struct {
   uint32_t       startTimeMs;
   uint32_t       onDelayMs; 
   uint32_t       offDelayMs;
   TDelayState  state;
   bool         shadeFunction;
} TDigoutDesc;

typedef struct {
   TDigOutNumber onSwitch;
   TDigOutNumber dirSwitch;
} TShadeDesc;

/*-----------------------------------------------------------------------------
*  Functions
*/
#include "DigOut_l.h"

/*-----------------------------------------------------------------------------
*  Variables
*/
static const TAccessFunc sDigOutFuncs[NUM_DIGOUT] PROGMEM = {
   {On0,  Off0,  State0,  Toggle0},
   {On1,  Off1,  State1,  Toggle1},
   {On2,  Off2,  State2,  Toggle2},
   {On3,  Off3,  State3,  Toggle3},
   {On4,  Off4,  State4,  Toggle4},
   {On5,  Off5,  State5,  Toggle5},
   {On6,  Off6,  State6,  Toggle6},
   {On7,  Off7,  State7,  Toggle7},
   {On8,  Off8,  State8,  Toggle8},
   {On9,  Off9,  State9,  Toggle9},
   {On10, Off10, State10, Toggle10},
   {On11, Off11, State11, Toggle11},
   {On12, Off12, State12, Toggle12},
   {On13, Off13, State13, Toggle13},
   {On14, Off14, State14, Toggle14},
   {On15, Off15, State15, Toggle15},
   {On16, Off16, State16, Toggle16},
   {On17, Off17, State17, Toggle17},
   {On18, Off18, State18, Toggle18},
   {On19, Off19, State19, Toggle19},
   {On20, Off20, State20, Toggle20},
   {On21, Off21, State21, Toggle21},
   {On22, Off22, State22, Toggle22},
   {On23, Off23, State23, Toggle23},
   {On24, Off24, State24, Toggle24},
   {On25, Off25, State25, Toggle25},
   {On26, Off26, State26, Toggle26},
   {On27, Off27, State27, Toggle27},
   {On28, Off28, State28, Toggle28},
   {On29, Off29, State29, Toggle29},
   {On30, Off30, State30, Toggle30}
};

static TDigoutDesc sState[eDigOutNum];
static TShadeDesc  sShade[eDigOutShadeNum];
          
/*-----------------------------------------------------------------------------
*  Initialisierung
*/
void DigOutInit(void) {
   uint8_t i;
   TDigoutDesc *pState;
   TShadeDesc  *pShade;

   pState = sState;
   for (i = 0; i < NUM_DIGOUT; i++) {
      pState->state = eDigOutNoDelay;
      pState->shadeFunction = false;
      pState++;
   }                   
   pShade = sShade;
   for (i = 0; i < eDigOutShadeNum; i++) {
      pShade->onSwitch = eDigOutInvalid;
      pShade->dirSwitch = eDigOutInvalid;
      pShade++;                   
   }
}
/*-----------------------------------------------------------------------------
*  Ausgang einschalten
*/
void DigOutOn(TDigOutNumber number) {

   TFuncOn fOn = (TFuncOn)pgm_read_word(&sDigOutFuncs[number].fOn);
   fOn();
}

/*-----------------------------------------------------------------------------
*  Ausgang ausschalten
*/
void DigOutOff(TDigOutNumber number) {

   TFuncOff fOff = (TFuncOff)pgm_read_word(&sDigOutFuncs[number].fOff);
   fOff();
}

/*-----------------------------------------------------------------------------
*  alle Ausgänge ausschalten
*/
void DigOutOffAll(void) {     
   uint8_t i;

   for (i = 0; i < NUM_DIGOUT; i++) {
      DigOutOff(i);
   }
}

/*-----------------------------------------------------------------------------
*  Ausgangszustand lesen
*/
bool DigOutState(TDigOutNumber number) {

   TFuncState fState = (TFuncState)pgm_read_word(&sDigOutFuncs[number].fState);
   return fState();
}

/*-----------------------------------------------------------------------------
*  alle Ausgangszustände lesen
*/
void DigOutStateAll(uint8_t *pBuf, uint8_t bufLen) {
   uint8_t i;
   uint8_t maxIdx;

   memset(pBuf, 0, bufLen);
   maxIdx = min(bufLen * 8, NUM_DIGOUT);
   for (i = 0; i < maxIdx; i++) {
      if (DigOutState(i) == true) {
         *(pBuf + i / 8) |= 1 << (i % 8);
      }
   }
}

/*-----------------------------------------------------------------------------
*  alle Ausgangszustände lesen
*  Ausgangzustände von Aausgängen mit Sonderfunktion (Shade, Delay-Zustand)
*  werden mit Zustand 0 (= ausgeschaltet) gemeldet
*  Diese Ausgangszustände werden beim Powerfail im EEPROM gespeichert
*/
void DigOutStateAllStandard(uint8_t *pBuf, uint8_t bufLen) {
   uint8_t i;
   TDigoutDesc *pState;  
   uint8_t maxIdx;

   pState = sState;
   memset(pBuf, 0, bufLen);
   maxIdx = min(bufLen * 8, NUM_DIGOUT);
   for (i = 0; i < maxIdx; i++) {   
      if (pState->state == eDigOutNoDelay) {
         /* nur Zustand von Standardausgang ist interessant */
         if (DigOutState(i) == true) {
            *(pBuf + i / 8) |= 1 << (i % 8);
         }
      }
      pState++;
   }
}

/*-----------------------------------------------------------------------------
*  alle Ausgangszustände setzen (mit zeitverzögerung zwischen eingeschalteten
*  Ausgängen zur Verringerung von Einschaltstromspitzen)
*/
void DigOutAll(uint8_t *pBuf, uint8_t bufLen) {
   uint8_t i;
   uint8_t maxIdx;

   maxIdx = min(bufLen * 8, NUM_DIGOUT);
   for (i = 0; i < maxIdx; i++) {
      if ((*(pBuf + i / 8) & (1 << (i % 8))) != 0) { 
         DigOutOn(i);
         /* Verzögerung nur bei eingeschalteten Ausgängen */
         DELAY_MS(200);
      } else {
         DigOutOff(i);
      }      
   }
}


/*-----------------------------------------------------------------------------
*  Ausgang wechseln
*/
void DigOutToggle(TDigOutNumber number) {

   TFuncToggle fToggle = (TFuncToggle)pgm_read_word(&sDigOutFuncs[number].fToggle);
   fToggle();
}

/*-----------------------------------------------------------------------------
*  Ausgang mit Verzögerung einschalten
*/
void DigOutDelayedOn(TDigOutNumber number, uint32_t onDelayMs) {
   sState[number].state = eDigOutDelayOn;
   sState[number].onDelayMs = onDelayMs;   
   GET_TIME_MS32(sState[number].startTimeMs);
}

/*-----------------------------------------------------------------------------
*  Ausgang mit Verzögerung ausschalten
*/
void DigOutDelayedOff(TDigOutNumber number, uint32_t offDelayMs) {

   DigOutOn(number);
   sState[number].state = eDigOutDelayOff;
   sState[number].offDelayMs = offDelayMs;
   GET_TIME_MS32(sState[number].startTimeMs);
}

/*-----------------------------------------------------------------------------
*  Ausgang mit Verzögerung ein- und ausschalten        
*  Die Ausschaltzeitverzögerung beginnt nach der Einschaltzeitverzögerung zu
*  laufen.
*/
void DigOutDelayedOnDelayedOff(TDigOutNumber number, uint32_t onDelayMs, uint32_t offDelayMs) {
   sState[number].state = eDigOutDelayOnOff;
   sState[number].onDelayMs = onDelayMs;
   sState[number].offDelayMs = offDelayMs;
   GET_TIME_MS32(sState[number].startTimeMs);
   /* startTimeMs für das Ausschalten wird beim Einschalten gesetzt */
}

/*-----------------------------------------------------------------------------
*  Zuordnung der Ausgänge für einen Rollladen
*  onSwitch schaltet das Stromversorgungsrelais
*  dirSwitch schaltet das Richtungsrelais
*/
void DigOutShadeConfig(TDigOutShadeNumber number, 
                       TDigOutNumber onSwitch, 
                       TDigOutNumber dirSwitch) {
   sShade[number].onSwitch = onSwitch;
   sState[onSwitch].shadeFunction = true;
   sShade[number].dirSwitch = dirSwitch;
   sState[dirSwitch].shadeFunction = true;
}

/*-----------------------------------------------------------------------------
*  Zuordnung der Ausgänge für einen Rollladen lesen
*/
void DigOutShadeGetConfig(TDigOutShadeNumber number, TDigOutNumber *pOnSwitch, TDigOutNumber *pDirSwitch) {

   *pOnSwitch = sShade[number].onSwitch;
   *pDirSwitch = sShade[number].dirSwitch;
}


/*-----------------------------------------------------------------------------
*  Abfrage, ob Ausgäng für Rollladenfunktion konfiguriert ist
*/
bool DigOutShadeFunction(TDigOutNumber number) {
   return sState[number].shadeFunction;
}

/*-----------------------------------------------------------------------------
*  Ausgang für Rollladen einschalten
*  Schaltet sich nach 30 s automatisch ab
*/
void DigOutShade(TDigOutShadeNumber number, TDigOutShadeAction action) {
   TDigOutNumber onSwitch;
   TDigOutNumber dirSwitch;

   onSwitch = sShade[number].onSwitch;
   dirSwitch = sShade[number].dirSwitch;
   if ((onSwitch == eDigOutInvalid) ||
       (dirSwitch == eDigOutInvalid)) {
      return;
   }

   switch (action) {
      case eDigOutShadeOpen:
         /* Richtungsrelais soll sich ebenfalls ausschalten        */
         /* mit größerer Verzögerung, damit sichergestellt ist, dass  */  
         /* nicht bei abschalten kurz in die andere Richtung gefahren */
         /* wird */
         /* 1. Strom (on) AUS */
         /* 2. Richtung (dir) EIN mit Verzögerung */
         /* 3. Strom (on) EIN mit Verzögerung kurz nach Richtung EIN */
         /* 4. Warten (30 s) */
         /* 5. Strom (on) AUS */
         /* 6. Richtung (dir) AUS kurz nach Strom AUS */
         DigOutOff(onSwitch);
         DigOutDelayedOnDelayedOff(dirSwitch, SHADE_DELAY_DIFF, SHADE_DELAY + SHADE_DELAY_DIFF * 2);
         DigOutDelayedOnDelayedOff(onSwitch, SHADE_DELAY_DIFF * 2, SHADE_DELAY);
         break;
      case eDigOutShadeClose:
         /* 1, Strom (on) AUS */
         /* 2. Richtung (dir) AUS mit Verzögerung falls vorher EIN) */
         /* 3. Strom (on) EIN mit Verzögerung kurz nach Richtung AUS */
         /* 4. Warten (30 s) */
         /* 5. Strom (on) AUS */
         DigOutOff(onSwitch);
         if (DigOutState(dirSwitch) == true) {
            DigOutDelayedOff(dirSwitch, SHADE_DELAY_DIFF);
         }
         DigOutDelayedOnDelayedOff(onSwitch, SHADE_DELAY_DIFF * 2, SHADE_DELAY);
         break;
      case eDigOutShadeStop:
         /* 1. Strom (on) AUS */
         /* 2. Richtung (dir) AUS mit Verzögerung falls vorher EIN */
         /* Ausschaltverzögerung deaktivieren und sofort abschalten */
         DigOutOff(onSwitch);
         sState[onSwitch].state = eDigOutNoDelay;
         /* dirSwitch verzögert abschalten */
         if (DigOutState(dirSwitch) == true) {
            DigOutDelayedOff(dirSwitch, SHADE_DELAY_DIFF);
         }
         break;
      default:
         break;
   }
}

/*-----------------------------------------------------------------------------
*  Ermittlung, ob Rollladenausgang aktiv (fährt gerade AUF oder ZU)
*  Wird verwendet, um bei Tastenbetätigung zwischen AUF/ZU und STOP unter-
*  scheiden zu können
*/
bool DigOutShadeState(TDigOutShadeNumber number, TDigOutShadeAction *pAction) {

   TDigOutNumber onSwitch;
   TDigOutNumber dirSwitch;

   onSwitch = sShade[number].onSwitch;
   dirSwitch = sShade[number].dirSwitch;
   if ((onSwitch == eDigOutInvalid) ||
       (dirSwitch == eDigOutInvalid)) {
      return false;
   }

   if (DigOutState(onSwitch) == false) {
      /* abgeschaltet*/
      *pAction = eDigOutShadeStop;
   } else if (DigOutState(dirSwitch) == false) {
      *pAction = eDigOutShadeClose;
   } else {
      *pAction = eDigOutShadeOpen;
   }
   return true;
}

/*-----------------------------------------------------------------------------
*  Statemachine für zeitgesteuerung Aktionen
*  mit jedem Aufruf wird ein Ausgang bearbeitet
*/
void DigOutStateCheck(void) {
   static uint8_t  sDigoutNum = 0;   
   TDigoutDesc   *pState; 
   TDelayState   delayState;
   uint32_t        actualTime;

   GET_TIME_MS32(actualTime);
   pState = &sState[sDigoutNum];
   delayState = pState->state;
   if (delayState == eDigOutNoDelay) {
      /* nichts zu tun */
   } else if (delayState == eDigOutDelayOn) {
      if (((uint32_t)(actualTime - pState->startTimeMs)) >= pState->onDelayMs) {
         DigOutOn(sDigoutNum);
         delayState = eDigOutNoDelay;
      }
   } else if (delayState == eDigOutDelayOff) {
      if (((uint32_t)(actualTime - pState->startTimeMs)) >= pState->offDelayMs) {
         DigOutOff(sDigoutNum);
         delayState = eDigOutNoDelay;
      }                                                   
   } else if (delayState == eDigOutDelayOnOff) {
      if (DigOutState(sDigoutNum) == false) {
         /* einschaltverzögerung aktiv*/
         if (((uint32_t)(actualTime - pState->startTimeMs)) >= pState->onDelayMs) {
             DigOutOn(sDigoutNum);
             /* ab jetzt beginnt die Ausschaltverzögerung */
             pState->startTimeMs = actualTime;
         }
      } else {
         if (((uint32_t)(actualTime - pState->startTimeMs)) >= pState->offDelayMs) {
            DigOutOff(sDigoutNum);
            delayState = eDigOutNoDelay;
         }                                                   
      }
   }
   sDigoutNum++;
   if (sDigoutNum >= NUM_DIGOUT) {
      sDigoutNum = 0;
   }
   pState->state = delayState;
}


/*-----------------------------------------------------------------------------
*  Zugriffsfunktion für die Umsetzung auf die Nummer des Ausgangs
*/
static void On0(void) {
   DIGOUT_0_ON;
}
static void Off0(void) {
   DIGOUT_0_OFF;
}
static bool State0(void) {
   return DIGOUT_0_STATE;
}
static void Toggle0(void) {
   DIGOUT_0_TOGGLE;
}                  

static void On1(void) {
   DIGOUT_1_ON;
}
static void Off1(void) {
   DIGOUT_1_OFF;
}
static bool State1(void) {
   return DIGOUT_1_STATE;
}
static void Toggle1(void) {
   DIGOUT_1_TOGGLE;
}

static void On2(void) {
   DIGOUT_2_ON;
}
static void Off2(void) {
   DIGOUT_2_OFF;
}
static bool State2(void) {
   return DIGOUT_2_STATE;
}
static void Toggle2(void) {
   DIGOUT_2_TOGGLE;
}

static void On3(void) {
   DIGOUT_3_ON;
}
static void Off3(void) {
   DIGOUT_3_OFF;
}
static bool State3(void) {
   return DIGOUT_3_STATE;
}
static void Toggle3(void) {
   DIGOUT_3_TOGGLE;
}

static void On4(void) {
   DIGOUT_4_ON;
}
static void Off4(void) {
   DIGOUT_4_OFF;
}
static bool State4(void) {
   return DIGOUT_4_STATE;
}
static void Toggle4(void) {
   DIGOUT_4_TOGGLE;
}

static void On5(void) {
   DIGOUT_5_ON;
}
static void Off5(void) {
   DIGOUT_5_OFF;
}
static bool State5(void) {
   return DIGOUT_5_STATE;
}
static void Toggle5(void) {
   DIGOUT_5_TOGGLE;
}

static void On6(void) {
   DIGOUT_6_ON;
}
static void Off6(void) {
   DIGOUT_6_OFF;
}
static bool State6(void) {
   return DIGOUT_6_STATE;
}
static void Toggle6(void) {
   DIGOUT_6_TOGGLE;
}

static void On7(void) {
   DIGOUT_7_ON;
}
static void Off7(void) {
   DIGOUT_7_OFF;
}
static bool State7(void) {
   return DIGOUT_7_STATE;
}
static void Toggle7(void) {
   DIGOUT_7_TOGGLE;
}

static void On8(void) {
   DIGOUT_8_ON;
}
static void Off8(void) {
   DIGOUT_8_OFF;
}
static bool State8(void) {
   return DIGOUT_8_STATE;
}
static void Toggle8(void) {
   DIGOUT_8_TOGGLE;
}

static void On9(void) {
   DIGOUT_9_ON;
}
static void Off9(void) {
   DIGOUT_9_OFF;
}
static bool State9(void) {
   return DIGOUT_9_STATE;
}
static void Toggle9(void) {
   DIGOUT_9_TOGGLE;
}

static void On10(void) {
   DIGOUT_10_ON;
}
static void Off10(void) {
   DIGOUT_10_OFF;
}
static bool State10(void) {
   return DIGOUT_10_STATE;
}
static void Toggle10(void) {
   DIGOUT_10_TOGGLE;
}

static void On11(void) {
   DIGOUT_11_ON;
}
static void Off11(void) {
   DIGOUT_11_OFF;
}
static bool State11(void) {
   return DIGOUT_11_STATE;
}
static void Toggle11(void) {
   DIGOUT_11_TOGGLE;
}

static void On12(void) {
   DIGOUT_12_ON;
}
static void Off12(void) {
   DIGOUT_12_OFF;
}
static bool State12(void) {
   return DIGOUT_12_STATE;
}
static void Toggle12(void) {
   DIGOUT_12_TOGGLE;
}

static void On13(void) {
   DIGOUT_13_ON;
}
static void Off13(void) {
   DIGOUT_13_OFF;
}
static bool State13(void) {
   return DIGOUT_13_STATE;
}
static void Toggle13(void) {
   DIGOUT_13_TOGGLE;
}

static void On14(void) {
   DIGOUT_14_ON;
}
static void Off14(void) {
   DIGOUT_14_OFF;
}
static bool State14(void) {
   return DIGOUT_14_STATE;
}
static void Toggle14(void) {
   DIGOUT_14_TOGGLE;
}

static void On15(void) {
   DIGOUT_15_ON;
}
static void Off15(void) {
   DIGOUT_15_OFF;
}
static bool State15(void) {
   return DIGOUT_15_STATE;
}
static void Toggle15(void) {
   DIGOUT_15_TOGGLE;
}

static void On16(void) {
   DIGOUT_16_ON;
}
static void Off16(void) {
   DIGOUT_16_OFF;
}
static bool State16(void) {
   return DIGOUT_16_STATE;
}
static void Toggle16(void) {
   DIGOUT_16_TOGGLE;
}

static void On17(void) {
   DIGOUT_17_ON;
}
static void Off17(void) {
   DIGOUT_17_OFF;
}
static bool State17(void) {
   return DIGOUT_17_STATE;
}
static void Toggle17(void) {
   DIGOUT_17_TOGGLE;
}

static void On18(void) {
   DIGOUT_18_ON;
}
static void Off18(void) {
   DIGOUT_18_OFF;
}
static bool State18(void) {
   return DIGOUT_18_STATE;
}
static void Toggle18(void) {
   DIGOUT_18_TOGGLE;
}

static void On19(void) {
   DIGOUT_19_ON;
}
static void Off19(void) {
   DIGOUT_19_OFF;
}
static bool State19(void) {
   return DIGOUT_19_STATE;
}
static void Toggle19(void) {
   DIGOUT_19_TOGGLE;
}

static void On20(void) {
   DIGOUT_20_ON;
}
static void Off20(void) {
   DIGOUT_20_OFF;
}
static bool State20(void) {
   return DIGOUT_20_STATE;
}
static void Toggle20(void) {
   DIGOUT_20_TOGGLE;
}

static void On21(void) {
   DIGOUT_21_ON;
}
static void Off21(void) {
   DIGOUT_21_OFF;
}
static bool State21(void) {
   return DIGOUT_21_STATE;
}
static void Toggle21(void) {
   DIGOUT_21_TOGGLE;
}

static void On22(void) {
   DIGOUT_22_ON;
}
static void Off22(void) {
   DIGOUT_22_OFF;
}
static bool State22(void) {
   return DIGOUT_22_STATE;
}
static void Toggle22(void) {
   DIGOUT_22_TOGGLE;
}

static void On23(void) {
   DIGOUT_23_ON;
}
static void Off23(void) {
   DIGOUT_23_OFF;
}
static bool State23(void) {
   return DIGOUT_23_STATE;
}
static void Toggle23(void) {
   DIGOUT_23_TOGGLE;
}

static void On24(void) {
   DIGOUT_24_ON;
}
static void Off24(void) {
   DIGOUT_24_OFF;
}
static bool State24(void) {
   return DIGOUT_24_STATE;
}
static void Toggle24(void) {
   DIGOUT_24_TOGGLE;
}

static void On25(void) {
   DIGOUT_25_ON;
}
static void Off25(void) {
   DIGOUT_25_OFF;
}
static bool State25(void) {
   return DIGOUT_25_STATE;
}
static void Toggle25(void) {
   DIGOUT_25_TOGGLE;
}

static void On26(void) {
   DIGOUT_26_ON;
}
static void Off26(void) {
   DIGOUT_26_OFF;
}
static bool State26(void) {
   return DIGOUT_26_STATE;
}
static void Toggle26(void) {
   DIGOUT_26_TOGGLE;
}

static void On27(void) {
   DIGOUT_27_ON;
}
static void Off27(void) {
   DIGOUT_27_OFF;
}
static bool State27(void) {
   return DIGOUT_27_STATE;
}
static void Toggle27(void) {
   DIGOUT_27_TOGGLE;
}

static void On28(void) {
   DIGOUT_28_ON;
}
static void Off28(void) {
   DIGOUT_28_OFF;
}
static bool State28(void) {
   return DIGOUT_28_STATE;
}
static void Toggle28(void) {
   DIGOUT_28_TOGGLE;
}

static void On29(void) {
   DIGOUT_29_ON;
}
static void Off29(void) {
   DIGOUT_29_OFF;
}
static bool State29(void) {
   return DIGOUT_29_STATE;
}
static void Toggle29(void) {
   DIGOUT_29_TOGGLE;
}

static void On30(void) {
   DIGOUT_30_ON;
}
static void Off30(void) {
   DIGOUT_30_OFF;
}
static bool State30(void) {
   return DIGOUT_30_STATE;
}
static void Toggle30(void) {
   DIGOUT_30_TOGGLE;
}

