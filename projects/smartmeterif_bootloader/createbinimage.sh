#!/bin/bash

FILE="smartmeterif_bootloader"

dd if=/dev/zero ibs=1k count=56 | tr "\000" "\377" > 56kff.bin
avr-objcopy -I ihex -O binary --gap-fill 0xff --pad-to 0x10000 $FILE.hex 8kbl.bin
cat 56kff.bin 8kbl.bin > $FILE.bin

rm 56kff.bin
rm 8kbl.bin
