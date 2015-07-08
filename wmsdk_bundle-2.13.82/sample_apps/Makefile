# Copyright (C) 2008-2013 Marvell International Ltd.
# All Rights Reserved.

# Batch Build Applications using GNU Cross-Compiler and Make.
#
# Usage Guide:
#
#     The Applications are built against a pre-built SDK
#     and for a specified hardware board.
#
#     The pre-built SDK location is passed using SDK_PATH
#     variable. It should be an absolute location.
#
#     The board file is passed using the BOARD_FILE variable.
#
#     all:     build all the apps
#     clean:   cleans the apps directory to pristine state
#
#     install: install the app artifacts in location pointed
#              to by INSTALL_DIR variable.
#     The Makefile simply does recursive build for all the
#     apps.

ifneq ($(NOISY),1)
AT=@
SILENT=-s
endif

# Location of GNU toolchain specific files
TOOLCHAIN_DIR := $(CURDIR)/toolchains/gnu

# Install in ./bin if no INSTALL_DIR is passed
INSTALL_DIR   ?= $(CURDIR)/bin

export TOOLCHAIN_DIR INSTALL_DIR
export SDK_PATH BOARD_FILE

-include $(SDK_PATH)/.config

# the apps to build
ifeq ($(APP_BIN_NAME),all)
APPS = tuya_user/dev_access/xf_oil_heater
else
APPS = tuya_user/dev_access/$(APP_BIN_NAME)
endif

# Install only a select few apps
INSTALL_APPS = $(APPS)

all: apps what_next

app_build := $(subst /,@,$(APPS))
apps:  $(app_build)

$(app_build): check_precond
	M=$(subst @,/,$@); \
	$(MAKE) $(SILENT) -C $(CURDIR)/$$M all;\
	if [ $$? != 0 ]; then exit 1; fi; \

.PHONY: board_file

board_file:
	@for M in $(APPS); do \
		echo $$M; \
		$(MAKE) $(SILENT) -C $(CURDIR)/$$M board_file;\
		if [ $$? != 0 ]; then exit 1; fi; \
	done;


clean:
	@for M in $(APPS); do \
		$(MAKE) $(SILENT) -C $(CURDIR)/$$M clean; \
	done;

install:
	@for M in $(INSTALL_APPS); do \
		$(MAKE) $(SILENT) -C $(CURDIR)/$$M install; \
	done;

# Convenience Checking rules
#
# check_perm:
#  -- Ensure that the tarball is extracted with the right permissions
# check_sdk:
#   -- Ensure that SDK_PATH variable is set and points to a pre-built
#      SDK
#

check_precond: check_perm check_sdk

check_perm:
	@PERM=`ls -l Makefile | awk '{print $$1}'`; \
	if [  "$$PERM" =  "----------+" ]; then \
	echo "ERROR: File permissions do not seem right."; \
	echo "Please use the tar cmd line program to untar the software tarballs!";	\
	exit 1; \
	fi

check_sdk:
#   Ensure that SDK_PATH variable is set.
ifeq ($(SDK_PATH),)
	$(error "ERROR: SDK_PATH not set. Please set SDK_PATH variable to a configured and compiled SDK")
endif
	@if [ ! -e $(SDK_PATH)/.config ] || [ ! -e $(SDK_PATH)/incl/autoconf.h ]; then \
		echo "ERROR: $(SDK_PATH) does not contain configured and compiled SDK"; \
		exit 1; \
	fi

what_next: apps
	@echo
	@echo "############################################"
	@echo "Sample Apps build is now ready!"
	@echo "Now, you can go to $(SDK_PATH)/tools/mc200/OpenOCD and use:"
	@echo " ramload.sh: for loading the axf in RAM"
	@echo " flashprog.sh: for writing images to flash"
	@echo ""
	@echo "Note: If intended demo is not built, make sure"
	@echo "relevant config options are enabled."
	@echo "############################################"
	@echo
