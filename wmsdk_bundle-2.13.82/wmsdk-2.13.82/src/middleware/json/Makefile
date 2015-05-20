#                Copyright 2008-2013, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.
#
# Makefile to build the module for Read-Only Flash File System

include $(SDK_PATH)/build/env/common.mak

DST_NAME = libjson.a
SRCS =  json_parse.c

ifeq (y,$(CONFIG_JSON_TESTS))
SRCS += test.c
endif
all:  lib


include $(SDK_PATH)/build/env/rules.mak
