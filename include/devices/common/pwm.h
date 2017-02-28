/*
 * pwm.h
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
#ifndef _PWM_H
#define _PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <pwm_c.h>

/*-----------------------------------------------------------------------------
*  Macros
*/

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef enum {
   ePwm0 = 0,
   ePwm1 = 1,
   ePwm2 = 2,
   ePwm3 = 3,
   ePwm4 = 4,
   ePwm5 = 5,
   ePwm6 = 6,
   ePwm7 = 7,
   ePwm8 = 8,
   ePwm9 = 9,
   ePwm10 = 10,
   ePwm11 = 11,
   ePwm12 = 12,
   ePwm13 = 13,
   ePwm14 = 14,
   ePwm15 = 15,
   ePwm16 = 16,
   ePwm17 = 17,
   ePwm18 = 18,
   ePwm19 = 19,
   ePwm20 = 20,
   ePwm21 = 21,
   ePwm22 = 22,
   ePwm23 = 23,
   ePwm24 = 24,
   ePwm25 = 25,
   ePwm26 = 26,
   ePwm27 = 27,
   ePwm28 = 28,
   ePwm29 = 29,
   ePwm30 = 30,
   ePwm31 = 31,
   ePwmInvalid = 255
} TPwmNumber;

/*-----------------------------------------------------------------------------
*  Variables
*/

/*-----------------------------------------------------------------------------
*  Functions
*/
void    PwmInit(void);
void    PwmExit(void);
bool    PwmGet(TPwmNumber channel, uint16_t *pValue);
bool    PwmIsOn(TPwmNumber channel, bool *pOn);
bool    PwmGetAll(uint16_t *buf, uint8_t buf_size);
bool    PwmSet(TPwmNumber channel, uint16_t value);
bool    PwmOn(TPwmNumber channel, bool on);
void    PwmCheck(void);

#ifdef __cplusplus
}
#endif

#endif
