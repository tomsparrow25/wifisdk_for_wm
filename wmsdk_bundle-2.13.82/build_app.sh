#!/bin/sh
#$1: auto add suffix .bin
#$2: chose board

# set app bin name
if [ -z "$1" ];then
	echo "please input the app bin name(no suffix \".bin\")!!!"
	exit 1
else
	APP_BIN_NAME=$1
fi

#set is tuya user
if [ -z "$2" ];then
	BOARD=tuya_mv_01
else
	BOARD=$2
fi

echo "BOARD=$BOARD"
echo "APP_BIN_NAME=$APP_BIN_NAME"
make apps_clean BOARD=$BOARD APP_BIN_NAME=$APP_BIN_NAME && \
make flashpatch BOARD=$BOARD APP_BIN_NAME=$APP_BIN_NAME
