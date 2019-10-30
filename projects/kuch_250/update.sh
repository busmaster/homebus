#!/bin/bash

  avr-objcopy -O binary kuch_250.elf kuch_250.bin
  ../../tools/addchecksum/bin/addchecksum kuch_250.bin kuch_250_cs.bin 122880
  ../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus1 kuch_250_cs.bin 250
  rm kuch_250.bin
  rm kuch_250_cs.bin
