#!/bin/bash

  avr-objcopy -O binary kuch_251.elf kuch_251.bin
  ../../tools/addchecksum/bin/addchecksum kuch_251.bin kuch_251_cs.bin 122880
  ../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus1 kuch_251_cs.bin 251
  rm kuch_251.bin
  rm kuch_251_cs.bin
