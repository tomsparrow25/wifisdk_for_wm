# Copyright (C) 2013 Marvell International Ltd.
# All Rights Reserved.

# boot2 (semi-hosting) wrapper/helper script

#!/bin/bash

BOOT2_SEMIHOST=boot2-semihost.axf

IS_CYGWIN=`uname | grep -E CYGWIN`

if [ ! $IS_CYGWIN ] && [ `id -u` != 0 ];then
        echo "Please run $0 with root privileges!"
        exit 1
fi

if [ ! $IS_CYGWIN ]; then
	OPENOCD_PATH="./bin/linux/openocd"
else
	OPENOCD_PATH="./bin/windows/openocd"
fi

if [ ! -e "$OPENOCD_PATH" ]; then
	echo "Please execute $0 from <sdk_tools_top>/mc200/OpenOCD directory"
	exit 1
fi

if [ ! -e "$BOOT2_SEMIHOST" ]; then
	echo "File $BOOT2_SEMIHOST does not exist"
	exit 1
fi

entry_point=`readelf -h $BOOT2_SEMIHOST | grep "Entry point" | awk '{print $4}'`
$OPENOCD_PATH -f openocd.cfg -c init -c "sh_load $BOOT2_SEMIHOST $entry_point"
