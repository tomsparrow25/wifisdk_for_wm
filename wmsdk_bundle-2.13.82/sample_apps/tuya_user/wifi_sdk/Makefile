# Copyright (C) 2008-2012 Marvell International Ltd.
# All Rights Reserved.
#
# Application Makefile
#
#    Builds and Installs Application ELF Executable Image (.axf)
#    and image suitable for flashing (.bin).
#
#    The Makefile is built using Makefile.targets
#    available in $(TOOLCHAIN_DIR).
#
# Usage:
#
#     Targets:
#
#	  all: Builds the firmware binary image in both ELF (axf) format
#          (suitable for loading into RAM) and bin format (suitable
#          for flashing)
#
#          Should be called with SDK_PATH set to root of a pre-built
#          SDK against which the application should be built.
#
# 		   If BOARD_FILE is set (to the absolute path to the file),
#		   then the file is copied locally (to $(SRC_DIR)/board.c) and
#          then used for build. If its not present, then the application
#          sources should have the needed board-specific functions.
#
#     clean: Cleans all the build artifacts
#
#     install: Installs ELF image, Bin Image, and MAP file to
#              $(INSTALL_DIR). By default INSTALL_DIR = ./bin.
#
# Description:
#
#   Minimally, in this file, only the following need to be specified.
#
#   SRCS     = list of source files
#   DST_NAME = prefix to be used for generated build artifcats.
#
#   Default Variables:
#
#   A number of variables are used in Makefile.targets that can be
#   overridden here.
#
#   SRC_DIR:  directory for source files (default ./src)
#   OBJ_DIR:  directory for intermediate object files (default ./obj)
#   BIN_DIR:  directory for final build artifacts (default ./bin)
#
#   LDSCRIPT: Linker script (default $(TOOLCHAIN_DIR)/mc200.ld)
#   EXTRACFLAGS: pass any additional CFLAGS to be passed to the C Compiler.

include config.mk


LDSCRIPT = $(TOOLCHAIN_DIR)/mc200.ld

ifeq ($(APPCONFIG_OVERLAY_ENABLE),y)
	LDSCRIPT = ./src/wm_demo_overlay.ld
	SRCS-OPT += overlays.c 	wm_demo_overlays.c
	EXTRACFLAGS += -DAPPCONFIG_OVERLAY_ENABLE
endif

ifeq ($(APPCONFIG_PROV_EZCONNECT),y)
	EXTRACFLAGS += -DAPPCONFIG_PROV_EZCONNECT
endif

DST_NAME = wifi_sdk

SRCS = main.c \
	reset_prov_helper.c \
	led_indicator.c \
	wmcloud.c \
	wmcloud_helper.c \
	wm_demo_wps_cli.c \
	$(SRCS-OPT) board.c \
	tuya_sdk/driver/key.c \
	tuya_sdk/common/libemqtt.c \
	tuya_sdk/common/mqtt_client.c \
	tuya_sdk/common/md5.c \
	tuya_sdk/common/tuya_httpc.c \
	tuya_sdk/common/tuya_ws_db.c \
	tuya_sdk/common/sysdata_adapter.c \
	tuya_sdk/common/cJSON.c	

ifeq (y,$(LONG_POLL_CLOUD))
SRCS += wmcloud_lp.c wm_demo_cloud.c
endif

ifeq (y,$(WEBSOCKET_CLOUD))
SRCS += wmcloud_ws.c wm_demo_cloud.c
endif

ifeq (y,$(XIVELY_CLOUD))
SRCS += wmcloud_xively.c wm_demo_xively_cloud.c
endif

ifeq (y,$(ARRAYENT_CLOUD))
SRCS += wmcloud_arrayent.c wm_demo_arrayent_cloud.c
endif


SRCS-$(APPCONFIG_PM_ENABLE) += power_mgr_helper.c
EXTRACFLAGS-$(APPCONFIG_PM_ENABLE) += -DAPPCONFIG_PM_ENABLE


EXTRACFLAGS += -I./src -D APPCONFIG_DEBUG_ENABLE=1 -D APPCONFIG_DEMO_CLOUD=1 $(EXTRACFLAGS-y)
#add by nzy 20150521
EXTRACFLAGS += -I$(dir $(BOARD_FILE))../app_framework
EXTRACFLAGS += -I$(dir $(BOARD_FILE))../middleware/provisioning
EXTRACFLAGS += -I./src/tuya_sdk/common -I./src/tuya_sdk/driver

#device information define
SW_VER ?= \"1.0.0\"
SW_TAG ?= \"xxxx\"
DEF_NAME ?= \"²å×ù\"
SCHEMA_ID ?= \"100000\"
UI_ID ?= \"100000\"

EXTRACFLAGS += -DSW_VER=${SW_VER} -DSW_TAG=${SW_TAG} -DDEF_NAME=${DEF_NAME}
EXTRACFLAGS += -DSCHEMA_ID=${SCHEMA_ID} -DUI_ID=${UI_ID}

SRCS += $(SRCS-y)

MAKE_FTFS = 1
FTFS_DIR  = ./www
FTFS_API_VERSION = 100

include $(TOOLCHAIN_DIR)/targets.mk
include $(TOOLCHAIN_DIR)/rules.mk


test:
	echo $(OBJ_LIST);
