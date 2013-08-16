#!/bin/bash

dev=$1
# replace / by _
dev=$(echo "${dev////_}")
filename="/tmp/busportserver/""$dev""_cmdPts"
if [ -f $filename ]; then
   cmddev=$(cat $filename)
   echo "add device" > $cmddev
   answer=$(head -1 $cmddev)
   echo $answer
fi
