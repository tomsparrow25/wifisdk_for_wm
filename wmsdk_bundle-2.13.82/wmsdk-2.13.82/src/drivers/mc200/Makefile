#                Copyright 2011, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.
#
# Makefile to build all the drivers

include $(SDK_PATH)/build/env/common.mak

DST_NAME = libdrv.a

SRCS =  mdev_gpio.c mdev_i2c.c mdev_pinmux.c mdev_iflash.c mdev_uart.c \
	mdev_gpt.c mdev_aes.c mdev_crc.c mdev_rtc.c mdev_wdt.c \
	mdev_acomp.c mdev_adc.c mdev_ssp.c mdev_dac.c mdev_sdio.c \
	lowlevel/core_cm3.o \
	lowlevel/mc200_startup.o \
	lowlevel/mc200_aes.o \
	lowlevel/mc200_clock.o \
	lowlevel/mc200_dma.o \
	lowlevel/mc200_driver.o \
	lowlevel/mc200_flash.o \
	lowlevel/mc200_gpio.o \
	lowlevel/mc200_i2c.o \
	lowlevel/mc200_pinmux.o \
	lowlevel/mc200_pmu.o \
	lowlevel/mc200_qspi0.o \
	lowlevel/mc200_qspi1.o \
	lowlevel/mc200_sdio.o \
	lowlevel/mc200_uart.o \
	lowlevel/mc200_wdt.o \
	lowlevel/mc200_xflash.o \
	lowlevel/mc200_gpt.o \
	lowlevel/mc200_crc.o \
	lowlevel/mc200_rtc.o \
	lowlevel/mc200_acomp.o \
	lowlevel/mc200_adc.o \
	lowlevel/mc200_ssp.o \
	lowlevel/mc200_spi_flash.o \
	lowlevel/mc200_dac.o

ifeq (y, $(CONFIG_USB_DRIVER))
SRCS += mdev_usb.c
SRCS += lowlevel/mc200_usb.o
endif
ifeq (y, $(CONFIG_USB_DRIVER_HOST))
SRCS += mdev_usb_host.c
SRCS += lowlevel/mc200_usb.o
endif
all:  lib

include $(SDK_PATH)/build/env/rules.mak

