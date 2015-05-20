# Copyright 2008-2013-2013 Marvell International Ltd.
# All Rights Reserved.

include $(SDK_PATH)/build/env/common.mak

DST_NAME = libutil.a
SRCS =  boot_flags.c flash.c hexbin.c epoch.c random.c dumphex.c panic.c \
	crc/crc32.c wm_utils.c

ifeq ($(CONFIG_SOFTWARE_CRC32), y)
SRCS += crc/soft_crc32.c
endif

all:  lib

include $(SDK_PATH)/build/env/rules.mak
