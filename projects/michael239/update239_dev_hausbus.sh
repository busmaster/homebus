#!/bin/bash

avr-objcopy -O binary michael239.elf michael239.bin
../../tools/addchecksum/bin/addchecksum michael239.bin michael239_cs.bin 122880
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus1 michael239_cs.bin 239
rm michael239.bin
rm michael239_cs.bin

