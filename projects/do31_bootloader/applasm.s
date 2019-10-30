/*
 * applasm.s
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

   .global ApplicationEntry

;-----------------------------------------------------------------------------
;  Summe �ber Flash berechnen
;  void ApplicationEntry(void)
;
ApplicationEntry:
   ; Z-Pointer auf Adresse 0 stellen (Resetvektor der Applikation)
   ldi r30, 0  ; R30 = ZL
   ldi r31, 0  ; R31 = ZH 
   ; indirekter Sprung (auf Z)
   ijmp



