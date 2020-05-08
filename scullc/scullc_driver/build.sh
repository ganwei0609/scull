#!/bin/bash
if [ -z $1 ]; then
	echo "please type ko name"
	exit 1
fi
echo "$1"
lsmod | grep "$1"

if [ 0 == $? ]; then
	rmmod $1.ko
fi

make clean && make

insmod $1.ko
exit 0
