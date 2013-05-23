#!/bin/bash

avr-objcopy -O binary klaus240.elf klaus240.bin
../../tools/addchecksum/bin/addchecksum klaus240.bin klaus240_cs.bin 122880
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus klaus240_cs.bin 240
rm klaus240.bin
rm klaus240_cs.bin

