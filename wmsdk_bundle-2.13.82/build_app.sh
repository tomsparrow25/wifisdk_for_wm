#!/bin/sh
#$1: *bin
#$2: is make tuya_user or samples

# set app bin name
if [ -z "$1" ];then
	APP_BIN_NAME=wifi_sdk.bin
else
	APP_BIN_NAME=$1
fi

#set is tuya user
if [ -z "$2" ];then
	TUYA_USER=1
else
	if [ "$2" != "0" ];then
		TUYA_USER=1
	else
		TUYA_USER=0
	fi
fi

# set board name
BOARD=tuya_mv_01

echo "BOARD=$BOARD"
echo "TUYA_USER=$TUYA_USER"
echo "APP_BIN_NAME=$APP_BIN_NAME"
make apps_clean && make flashpatch BOARD=$BOARD APP_BIN_NAME=$APP_BIN_NAME TUYA_USER=$TUYA_USER
