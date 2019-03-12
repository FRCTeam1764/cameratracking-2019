#!/bin/bash

cd "${0%/*}"


#echo "---New Instance---" >> STARTUPLOG.log


while true; do 
./my_test -c 0 -s -r distortion.xml
echo RESTART SERVER
done
