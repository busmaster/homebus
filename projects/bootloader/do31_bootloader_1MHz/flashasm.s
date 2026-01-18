/*
 * flashasm.s
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

   .section .text

   .global FlashSum
   .global FlashFillPagePuffer
   .global FlashProgramPagePuffer
   .global FlashPageErase

;-----------------------------------------------------------------------------
;  Summe über Flash berechnen
;  uint16_t FlashSum(uint16_t wordAddr, uint8_t numWords)
;
FlashSum:
   ; Z-Register aufsetzen
   movw r30, r24 ; lsb  (r25:r24 -> z, r24 lsb, r25 msb wordaddr)
   ; Übergabeparameter mit 2 multiplizieren und höchstwertiges Bit nach RAMPZ0 schieben
   clc
   rol r30
   rol r31
   brcc  1f

   ldi r23, 1
   rjmp 2f
1:
   ldi r23, 0
2:
   lds r26 , 0x5b ; RAMPZ sichern
   sts 0x5b, r23  ; RAMPZ0 setzen 

   ; numBytes 
                  ; numWords wird in r22 übergeben und wird als loop counter verwendet 
   ldi r24, 0     ; sum lsb
   ldi r25, 0     ; sum msb
   
1:
   elpm r27, z+
   clc
   adc  r24, r27
   elpm r27, z+
   adc  r25, r27

   subi r22, 1 
   brne 1b
   
   ; returnwert in r24/25 zurückgeben (sum liegt schon dort)

   sts 0x5b, r26  ; RAMPZ restore 
   ret
/* register-verwendung */
/* r18 spmcsr  */

;-----------------------------------------------------------------------------
;  Pagepuffer mit Daten füllen
;  void FlashFillPagePuffer(uint16_t offset, uint16_t *pBuf, uint8_t numWords) {
;               
FlashFillPagePuffer:
   ; offset    r25:r24
   ; pBuf      r23:r22
   ; numWords  r20
   ; transfer data from RAM to Flash page buffer
   ; offset -> Z 
   movw r30, r24
   clc
   rol r30
   rol r31
   
   ; numWords in r20 (als loop counter verwendet)

   ; pBuf -> x
   movw r26, r22

1:
   ld  r0, x+
   ld  r1, x+
   ldi r18, 0x01 ; (1<<SPMEN)
   call Do_spm
   adiw r30, 2
   subi r20, 1 ;use subi for PAGESIZEB<=256
   brne 1b
   clr r1    ; r1 muss bei WinAVR wieder auf 0 gesetzt werden
   ret

;-----------------------------------------------------------------------------
;  Pagepuffer in Flash programmieren
;  void FlashProgramPagePuffer(uint16_t pageWordAddr) {
;
FlashProgramPagePuffer:
   ; pageWordAddr    r25:r24
   ; Z-Register aufsetzen
   movw r30, r24
   ; Übergabeparameter mit 2 multiplizieren und höchstwertiges Bit nach RAMPZ0 schieben
   clc
   rol r30
   rol r31
   brcc  1f

   ldi r23, 1
   rjmp 2f
1:
   ldi r23, 0
2:    
   lds r26 , 0x5b ; RAMPZ sichern
   sts 0x5b, r23  ; RAMPZ0 setzen 

   ; execute page write
   ldi r18, 0x05      ; (1<<PGWRT) | (1<<SPMEN)
   call Do_spm

   ; re-enable the RWW section
   ldi r18, 0x11      ; (1<<RWWSRE) | (1<<SPMEN)
   call Do_spm

   sts 0x5b, r26  ; RAMPZ restore 
   ret

;-----------------------------------------------------------------------------
;  Page löschen
;  void FlashPageErase(uint16_t pageWordAddr) {
;
FlashPageErase:
   ; pageWordAddr    r25:r24
   ldi r18, 0x03        ; (1<<PGERS) | (1<<SPMEN)
    
   ; Z-Register aufsetzen
   movw r30, r24
   ; Übergabeparameter mit 2 multiplizieren und höchstwertiges Bit nach RAMPZ0 schieben
   clc
   rol r30
   rol r31
   brcc  1f

   ldi r23, 1
   rjmp 2f
1:
   ldi r23, 0
2:
   lds r26 , 0x5b ; RAMPZ sichern
   sts 0x5b, r23  ; RAMPZ0 setzen 
   call Do_spm               
    
   ; re-enable the RWW section
   ldi r18, 0x11      ; (1<<RWWSRE) | (1<<SPMEN)
   call Do_spm

   sts 0x5b, r26  ; RAMPZ restore 
   ret

;
; Do_spm
; 
Do_spm:
   ; check for previous SPM complete
1:
   lds r23, 0x68  ; SPMCSR
   sbrc r23, 0    ; SPMEN
   rjmp 1b
   ; check that no EEPROM write access is present
1:
   sbic 0x1c, 1  ; EECR, EEWE
   rjmp 1b
   ; SPM timed sequence
   sts 0x68, r18   ; SPMCSR
   spm
   ret
   
