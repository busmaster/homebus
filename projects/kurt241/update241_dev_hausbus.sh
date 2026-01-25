#!/bin/bash

avr-objcopy -O binary kurt241.elf kurt241.bin
../../tools/addchecksum/bin/addchecksum kurt241.bin kurt241_cs.bin 122880
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus10 kurt241_cs.bin 241
rm kurt241.bin
rm kurt241_cs.bin

