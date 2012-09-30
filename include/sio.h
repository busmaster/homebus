/*-----------------------------------------------------------------------------
*  Sio.h
*/
#ifndef _SIO_H
#define _SIO_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
*  Includes
*/
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
