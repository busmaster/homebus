#!/bin/bash

avr-objcopy -O binary switchv2_garage.elf switchv2_garage.bin
../../tools/addchecksum/bin/addchecksum switchv2_garage.bin switchv2_garage_cs.bin 6144
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus switchv2_garage_cs.bin $1
rm switchv2_garage.bin
rm switchv2_garage_cs.bin
