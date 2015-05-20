#!/bin/sh
#$1 = *bin

make apps_clean && make BOARD=mc200_8801 flashpatch APP_BIN_NAME=$1