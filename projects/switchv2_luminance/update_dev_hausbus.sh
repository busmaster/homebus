#!/bin/bash

avr-objcopy -O binary switchv2_luminance.elf switchv2_luminance.bin
../../tools/addchecksum/bin/addchecksum switchv2_luminance.bin switchv2_luminance_cs.bin 6144
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus switchv2_luminance_cs.bin $1
rm switchv2_luminance.bin
rm switchv2_luminance_cs.bin
