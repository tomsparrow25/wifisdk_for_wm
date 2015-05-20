#                Copyright 2008-2013, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.
#
# Makefile to build the crypto module
#

include $(SDK_PATH)/build/env/common.mak

DST_NAME = libwmcrypto.a
SRCS =	wmcrypto_mem.c

ifeq ($(CONFIG_CYASSL), y)
SRCS += wmcrypto_tests.c
endif

ifeq ($(CONFIG_CYASSL), y)
SRCS += crypto_ctaocrypt.c
else
SRCS += crypto_mrvlcrypto.c
endif

all:  lib


include $(SDK_PATH)/build/env/rules.mak
