/*-----------------------------------------------------------------------------
*  SioType0Poll.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include <avr/interrupt.h>
#include <avr/io.h>

#include "Types.h"
#include "Sysdef.h"
#include "Sio.h"

/*-----------------------------------------------------------------------------
*  Macros
*/          
#undef BRX2

#if (F_CPU == 1000000UL)
#define BRREG    12 /* 9600 @ 1MHz  */
#define BRX2        /* U2X = 1      */   
#else
#error adjust baud rate settings for your CPU clock frequency
#endif

/*-----------------------------------------------------------------------------
*  typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/                                

/*-----------------------------------------------------------------------------
*  Sio Initialisierung
*/
void SioInit(void) {
     
   unsigned int baud = BRREG; 

   /* Set baud rate UART0: 9600 */
   UBRRH = (unsigned char)(baud >> 8);
   UBRRL = (unsigned char)baud;
#ifdef BRX2
   /* double baud rate */
   UCSRA = 1 << U2X;
#endif
   /* Enable Receiver und Transmitter */
   UCSRB = (1 << RXEN) | (1 << TXEN);
   /* Set frame format: 8 data, 1 stop bit */
   UCSRC = (1 << URSEL) | (3 << UCSZ0); 
}

/*-----------------------------------------------------------------------------
*  Sio Sendepuffer schreiben
*/
void SioWrite(UINT8 ch) {
   
   /* Wait for empty transmit buffer */
   while ((UCSRA & (1 << UDRE)) == 0);
   UDR = ch;
}

/*-----------------------------------------------------------------------------
*  Sio Empfangspuffer lesen
*/
UINT8 SioRead(UINT8 *pCh) {

   if ((UCSRA & (1<<RXC)) != 0) {
      *pCh = UDR;
      return 1;
   } else {
      return 0;
   }
}

/*-----------------------------------------------------------------------------
*  zurückschreiben
*/
void SioReadFlush(void) {
   
   UINT8 dummy;
   while (UCSRA & (1<<RXC)) {
      dummy = UDR;
   }
}
