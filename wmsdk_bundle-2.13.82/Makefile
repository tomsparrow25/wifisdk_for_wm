# Copyright (C) 2008-2013 Marvell International Ltd.
# All Rights Reserved.
# Batch Build Applications using GNU Cross-Compiler and Make.
#
# Usage Guide:
#
# A simple Makefile to build the WMSDK and associated components.
#
#
#     mc200_defconfig   set SDK config for mc200 board. User can also change
#                       defconfig according to its requirement. All defconfigs
#                       are available at the path "wmsdk-x.y.z/build/config"
#     all:     		build SDK, boot2, sample applications
#     clean:   		clean everything

# CONFIG_CMD_FILE stores the various build configuration parameters
#
# Following are the build configuration parameters that are being saved
# in this CONFIG_CMD_FILE:
# SDK_DIR: 		SDK directory path.
# BOOT2_DIR: 		boot2 directory path
# SAMPLE_APP_DIR:	Sample applications directory path
# BOARD: 			Name of the board for which the applications
# 					need to be build
#
# Whenever user explicitly passes the value for any one or more of these
# parameters, values are saved in the CONFIG_CMD_FILE.
# Existing values in CONFIG_CMD_FILE are overwritten in this case.
#
# On the next time when user does not pass any values for these parameters,
# the values are read from CONFIG_CMD_FILE and used for building the
# SDK and applications
#
# In case if user does not set these parameters at all, the values for these
# parameters are found and set by this Makefile itself
#
CONFIG_CMD_FILE=.config_cmd

-include Makefile.devel
-include $(CONFIG_CMD_FILE)

# Optional variables to be passed
# All of the bundled components
export SDK_DIR ?= $(abspath $(wildcard wmsdk-*))

ifdef BUILD_BOOT2
export BOOT2_DIR ?= $(abspath $(wildcard boot2*))
endif

export SAMPLE_APP_DIR ?= $(abspath $(wildcard sample_apps))
export BOARDS_DIR ?= $(SDK_DIR)/src/boards

ifeq ($(strip $(SDK_DIR)),)
${error "SDK_DIR variable undef. Is wmsdk present in current directory ?"}
endif

ifdef BUILD_BOOT2
ifeq ($(strip $(BOOT2_DIR)),)
${error "BOOT2_DIR variable undef. Is boot2 present in current directory ?"}
endif
endif

ifeq ($(strip $(SAMPLE_APP_DIR)),)
${error "SAMPLE_APP_DIR variable undef. Is sample_apps present in current directory ?"}
endif

SDK_LAST_CONFIG=.last_config
# Board File
#
# The WMSDK libraries are developed such that they are independent
# of any specific 88MC200 based board.
#
# This is achieved through a board abstraction layer that needs to
# be implemented in a "board file". Supported boards are available
# in the boards directory.
#
# Boot2 and Sample Applications are built for a specific board.
# The default board is set to lk20-v3 board (which includes an
# 88MC200-A1 chip (88 pin) and an 88W8787 based Wi-Fi module).
#
# Following are the different possible cases:
# * User do not pass values for BOARD and BOARD_FILE:
#   Following steps are executed:
# - Set the default values to BOARD and BOARD_FILE
# - This results copying BOARD_FILE to application
# - Save BOARD to CONFIG_CMD_FILE as S_BOARD
#
# * User passes value for BOARD and not for BOARD_FILE:
#   Following steps are executed:
# - Set BOARD_FILE based on value of BOARD
# - This results copying BOARD_FILE to application
# - Save BOARD to CONFIG_CMD_FILE as S_BOARD
#
# * User passes value for BOARD_FILE and not for BOARD:
#   Following steps are executed:
# - Set BOARD to the value given by S_BOARD in CONFIG_CMD_FILE
# - Use the passed value of BOARD_FILE
# - This results copying BOARD_FILE to application
#
# * User do not pass values for BOARD and BOARD_FILE after
#   going through one or more cases mentioned above:
#   Following steps are executed:
# - Set BOARD to the value given by S_BOARD in CONFIG_CMD_FILE
# - BOARD_FILE is not set, hence no board file is copied to
#   application
#

ifndef BOARD
ifndef S_BOARD
export BOARD ?= lk20-v3
else
export BOARD ?= $(S_BOARD)
endif
endif

ifeq ($(BOARD_FILE), )
BOARD_FILE := $(abspath $(BOARDS_DIR)/$(BOARD).c)
else
BOARD=`basename $(BOARD_FILE) .c`
endif
# Linux/Cygwin?
OS := $(shell uname)

#
# On Windows/Cygwin, make uses paths that start with
# /cygdrive/c/folder, while the cross toolchain uses paths like
# C:\folder.
# If the variable CYGPATH is available, then toolchains intelligently
# convert between the two path notations
#
ifneq ($(OS), Linux)
ifneq ($(OS), Darwin)
export CYGPATH=cygpath
CYG_COMPAT :=  CC=$(SDK_DIR)/build/toolchain/convertcc.sh LD=$(SDK_DIR)/build/toolchain/convertld.sh  AR=$(SDK_DIR)/build/toolchain/convertar.sh
endif
endif

# All generated binaries are stored in the directory which is named
# based on board for which they are build
BIN_TARGET= $(BOARD)

# Directory where built components will be installed
export BIN_DIR=bin
# Directory where SDK will be installed.
export SDK_BIN_PATH ?= $(CURDIR)/$(BIN_DIR)/$(notdir $(SDK_DIR))
# Directory where IAR build will be installed
SDK_IAR_PATH = $(CURDIR)/$(BIN_DIR)/wmsdk-iar
IAR_APP_DIR = $(SAMPLE_APP_DIR)
# Directory where the applications will be installed
APP_BIN_PATH = $(CURDIR)/$(BIN_DIR)/$(BIN_TARGET)

# Tools
export OPENOCD_DIR=$(SDK_DIR)/tools/mc200/OpenOCD
export TOOLS_DIR=$(SDK_DIR)/tools/src

ifdef BUILD_BOOT2
.PHONY : all clean apps apps_clean sdk sdk_clean boot2 boot2_clean help
else
.PHONY : all clean apps apps_clean sdk sdk_clean help flashpatch
endif

ifneq ($(NOISY),1)
AT=@
SILENT=-s
else
AT=
endif


# By default, build all the components - SDK, boot2, Sample Applications
#
# Build and install the sdk first as boot2 and apps require the
# pre-built SDK (libraries).
#
# When developing applications, it is often convenient to change
# the all target to only build the apps to reduce the build time.


#all: apps
ifdef BUILD_BOOT2
all: sdk boot2 apps
else
all: sdk apps
endif

# make flashpatch add by nzy 20150516
# set by user which bins want to flashpatch.
APP_BIN_NAME ?= hello_world.bin
APP_BIN_FTFS := $(wildcard $(APP_BIN_PATH)/$(basename $(APP_BIN_NAME)).ftfs)
WIFI_FW_NAME = sd8801_uapsta_beta.bin.xz
WIFI_FW_PATH = $(CURDIR)/wifi-firmware
BOOT2_NAME = boot2.bin
FLASH_PATCH_PATH = $(CURDIR)/flashpatch
OUTPUT = $(CURDIR)/output

flashpatch: apps
	${AT}cp -a $(SDK_BIN_PATH)/tools/mc200/OpenOCD/layout.txt $(FLASH_PATCH_PATH)
	${AT}echo boot2 $(FLASH_PATCH_PATH)/$(BOOT2_NAME) > $(FLASH_PATCH_PATH)/config.txt
	${AT}echo mc200fw $(APP_BIN_PATH)/$(APP_BIN_NAME) >> $(FLASH_PATCH_PATH)/config.txt
	${AT}echo wififw $(WIFI_FW_PATH)/$(WIFI_FW_NAME) >> $(FLASH_PATCH_PATH)/config.txt
ifneq ($(APP_BIN_FTFS),)
	${AT}echo ftfs $(APP_BIN_FTFS) >> $(FLASH_PATCH_PATH)/config.txt;
endif
	${AT}$(FLASH_PATCH_PATH)/flashpatch -l $(FLASH_PATCH_PATH)/layout.txt -b $(FLASH_PATCH_PATH)/config.txt
	${AT} mkdir -p $(OUTPUT)
	${AT} mv iflash.bin $(OUTPUT)/$(APP_BIN_NAME)
	${AT} rm -f xflash.bin flashprog.config flashprog.layout

# Clean everything
ifdef BUILD_BOOT2
clean: sdk_clean boot2_clean apps_clean
else
clean: sdk_clean apps_clean
endif
	${AT}rm -fr $(CURDIR)/$(BIN_DIR)/*
	${AT}rm -f $(CONFIG_CMD_FILE) $(SDK_DIR)/$(SDK_LAST_CONFIG)

# Create the bin directory which stores all the application binaries
preq: params
	${AT}mkdir -p $(APP_BIN_PATH)

# Build Sample Applications and install them in APP_BIN_PATH directory
apps: preq sdk
	@echo ""
	@echo "[Building Sample Applications]"
	${AT}$(MAKE) $(SILENT) $(WMSDK_PARALLEL_MAKE) -C $(SAMPLE_APP_DIR) SDK_PATH=$(SDK_BIN_PATH) BOARD_FILE=$(BOARD_FILE) all $(CYG_COMPAT)
	${AT}$(MAKE) $(SILENT) -C $(SAMPLE_APP_DIR) SDK_PATH=$(SDK_BIN_PATH) install INSTALL_DIR=$(APP_BIN_PATH) $(CYG_COMPAT)

# Clean Sample Applications
apps_clean:
	@echo "[Cleaning Sample Applications]"
	${AT}$(MAKE) $(SILENT) -C $(SAMPLE_APP_DIR) SDK_PATH=$(SDK_BIN_PATH) clean

# SDK Configuration
menuconfig config oldconfig:
	$(MAKE) -C $(SDK_DIR) $@

%_defconfig: FORCE
	@echo "[Configuring SDK with $@]"
	${AT}touch $(SDK_DIR)/$(SDK_LAST_CONFIG)
	@if [ $$(diff $(SDK_DIR)/build/config/$@ $(SDK_DIR)/$(SDK_LAST_CONFIG) 2> /dev/null | wc -l) -ne "0" ]; then \
		/bin/cp -af $(SDK_DIR)/build/config/$@ $(SDK_DIR)/$(SDK_LAST_CONFIG); \
		$(MAKE) -C $(SDK_DIR) $@; \
		echo "[Config file has changed, hence cleaning the SDK]"; \
		$(MAKE) $(SILENT) -C $(SDK_DIR) clean; \
	fi;
	@if [ $$(diff $@ $(SDK_DIR)/$(SDK_LAST_CONFIG) 2> /dev/null | wc -l) -ne "0" ]; then \
		/bin/cp -af $@ $(SDK_DIR)/$(SDK_LAST_CONFIG); \
		$(MAKE) -C $(SDK_DIR) $(abspath $@); \
		echo "[Config file has changed, hence cleaning the SDK]"; \
		$(MAKE) $(SILENT) -C $(SDK_DIR) clean; \
	fi;

DEFCONFIG ?=
# Build SDK
sdk: $(DEFCONFIG)
	@echo "[Building SDK]"
	${AT}mkdir -p $(SDK_BIN_PATH)
	${AT}$(MAKE) $(SILENT) -C $(SDK_DIR) all INSTALL_DIR=$(SDK_BIN_PATH)

# Clean SDK
sdk_clean:
	@echo "[Cleaning WMSDK]"
	${AT}$(MAKE) $(SILENT) -C $(SDK_DIR) INSTALL_DIR=$(SDK_BIN_PATH) clean

# Boot2 Build
#
# Build boot2 with default configuration setting.
#
ifdef BUILD_BOOT2
boot2: preq sdk
	@echo "[Building Boot2]"
	$(MAKE) $(SILENT) -C $(BOOT2_DIR) SDK_PATH=$(SDK_BIN_PATH) BOARD_FILE=$(BOARD_FILE) $(CYG_COMPAT)
	-${AT}cp $(BOOT2_DIR)/bin/boot2.bin $(APP_BIN_PATH)
endif

# Clean Boot2
ifdef BUILD_BOOT2
boot2_clean:
	@echo "[Cleaning Boot2]"
	${AT}$(MAKE) $(SILENT) -C $(BOOT2_DIR) SDK_PATH=$(SDK_BIN_PATH) clean
endif

# Help
help:
	@echo "1. Select WMSDK configuration"
	@echo "   $$ make <defconfig>"
	@echo "   where defconfig can be any of the following:"
	@echo "      mc200_defconfig - for 878x based boards"
	@echo "      mc200_8801_defconfig - for 8801 based boards"
	@echo "      other deconfigs can be found at wmsdk-x.y.z/build/config"
	@echo "2. Compile SDK and sample applications"
	ifdef BUILD_BOOT2
	@echo "   $$ make [BOARD=<board_name>] [sdk | boot2 | apps]"
	else
	@echo "   $$ make [BOARD=<board_name>] [sdk | apps]"
	endif
	@echo "   Supported boards can be found in wmsdk-x.y.z/src/boards/".
	@echo "   Default board is lk20-v3"
	@echo "         sdk   - compiles SDK"
	
	ifdef BUILD_BOOT2
	@echo "         boot2 - compiles boot2 for specified board"
	endif
	
	@echo "         apps  - compiles applications for specified board"
	
	ifdef BUILD_BOOT2
	@echo "   	<none> - Compiles everything (SDK, boot2, applications)"
	else
	@echo "   	<none> - Compiles everything (SDK, applications)"
	endif
	
	@echo "3. Compile an application"
	@echo "   $$ make [BOARD=<board_name>] APPS=<application name>"
	@echo "   where APPS is relative path of application with respect to\
	  wmsdk_bundle-2.x.y/sample_apps e.g: wlan/wm_demo"

.PHONY: tools_install tools_clean

# Tools Build
#
tools_install:
	@echo "[Building Tools]"
	$(MAKE) $(SILENT) -C $(TOOLS_DIR) SDK_PATH=$(SDK_BIN_PATH)
	@echo "[Installing Tools]"
	$(MAKE) $(SILENT) -C $(TOOLS_DIR) SDK_PATH=$(SDK_BIN_PATH) install
	$(MAKE) $(SILENT) -C $(TOOLS_DIR) SDK_PATH=$(SDK_DIR) install

tools_clean:
	@echo "[Cleaning Tools]"
	${AT}$(MAKE) $(SILENT) -C $(TOOLS_DIR) SDK_PATH=$(SDK_BIN_PATH) clean

params:
	@echo "SDK_DIR ?= $(SDK_DIR)" > $(CONFIG_CMD_FILE)
	@echo "BOOT2_DIR ?= $(BOOT2_DIR)" >> $(CONFIG_CMD_FILE)
	@echo "SAMPLE_APP_DIR ?= $(SAMPLE_APP_DIR)" >> $(CONFIG_CMD_FILE)
	@echo "REF_APPS_DIR ?= $(REF_APPS_DIR)" >> $(CONFIG_CMD_FILE)
	@echo "S_BOARD ?= $(BOARD)" >> $(CONFIG_CMD_FILE)


# Build for IAR toolchain
#
# Just Install directory is kept inside sample_apps/wmsdk and libraries are
# stripped to remove debug information
#
.PHONY : iar_sdk iar_sdk_clean

# Build SDK for IAR
iar_sdk:
	@echo "[Building SDK]"
	${AT}mkdir -p $(SDK_IAR_PATH)
	${AT}$(MAKE) $(SILENT) -C $(SDK_DIR) all INSTALL_DIR=$(SDK_IAR_PATH)
	${AT}$(MAKE) $(SILENT) -C $(SDK_DIR) strip_libs INSTALL_DIR=$(SDK_IAR_PATH)
ifneq ($(BOARD_FILE),)
	${AT}$(MAKE) $(SILENT) -C $(IAR_APP_DIR) SDK_PATH=$(SDK_IAR_PATH) BOARD_FILE=$(BOARD_FILE) board_file
endif

# Clean SDK installed for IAR
iar_sdk_clean:
	${AT}$(MAKE) $(SILENT) -C $(SDK_DIR) INSTALL_DIR=$(SDK_IAR_PATH) clean

-include Makefile.ref

FORCE:
