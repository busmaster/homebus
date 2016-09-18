#!/bin/bash

FILE="pwm4_bootloader"

dd if=/dev/zero ibs=1k count=28 | tr "\000" "\377" > 28kff.bin
avr-objcopy -I ihex -O binary --gap-fill 0xff --pad-to 0x8000 $FILE.hex 4kbl.bin
cat 28kff.bin 4kbl.bin > $FILE.bin

rm 28kff.bin
rm 4kbl.bin
