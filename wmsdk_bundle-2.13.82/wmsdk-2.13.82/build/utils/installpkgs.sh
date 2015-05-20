#!/bin/bash

# Copyright (C) 2010-2013, Marvell International Ltd.
# All Rights Reserved.
#
# This script can be invoked to resolve the dependencies required for wmsdk
# application development.
#
#set -x

connectivity=0
FILE="/tmp/tmpcon"

fed=$(cat /etc/issue | grep -c "Fedora")
ubu=$(cat /etc/issue | grep -c "Ubuntu")
FED_PKGS="gcc-c++ glibc-static libusb libftdi ncurses-libs python texlive-latex"
FED_64_PKGS="glibc.i686 libftdi.i686 libusb.i686 ncurses-libs.i686 "
UBUNTU_PKGS="gcc libusb-dev libftdi-dev libncursesw5 python"
UBUNTU_64_PKGS="ia32-libs lib32ncursesw5 "
VERSION=`uname -m`
check_connectivity()
{
	if [ $fed -gt 0 ]; then
		URL="www.fedoraproject.org"
	fi

	if [ $ubu -gt 0 ]; then
		URL="www.ubuntu.com"
	fi

	echo -n "Checking for internet connection..."
	wget $URL -O /dev/null -o $FILE
	if [ $(tail -5  $FILE | grep -c "saved") ]; then
		echo "ok."
		connectivity=1
	else
		echo "failed."
		connectivity=0
	fi
}

check_connectivity
if [ $fed -gt 0 ]; then
	if [ $connectivity == 1 ]; then
		echo ""
		echo "Installing packages, enter root password:"
		su - root -c "yum install $FED_PKGS"
		if [ $VERSION == "x86_64" ]; then
		    su - root -c "yum install $FED_64_PKGS"
		fi
	else
		echo "Cannot access the said package servers. Please install the following packages manually"
	        echo "$FED_PKGS"
	fi
fi

if [ $ubu -gt 0 ]; then
	if [ $connectivity == 1 ]; then
		sudo apt-get update
		sudo apt-get install $UBUNTU_PKGS
		if [ $VERSION == "x86_64" ]; then
		    sudo apt-get install $UBUNTU_64_PKGS
		    sudo ln -s /lib32/libncursesw.so.5 /lib32/libtinfo.so.5
		else
		    sudo ln -s /lib/libncursesw.so.5 /lib/libtinfo.so.5
		fi
	else
		echo "Cannot access the said package servers. Please install the following packages manually:"
		echo "$UBUNTU_PKGS"
	fi
fi

rm -f $FILE

