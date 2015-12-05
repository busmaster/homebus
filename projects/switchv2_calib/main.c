/*-----------------------------------------------------------------------------
*  Includes
*/
#include <mega88.h>

/*-----------------------------------------------------------------------------
*  Macros
*/

/* Bits in TIMSK0 */
#define TOIE0    1
#define TOCIE0A  2
#define TOCIE0B  3
                       
/* Bits in GIMSK */
#define PCIE     5

/* Bits in GIFR */
#define PCIF     5

/* Bits in PCMSK */
#define PCINT3   3
#define PCINT4   4
        
                      
/* Bits in MCUCR */
#define SM0      3
#define SM1      4

/* ASCII-Zeichen*/
#define STX 2
#define ESC 27   

/* Startwert der Checksum-Berechung*/
#define CHECKSUM_START 0x55

/* EEPROM-Belegung */
#define MODUL_ADDRESS 0
                      
/* nach dem Timeout wird die Paketsendung beendet, auch falls Taste gedrückt */                                 
#define SEND_TIMEOUT 8

#define STARTUP_DATA   0xff

/* Delay in Clocks (compiler auf speed optimierung) */
#define NUM_BASE 60


#define BIT_HIGH     0b00000010
#define BIT_LOW      0b00000000

#define SET_BIT_LOW       PORTD = BIT_LOW
#define SET_BIT_HIGH      PORTD = BIT_HIGH

#define SET_BIT(dat, msk)   SetBit(dat, msk)
                        
/* Addresse für Oszillator-Korrekturwert im EEPROM */
#define OSCCAL_CORR   1
/*-----------------------------------------------------------------------------
*  typdefs
*/
typedef unsigned char  UINT8;
typedef unsigned short UINT16;
typedef unsigned short UINT32;

/*-----------------------------------------------------------------------------
*  Variables
*/   

/*-----------------------------------------------------------------------------
*  Functions
*/
static void DelayClocks(UINT8 num);
static void SetBit(UINT8 dat, UINT8 bitMask);

/*-----------------------------------------------------------------------------
*  main
*/
void main(void) {     

   UINT8        data;
   UINT8 eeprom *ptrToEeprom;
   UINT8        corr;
 
   /* Oszillator-Korrekturwert aus EEPORM lesen */
   ptrToEeprom = (UINT8 eeprom *)OSCCAL_CORR;
   corr = *ptrToEeprom;
   OSCCAL += corr;
   
   PORTC = 0x00;
   DDRC = 0x3F;             
   
   PORTB = 0x38;
   DDRB = 0xC7;
   
   PORTD = 0x00;
   DDRD = BIT_HIGH | 0b00100000; /* PD5 Ausgang low */

   data = 0;          
   
   while (1) {
 
      SET_BIT(data, 0x01);
      DelayClocks(NUM_BASE);   

      data = !data;
   }   
}


/*-----------------------------------------------------------------------------
*  Delayfunktion zur Verzögerung um num Prozessortakte
*  kleinster erlaubter Parameter: 30 (wird nicht geprüft)
*  der Funktionspro- und -epilog wird mitgerechnet und ändert sich ev. mit 
*  Compiler!
*/
#pragma warn- // this will prevent warnings
static void DelayClocks(UINT8 num) {
#asm                                   
    ld    r30,y 
    subi  r30, 26
    mov   r31, r30
    andi  r31, 3
    lsr   r30     ; Anzahl durch 4 dividieren 
    lsr   r30
loop:             ; Schleife braucht 4 clocks
    subi  R30, 1  ; Schleifenzähler um 1 decr. 
    nop
    brne  loop  

    cpi   r31, 0
    breq  delay0

    cpi   r31, 1
    breq  delay1

    cpi   r31, 2
    breq  delay2

    cpi   r31, 3
    breq  delay3
    
delay0:
    nop
    nop
    nop
    rjmp end

delay1:
    nop
    nop
    rjmp end

delay2:
    nop
    rjmp end

delay3:
    rjmp end
    
end:
#endasm
}
#pragma warn+ // enable warnings

/* Durchlaufdauer 9 Takte, bis zum Portzugriff 7 Takte */
/* mit den nop wird konstante Gesamtdauer und Dauer bis zum Portzugriff */ 
/* unabhängig von 0 oder 1 gemacht */
static void SetBit(UINT8 dat, UINT8 bitMask) {

  if ((dat & bitMask) != 0) { 
     #asm("nop");    
     SET_BIT_HIGH;
  } else {    
     SET_BIT_LOW;
     #asm("nop");
     #asm("nop");
  }
}

