#!/bin/bash

# Copyright (C) 2012 Marvell International Ltd.
# All Rights Reserved.

# Load application to ram helper script
OS=`uname`
IFC_FILE=ftdi.cfg

if [ $OS == "Linux" ] || [ $OS == "Darwin" ]; then
	if [ `id -u` != 0 ]; then
		echo "Please run $0 with root privileges!"
		exit 1
	fi
fi

if [ $OS == "Linux" ]; then
	OPENOCD_PATH="./bin/linux/openocd"
	FILE=$1
else if [ $OS == "Darwin" ]; then
	OPENOCD_PATH=`which openocd`
	FILE=$1
else
	OPENOCD_PATH="./bin/windows/openocd"
	FILE=`cygpath -m $1`
fi
fi

print_usage() {
	echo ""
	echo "Usage: $0 <app.axf>"
	echo "Optional Usage:"
	echo "[-i | --interface] JTAG hardware interface name, supported ones
	are ftdi, jlink, amontec. Default is ftdi."
}

if [ ! -e "$OPENOCD_PATH" ]; then
	echo "Please execute $0 from <sdk_tools_top>/mc200/OpenOCD directory"
	print_usage
	exit 1
fi

if [ -z "$FILE" -o $# -lt 1 ]; then
	print_usage
	exit 1
fi

while test -n "$2"; do
	case "$2" in
	--interface|-i)
		IFC_FILE=$3.cfg
		shift 2
	;;
	*)
		print_usage
		exit 1
	;;
	esac
done

test -f "$FILE" ||
{ echo "Error: Firmware image $FILE does not exists"; exit 1; }

# Check if overlays are enabled, warn and exit if yes
nm $FILE | grep "_ol_ovl" > /dev/null &&
{ echo "Error: Provided firmware image (.axf) is overlays enabled and hence
ramload is not possible, please use flashprog to flash the image."; exit 1;
}

READELF=`which arm-none-eabi-readelf`

# Even if toolchain is not installed, native readelf works
if [ "$READELF" == "" ]; then
	READELF=`which readelf`
fi

entry_point=`$READELF -h $FILE | grep "Entry point" | awk '{print $4}'`

$OPENOCD_PATH -s interface -f $IFC_FILE -f openocd.cfg -c init -c "load $FILE $entry_point" -c shutdown
