# Copyright (C) 2008-2012 Marvell International Ltd.
# All Rights Reserved.


DST_NAME = pm_mc200_wifi_demo

SRCS =  pm_mc200_wifi_demo_app.c \
	pm_mc200_wifi_demo_event_handler.c \
    board.c

EXTRACFLAGS += -I./src
LDSCRIPT = $(TOOLCHAIN_DIR)/mc200.ld

include $(TOOLCHAIN_DIR)/targets.mk
include $(TOOLCHAIN_DIR)/rules.mk
