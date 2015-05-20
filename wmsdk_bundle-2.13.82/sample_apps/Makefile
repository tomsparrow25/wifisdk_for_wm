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

# All the apps to build
APPS = baremetal \
	helloworld \
	io_demo/gpio \
	io_demo/adc \
	io_demo/uart/uart_echo_demo \
	io_demo/uart/uart_dma_rx_demo \
	io_demo/uart/uart_dma_tx_demo \
	io_demo/ssp/ssp_master_demo \
	io_demo/ssp/ssp_slave_demo \
	io_demo/ssp/ssp_full_duplex_demo \
	io_demo/i2c/i2c_master_demo \
	io_demo/i2c/i2c_slave_demo \
	module_demo/cli_demo \
	module_demo/psm_demo \
	pm_demo/pm_i2c_slave \
	pm_demo/pm_i2c_master \
	power_measure_demo/pm_mc200_demo \
	power_measure_demo/pm_mc200_wifi_demo \
	perf_demo \
	cloud_demo/xively_demo \
	cloud_demo/arrayent_demo \
	wlan/wlan_uap \
	wlan/wlan_prov \
	wlan/wlan_sniffer \
	wlan/wm_demo \
	mfg/fcc_app

ifeq (y, $(CONFIG_USB_DRIVER))
APPS += io_demo/usb/usb_client_cdc
endif

ifeq (y, $(CONFIG_USB_DRIVER_HOST))
APPS += io_demo/usb/usb_host_uvc \
	io_demo/usb/usb_host_cdc
endif

ifeq (y, $(CONFIG_WPA2_ENTP))
APPS = helloworld \
	wlan/wpa2_enterprise
endif

ifeq (y, $(CONFIG_ENABLE_TLS))
APPS += module_demo/tls_demo
endif

ifeq (y, $(CONFIG_P2P))
APPS = helloworld \
	wlan/raw_p2p_demo \
	wlan/p2p_demo
endif

# add by nzy 20150518
# usr to decide to make samples or tuya code.
TUYA_USER ?= 0 
ifeq ($(TUYA_USER),1)
APPS = tuya_user/wifi_sdk
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
