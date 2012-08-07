/*-----------------------------------------------------------------------------
*  DigOut.h
*/
#ifndef _DIGOUT_H
#define _DIGOUT_H

/*-----------------------------------------------------------------------------
*  Includes
*/
#include "Types.h"

/*-----------------------------------------------------------------------------
*  Macros
*/                     

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef enum {
   eDigOut0 = 0,
   eDigOut1 = 1,
   eDigOut2 = 2,
   eDigOut3 = 3,
   eDigOut4 = 4,
   eDigOut5 = 5,
   eDigOut6 = 6,
   eDigOut7 = 7,
   eDigOut8 = 8,
   eDigOut9 = 9,
   eDigOut10 = 10,
   eDigOut11 = 11,
   eDigOut12 = 12,
   eDigOut13 = 13,
   eDigOut14 = 14,
   eDigOut15 = 15,
   eDigOut16 = 16,
   eDigOut17 = 17,
   eDigOut18 = 18,
   eDigOut19 = 19,
   eDigOut20 = 20,
   eDigOut21 = 21,
   eDigOut22 = 22,
   eDigOut23 = 23,
   eDigOut24 = 24,
   eDigOut25 = 25,
   eDigOut26 = 26,
   eDigOut27 = 27,
   eDigOut28 = 28,
   eDigOut29 = 29,
   eDigOut30 = 30,
   eDigOutNum = 31,
   eDigOutInvalid = 255
} TDigOutNumber;       


typedef enum {
   eDigOutShade0 = 0,
   eDigOutShade1 = 1,
   eDigOutShade2 = 2,
   eDigOutShade3 = 3,
   eDigOutShade4 = 4,
   eDigOutShade5 = 5,
   eDigOutShade6 = 6,
   eDigOutShade7 = 7,
   eDigOutShade8 = 8,
   eDigOutShade9 = 9,
   eDigOutShade10 = 10,
   eDigOutShade11 = 11,
   eDigOutShade12 = 12,
   eDigOutShade13 = 13,
   eDigOutShade14 = 14,
   eDigOutShadeNum = 15,
   eDigOutShadeInvalid = 255
} TDigOutShadeNumber;

typedef enum {
   eDigOutShadeClose,
   eDigOutShadeOpen,
   eDigOutShadeStop,
   eDigOutShadeNone
} TDigOutShadeAction;


/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/
void DigOutInit(void);
void DigOutOn(TDigOutNumber number);
void DigOutOff(TDigOutNumber number);
void DigOutOffAll(void);
BOOL DigOutState(TDigOutNumber number);  
void DigOutStateAll(UINT8 *pBuf, UINT8 bufLen);
void DigOutStateAllStandard(UINT8 *pBuf, UINT8 bufLen);
void DigOutAll(UINT8 *pBuf, UINT8 bufLen);
void DigOutToggle(TDigOutNumber number);
void DigOutDelayedOn(TDigOutNumber number, UINT32 onDelayMs);
void DigOutDelayedOff(TDigOutNumber number, UINT32 offDelayMs);
void DigOutDelayedOnDelayedOff(TDigOutNumber number, UINT32 onDelayMs, UINT32 offDelayMs);
void DigOutShadeConfig(TDigOutShadeNumber number, TDigOutNumber onSwitch, TDigOutNumber dirSwitch);
void DigOutShadeGetConfig(TDigOutShadeNumber number, TDigOutNumber *pOnSwitch, TDigOutNumber *pDirSwitch);
BOOL DigOutShadeFunction(TDigOutNumber number);
void DigOutShade(TDigOutShadeNumber number, TDigOutShadeAction action);
BOOL DigOutShadeState(TDigOutShadeNumber number, TDigOutShadeAction *pAction);
void DigOutStateCheck(void);
  
#endif
