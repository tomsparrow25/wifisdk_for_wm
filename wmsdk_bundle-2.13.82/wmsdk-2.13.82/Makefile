#                Copyright 2008-2013, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.


.PHONY: verify config menuconfig

## Handle SDK Path
SDK_PATH= $(CURDIR)
export SDK_PATH

## Handle build display
ifeq ($(strip $(NOISY)),)
MAKEDIRPRINT := -s --no-print-directory
AT=@
NOISY_CONF=1
export MAKEDIRPRINT
endif

is_space=$(shell echo "$(SDK_PATH)" | grep [[:space:]]  )
ifneq ($(is_space),)
   $(error Your SDK path contains a "space". This is not supported.)
endif

## Handle build configuration
ifeq ($(shell uname), Linux)
KCONFIG_CMD=./tools/bin/Linux/conf
KCONFIG_CMD_M=./tools/bin/Linux/mconf
else
ifeq ($(shell uname), Darwin)
KCONFIG_CMD=./tools/bin/Mac/conf
KCONFIG_CMD_M = @echo -e "'menuconfig' is not supported on Mac OS X.\nPlease try 'make config'\n\n"
AXF2FW=./tools/bin/Mac/axf2firmware
else
KCONFIG_CMD=./tools/bin/Windows/conf.exe
KCONFIG_CMD_M = @echo -e "'menuconfig' is not supported on Windows.\nPlease try 'make config'\n\n"
endif
endif

ifeq ($(INSTALL_DIR),)
export INSTALL_DIR = $(SDK_PATH)
else
# Else the user asked us to install it outside of the SDK
export INSTALL_OUTSIDE = 1
endif

## The SDK Makefile rules

all: $(AXF2FW) check_perm extern-pre-sdk-install \
	sdk_install \
	extern-post-sdk-install \
	what_next

ifeq ($(shell uname), Linux)
make_tools:

else
ifeq ($(shell uname), Darwin)
make_tools:
	$(AT)$(MAKE) -C tools/src conf
	$(AT)$(MAKE) -C tools/src axf2firmware
	$(AT)mkdir -p tools/bin/Mac
	$(AT)$(COPY_CMD) tools/src/kconfig/conf tools/bin/Mac
	$(AT)$(COPY_CMD) tools/src/axf2firmware/axf2firmware tools/bin/Mac

$(AXF2FW): make_tools

else
make_tools:
	$(AT)$(MAKE) -C tools/src conf
	$(AT)$(MAKE) -C tools/src axf2firmware
	$(AT)$(COPY_CMD) tools/src/kconfig/conf.exe tools/bin/Windows
	$(AT)$(COPY_CMD) tools/src/axf2firmware/axf2firmware.exe tools/bin/Windows

endif
endif

check_perm:
	@PERM=`ls -l Makefile | awk '{print $$1}'`; \
	if [  "$$PERM" =  "----------+" ]; then \
	echo "ERROR: File permissions do not seem right."; \
	echo "Please use the tar cmd line program to untar the SDK tarballs!";	\
	exit 1; \
	fi

-include .kconfig.d

clean: extern_clean sdk_clean tools_clean

config: $(KCONFIG_CMD)
	$(KCONFIG_CMD) ./build/config/Kconfig

menuconfig:
	$(KCONFIG_CMD_M) ./build/config/Kconfig

oldconfig: $(KCONFIG_CMD)
	$(KCONFIG_CMD) -o ./build/config/Kconfig

%_defconfig: $(KCONFIG_CMD) FORCE
	@if [ -e $(abspath $@) ]; then  \
		$(KCONFIG_CMD) -D $(abspath $@) ./build/config/Kconfig; \
		echo "Configured using $(abspath $@)"; \
	else  \
		$(KCONFIG_CMD) -D ./build/config/$@ ./build/config/Kconfig; \
		echo "Configured using ./build/config/$@"; \
	fi

$(KCONFIG_CMD): make_tools

installpkgs:
	./build/utils/installpkgs.sh

.config_notfound:
	@echo " Run $$ make <defconfig> "
	@echo " where deconfigs can be found at wmsdk-x.y.z/build/config"

.config:
	@if [ -e .config ]; then $(MAKE) oldconfig; else $(MAKE) .config_notfound; fi;

distclean: clean
	$(AT)$(MAKE) -C src delete_symlinks $(MAKEDIRPRINT)
	-@rm -f .config
	-@rm -f ./src/incl/autoconf.h

.PHONY: build_config
build_config: ./src/incl/platform/arch/arch ./src/incl/autoconf.h

./src/incl/autoconf.h: .config build/config/Kconfig
	$(AT)$(MAKE) oldconfig

./src/incl/platform/arch/arch: .config
	$(AT)$(MAKE) -C src  create_symlinks $(MAKEDIRPRINT)

.PHONY: sdk sdk_install sdk_clean
sdk: build_config extern-pre-sdk-install
	@echo "wmsdk"
	$(AT)$(MAKE) $(WMSDK_PARALLEL_MAKE) -C src $(MAKEDIRPRINT) all

sdk_install: sdk
	$(AT)$(MAKE) -C src $(MAKEDIRPRINT) install
ifeq ($(INSTALL_OUTSIDE),1)
# This is already present when we are installing inside the SDK
# i.e. INSTALL_DIR = SDK_PATH
	$(AT)$(COPY_CMD) -a .config $(INSTALL_DIR)/
	$(AT)mkdir -p $(INSTALL_DIR)/tools/
	$(AT)$(COPY_CMD) -a tools/bin $(INSTALL_DIR)/tools/
	$(AT)$(COPY_CMD) -a tools/mc200 $(INSTALL_DIR)/tools/
endif

sdk_clean:
	$(AT)make -C src $(MAKEDIRPRINT) clean

tests:
	PATH=$(PATH):./tools/bin mtf

COPY_CMD = $(shell which cp | tail -1 )
.PHONY: docs docs_clean

what_next: extern-post-sdk-install
	@echo
	@echo "############################################"
	@echo "WMSDK build is ready!"
	@echo "Now, you may build applications as:"
	@echo " # cd /path/to/sample_apps/"
	@echo " # make SDK_PATH=$(INSTALL_DIR)"
	@echo "############################################"
	@echo

docs:
	$(MAKE) -C docs/ all

docs_clean:
	$(MAKE) -C docs/ clean

.PHONY: tools tools_clean tools_install

tools_install: tools
	$(MAKE) -C tools/src/ install

tools:
	$(MAKE) -C tools/src/

tools_clean:
	$(MAKE) -C tools/src/ clean

strip_libs:
	$(AT)$(MAKE) -C src/ $@

extern_clean:
	$(AT)$(MAKE) -C external/ clean $(MAKEDIRPRINT)

extern-pre-sdk: build_config
	$(AT)$(MAKE) -C external/ pre-sdk $(MAKEDIRPRINT)

extern-post-sdk: sdk_install
	$(AT)$(MAKE) -C external/ post-sdk $(MAKEDIRPRINT)

extern-pre-sdk-install: extern-pre-sdk
	$(AT)mkdir -p $(INSTALL_DIR)/incl $(INSTALL_DIR)/libs
	$(AT)$(MAKE) -C external/ pre-sdk-install $(MAKEDIRPRINT)

extern-post-sdk-install: extern-post-sdk
	$(AT)mkdir -p $(INSTALL_DIR)/incl $(INSTALL_DIR)/libs
	$(AT)$(MAKE) -C external/ post-sdk-install $(MAKEDIRPRINT)

usb_host_install:
	$(AT)mkdir -p $(USB_INSTALL_DIR)/libs $(USB_INSTALL_DIR)/incl
	$(AT)$(MAKE) -C external/ usb_host_install $(MAKEDIRPRINT)

FORCE:
