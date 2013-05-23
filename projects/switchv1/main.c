/*
 * main.c
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

/**  
*    push-button module Mega8
*
*    compilation with avr-gcc version 4.1.1 (WinAVR-20070122)
*    optimization -O2
*/
/*-----------------------------------------------------------------------------
*  Includes
*/
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

/*-----------------------------------------------------------------------------
*  Macros
*/
#define RXEN     4
#define TXEN     3
#define URSEL    7
#define UCSZ0    1        
#define UDRE     5
#define U2X      1   

#define TOIE0    0
#define CS00     0

/* ASCII-chars*/
#define STX 2
#define ESC 27   

/* start value for checksum calculation */
#define CHECKSUM_START 0x55

/* offset addresses in EEPROM */
#define MODUL_ADDRESS (const uint8_t *)0
#define OSCCAL_CORR   (const uint8_t *)1
                      
/* timeout for termination of paket retransmission in case of permanently    */
/* pressed button, timeout in seconds */                                 
#define SEND_TIMEOUT 8

#define STARTUP_DATA   0xff

/*-----------------------------------------------------------------------------
*  typdefs
*/
typedef unsigned char  UINT8;
typedef unsigned short UINT16;

/*-----------------------------------------------------------------------------
*  Variables
*/   
volatile static UINT8  sTimeS = 0;
volatile static UINT16 sCntMs = 0;

/* the version string is placed in the last 16 bytes in flash */
char version[] __attribute__ ((section (".version")));
char version[] = "Mega8 1.02";

/*-----------------------------------------------------------------------------
*  Functions
*/
void MsgSend(UINT8 data, UINT8 address);
void TransmitCharProt(UINT8 data);
void TransmitCharLL(UINT8 data);

/*-----------------------------------------------------------------------------
*  program entry
*/
int main(void) {     

   unsigned int baud = 12; /* 9600 @ 1MHz, U2X = 1  */
   UINT8        data;
   UINT8        addr;
   UINT8        startTimeS = 0;
   UINT8        keyPressed = 0;      
   UINT16       startCnt = 0;
   UINT16       timerCnt = 0;
   UINT16       diff = 0;

   /* get oscillator correction value from EEPROM */
   data = eeprom_read_byte(OSCCAL_CORR);
   OSCCAL += data;
   
   /* set baud rate */
   UBRRH = (unsigned char)(baud>>8);
   UBRRL = (unsigned char)baud;
   /* enable baud rate doubling */
   UCSRA = 1 << U2X;
   /* enable transmitter */
   UCSRB = 1 << TXEN;
   /* set frame format: 8 data, 1 stop bit */
   UCSRC = (1 << URSEL) | (3 << UCSZ0); 
               
   /* configure port pins for push button terminals with internal pull up */ 
   /* configure unused pins to output low */
   PORTC = 0x03;
   DDRC = 0x3C;             
   
   PORTB = 0x38;
   DDRB = 0xC7;
   
   PORTD = 0x01;
   DDRD = 0xFE;
   
   /* get module address from EEPROM */
   addr = eeprom_read_byte(MODUL_ADDRESS);
   
   /* configure Timer 0 */
   /* prescaler clk/64 -> Interrupt period 256/1000000 * 64 = 16.384 ms */
   TCCR0 = 3 << CS00; 
   TIMSK = 1 << TOIE0;

   sei();  /* set the Global Interrupt Enable Bit  */
   
   /* send startup message */
   /* delay depends on module address (address * 100 ms) */
   startCnt = sCntMs;
   do {
      cli();
      diff = sCntMs - startCnt;
      sei();
   } while (diff < ((UINT16)addr * 100));

   MsgSend(STARTUP_DATA, addr);

   set_sleep_mode(SLEEP_MODE_IDLE);
  
   while (1) { 
      cli();
      sleep_enable();
      sei();
      sleep_cpu();
      sleep_disable();

      data = 0;
      if ((PINC & 0x01) == 0x00) {
         data |= 1;
      }
      if ((PINC & 0x02) == 0x00) {
         data |= 2;
      }                        
    
      if (data != 0) {
         if (keyPressed == 0) {
            startTimeS = sTimeS;
            keyPressed = 1;
            timerCnt = 0;
         }
         if (((UINT8)(sTimeS - startTimeS)) < SEND_TIMEOUT) {
            /* on every 3rd timer interrupt the telegram is sent */
            /* telegram is repeated with 50 ms period (3 * 16.384) */
            if ((timerCnt % 3) == 0) {
               MsgSend(data, addr);
            }
            timerCnt++;
         }
      } else {
         keyPressed = 0;      
      }
   }
   return 0;
}

/*-----------------------------------------------------------------------------
*  send character in polling mode
*/
void TransmitCharLL(UINT8 data) {
   /* Wait for empty transmit buffer */
   while (!( UCSRA & ( 1 << UDRE)));
   /* Put data into buffer, sends the data */
   UDR = data;
}

/*-----------------------------------------------------------------------------
*  send character with protocoll translation
*  STX -> ESC + ~STX
*  ESC -> ESC + ~ESC
*/
void TransmitCharProt(UINT8 data) {

   if (data == STX) {
      TransmitCharLL(ESC);
      TransmitCharLL(~STX);
   } else if (data == ESC) {
      TransmitCharLL(ESC);
      TransmitCharLL(~ESC);
   } else {
      TransmitCharLL(data);
   }   
}

/*-----------------------------------------------------------------------------
*  send bus telegram:
*  protocoll (8 bit characters)
*  1st character: always STX (this character is unique in transmission)
*  2nd character: address of module (1 .. 255)
*  3rd character: payload: Bit0 and Bit1 represent the state of SW0 and SW1 
*                          at least one of the bits is set
*  4th character: checksum: sum of all characters before + 0x55
*/
void MsgSend(UINT8 data, UINT8 address) {

   UINT8 ch;
   UINT8 checkSum = CHECKSUM_START;
   
   ch = STX;
   TransmitCharLL(ch);
   checkSum += ch;

   ch = address;
   TransmitCharProt(ch);
   checkSum += ch;

   ch = data;
   TransmitCharProt(ch);
   checkSum += ch;

   ch = checkSum;
   TransmitCharProt(ch);
}

/*-----------------------------------------------------------------------------
*  Timer 0 overflow ISR
*  period:  16.384 ms
*/
ISR(TIMER0_OVF_vect) {

  static UINT8 intCnt = 0;
  sCntMs += 16;  /* ms counter */
  intCnt++;
  if (intCnt >= 61) { /* 16.384 ms * 61 = 1 s*/
     intCnt = 0;
     sTimeS++;
  }
}
