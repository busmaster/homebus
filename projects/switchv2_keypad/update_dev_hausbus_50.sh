#!/bin/bash

avr-objcopy -O binary switchv2_keypad.elf switchv2_keypad.bin
../../tools/addchecksum/bin/addchecksum switchv2_keypad.bin switchv2_keypad_cs.bin 6144
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus11 switchv2_keypad_cs.bin 50
rm switchv2_keypad.bin
rm switchv2_keypad_cs.bin
