#!/bin/bash

avr-objcopy -O binary kurt240.elf kurt240.bin
../../tools/addchecksum/bin/addchecksum kurt240.bin kurt240_cs.bin 122880
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus10 kurt240_cs.bin 240
rm kurt240.bin
rm kurt240_cs.bin

