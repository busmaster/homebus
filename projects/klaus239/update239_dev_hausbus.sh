#!/bin/bash

avr-objcopy -O binary klaus239.elf klaus239.bin
../../tools/addchecksum/bin/addchecksum klaus239.bin klaus239_cs.bin 28672
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus klaus239_cs.bin 239
rm klaus239.bin
rm klaus239_cs.bin

