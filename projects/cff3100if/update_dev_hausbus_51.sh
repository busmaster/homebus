#!/bin/bash

avr-objcopy -O binary cff3100if.elf cff3100if.bin
../../tools/addchecksum/bin/addchecksum cff3100if.bin cff3100if_cs.bin 14336
../../tools/firmwareupdate/bin/firmwareupdate /dev/hausbus11 cff3100if_cs.bin 51
rm cff3100if.bin
rm cff3100if_cs.bin
