/*
 * sio.h
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

#ifndef _SIO_H
#define _SIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "sysdef.h"

/*-----------------------------------------------------------------------------
*  Macros
*/                     

/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef enum {
   eSioBaud9600   
} TSioBaud;

typedef enum {
   eSioDataBits8   
} TSioDataBits;

typedef enum {
   eSioParityNo   
} TSioParity;

typedef enum {
   eSioStopBits1   
} TSioStopBits;

typedef enum {
   eSioModeHalfDuplex,
   eSioModeFullDuplex
} TSioMode;

typedef void (* TBusTransceiverPowerDownFunc)(bool powerDown);

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/

void    SioInit(void);
int     SioOpen(const char *pPortName, TSioBaud baud, TSioDataBits dataBits, TSioParity parity, TSioStopBits stopBits, TSioMode mode);
int     SioClose(int handle);
uint8_t SioWrite(int handle, uint8_t *pBuf, uint8_t bufSize);    
uint8_t SioRead(int handle, uint8_t *pBuf, uint8_t bufSize);
uint8_t SioUnRead(int handle, uint8_t *pBuf, uint8_t bufSize);
uint8_t SioGetNumRxChar(int handle);
void    SioSetTransceiverPowerDownFunc(int handle, TBusTransceiverPowerDownFunc btpdFunc);
void    SioSetIdleFunc(int handle, TIdleStateFunc idleFunc);

#ifdef __cplusplus
}
#endif
  
#endif
