# Copyright (C) 2008-2012 Marvell International Ltd.
# All Rights Reserved.


DST_NAME = perf_demo

# The API version for the filesystem

SRCS =  perf_demo_app.c \
        perf_demo_event_handler.c \
        perf_demo_drv.c \
	board.c

EXTRACFLAGS += -I./src
LDSCRIPT = $(TOOLCHAIN_DIR)/mc200.ld


MAKE_FTFS = 1
FTFS_DIR  = ./www
FTFS_API_VERSION=114
include $(TOOLCHAIN_DIR)/targets.mk
include $(TOOLCHAIN_DIR)/rules.mk

EXTRACFLAGS += -I . -DFW_VERSION=\"$(FW_VERSION)\" -DAPP_DEBUG_PRINT -DPERF_DEMO_APP_DEBUG_PRINT -DFTFS_API_VERSION=$(FTFS_API_VERSION)


