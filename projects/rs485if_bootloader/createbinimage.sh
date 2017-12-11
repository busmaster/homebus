#!/bin/bash

FILE="rs485if_bootloader"

dd if=/dev/zero ibs=1k count=120 | tr "\000" "\377" > 120kff.bin
avr-objcopy -I ihex -O binary --gap-fill 0xff --pad-to 0x20000 $FILE.hex 8kbl.bin
cat 120kff.bin 8kbl.bin > $FILE.bin

rm 120kff.bin
rm 8kbl.bin
