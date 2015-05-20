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

DST_NAME = libos.a
SRCS =  os.c \
	block_alloc.c
all:  lib

ifeq (y,$(CONFIG_BA_TESTS))
SRCS += block_alloc_test.c
EXTRACFLAGS += -DBLOCK_ALLOC_DEBUG -DEXTENDED_BLOCK_ALLOC_DEBUG
endif

ifeq ($(CONFIG_HEAP_TESTS), y)
SRCS += heap-alloc-test.c  
endif # ifeq ($(CONFIG_HEAP_TESTS), y)

ifeq ($(CONFIG_SEMAPHORE_DEBUG), y)
SRCS += semdbg.c
endif # ifeq ($(SEMAPHORE_DEBUG), y)

include $(SDK_PATH)/build/env/rules.mak
