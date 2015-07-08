#                Copyright 2008-2013, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.

## Handle build display
ifeq ($(NOISY_CONF),)
ifeq ($(strip $(NOISY)),)
MAKEDIRPRINT := -s --no-print-directory
AT=@
NOISY_CONF=1
export MAKEDIRPRINT
endif
endif

## Handle SDK version
SDK_VERSION_INTERNAL=2.13
SDK_VERSION=\"$(SDK_VERSION_INTERNAL)$(EXTRA_SDK_VERSION)\"
export SDK_VERSION

COPY_CMD = $(shell which cp | tail -1 )
# Specify SDK_PATH so it does not need to be specified
# in the Makefile of each individual source component.
SDK_PATH ?= $(CURDIR)/..
# Specify INC_DIR to use the include directory under this 
# directory. The include files get copied over to the top
# level of the SDK for applications to use. 
INC_DIR = $(CURDIR)/incl
export INC_DIR SDK_PATH 
include $(SDK_PATH)/build/env/common.mak

all: all-libs version_txt

version_txt:
	echo $(SDK_VERSION) > $(SDK_PATH)/version.txt

clean: cleanlibs cleaninstall delete_symlinks
	-@rm -f $(SDK_PATH)/version.txt

install: install-libs install-headers install-boards

OS_LIBS-$(CONFIG_OS_FREERTOS) = $(CURDIR)/platform/os/freertos/

NET_LIBS-$(CONFIG_LWIP_STACK)  = $(CURDIR)/platform/net/lwip/

LIB_WLAN = $(CURDIR)/wlan/wifidriver \
	$(CURDIR)/wlan/wlcmgr \
	$(CURDIR)/wlan/btdriver

ifeq (y,$(CONFIG_HOST_SUPP))
	LIB_WLAN += $(CURDIR)/wlan/wm_supplicant
endif

# Default is the MC200 arch
ARCH_NAME-y = mc200
ARCH_LIBS-y = $(CURDIR)/platform/arch/$(ARCH_NAME-y)/

LIB_CORE_GENERIC = $(CURDIR)/core/cli \
	$(CURDIR)/core/pwrmgr \
	$(CURDIR)/core/wmstdio \
	$(CURDIR)/core/wmtime  \
	$(CURDIR)/core/healthmon \
	$(CURDIR)/core/util \
	$(CURDIR)/core/partition \
	$(CURDIR)/core/mdev \
	$(CURDIR)/core/diagnostics \
	$(CURDIR)/core/overlays

LIB_DRIVERS += $(CURDIR)/drivers/$(ARCH_NAME-y)

include $(CURDIR)/drivers/peripherals/Makefile.peripherals

LIB_DRIVERS += $(PERI-y)

LIB_MIDDLEWARE_GENERIC = $(CURDIR)/middleware/psm \
			$(CURDIR)/middleware/json \
			$(CURDIR)/middleware/ftfs \
			$(CURDIR)/middleware/telnetd \
			$(CURDIR)/middleware/httpd \
			$(CURDIR)/middleware/httpc \
			$(CURDIR)/middleware/provisioning \
			$(CURDIR)/middleware/dhcpd\
			$(CURDIR)/middleware/rfget \
			$(CURDIR)/middleware/ezxml \
			$(CURDIR)/middleware/sysinfo \
			$(CURDIR)/middleware/mdns \
			$(CURDIR)/middleware/ttcp \
			$(CURDIR)/middleware/crypto \
			$(CURDIR)/middleware/websockets \

ifeq (y, $(CONFIG_XZ_DECOMPRESSION))
	LIB_MIDDLEWARE_GENERIC += $(CURDIR)/middleware/xz
endif

LIBS_LIST = $(ARCH_LIBS-y) $(OS_LIBS-y) $(LIB_CORE_GENERIC) \
		$(LIB_MIDDLEWARE_GENERIC)  $(LIB_DRIVERS)

LIBS_LIST += $(CURDIR)/libc/src

ifeq (y,$(CONFIG_WPS2))
	LIB_MIDDLEWARE_GENERIC +=  $(CURDIR)/middleware/wps
endif

ifeq (y, $(CONFIG_CYASSL))
	LIB_MIDDLEWARE_GENERIC +=  $(CURDIR)/middleware/tls
	LIB_MIDDLEWARE_GENERIC +=  $(CURDIR)/middleware/crypto/aescrypto
else
	LIB_MIDDLEWARE_GENERIC +=  $(CURDIR)/middleware/crypto/mrvlcrypto
endif

LIB_APP_FRAMEWORK = \
	$(CURDIR)/app_framework

LIBS_LIST += $(NET_LIBS-y) $(LIB_APP_FRAMEWORK) $(LIB_WLAN)

INSTALL_DIR ?= install_dir/
LIB_DIR_TARGET := $(INSTALL_DIR)/libs
INC_DIR_TARGET := $(INSTALL_DIR)/incl
BOARD_DIR_TARGET := $(INSTALL_DIR)/boards

$(LIB_DIR_TARGET):
	mkdir -p $@

# Make all the modules that go in the application.
# Iterate through the list of makefiles and "make all" recursively.
lib_targets = $(subst /,@,$(LIBS_LIST))
all-libs: $(lib_targets)

$(lib_targets):
	M=$(subst @,/,$@); \
	cdir=$$(cd $$M;pwd);MOD=$${cdir}; \
	$(MAKE) all $(MAKEDIRPRINT) -C $$M $* MOD=$$MOD;

cleanlibs:
	@for M in $(LIBS_LIST); do \
		cdir=$$(cd $$M;pwd);MOD=$${cdir}; \
		$(MAKE) clean $(MAKEDIRPRINT) -C $$M $* MOD=$$MOD; \
		if [ $$? != 0 ]; then exit 1; fi; \
		done

cleaninstall:
	-@rm -fr $(LIB_DIR_TARGET) $(INC_DIR_TARGET) $(BOARD_DIR_TARGET)
	-@rm -f make.txt

delete_symlinks:
ifeq ($(OS), Linux)
	-@rm -f ./incl/platform/arch/arch
else
	-@rm -rf ./incl/platform/arch/arch
endif

create_symlinks:
ifeq ($(OS), Linux)
	@if [ -e ./incl/platform/arch/arch -a ! -L ./incl/platform/arch/arch  ]; then echo "Error: ./src/incl/platform/arch/arch/ is not a link"; fi;
	-@rm -fr ./incl/platform/arch/arch
	@ln -s $(CURDIR)/incl/platform/arch/arch-$(ARCH_NAME-y)/ $(CURDIR)/incl/platform/arch/arch
else
# On cygwin, the compilers get confused if a symlink is used
	-@rm -fr ./incl/platform/arch/arch
	-@rm -rf ../incl/platform/arch/arch
	@/bin/cp -a $(CURDIR)/incl/platform/arch/arch-$(ARCH_NAME-y)/ $(CURDIR)/incl/platform/arch/arch
endif

install-libs: $(LIB_DIR_TARGET)
ifeq ($(TOOL_CHAIN),GNU)
	#@mkdir -p $(LIB_DIR_TARGET)
	for M in $(LIBS_LIST); do \
		$(COPY_CMD) -f -a $$M/*.a $(LIB_DIR_TARGET); \
		done
else
	@mkdir -p $(LIB_DIR_TARGET)/rvct $(LIB_DIR_TARGET)/ads 
	for M in $(LIBS_LIST); do \
		$(COPY_CMD) -f -a $$M/*.a $(LIB_DIR_TARGET)/rvct; \
		done
endif

install-headers:
	@mkdir -p $(INC_DIR_TARGET)/
	@$(COPY_CMD) -f -a -L $(CURDIR)/incl/* $(INC_DIR_TARGET)/

install-boards:
	mkdir -p $(BOARD_DIR_TARGET)/
	$(COPY_CMD) -f -a $(CURDIR)/boards/* $(BOARD_DIR_TARGET)/

strip_libs:
	@cd $(LIB_DIR_TARGET) && $(STRIP) --strip-debug *.a
