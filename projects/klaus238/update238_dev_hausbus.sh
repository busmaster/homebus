#!/bin/bash

avr-objcopy -O binary klaus238.elf klaus238.bin
../../tools/addchecksum/bin/addchecksum klaus238.bin klaus238_cs.bin 28672
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus0 klaus238_cs.bin 238
rm klaus238.bin
rm klaus238_cs.bin

