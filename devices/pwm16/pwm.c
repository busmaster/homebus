/*
 * pwm.c
 * 
 * Copyright 2016 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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
#include "pwm.h"
#include "pca9685.h"
#include "i2cmaster.h"
#include "math.h"



/*-----------------------------------------------------------------------------
*  Functions
*/

/*-----------------------------------------------------------------------------
*  Variables
*/
static TPwmDesc sState[NUM_PWM_CHANNEL];

static uint16_t pwmtable_256[256] =  {
   0, 11, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 17, 17, 17, 18, 18, 19,
   19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29, 29, 30,
   31, 32, 32, 33, 34, 35, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
   50, 51, 52, 54, 55, 56, 58, 59, 60, 62, 63, 65, 66, 68, 69, 71, 72, 74, 76, 78, 79,
   81, 83, 85, 87, 89, 91, 93, 96, 98, 100, 102, 105, 107, 110, 112, 115, 118, 120, 123,
   126, 129, 132, 135, 138, 141, 145, 148, 152, 155, 159, 162, 166, 170, 174, 178, 182,
   186, 191, 195, 200, 204, 209, 214, 219, 224, 229, 235, 240, 246, 252, 258, 264, 270,
   276, 282, 289, 296, 303, 310, 317, 324, 332, 340, 347, 356, 364, 372, 381, 390, 399,
   408, 418, 428, 438, 448, 458, 469, 480, 491, 503, 514, 526, 538, 551, 564, 577, 591,
   604, 618, 633, 648, 663, 678, 694, 710, 727, 744, 761, 779, 797, 815, 834, 854, 874,
   894, 915, 936, 958, 981, 1003, 1027, 1051, 1075, 1100, 1126, 1152, 1179, 1207, 1235,
   1264, 1293, 1323, 1354, 1386, 1418, 1451, 1485, 1520, 1555, 1591, 1628, 1666, 1705,
   1745, 1786, 1827, 1870, 1914, 1958, 2004, 2051, 2098, 2147, 2197, 2249, 2301, 2355,
   2410, 2466, 2523, 2582, 2643, 2704, 2767, 2832, 2898, 2965, 3035, 3105, 3178, 3252,
   3328, 3405, 3485, 3566, 3649, 3734, 3821, 3910, 4002, 4095
};

/*-----------------------------------------------------------------------------
*  init
*/
void PwmInit(void) {
	
    uint8_t channel;	
	
	i2c_init();
    pca9685_init();

	for (channel=0; channel < NUM_PWM_CHANNEL; channel++) {
       sState[channel].state = 0;
	   sState[channel].state = ePwmNoFade;
	   PwmOn(channel, false);
	}
	PWM_ALLOUT_ENABLE;
}

/*-----------------------------------------------------------------------------
*  exit - switch off all outputs
*/
void PwmExit(void) {
	
    setAllPWMOff();
}

/*-----------------------------------------------------------------------------
*  cyclic task
*/
void PwmCheck(void) {
	
   uint8_t sChannelNum = 0;  
   TPwmDesc       *pState; 
   TFadeState     fadeState;
   uint32_t       actualTime;

   GET_TIME_MS32(actualTime);
   for(sChannelNum=0; sChannelNum<NUM_PWM_CHANNEL;sChannelNum++){
	   pState = &sState[sChannelNum];
	   fadeState = pState->state;
	   if (fadeState == ePwmNoFade) {
		  /* nichts zu tun */
	   } else if (fadeState == ePwmFadeUp) {
		  if ((uint32_t)(actualTime-pState->nextTimeMs) >= pState->deltaDelayMs) {
			  if(pState->actualStep+pState->deltaStep > 255) {
				  pState->actualStep = pState->fadeEnd;
			  } else {
			     pState->actualStep+=pState->deltaStep;
			  }
			  if (pState->actualStep >= pState->fadeEnd) {
				 fadeState = ePwmNoFade;
				 pState->pwm = pState->fadeEnd;             		 
			  } else {
				  pState->nextTimeMs+=pState->deltaDelayMs;
				  pState->pwm = pState->actualStep;
			  }
			  PwmUpdate(sChannelNum);
		  }                                               
	   } else if (fadeState == ePwmFadeDown) {
		  if ((uint32_t)(actualTime-pState->nextTimeMs) >= pState->deltaDelayMs) {
			  if(pState->actualStep > pState->deltaStep) {
				 pState->actualStep-=pState->deltaStep;
			  } else {
				  pState->actualStep = pState->fadeEnd;
			  }
			  if (pState->actualStep <= pState->fadeEnd) {
				 fadeState = ePwmNoFade;
				 pState->pwm = pState->fadeEnd;
                 if(pState->fadeEnd == 0 ) {
					 pState->on=false;
				 }        		 
			  } else {
				  pState->nextTimeMs+=pState->deltaDelayMs;
				  pState->pwm = pState->actualStep;
			  }
			  PwmUpdate(sChannelNum);
		  }                                               
	   } else if (fadeState == ePwmDelayOff) {
          if (((uint32_t)(actualTime - pState->startTimeMs)) >= pState->offDelayMs) {
             PwmOn(sChannelNum, false);
             fadeState = ePwmNoFade;
          }                                                   
       }
	   pState->state = fadeState;
   }
}

/*-----------------------------------------------------------------------------
*  get channnel value
*/
bool PwmGet(TPwmNumber channel, uint8_t *pValue) {
    
    bool rc;
    
    if ((channel >= 0) && (channel < NUM_PWM_CHANNEL)) {
        *pValue = sState[channel].pwm;
        rc = true;
    } else {
        rc = false;
    }
    
    return rc;
}

bool PwmIsOn(TPwmNumber channel, bool *pOn) {

    bool rc;
    
    if ((channel >= 0) && (channel < NUM_PWM_CHANNEL)) {
        *pOn = sState[channel].on;
        rc = true;
    } else {
        rc = false;
    }
    
    return rc;
}

/*-----------------------------------------------------------------------------
*  get all channels actual value
*/
bool PwmGetAll(uint8_t *buf, uint8_t buf_size) {
    
    uint8_t i;
    
    if (buf_size < (NUM_PWM_CHANNEL * sizeof(uint8_t))) {
        return false;
    }
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        *(buf + i) = sState[i].pwm;
    }

    return true;
}

/*-----------------------------------------------------------------------------
*  program the pwm value to the hardware
*/
bool ActivatePwm(TPwmNumber channel, uint8_t value) {
    
    bool rc = false;
    
	if((channel >= 0) && (channel < NUM_PWM_CHANNEL)) {
		setPWMDimmed((uint8_t)channel, readTable(value));
		rc = true;
    }
    return rc;
}

/*-----------------------------------------------------------------------------
*  set immediate if the channel is on
*/
bool PwmSet(TPwmNumber channel, uint8_t value) {
    
    bool rc = true;
    
    if ((channel >= 0) && (channel < NUM_PWM_CHANNEL)) {
        sState[channel].pwm = value;
        if (sState[channel].on) {
            ActivatePwm(channel, value);
        }
        rc = true;
    } else { 
        rc = false;
    }
    
    return rc;
}

/*-----------------------------------------------------------------------------
*  update immediate if the channel is on
*/
bool PwmUpdate(TPwmNumber channel) {
    
    bool rc = true;
    
    if ((channel >= 0) && (channel < NUM_PWM_CHANNEL)) {
        if (sState[channel].on) {
            ActivatePwm(channel, sState[channel].pwm);
        }
        rc = true;
    } else { 
        rc = false;
    }
    
    return rc;
}

/*-----------------------------------------------------------------------------
*  set on/off - do not change the pwm value
*/
bool PwmOn(TPwmNumber channel, bool on) {

    bool rc;
    
    if ((channel >= 0) && (channel < NUM_PWM_CHANNEL)) {
        sState[channel].on = on;
        if (on) {
            ActivatePwm(channel, sState[channel].pwm);
        } else {
            ActivatePwm(channel, 0);
        }
        rc = true;
    } else {
        rc = false;
    }
    
    return rc;
}

/*-----------------------------------------------------------------------------
*  PwmDimmStepTime
*/

bool PwmDimmStepTime(TPwmNumber channel, uint8_t fadeValueStart, uint8_t fadeValueStop, uint16_t fadeTime) {
	
	bool rc;	
	uint32_t fadeSteps=0;
	
    if ((channel >= 0) && (channel < NUM_PWM_CHANNEL)) {
		if (fadeValueStart != fadeValueStop) {
        
		   rc = true;
		
		   sState[channel].state = ePwmNoFade;
		   sState[channel].fadeEnd = fadeValueStop;
           sState[channel].deltaStep = 1;		
		   if (fadeValueStart > fadeValueStop) {
			   sState[channel].state = ePwmFadeDown;
			   fadeSteps = fadeValueStart - fadeValueStop;
		   	   sState[channel].actualStep = fadeValueStart;
		   } else if (fadeValueStop > fadeValueStart) {
			   sState[channel].state = ePwmFadeUp;
			   fadeSteps = fadeValueStop - fadeValueStart;
			   sState[channel].actualStep = fadeValueStart;
		   } 
		   if (fadeSteps > 0) {
		      if((fadeTime / fadeSteps) < 4) { // task wird nur alle 2ms aufgerufen -> zu viele Schritte
		         sState[channel].deltaStep = (4 * fadeSteps) / fadeTime;
		      }		
		      sState[channel].deltaDelayMs = fadeTime / (fadeSteps * sState[channel].deltaStep);	
		      sState[channel].on = true; 
		      sState[channel].pwmLastFade = sState[channel].pwm;
		      sState[channel].pwm = fadeValueStart;				
		      GET_TIME_MS32(sState[channel].nextTimeMs);
		      PwmUpdate(channel);
		   }
		} else {
			rc = true;
			
			sState[channel].state = ePwmNoFade;
			if(sState[channel].pwm > 0)  {
				sState[channel].on = true;
			} else {
				sState[channel].on = false;
			}
		}
    } else {
        rc = false;
    }	
	return rc;
}

bool PwmDimmTo(TPwmNumber channel, uint8_t fadeValueStop, uint16_t fadeTime) {
	
	bool rc;
	uint8_t fadeValueStart;
	
	fadeValueStart = sState[channel].pwm;
	rc = PwmDimmStepTime(channel, fadeValueStart, fadeValueStop, fadeTime);
	return rc;
}

uint16_t readTable(uint8_t value) {
	return pwmtable_256[value];
}

bool PwmToggle(TPwmNumber channel) {
	bool pOn;
	bool rc;

	rc = PwmIsOn(channel, &pOn);
	if(rc==false) {
		return rc;
	}
	if (pOn) {
	   rc = PwmOn(channel,false);
	} else {
	   rc = PwmOn(channel,true);
	}		
	return rc;
}

bool PwmGetAllState(uint8_t *buf, uint8_t buf_size) {
    
    uint8_t i;
    
    if (buf_size < (NUM_PWM_CHANNEL * sizeof(uint8_t))) {
        return false;
    }
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        *(buf + i) = sState[i].state;
    }

    return true;
}

/*-----------------------------------------------------------------------------
*  Ausgang mit Verzögerung ausschalten
*/
bool PwmDelayedOff(TPwmNumber channel, uint32_t offDelayMs) {
   bool rc;

   rc = PwmOn(channel, true);
   sState[channel].state = ePwmDelayOff;
   sState[channel].offDelayMs = offDelayMs;
   GET_TIME_MS32(sState[channel].startTimeMs);
   
   return rc;
}

/*-----------------------------------------------------------------------------
*  Status schreiben
*/
bool PwmSetState(TPwmNumber channel, TFadeState state) {
   
    if ((channel >= 0) && (channel < NUM_PWM_CHANNEL)) {
		sState[channel].state = state;
        return true;
    }

   return false;
}

bool PwmStarDimm(TPwmNumber channel) {
	bool rc = false;
	
    if ((channel >= 0) && (channel < NUM_PWM_CHANNEL)) {
	   if(sState[channel].on == false) {
		   rc = PwmDimmStepTime(channel, 0, 255, 5100);		   
		   sState[channel].pwmLastDir = 1;
	   }
	   else if((sState[channel].pwmLastDir != 0) && sState[channel].on) {
		   sState[channel].pwmLastDir = 0;
		   if(sState[channel].pwm > 30) {
		   rc = PwmDimmTo(channel, 30, 20UL * (sState[channel].pwm - 30));
		   }
	   } else {
		   sState[channel].pwmLastDir = 1;
		   rc = PwmDimmTo(channel, 255, 20UL * (255 - sState[channel].pwm));
	   }
    }

   return rc;
}