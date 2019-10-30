#!/bin/bash

avr-objcopy -O binary smartmeterif.elf smartmeterif.bin
../../tools/addchecksum/bin/addchecksum smartmeterif.bin smartmeterif_cs.bin 57344
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus smartmeterif_cs.bin 47
rm smartmeterif.bin
rm smartmeterif_cs.bin

