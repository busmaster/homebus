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

#include "sysdef.h"
#include "board.h"
#include "pwm.h"

/*-----------------------------------------------------------------------------
*  typedefs
*/

typedef struct {
    uint16_t pwm;
} TPwmDesc;

/*-----------------------------------------------------------------------------
*  Functions
*/

/*-----------------------------------------------------------------------------
*  Variables
*/
static TPwmDesc sState[NUM_PWM_CHANNEL];
          
/*-----------------------------------------------------------------------------
*  init
*/
void PwmInit(void) {

    /* timer 1: use oc1a, oc1b, oc1c */
    OCR1A = 0;
    OCR1B = 0;
    OCR1C = 0;
    /* fast pwm mode 10 bit, 
     * clear output on compare match, set on top,
     * clock prescaler 8 -> @1.8432MHz: 1843200 / 1024 / 8 = 225 Hz pwm
     */
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << COM1C1) | (1 << WGM11) | (1 << WGM10);
    TCCR1B = (1 << WGM12) | (1 << CS11);

    /* timer 4: use oc4a */
    TC4H = 0;
    OCR4A = 0;

    /* count top 1023 */
    TC4H = 0x3;
    OCR4C = 0xff;

    /* fast pwm mode 
     * clear output on compare match, set on top,
     * clock prescaler 8 -> @1.8432MHz: 1843200 / 1024 / 8 = 225 Hz pwm
     */
    TCCR4A = (1 << COM4A1) | (1 << PWM4A);
    TCCR4B = (1 << CS42);
}

/*-----------------------------------------------------------------------------
*  exit - switch off all outputs
*/
void PwmExit(void) {
    
    TCCR1A = 0;
    TCCR4A = 0;
    memset(sState, 0, sizeof(sState));
}

/*-----------------------------------------------------------------------------
*  cyclic task
*/
void PwmCheck(void) {
}

/*-----------------------------------------------------------------------------
*  get channnel value
*/
bool PwmGet(TPwmNumber channel, uint16_t *pValue) {
    
    bool rc = true;
  
    switch (channel) {
    case ePwm0:
        *pValue = sState[0].pwm;
        break;
    case ePwm1:
        *pValue = sState[1].pwm;
        break;
    case ePwm2:
        *pValue = sState[2].pwm;
        break;
    case ePwm3:
        *pValue = sState[3].pwm;
        break;
    default:
        rc = false;
        break;
    }
    
    return rc;
}

/*-----------------------------------------------------------------------------
*  get all channels actual value
*/
bool PwmGetAll(uint16_t *buf, uint8_t buf_size) {
    
    uint8_t i;
    
    if (buf_size < sizeof(sState)) {
        return false;
    }
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        *(buf + i) = sState[i].pwm;
    }
    
    return true;
}

/*-----------------------------------------------------------------------------
*  set immediate
*/
bool PwmSet(TPwmNumber channel, uint16_t value) {
    
    bool rc = true;
    
    /* todo: use compare match interrupt to update oc register */
    
    switch (channel) {
    case ePwm0:
        OCR1C = value;
        sState[0].pwm = value;
        break;
    case ePwm1:
        TC4H = (value & 0x3ff) >> 8;
        OCR4A = value;
        sState[1].pwm = value;
        break;
    case ePwm2:
        OCR1B = value;
        sState[2].pwm = value;
        break;
    case ePwm3:
        OCR1A = value;
        sState[3].pwm = value;
        break;
    default:
        rc = false;
        break;
    }
    
    return rc;
}




