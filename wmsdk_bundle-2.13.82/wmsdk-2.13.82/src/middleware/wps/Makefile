#                Copyright 2008-2013, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.
#
# Makefile to build the HTTP client module
#

include $(SDK_PATH)/build/env/common.mak

DST_NAME = libwps.a
SRCS =  wps_mem.c wps_state.c wps_eapol.c wps_msg.c wps_start.c wps_os.c wps_l2.c wps_util.c wps_main.c \
	wps_wlan.c

ifeq ($(CONFIG_P2P), y)
SRCS += wfd_main.c wfd_action.c
endif

ifeq ($(CONFIG_WPS_DEBUG), y)
CFLAGS += -Os -Wall -g -DSTDOUT_DEBUG
CFLAGS += -Os -Wall -g -DSTDOUT_DEBUG_MSG
endif

CFLAGS += -Os -Wall -g

all:  lib

include $(SDK_PATH)/build/env/rules.mak
