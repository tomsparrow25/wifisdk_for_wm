#!/bin/bash

# Copyright (C) 2012 Marvell International Ltd.
# All Rights Reserved.

# Flashprog wrapper/helper script


LAYOUT_FILE=flashprog.layout
CONFIG_FILE=flashprog.config
BLOB_INT=blob_int.bin
BLOB_EXT=blob_ext.bin
BLOB_SPI=blob_spi.bin

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
else if [ $OS == "Darwin" ]; then
	OPENOCD_PATH=`which openocd`
else
	OPENOCD_PATH="./bin/windows/openocd"
fi
fi

print_usage() {
	echo ""
	echo "Usage: $0"
	echo ""
	echo "Optional Usage:"
	echo "[-l | --new-layout] Flash partition layout file <layout.txt>"
	echo "[-b | --batch]  Batch processing mode config file <config.txt>"
	echo "[-i | --interface] JTAG hardware interface name, supported ones
	are ftdi, jlink, amontec. Default is ftdi."
	echo "[-d | --debug]  Debug mode, dump current flash contents"
	echo "[-f | --to-file]  Write to a file instead of the flash <int,ext,spi>"
	echo "                  corresponding blob_[int/ext/spi].bin are created"
	echo "[-x | --blob]  Write the blob files (created with -f) to flash"
	echo "[--<component_name> <file_path>] As an alternate to batch mode"
}

copy_layout_file() {
	if [ -z "$1" ]; then
		echo "Layout file name missing"
		print_usage
		exit 1
	fi
	if [ -f "$1" ]; then
		rm -f $LAYOUT_FILE
		cp -a "$1" $LAYOUT_FILE
	else
		echo "Layout file $1 does not exists"
		print_usage
		exit 1
	fi
}

copy_config_file() {
	if [ -z "$1" ]; then
		echo "Batch config file name missing"
		print_usage
		exit 1
	fi
	if [ -f "$1" ]; then
		rm -f $CONFIG_FILE
		cp -a "$1" $CONFIG_FILE
	else
		echo "Batch config file $1 does not exists"
		print_usage
		exit 1
	fi
}

copy_config_option() {
	comp=`echo "$1" | sed -e 's/--//'`
	if [ -z "$2" ]; then
		print_usage
		exit 1
	fi

	if [ ! -f "$2" ]; then
		echo "File $2 does not exists"
		print_usage
		exit 1
	fi
	echo "$comp $2" >> $CONFIG_FILE
}

cleanup() {
	rm -f $LAYOUT_FILE
	rm -f $CONFIG_FILE
}

cleanup

if [ ! -e "$OPENOCD_PATH" ]; then
	echo "Please execute $0 from <sdk_tools_top>/mc200/OpenOCD directory"
	print_usage
	exit 1
fi

non_interactive=0
blob_create=0
while test -n "$1"; do
	case "$1" in
	--new-layout|-l)
		non_interactive=1
		copy_layout_file $2
		shift 2
	;;
	--batch|-b)
		non_interactive=1
		copy_config_file $2
		shift 2
	;;
	--debug|-d)
		non_interactive=1
		echo "debug" "flash" > $CONFIG_FILE
		shift 1
	;;
	--to-file|-f)
		non_interactive=1
		rm -f $BLOB_INT $BLOB_EXT $BLOB_SPI
		if $( echo $2 | grep --quiet 'int' ); then
			dd if=/dev/zero bs=1M count=1 2> /dev/null | tr '\000' '\377' > $BLOB_INT
		fi
		if $( echo $2 | grep --quiet 'ext' ); then
			dd if=/dev/zero bs=1M count=1 2> /dev/null | tr '\000' '\377' > $BLOB_EXT
		fi
		if $( echo $2 | grep --quiet 'spi' ); then
			dd if=/dev/zero bs=1M count=1 2> /dev/null | tr '\000' '\377' > $BLOB_SPI
		fi
		blob_create=1
		shift 2
	;;
	--blob|-x)
		non_interactive=1
		echo "blob" "write" > $CONFIG_FILE
		shift 1
	;;
	--interface|-i)
		IFC_FILE=$2.cfg
		shift 2
	;;
	--help|-h)
		print_usage
		exit 1
	;;
	*)
		non_interactive=1
		copy_config_option $1 $2
		shift 2
	;;
	esac
done

if [ "$blob_create" == 1 ]; then
	sed -i '1iblob create' $CONFIG_FILE
fi

READELF=`which arm-none-eabi-readelf`

# Even if toolchain is not installed, native readelf works
if [ "$READELF" == "" ]; then
	READELF=`which readelf`
fi

entry_point=`$READELF -h flashprog.axf | grep "Entry point" | awk '{print $4}'`

# See if it is non_interactive mode or not
if [ "$non_interactive" == 1 ]; then
(
if awk -Wv 2>/dev/null | head -1 | grep -q '^mawk'; then
	awk_args=('-W' 'interactive')
else
	awk_args=()
fi
# Save sub-shell PID here, this will be used to kill OpenOCD process once it
# is done without waiting for user intervention.
SPID=$BASHPID; exec $OPENOCD_PATH -s interface -f $IFC_FILE -f openocd.cfg -c init -c "sh_load flashprog.axf $entry_point" | awk "${awk_args[@]}" '{print} /Please press CTRL\+C to exit./ {print "Exiting."; system("kill " system("echo $SPID"))}'
)
else
$OPENOCD_PATH -s interface -f $IFC_FILE -f openocd.cfg -c init -c "sh_load flashprog.axf $entry_point"
fi
