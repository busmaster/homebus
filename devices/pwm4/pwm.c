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
    bool     on;
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

    PwmSet(0, 0);
    PwmOn(0, false);
    PwmSet(1, 0);
    PwmOn(1, false);
    PwmSet(2, 0);
    PwmOn(2, false);
    PwmSet(3, 0);
    PwmOn(3, false);
}

/*-----------------------------------------------------------------------------
*  exit - switch off all outputs
*/
void PwmExit(void) {
    TCCR1A = 0;
    TCCR4A = 0;

    PwmOn(0, false);
    PwmOn(1, false);
    PwmOn(2, false);
    PwmOn(3, false);
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
bool PwmGetAll(uint16_t *buf, uint8_t buf_size) {
    
    uint8_t i;
    
    if (buf_size < (NUM_PWM_CHANNEL * sizeof(uint16_t))) {
        return false;
    }
    for (i = 0; i < NUM_PWM_CHANNEL; i++) {
        if (sState[i].on) {
            *(buf + i) = sState[i].pwm;
        } else {
            *(buf + i) = 0;
        }
    }

    return true;
}

/*-----------------------------------------------------------------------------
*  program the pwm value to the hardware
*/
static bool ActivatePwm(TPwmNumber channel, uint16_t value) {
    
    bool rc = true;
    
    switch (channel) {
    case ePwm0:
        value >>= 6;
        /* if OCR1X is set to 0 a narrow spike will be generated 
         * workaround: set port pin ddr to input
         */
        if (value == 0) {
            DDRB &= ~(1 << PB7);
        } else {
            DDRB |= (1 << PB7);
        }
        OCR1C = value;
        break;
    case ePwm1:
        value >>= 6;
        TC4H = (value & 0x3ff) >> 8;
        OCR4A = value;
        break;
    case ePwm2:
        value >>= 6;
        if (value == 0) {
            DDRB &= ~(1 << PB6);
        } else {
            DDRB |= (1 << PB6);
        }
        OCR1B = value;
        break;
    case ePwm3:
        value >>= 6;
        if (value == 0) {
            DDRB &= ~(1 << PB5);
        } else {
            DDRB |= (1 << PB5);
        }
        OCR1A = value;
        break;
    default:
        rc = false;
        break;
    }
    return rc;
}

/*-----------------------------------------------------------------------------
*  set immediate if the channel is on
*/
bool PwmSet(TPwmNumber channel, uint16_t value) {
    
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




