/*-----------------------------------------------------------------------------
*  Sysdef.h
*/
#ifndef _SYSDEF_H
#define _SYSDEF_H

/*-----------------------------------------------------------------------------
*  Includes
*/
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
           UINT8 __flags;                               \
           __flags = DISABLE_INT;                       \
           x = gTimeMs16;                               \
           RESTORE_INT(__flags);                        \
        }

/* ms-Zähler (32 Bit) */
#define GET_TIME_MS32(x) {                              \
           UINT8 __flags;                               \
           __flags = DISABLE_INT;                       \
           x = gTimeMs32;                               \
           RESTORE_INT(__flags);                        \
        }

/* s-Zähler (16 Bit) */
#define GET_TIME_S(x) {                                 \
           UINT8 __flags;                               \
           __flags = DISABLE_INT;                       \
           x = gTimeS;                                  \
           RESTORE_INT(__flags);                        \
        }

/* ms-Delay (8 Bit) */
#define DELAY_MS(x) {                                       \
           UINT8 __startTime;                               \
           __startTime = GET_TIME_MS;                       \
           while (((UINT8)(GET_TIME_MS - __startTime)) < x);\
        }

/* s-Delay (16 Bit) */
#define DELAY_S(x) {                                             \
           UINT16 __startTime;                                   \
           UINT16 __actualTime;                                  \
           GET_TIME_S(__startTime);                              \
           do {                                                  \
               GET_TIME_S(__actualTime);                         \
           } while (((UINT8)(__actualTime - __startTime)) < x);  \
        }


        
/*-----------------------------------------------------------------------------
*  typedefs
*/
typedef void (* TIdleStateFunc)(BOOL setIdle);

/*-----------------------------------------------------------------------------
*  Variables
*/                                
extern volatile UINT8  gTimeMs;
extern volatile UINT16 gTimeMs16;
extern volatile UINT32 gTimeMs32;
extern volatile UINT16 gTimeS;

/*-----------------------------------------------------------------------------
*  Functions
*/
                            
#endif                            
