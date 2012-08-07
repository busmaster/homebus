   .section .text

   .global ApplicationEntry

;-----------------------------------------------------------------------------
;  Summe über Flash berechnen
;  void ApplicationEntry(void)
;
ApplicationEntry:
   ; Z-Pointer auf Adresse 0 stellen (Resetvektor der Applikation)
   ldi r30, 0  ; R30 = ZL
   ldi r31, 0  ; R31 = ZH 
   ; indirekter Sprung (auf Z)
   ijmp



