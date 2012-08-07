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
#include "Types.h"
#include "sysDef.h"

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

typedef void (* TBusTransceiverPowerDownFunc)(BOOL powerDown);

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Functions
*/

void  SioInit(void);
int   SioOpen(const char *pPortName, TSioBaud baud, TSioDataBits dataBits, TSioParity parity, TSioStopBits stopBits, TSioMode mode);
int   SioClose(int handle);
UINT8 SioWrite(int handle, UINT8 *pBuf, UINT8 bufSize);    
UINT8 SioRead(int handle, UINT8 *pBuf, UINT8 bufSize);
UINT8 SioUnRead(int handle, UINT8 *pBuf, UINT8 bufSize);
UINT8 SioGetNumRxChar(int handle);
void  SioSetTransceiverPowerDownFunc(int handle, TBusTransceiverPowerDownFunc btpdFunc);
void  SioSetIdleFunc(int handle, TIdleStateFunc idleFunc);

#ifdef __cplusplus
}
#endif
  
#endif
