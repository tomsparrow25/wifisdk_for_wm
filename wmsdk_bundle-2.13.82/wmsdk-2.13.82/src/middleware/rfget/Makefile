#                Copyright 2008-2013, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.
#

include $(SDK_PATH)/build/env/common.mak

DST_NAME = librfget.a
SRCS = write_firmware.c verify_firmware.c write_fs.c rfget.c

ifeq ($(CONFIG_WIFI_FW_UPGRADE), y)
SRCS += write_wifi_firmware.c
endif

EXTRACFLAGS += -DMRVL_SDK

all:  lib


include $(SDK_PATH)/build/env/rules.mak

