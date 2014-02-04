/*
 * sysdef.h
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

#ifndef _SYSDEF_H
#define _SYSDEF_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

/*-----------------------------------------------------------------------------
*  Macros
*/                     
#define DISABLE_INT         ((SREG & (1 << SREG_I)) != 0); \
                            cli()

#define ENABLE_INT          sei()

#define RESTORE_INT(flags)  if (flags != 0) {   \
                               sei();     \
                            }         

#define max(x,y)            ((x) > (y) ? (x) : (y))
#define min(x,y)            ((x) > (y) ? (y) : (x))

#define ARRAY_CNT(x)        (sizeof(x) / sizeof(x[0]))


/* ms-Zähler (8 Bit) */
#define GET_TIME_MS         gTimeMs

/* ms-Zähler (16 Bit) */
#define GET_TIME_MS16(x) {                              \
           uint8_t __flags;                             \
           __flags = DISABLE_INT;                       \
           x = gTimeMs16;                               \
           RESTORE_INT(__flags);                        \
        }
		
/* 100ms-Zähler (16 Bit) */
#define GET_TIME_10MS16(x) {                            \
           uint8_t __flags;                             \
           __flags = DISABLE_INT;                       \
           x = gTime10Ms16;                             \
           RESTORE_INT(__flags);                        \
        }

/* ms-Zähler (32 Bit) */
#define GET_TIME_MS32(x) {                              \
           uint8_t __flags;                             \
           __flags = DISABLE_INT;                       \
           x = gTimeMs32;                               \
           RESTORE_INT(__flags);                        \
        }

/* s-Zähler (16 Bit) */
#define GET_TIME_S(x) {                                 \
           uint8_t __flags;                             \
           __flags = DISABLE_INT;                       \
           x = gTimeS;                                  \
           RESTORE_INT(__flags);                        \
        }

/* ms-Delay (8 Bit) */
#define DELAY_MS(x) {                                         \
           uint8_t __startTime;                               \
           __startTime = GET_TIME_MS;                         \
           while (((uint8_t)(GET_TIME_MS - __startTime)) < x);\
        }

/* s-Delay (16 Bit) */
#define DELAY_S(x) {                                             \
           uint16_t __startTime;                                 \
           uint16_t __actualTime;                                \
           GET_TIME_S(__startTime);                              \
           do {                                                  \
               GET_TIME_S(__actualTime);                         \
           } while (((uint8_t)(__actualTime - __startTime)) < x);\
        }


        
/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef void (* TIdleStateFunc)(bool setIdle);

/*-----------------------------------------------------------------------------
*  Variables
*/                                
extern volatile uint8_t  gTimeMs;
extern volatile uint16_t gTimeMs16;
extern volatile uint32_t gTimeMs32;
extern volatile uint16_t gTimeS;
extern volatile uint16_t gTime10Ms16;

/*-----------------------------------------------------------------------------
*  Functions
*/
                            
#endif                            
