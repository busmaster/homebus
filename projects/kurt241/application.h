/*-----------------------------------------------------------------------------
*  Application.h
*/
#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "button.h"

/*-----------------------------------------------------------------------------
*  Functions
*/
void ApplicationInit(void);
void ApplicationStart(void);
void ApplicationCheck(void);
const char *ApplicationVersion(void);      
void ApplicationEventButton(TButtonEvent *buttonEvent);      

/* vom Anwenderprogramm zu implementierende Funktionen */
/* die Zahl bedeutet Schaltermodulnummer/Tasternummer  */
/* z.B. 5_1: Funktion für Ereignis von Schaltermodul 5, Eingang 1 */

void ApplicationPressed1_0(void);
void ApplicationReleased1_0(void);
void ApplicationPressed1_1(void); 
void ApplicationReleased1_1(void);

void ApplicationPressed2_0(void);
void ApplicationReleased2_0(void);
void ApplicationPressed2_1(void);
void ApplicationReleased2_1(void);

void ApplicationPressed3_0(void);
void ApplicationReleased3_0(void);
void ApplicationPressed3_1(void);
void ApplicationReleased3_1(void);
                                     
void ApplicationPressed4_0(void);
void ApplicationReleased4_0(void);
void ApplicationPressed4_1(void);
void ApplicationReleased4_1(void);

void ApplicationPressed5_0(void);
void ApplicationReleased5_0(void);
void ApplicationPressed5_1(void);
void ApplicationReleased5_1(void);
                                     
void ApplicationPressed6_0(void);
void ApplicationReleased6_0(void);
void ApplicationPressed6_1(void);
void ApplicationReleased6_1(void);
                                     
void ApplicationPressed7_0(void);
void ApplicationReleased7_0(void);
void ApplicationPressed7_1(void);
void ApplicationReleased7_1(void);
                                     
void ApplicationPressed8_0(void);
void ApplicationReleased8_0(void);
void ApplicationPressed8_1(void);
void ApplicationReleased8_1(void);

void ApplicationPressed9_0(void);
void ApplicationReleased9_0(void);
void ApplicationPressed9_1(void);
void ApplicationReleased9_1(void);

void ApplicationPressed10_0(void); 
void ApplicationReleased10_0(void);
void ApplicationPressed10_1(void); 
void ApplicationReleased10_1(void);
                                     
void ApplicationPressed11_0(void); 
void ApplicationReleased11_0(void);
void ApplicationPressed11_1(void); 
void ApplicationReleased11_1(void);
                                     
void ApplicationPressed12_0(void); 
void ApplicationReleased12_0(void);
void ApplicationPressed12_1(void); 
void ApplicationReleased12_1(void);
                                      
void ApplicationPressed13_0(void);
void ApplicationReleased13_0(void);
void ApplicationPressed13_1(void);
void ApplicationReleased13_1(void);
                                      
void ApplicationPressed14_0(void);
void ApplicationReleased14_0(void);
void ApplicationPressed14_1(void);
void ApplicationReleased14_1(void);
                                      
void ApplicationPressed15_0(void);
void ApplicationReleased15_0(void);
void ApplicationPressed15_1(void);
void ApplicationReleased15_1(void);
                                      
void ApplicationPressed16_0(void);
void ApplicationReleased16_0(void);
void ApplicationPressed16_1(void);
void ApplicationReleased16_1(void);

void ApplicationPressed17_0(void);
void ApplicationReleased17_0(void);
void ApplicationPressed17_1(void);
void ApplicationReleased17_1(void);
                                      
void ApplicationPressed18_0(void);
void ApplicationReleased18_0(void);
void ApplicationPressed18_1(void);
void ApplicationReleased18_1(void);
                                      
void ApplicationPressed19_0(void);
void ApplicationReleased19_0(void);
void ApplicationPressed19_1(void);
void ApplicationReleased19_1(void);
                                      
void ApplicationPressed20_0(void);
void ApplicationReleased20_0(void);
void ApplicationPressed20_1(void);
void ApplicationReleased20_1(void);
                                      
void ApplicationPressed21_0(void);
void ApplicationReleased21_0(void);
void ApplicationPressed21_1(void);
void ApplicationReleased21_1(void);
                                      
void ApplicationPressed22_0(void);
void ApplicationReleased22_0(void);
void ApplicationPressed22_1(void);
void ApplicationReleased22_1(void);
                                      
void ApplicationPressed23_0(void);
void ApplicationReleased23_0(void);
void ApplicationPressed23_1(void);
void ApplicationReleased23_1(void);
                                      
void ApplicationPressed24_0(void);
void ApplicationReleased24_0(void);
void ApplicationPressed24_1(void);
void ApplicationReleased24_1(void);
                                      
void ApplicationPressed25_0(void);
void ApplicationReleased25_0(void);
void ApplicationPressed25_1(void);
void ApplicationReleased25_1(void);
                                      
void ApplicationPressed26_0(void);
void ApplicationReleased26_0(void);
void ApplicationPressed26_1(void);
void ApplicationReleased26_1(void);
                                      
void ApplicationPressed27_0(void);
void ApplicationReleased27_0(void);
void ApplicationPressed27_1(void);
void ApplicationReleased27_1(void);
                                      
void ApplicationPressed28_0(void);
void ApplicationReleased28_0(void);
void ApplicationPressed28_1(void);
void ApplicationReleased28_1(void);
                                      
void ApplicationPressed29_0(void);
void ApplicationReleased29_0(void);
void ApplicationPressed29_1(void);
void ApplicationReleased29_1(void);
                                      
void ApplicationPressed30_0(void);
void ApplicationReleased30_0(void);
void ApplicationPressed30_1(void);
void ApplicationReleased30_1(void);
                                      
void ApplicationPressed31_0(void);
void ApplicationReleased31_0(void);
void ApplicationPressed31_1(void);
void ApplicationReleased31_1(void);
                                      
void ApplicationPressed32_0(void);
void ApplicationReleased32_0(void);
void ApplicationPressed32_1(void);
void ApplicationReleased32_1(void);
    
void ApplicationPressed33_0(void);
void ApplicationReleased33_0(void);
void ApplicationPressed33_1(void);
void ApplicationReleased33_1(void);
                                      
void ApplicationPressed34_0(void);
void ApplicationReleased34_0(void);
void ApplicationPressed34_1(void);
void ApplicationReleased34_1(void);
                                      
void ApplicationPressed35_0(void);
void ApplicationReleased35_0(void);
void ApplicationPressed35_1(void);
void ApplicationReleased35_1(void);
                                      
void ApplicationPressed36_0(void);
void ApplicationReleased36_0(void);
void ApplicationPressed36_1(void);
void ApplicationReleased36_1(void);
                                      
void ApplicationPressed37_0(void);
void ApplicationReleased37_0(void);
void ApplicationPressed37_1(void);
void ApplicationReleased37_1(void);
                                      
void ApplicationPressed38_0(void);
void ApplicationReleased38_0(void);
void ApplicationPressed38_1(void);
void ApplicationReleased38_1(void);
                                      
void ApplicationPressed39_0(void);
void ApplicationReleased39_0(void);
void ApplicationPressed39_1(void);
void ApplicationReleased39_1(void);
                                      
void ApplicationPressed40_0(void);
void ApplicationReleased40_0(void);
void ApplicationPressed40_1(void);
void ApplicationReleased40_1(void);
                                      
void ApplicationPressed41_0(void);
void ApplicationReleased41_0(void);
void ApplicationPressed41_1(void);
void ApplicationReleased41_1(void);
                                      
void ApplicationPressed42_0(void);
void ApplicationReleased42_0(void);
void ApplicationPressed42_1(void);
void ApplicationReleased42_1(void);
                                      
void ApplicationPressed43_0(void);
void ApplicationReleased43_0(void);
void ApplicationPressed43_1(void);
void ApplicationReleased43_1(void);
                                      
void ApplicationPressed44_0(void);
void ApplicationReleased44_0(void);
void ApplicationPressed44_1(void);
void ApplicationReleased44_1(void);
                                      
void ApplicationPressed45_0(void);
void ApplicationReleased45_0(void);
void ApplicationPressed45_1(void);
void ApplicationReleased45_1(void);
                                      
void ApplicationPressed46_0(void);
void ApplicationReleased46_0(void);
void ApplicationPressed46_1(void);
void ApplicationReleased46_1(void);
                                      
void ApplicationPressed47_0(void);
void ApplicationReleased47_0(void);
void ApplicationPressed47_1(void);
void ApplicationReleased47_1(void);
                                      
void ApplicationPressed48_0(void);
void ApplicationReleased48_0(void);
void ApplicationPressed48_1(void);
void ApplicationReleased48_1(void);

void ApplicationPressed49_0(void);
void ApplicationReleased49_0(void);
void ApplicationPressed49_1(void);
void ApplicationReleased49_1(void);
                                      
void ApplicationPressed50_0(void);
void ApplicationReleased50_0(void);
void ApplicationPressed50_1(void);
void ApplicationReleased50_1(void);
                                      
void ApplicationPressed51_0(void);
void ApplicationReleased51_0(void);
void ApplicationPressed51_1(void);
void ApplicationReleased51_1(void);
                                      
void ApplicationPressed52_0(void);
void ApplicationReleased52_0(void);
void ApplicationPressed52_1(void);
void ApplicationReleased52_1(void);
                                      
void ApplicationPressed53_0(void);
void ApplicationReleased53_0(void);
void ApplicationPressed53_1(void);
void ApplicationReleased53_1(void);
                                      
void ApplicationPressed54_0(void);
void ApplicationReleased54_0(void);
void ApplicationPressed54_1(void);
void ApplicationReleased54_1(void);
                                      
void ApplicationPressed55_0(void);
void ApplicationReleased55_0(void);
void ApplicationPressed55_1(void);
void ApplicationReleased55_1(void);
                                      
void ApplicationPressed56_0(void);
void ApplicationReleased56_0(void);
void ApplicationPressed56_1(void);
void ApplicationReleased56_1(void);
                                      
void ApplicationPressed57_0(void);
void ApplicationReleased57_0(void);
void ApplicationPressed57_1(void);
void ApplicationReleased57_1(void);
                                      
void ApplicationPressed58_0(void);
void ApplicationReleased58_0(void);
void ApplicationPressed58_1(void);
void ApplicationReleased58_1(void);
                                      
void ApplicationPressed59_0(void);
void ApplicationReleased59_0(void);
void ApplicationPressed59_1(void);
void ApplicationReleased59_1(void);
                                      
void ApplicationPressed60_0(void);
void ApplicationReleased60_0(void);
void ApplicationPressed60_1(void);
void ApplicationReleased60_1(void);
                                      
void ApplicationPressed61_0(void);
void ApplicationReleased61_0(void);
void ApplicationPressed61_1(void);
void ApplicationReleased61_1(void);
                                      
void ApplicationPressed62_0(void);
void ApplicationReleased62_0(void);
void ApplicationPressed62_1(void);
void ApplicationReleased62_1(void);
                                      
void ApplicationPressed63_0(void);
void ApplicationReleased63_0(void);
void ApplicationPressed63_1(void);
void ApplicationReleased63_1(void);
                                      
void ApplicationPressed64_0(void);
void ApplicationReleased64_0(void);
void ApplicationPressed64_1(void);
void ApplicationReleased64_1(void);
  
#endif
