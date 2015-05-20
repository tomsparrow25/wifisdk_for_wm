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

DST_NAME = libmdns.a

SRCS =	mdns_main.c \
	mdns.c \
	mdns_respond.c \
	mdns_query.c \
	debug.c \
	dname.c

ifeq (y,$(CONFIG_MDNS_QUERY))
	EXTRACFLAGS += -DMDNS_QUERY_API
endif

ifeq (y,$(CONFIG_MDNS_DEBUG))
    EXTRACFLAGS += -DMDNS_LOG -DMDNS_DBG
endif
ifeq (y,$(CONFIG_MDNS_TESTS))
    EXTRACFLAGS += -DMDNS_TESTS
	SRCS += test.c
endif
ifeq (y,$(CONFIG_MDNS_CHECK_ARGS))
    EXTRACFLAGS += -DMDNS_CHECK_ARGS
endif

all:  lib

include $(SDK_PATH)/build/env/rules.mak

