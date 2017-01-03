#!/bin/bash

avr-objcopy -O binary switchv2_wind.elf switchv2_wind.bin
../../tools/addchecksum/bin/addchecksum switchv2_wind.bin switchv2_wind_cs.bin 6144
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus switchv2_wind_cs.bin 45
rm switchv2_wind.bin
rm switchv2_wind_cs.bin
