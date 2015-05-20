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

MFG_DATA_VERSION = 1

EXTRACFLAGS += 	-DHTTP_CONTENT_TYPE_JSON -DMFG_DATA_VERSION=$(MFG_DATA_VERSION)
DST_NAME = libapp_framework.a
SRCS-y =  app_main.c \
	app_network_config.c \
        app_fs.c \
        app_provisioning.c \
        app_network_mgr.c \
        app_ctrl.c \
        app_sys_http_handlers.c \
	app_psm.c \
	app_reboot.c \
	app_mdns_services.c \
	app_httpd.c

SRCS=$(SRCS-y)

#Set the flag CONFIG_AUTO_TEST_BUILD to y if test API's are required (ie.. if it is a debug build)
ifeq (y,$(CONFIG_AUTO_TEST_BUILD))
    SRCS += app_test_http_handlers.c
    EXTRACFLAGS += -DCONFIG_AUTO_TEST_BUILD
endif

#Add the P2P APIs file if it is enabled
ifeq (y, $(CONFIG_P2P))
    SRCS += app_p2p.c
endif

all:  lib

include $(SDK_PATH)/build/env/rules.mak

