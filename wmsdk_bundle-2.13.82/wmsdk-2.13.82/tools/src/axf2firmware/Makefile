# Copyright 2008-2012 Marvell International Ltd.
# All Rights Reserved.

OS := $(shell uname)
ifneq ($(OS), Darwin)
STATIC = -static
endif

SDK_REL_PATH=../../../

all:	axf2firmware

axf2firmware: $(SDK_REL_PATH)/src/core/util/crc/soft_crc32.c axf2firmware.c
	@echo " [cc] $@"
	@gcc -O2 -g -Wall $(STATIC) -I$(SDK_REL_PATH)/src/core/util/crc -I$(SDK_REL_PATH)/src/incl/sdk -o $@ $^

install:
	@echo " [install] axf2firmware"
	@/bin/cp -a axf2firmware $(SDK_PATH)/tools/bin/Linux/

clean:
	rm -f axf2firmware *.o
