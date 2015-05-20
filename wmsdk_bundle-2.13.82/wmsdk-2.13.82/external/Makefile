# Copyright (C) 2012-2013 Marvell International Ltd.
# All Rights Reserved.

# This file is placed in the external directory that helps
# in building FreeRTOS and lwIP

export CONFIG_DEBUG_BUILD
EXTERN_DIR=$(CURDIR)
COPY_CMD = $(shell which cp | tail -1 )

include $(SDK_PATH)/.config
include $(SDK_PATH)/Makefile.vars

.PHONY: pre-sdk post-sdk clean install freertos usb usb_host btstack lwip cyassl libxively fw_supplicant

pre-sdk: freertos usb lwip cyassl btstack libxively fw_supplicant
	@echo "Pre-SDK done"
usb lwip cyassl btstack libxively fw_supplicant: freertos

post-sdk: usb_host
	@echo "Post-SDK done"

clean: freertos_clean usb_clean lwip_clean cyassl_clean btstack_clean libxively_clean usb_host_clean fw_supplicant_clean

pre-sdk-install: freertos_install usb_install lwip_install cyassl_install btstack_install libxively_install fw_supplicant_install arrayent_install

post-sdk-install: post_fw_supplicant_install

freertos:
	@echo "external/freertos"
	$(MAKE) $(WMSDK_PARALLEL_MAKE) -C freertos/

usb:
ifeq (y, $(CONFIG_USB_DRIVER))
	@echo "external/usb"
	$(MAKE) -C usb/
endif

usb_host:
ifeq (y, $(CONFIG_USB_DRIVER_HOST))
	@if [ -d "$(SDK_PATH)/../usb-host" ] ; then \
	echo "usb-host" ; \
	echo "$(SDK_PATH)/../usb-host" ; \
	$(MAKE) -C $(SDK_PATH)/../usb-host SDK_PATH=$(SDK_BIN_PATH); \
	fi
endif

btstack:
	@if [ -d "$(BTSTACK_PATH)" ] ; then \
	echo "btstack" ; \
	echo "$(BTSTACK_PATH)" ; \
	$(MAKE) -C $(BTSTACK_PATH); \
	fi

lwip:
	@echo "external/lwip"
	$(MAKE) $(WMSDK_PARALLEL_MAKE) -C lwip/ sdk_artifacts

cyassl:
ifeq (y, $(CONFIG_CYASSL))
	@echo "external/cyassl"
	 $(MAKE) $(WMSDK_PARALLEL_MAKE) -C cyassl/
endif

libxively:
	@echo "external/libxively"
	$(MAKE) $(WMSDK_PARALLEL_MAKE) -C libxively/ -f Makefile.wmsdk all
	@echo ""

fw_supplicant:
ifeq (y, $(CONFIG_HOST_SUPP))
	@if [ -d "$(FW_SUPPLICANT_PATH)" ] ; then \
	echo "fw_supplicant" ; \
	echo "$(FW_SUPPLICANT_PATH)" ; \
	$(MAKE) -C $(FW_SUPPLICANT_PATH); \
	fi
	$(AT)mkdir -p $(EXT_FW_SUPPLICANT_PATH)
endif

freertos_clean:
	$(MAKE) -C freertos/ clean

usb_clean:
ifeq (y, $(CONFIG_USB_DRIVER))
	$(MAKE) -C usb/ clean
endif

usb_host_clean:
ifeq (y, $(CONFIG_USB_DRIVER_HOST))
	@if [ -d "$(SDK_PATH)/../usb-host" ] ; then \
	$(MAKE) -C $(SDK_PATH)/../usb-host clean; \
	fi
endif

btstack_clean:
	@if [ -d "$(BTSTACK_PATH)" ] ; then \
	$(MAKE) -C $(BTSTACK_PATH) clean; \
	fi

lwip_clean:
	$(MAKE) -C lwip/ clean

cyassl_clean:
ifeq (y, $(CONFIG_CYASSL))
	$(MAKE) -C cyassl/ clean
endif

libxively_clean:
	$(MAKE) -C libxively/ -f Makefile.wmsdk clean

fw_supplicant_clean:
ifeq (y, $(CONFIG_HOST_SUPP))
	@if [ -d "$(FW_SUPPLICANT_PATH)" ] ; then \
	$(MAKE) -C $(FW_SUPPLICANT_PATH) clean; \
	rm -fr $(FW_SUPPLICANT_PATH)/include; \
	fi
endif

freertos_install:
# freertos
	rm -fr $(INSTALL_DIR)/incl/freertos
	$(AT)$(COPY_CMD) -a freertos/build/include $(INSTALL_DIR)/incl/freertos
	$(AT)$(COPY_CMD) -a freertos/build/libfreertos.a $(INSTALL_DIR)/libs/


usb_install:
# usb
ifeq (y, $(CONFIG_USB_DRIVER))
	rm -fr $(INSTALL_DIR)/incl/usb
	$(AT)$(COPY_CMD) -a usb/incl/ $(INSTALL_DIR)/incl/usb
	$(AT)$(COPY_CMD) -a usb/lib/libusb.a $(INSTALL_DIR)/libs/
endif

usb_host_install:
# usb_host
	@if [ -d "$(USB_HOST_PATH)" ] ; then \
	rm -fr $(USB_INSTALL_DIR)/incl; \
	$(COPY_CMD) -a $(USB_HOST_PATH)/incl/ $(USB_INSTALL_DIR)/incl; \
	$(COPY_CMD) -a $(USB_HOST_PATH)/libs/libusbhost.a $(USB_INSTALL_DIR)/libs; \
	fi

btstack_install:
# btstack
	@if [ -d "$(BTSTACK_PATH)" ] ; then \
	rm -fr $(INSTALL_DIR)/incl/btstack; \
	$(COPY_CMD) -a $(BTSTACK_PATH)/build/include/ $(INSTALL_DIR)/incl/btstack; \
	$(COPY_CMD) -a $(BTSTACK_PATH)/build/libbtstack.a $(INSTALL_DIR)/libs; \
	fi

lwip_install:
# lwip
	rm -fr $(INSTALL_DIR)/incl/lwip
	$(AT)$(COPY_CMD) -a lwip/Debug/lwip/ $(INSTALL_DIR)/incl/lwip
	$(AT)$(COPY_CMD) -a lwip/Debug/liblwip.a $(INSTALL_DIR)/libs/

cyassl_install:
# cyassl
ifeq (y, $(CONFIG_CYASSL))
	rm -fr $(INSTALL_DIR)/incl/cyassl
	$(AT)$(COPY_CMD) -a cyassl/cyassl/ $(INSTALL_DIR)/incl/cyassl
	$(AT)$(COPY_CMD) -a cyassl/lib/libctaocrypt.a $(INSTALL_DIR)/libs/
	$(AT)$(COPY_CMD) -a cyassl/lib/libcyassl.a $(INSTALL_DIR)/libs/
endif

libxively_install:
	rm -fr $(INSTALL_DIR)/incl/libxively
	$(AT)mkdir $(INSTALL_DIR)/incl/libxively
	$(AT)$(COPY_CMD) -a libxively/src/libxively/*.h $(INSTALL_DIR)/incl/libxively
	$(AT)$(COPY_CMD) -a libxively/obj/libxively.a $(INSTALL_DIR)/libs/

arrayent_install:
	rm -fr $(INSTALL_DIR)/incl/arrayent
	$(AT)mkdir $(INSTALL_DIR)/incl/arrayent
	$(AT)$(COPY_CMD) -a arrayent/source/include/*.h  $(INSTALL_DIR)/incl/arrayent
	$(AT)$(COPY_CMD) -a arrayent/source/libaca.a $(INSTALL_DIR)/libs/


fw_supplicant_install:
# fw_supplicant
ifeq (y, $(CONFIG_HOST_SUPP))
	@if [ -d "$(FW_SUPPLICANT_PATH)" ] ; then \
	$(AT)mkdir -p $(EXT_FW_SUPPLICANT_PATH)/incl; \
	$(AT)$(COPY_CMD) -a $(FW_SUPPLICANT_PATH)/include/* $(EXT_FW_SUPPLICANT_PATH)/incl; \
	$(AT)$(COPY_CMD) -a $(FW_SUPPLICANT_PATH)/lib/libfw_supplicant.a $(EXT_FW_SUPPLICANT_PATH)/; \
	fi
endif

post_fw_supplicant_install:
# post_fw_supplicant
ifeq (y, $(CONFIG_HOST_SUPP))
	@if [ -d "$(EXT_FW_SUPPLICANT_PATH)" ] ; then \
	rm -fr $(INSTALL_DIR)/incl/fw_supp; \
	$(AT)$(COPY_CMD) -a $(EXT_FW_SUPPLICANT_PATH)/incl/ $(INSTALL_DIR)/incl/fw_supp; \
	$(AT)$(COPY_CMD) -a $(EXT_FW_SUPPLICANT_PATH)/libfw_supplicant.a $(INSTALL_DIR)/libs; \
	fi
endif
