#
# Copyright (C) 2008-2013, Marvell International Ltd.
# All Rights Reserved.
#
# Makefile to build boot2.bin
#

OS := $(shell uname)

CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
OBJCOPY = arm-none-eabi-objcopy

ifeq ($(NOISY),1)
AT=
else
AT=@
endif

LINKER_FLAGS =-T$(LDSCRIPT) -nostartfiles -Xlinker -oboot2.axf -Xlinker -M -Xlinker -Map=boot2.map -Xlinker --gc-sections

DEBUGGER = gdb
OPT = s
CSTANDARD = gnu89

# Compiler flags definition.
CFLAGS=-g$(DEBUGGER) \
       -O$(OPT) \
       -std=$(CSTANDARD) \
       -I . \
       -I $(SDK_PATH)/ \
       -I $(SDK_PATH)/incl/sdk/drivers/ \
       -I $(SDK_PATH)/incl/sdk/drivers/mc200/ \
       -I $(SDK_PATH)/incl/sdk/drivers/mc200/regs/ \
       -I $(SDK_PATH)/incl/platform/arch/ \
       -I $(SDK_PATH)/incl/platform/os/freertos/ \
       -I $(SDK_PATH)/incl/sdk/ \
       -I $(SDK_PATH)/incl/WLAN/ \
       -I $(SDK_PATH)/incl/freertos/ \
       -mthumb \
       -mcpu=cortex-m3 \
       -ffunction-sections \
       -fdata-sections \
       -DBOOT2_VERSION=$(BOOT2_VERSION)

INC_FLAGS = -include $(SDK_PATH)/incl/autoconf.h \

CFLAGS += $(INC_FLAGS)


ifneq ($(OVERRIDE_BACKUP_FIRMWARE_DELAY),)
CFLAGS+=-DOVERRIDE_BACKUP_FIRMWARE_DELAY=$(OVERRIDE_BACKUP_FIRMWARE_DELAY)
endif

BOOTHEADER_CFLAGS=-Wall \
		  -I $(SDK_PATH)/incl/platform/arch/arch-mc200/

ifneq ($(SECURE_BOOT_PASSWORD),)
BOOTHEADER_CFLAGS+=-DSECURE_BOOT_PASSWORD=$(SECURE_BOOT_PASSWORD)
endif

ifneq ($(DEBUG),)
CFLAGS+=-DDEBUG
endif


LDSCRIPT = boot2.ld

# Source files
SOURCE = boot2.c main.c crc32.c board.c
DRV_LIBS = libdrv.a

# Override LIB_LIST
LIBS = $(addprefix $(SDK_PATH)/libs/, $(DRV_LIBS))

# List of all objects files.
OBJS = $(addprefix ./, $(addsuffix .o,$(basename $(SOURCE))))

ifneq ($(OS), Linux)
export CYGPATH=cygpath
endif

# Options for semi-hosting based debug image
ASFLAGS=-mcpu=cortex-m3 --gdwarf-2 -mthumb-interwork
STACK_BASE=     -Wl,--defsym -Wl,__cs3_stack=0x0015FFFC
HEAP_START=     -Wl,--defsym -Wl,__cs3_heap_start=0x20000000
HEAP_END=       -Wl,--defsym -Wl,__cs3_heap_end=0x20016000
LDFLAGS=-s -T mc200-m-hosted.ld $(STACK_BASE) $(HEAP_START) $(HEAP_END) -Xlinker --gc-sections

all: boot2.bin version_txt

version_txt:
	@echo $(BOOT2_VERSION) > ../version.txt

%.o:%.c
	@echo " [cc] $@"
	$(AT)$(CC) -c  $(CFLAGS) $< -o ./$(basename $*).o

%.o:%.s
	@echo " [as] $<"
	$(AT)$(AS) $(ASFLAGS) $< -o ./$(basename $*).o

bootloader.bin : boot2.axf Makefile
	$(AT)$(OBJCOPY) boot2.axf -O binary bootloader.bin

boot2.axf : $(OBJS) Makefile
	$(AT)$(CC) $(CFLAGS) $(OBJS) $(LIBS) $(LINKER_FLAGS)

bootrom_header: bootrom_header.c soft_crc32.c
	@echo " [cc] $@"
	$(AT)gcc $(BOOTHEADER_CFLAGS) $^ -o $@

bootheader.bin: bootrom_header bootloader.bin
	@echo " [create] bootheader.bin"
	$(AT)./bootrom_header bootloader.bin

boot2.bin: bootloader.bin bootheader.bin
	@echo " [create] final boot2.bin"
	$(AT)cat bootheader.bin bootloader.bin > boot2.bin
	$(AT)rm -f bootheader.bin bootloader.bin

boot2-semihost.axf: $(OBJS) reset.o Makefile
	@echo " [make] $@"
	$(AT)$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

clean :
	-rm -f *.o
	-rm -f *.map
	-rm -f *.axf
	-rm -f *.bin
	-rm -f bootrom_header
	-rm -f ../version.txt
