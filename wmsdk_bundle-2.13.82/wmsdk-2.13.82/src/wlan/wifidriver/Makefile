#                Copyright 2008-2013, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.
#
# Makefile to build the module for Wifi driver

include $(SDK_PATH)/build/env/common.mak

EXTRACFLAGS += -I. -Iincl

ifeq ($(MFG),1)
EXTRACFLAGS += -DMFG_BRIDGE
endif

ifeq (y, $(FIT_FOR_PM3))
        EXTRACFLAGS += -DFIT_FOR_PM3
endif

DST_NAME = libwifidriver.a
CLEAN_FILES = $(DST_NAME)

ifeq ($(CONFIG_WPS2), y)
SRCS = wifi-wps.c
endif # CONFIG_WPS2

ifeq ($(CONFIG_P2P), y)
SRCS += wifi_p2p.c wifi-p2p.c wifi_p2p_uap.c
endif

SRCS += wifi-mem.c wifi_pwrmgr.c \
	wifi.c wifi-uap.c wifi-debug.c wifi-sdio.c mlan_uap_ioctl.c\
	mlan_11n.c mlan_11n_rxreorder.c mlan_init.c mlan_cmdevt.c \
	mlan_join.c mlan_cfp.c mlan_glue.c mlan_txrx.c mlan_sta_rx.c \
	mlan_misc.c mlan_shim.c mlan_wmm.c mlan_11n_aggr.c \
	mlan_sta_cmd.c mlan_sta_cmdresp.c mlan_sta_event.c \
	mlan_wmsdk.c mlan_11h.c mlan_scan.c mlan_11d.c \
	mlan_sta_ioctl.c mlan_sdio.c mlan_uap_cmdevent.c

all:  lib

include $(SDK_PATH)/build/env/rules.mak
