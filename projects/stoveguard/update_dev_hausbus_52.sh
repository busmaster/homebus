#!/bin/bash

avr-objcopy -O binary stoveguard.elf stoveguard.bin
../../tools/addchecksum/bin/addchecksum stoveguard.bin stoveguard_cs.bin 14336
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus11 stoveguard_cs.bin 52
rm stoveguard.bin
rm stoveguard_cs.bin
