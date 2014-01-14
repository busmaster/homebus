#!/bin/bash

echo $#

echo $1
echo $2

if [ "$#" == "3" ]
  then
  avr-objcopy -O binary $2".elf" $2".bin"
  ../../tools/addchecksum/bin/addchecksum $2".bin" $2"_cs.bin" 122880
  ../../tools/firmwareupdate/bin/firmwareupdate $1 $2"_cs.bin" $3
  rm $2".bin"
  rm $2"_cs.bin"
  else
  echo "Usage: update.sh port projekt adress"
fi

