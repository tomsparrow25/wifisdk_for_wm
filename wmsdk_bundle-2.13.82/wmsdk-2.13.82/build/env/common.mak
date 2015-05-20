# Copyright (C) 2008-2012 Marvell International Ltd.
# All Rights Reserved.

# common.mak provides common variables required by the build system
#
# The following are some of the important variables
#
# CFLAGS: The common flags that are passed to the compiler for
# building .o (object) files from .c files
#
# INCLUDE_DIRS: The common set of include paths that are required by
# the application

ifneq "$(INCLUDE_COMMON_MAK)" "1"
INCLUDE_COMMON_MAK = 1

# Pull in the build configurations
include $(SDK_PATH)/.config
-include $(SDK_PATH)/Makefile.vars

#### Default Variables used in the build system
TOOL_CHAIN?=GNU
INC_DIR?= $(SDK_PATH)/incl
LIB_DIR_PREFIX := libs
ENV_DIR         = env
TOOLS_DIR       = $(SDK_PATH)/tools/bin
BUILD_DIR      := .
BUILD_DIR_UNX  := $(BUILD_DIR)
OBJ_DIR        := .

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
export CYGPATH=cygpath
endif

#### Flags to control output to screen
NOISY ?= $(noisy)

ifeq ($(strip $(NOISY)),)
AT := @
ARQUIET := 2> /dev/null
endif

####
ifeq ($(TOOL_CHAIN), GNU)
 include $(SDK_PATH)/build/toolchain/GNU/GNU.flags
 INCLUDE_DIRS +=  $(INC_DIR)/libc
endif

ifneq ($(OS), Linux)
export CC := $(SDK_PATH)/build/toolchain/convertcc.sh
export LD := $(SDK_PATH)/build/toolchain/convertld.sh
export AR := $(SDK_PATH)/build/toolchain/convertar.sh
endif

#### OS Specific configuration
OS_INCLUDE_DIRS-$(CONFIG_OS_FREERTOS) += $(INC_DIR)/platform/os/freertos
ifeq ($(CONFIG_OS_FREERTOS),y)
  ifeq ($(FREERTOS_INC_DIR),)
      $(error Please specify FreeRTOS include directories using FREERTOS_INC_DIR variable)
  else
      OS_INCLUDE_DIRS-$(CONFIG_OS_FREERTOS) += $(FREERTOS_INC_DIR)
  endif
endif

INCLUDE_DIRS += $(OS_INCLUDE_DIRS-y)

#### Network Specific configuration
TCP_INCLUDE_DIRS-$(CONFIG_LWIP_STACK) += $(INC_DIR)/platform/net/lwip
ifeq ($(CONFIG_LWIP_STACK),y)
  ifeq ($(LWIP_INC_DIR),)
      $(error Please specify lwIP include directories using LWIP_INC_DIR variable)
  else
      TCP_INCLUDE_DIRS-$(CONFIG_LWIP_STACK) += $(LWIP_INC_DIR)
  endif
endif

#### WLAN
WD_INCLUDE_DIR-y = $(INC_DIR)/sdk/drivers/wlan
MLAN_INCLUDE_DIR-y = $(INC_DIR)/sdk/drivers/wlan/mlan

INCLUDE_DIRS += $(WD_INCLUDE_DIR-y) $(TCP_INCLUDE_DIRS-y) $(MLAN_INCLUDE_DIR-y)

#### CyaSSL Specific configuration
ifeq ($(CONFIG_CYASSL), y)
  ifeq ($(CYASSL_BASEDIR),)
    $(error Please pass CYASSL_BASEDIR=<path> to make)
  endif # CYASSL_DIR

  TMP_INCLUDE_DIRS += $(CYASSL_BASEDIR)/include $(CYASSL_BASEDIR)/cyassl/ctaocrypt \
		 $(CYASSL_BASEDIR)  \
		 $(CYASSL_BASEDIR)/cyassl  \
		 $(SDK_PATH)/src/incl/tls

  INCLUDE_DIRS += $(TMP_INCLUDE_DIRS)
  CFLAGS += -DFREERTOS -DCYASSL_LWIP -DNO_FILESYSTEM  -DNO_DEV_RANDOM -DNDEBUG -DNO_PSK -DWMSDK_BUILD \
       -DMC200_AES_HW_ACCL
endif #CONFIG_CYASSL

SUPP_INCLUDE_DIRS = $(EXT_FW_SUPPLICANT_PATH)/incl

INCLUDE_DIRS += $(SUPP_INCLUDE_DIRS)
######## USB specific configuration
ifeq ($(CONFIG_USB_DRIVER), y)
 ifeq ($(USB_INC_DIR),)
   $(error Please pass USB_INC_DIR=<path> to make)
 endif
 INCLUDE_DIRS += $(USB_INC_DIR)
endif # CONFIG_USB_DRIVER

ifeq ($(CONFIG_USB_DRIVER_HOST), y)
 INCLUDE_DIRS += $(USB_HOST_INC_DIR)
endif # CONFIG_USB_DRIVER_HOST

#### Other flags
ARFLAGS   = -r

#TODO: Remove mc200 and mc200/regs from this list
INCLUDE_DIRS +=  $(INC_DIR)/sdk \
		 $(INC_DIR)/sdk/drivers \
		 $(INC_DIR)/platform/arch \
		 $(INC_DIR)/sdk/drivers/mc200 \
		 $(INC_DIR)/sdk/drivers/mc200/regs

CFLAGS += $(addprefix -I, $(INCLUDE_DIRS))
CFLAGS += -DSDK_VERSION=$(SDK_VERSION)

# Export environment variables for rules.mak to use
export CC LD AS OS VERSION TOOL_CHAIN FWVERSION CFLAGS EXTRACFLAGS ASFLAGS
export MAKETXT_RULE

# Default target is mc200
TARGET-y=$(SDK_PATH)/build/target/mc200/

##### End INCLUDE_COMMON_MAK
endif
