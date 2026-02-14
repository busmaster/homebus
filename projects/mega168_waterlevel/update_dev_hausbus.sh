#!/bin/bash

avr-objcopy -O binary switchv3.elf switchv3.bin
../../tools/addchecksum/bin/addchecksum switchv3.bin switchv3_cs.bin 14336
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus0 switchv3_cs.bin $1
rm switchv3.bin
rm switchv3_cs.bin
