# Copyright 2008-2012 Marvell International Ltd.
# All Rights Reserved.



IS_CS = $(shell arm-none-eabi-gcc --version | grep -i "Sourcery CodeBench" )

ifeq ($(IS_CS), )
include arm-gnu.mk
else
include code-sourcery.mk
endif


# Tools makefile for GNU tools
include $(SDK_PATH)/.config


CC=arm-none-eabi-gcc
AS=arm-none-eabi-as

.c.o:
	@echo " [cc] $<"
	@$(CC) $(CFLAGS) $(EXTRACFLAGS) -c $<
.s.o:
	@echo " [as] $<"
	@$(AS) $(ASFLAGS) -o $@ $<


build: flashprog.axf

OBJS += flashprog.o flash.o crc32.o firmware.o


ifeq (y,$(CONFIG_SPI_FLASH_DRIVER))
MC200_LIB = libspiflash.a
endif

MC200_LIB += libdrv.a libpart.a libftfs.a

LIBS = $(addprefix $(SDK_PATH)/libs/, $(MC200_LIB))
CFLAGS+=-I $(SDK_PATH)/incl/platform/arch/  \
	-I $(SDK_PATH)/incl/platform/os/freertos/  \
	-I $(SDK_PATH)/incl/freertos/  \
	-I $(SDK_PATH)/incl/sdk/drivers/mc200/ \
	-I $(SDK_PATH)/incl/sdk/drivers/mc200/regs/ \
	-I $(SDK_PATH)/incl/sdk/drivers/ \
	-I $(SDK_PATH)/incl/sdk/ \
	-include $(SDK_PATH)/incl/autoconf.h

flashprog.axf: $(OBJS)
	@echo " [make] $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

install:
	@echo " [install] flashprog.axf"
	@/bin/cp -a flashprog.axf $(SDK_PATH)/tools/mc200/OpenOCD/

clean:
	rm -f *.o flashprog.axf
