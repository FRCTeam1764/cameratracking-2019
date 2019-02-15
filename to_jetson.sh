#!/bin/bash

if [ $# -eq 0 ]; then
	echo "No jetson IP provided. Using slower Bounjour addressing."
	addr=1764-tegra.local
else
	addr=$1
fi 

sshpass -p ubuntu ssh ubuntu@$addr -C rm -rf /home/ubuntu/imported/cvcam
sshpass -p ubuntu scp -r . ubuntu@$addr:/home/ubuntu/imported/cvcam
sshpass -p ubuntu ssh ubuntu@$addr -C "cd /home/ubuntu/imported/cvcam/ && ./make_from_scratch.sh"

