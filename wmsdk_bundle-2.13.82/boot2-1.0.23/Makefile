#
# Copyright (C) 2008-2013, Marvell International Ltd.
# All Rights Reserved.
#
# Makefile to build boot2-<options>-<board>.bin
#

ifeq ($(SDK_PATH),)
$(error "Please specify absolute path to SDK: SDK_PATH=/path/to/sdk")
endif

# Handle boot2 version
BOOT2_VERSION_INTERNAL=1.0
BOOT2_VERSION=\"$(BOOT2_VERSION_INTERNAL)$(BOOT2_EXTRA_VERSION)\"
export BOOT2_VERSION

#Config option - Boot override delay in ms
#export OVERRIDE_BACKUP_FIRMWARE_DELAY ?= 8000

#Config option - Secure boot password
#export SECURE_BOOT_PASSWORD ?= 0x12345678

#Config option - Debug image (semi-hosting based)
#export	DEBUG ?= boot2-semihost.axf

ifneq ($(SECURE_BOOT_PASSWORD),)
SECURITY=-secure
endif

BOARD=$(basename $(notdir $(BOARD_FILE)))

all:
ifneq ($(BOARD_FILE),)
	@cp $(BOARD_FILE) src/board.c
endif
	@$(MAKE) -C src $(DEBUG)
	@if [ -e src/boot2.bin ]; then \
	mkdir -p bin; \
	cp src/boot2.bin bin/boot2$(SECURITY).bin; \
	echo " [bin] $(CURDIR)/bin/boot2$(SECURITY).bin"; fi
	@if [ -n "$(DEBUG)" ]  && [ -e src/$(DEBUG) ]; then \
	cp src/$(DEBUG) $(SDK_PATH)/tools/mc200/OpenOCD/; \
	cp src/boot2-semihost.sh $(SDK_PATH)/tools/mc200/OpenOCD/; \
	echo " [debug] $(SDK_PATH)/tools/mc200/OpenOCD/$(DEBUG)"; \
	echo; \
	echo "############################################"; \
	echo "boot2 debug build is ready!"; \
	echo "Now, you may execute it using:"; \
	echo " # cd /path/to/WMSDK/tools/mc200/OpenOCD"; \
	echo " # sudo ./boot2-semihost.sh"; \
	echo "############################################"; \
	echo; fi

clean:
	@$(MAKE) -C src clean
	@rm -f bin/*.bin
	@rm -f $(SDK_PATH)/tools/mc200/OpenOCD/boot2-semihost.sh
	@if [ -n "$(DEBUG)" ]; then \
	rm -f $(SDK_PATH)/tools/mc200/OpenOCD/$(DEBUG); fi
