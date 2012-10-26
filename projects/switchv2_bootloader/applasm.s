   .section .text

   .global ApplicationEntry

;-----------------------------------------------------------------------------
;  jump to reset vector of application section
;  void ApplicationEntry(void)
;
ApplicationEntry:
   ; set up Z-pointer to 0 (reset vector of application)
   ldi r30, 0  ; R30 = ZL
   ldi r31, 0  ; R31 = ZH 
   ; idirect jump to Z
   ijmp



