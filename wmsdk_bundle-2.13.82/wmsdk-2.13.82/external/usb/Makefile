ifeq ($(SDK_PATH), )
$(error You need to give SDK_PATH)
endif

ifeq ($(NOISY),1)
AT=
else
AT=@
endif

CROSS_COMPILE := arm-none-eabi
LIBDIR := lib
OBJDIR := obj

LIBNAME := libusb.a

CC=$(CROSS_COMPILE)-gcc
AR=$(CROSS_COMPILE)-ar

CFLAGS=-mthumb -g -Os -ffunction-sections -ffreestanding -MMD -Wall \
	-fno-strict-aliasing -mcpu=cortex-m3 -fdata-sections -fno-common

CFLAGS += -I$(CURDIR) -I$(CURDIR)/incl

CFLAGS += -I$(SDK_PATH)/src/incl/libc -I$(SDK_PATH)/src/incl/platform/os/freertos \
	 -I$(SDK_PATH)/src/incl/sdk \
	-I$(SDK_PATH)/src/incl/sdk/drivers -I$(SDK_PATH)/src/incl/sdk/drivers/mc200\
	-I$(SDK_PATH)/src/incl/sdk/drivers/mc200/regs \
	-I$(FREERTOS_INC_DIR)

VPATH = src/core : src/common : src/device_class : src/device_api

C_SRCS := asic_usb.c vusbhs_dev_cncl.c vusbhs_dev_deinit.c vusbhs_dev_main.c vusbhs_dev_shut.c vusbhs_dev_utl.c dev_cncl.c dev_ep_deinit.c dev_main.c dev_recv.c dev_send.c dev_shut.c dev_utl.c agLinkedList.c usb_list.c usbdevice.c usbcdcdevice.c usbmscdevice.c usbhiddevice.c usbsysinit.c handle_MS_BOT_protocol.c MS_SCSI_read_cmds.c midia_sd_.c MS_SCSI_other_cmds.c MS_SCSI_top.c MS_SCSI_write_cmds.c

include build/rules.mk

$(LIBDIR)/$(LIBNAME): $(C_OBJS) $(ASM_OBJS)
	${AT}$(AR) rc $@ $(C_OBJS) $(ASM_OBJS)
	@echo " [ar] $@"
	@echo ""

clean:
	-@rm -f $(OBJDIR)/*.o $(OBJDIR)/*.d \
	$(OBJDIR)/*.dep $(LIBDIR)/$(LIBNAME)

