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

ifeq ($(APPCONFIG_PROV_EZCONNECT),y)
	EXTRACFLAGS += -DAPPCONFIG_PROV_EZCONNECT
endif

WF_SDK_PATH = ../../wifi_sdk/src

DST_NAME = xf_oil_heater

SRCS = $(WF_SDK_PATH)/main.c \
	$(WF_SDK_PATH)/led_indicator.c \
	$(WF_SDK_PATH)/tuya_sdk/driver/key.c \
	$(WF_SDK_PATH)/tuya_sdk/common/libemqtt.c \
	$(WF_SDK_PATH)/tuya_sdk/common/mqtt_client.c \
	$(WF_SDK_PATH)/tuya_sdk/common/md5.c \
	$(WF_SDK_PATH)/tuya_sdk/common/tuya_httpc.c \
	$(WF_SDK_PATH)/tuya_sdk/common/tuya_ws_db.c \
	$(WF_SDK_PATH)/tuya_sdk/common/sysdata_adapter.c \
	$(WF_SDK_PATH)/tuya_sdk/common/cJSON.c	\
	$(WF_SDK_PATH)/tuya_sdk/common/base64.c \
	$(WF_SDK_PATH)/tuya_sdk/common/uni_pointer.c \
	$(WF_SDK_PATH)/tuya_sdk/common/mem_pool.c \
	$(WF_SDK_PATH)/tuya_sdk/common/smart_wf_frame.c \
	$(WF_SDK_PATH)/tuya_sdk/common/app_agent.c \
	src/board.c \
	src/device.c

EXTRACFLAGS-$(APPCONFIG_PM_ENABLE) += -DAPPCONFIG_PM_ENABLE
EXTRACFLAGS += -I./src -D APPCONFIG_DEBUG_ENABLE=1 -D APPCONFIG_DEMO_CLOUD=1 $(EXTRACFLAGS-y)

#add by nzy 20150521
EXTRACFLAGS += -I$(dir $(BOARD_FILE))../app_framework
EXTRACFLAGS += -I$(dir $(BOARD_FILE))../middleware/provisioning
EXTRACFLAGS += -I$(WF_SDK_PATH)
EXTRACFLAGS += -I$(WF_SDK_PATH)/tuya_sdk/common 
EXTRACFLAGS += -I$(WF_SDK_PATH)/tuya_sdk/driver
SRCS += $(SRCS-y)

MAKE_FTFS = 1
FTFS_DIR  = ./www
FTFS_API_VERSION = 100

include $(TOOLCHAIN_DIR)/targets.mk
include $(TOOLCHAIN_DIR)/rules.mk
