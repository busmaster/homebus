#!/bin/bash

avr-objcopy -O binary klaus241.elf klaus241.bin
../../tools/addchecksum/bin/addchecksum klaus241.bin klaus241_cs.bin 122880
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus klaus241_cs.bin 241
rm klaus241.bin
rm klaus241_cs.bin

