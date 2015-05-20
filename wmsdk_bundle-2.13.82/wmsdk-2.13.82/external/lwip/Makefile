LWIP_SRC_DIR=$(CURDIR)/src/
LWIP_INC_DIR=$(CURDIR)/src/include/
LWIP_SYSARCH_INC_DIR=$(CURDIR)/contrib/port/FreeRTOS/STR91x/

ifeq ($(CONFIG_DEBUG_BUILD),y)
CFLAGS+= -DLWIP_SDK_DEBUG
endif

# Directory for output files (lst, obj, dep, elf, sym, map, hex, bin etc.).
OUTDIR = Debug

# Toolchain definition.
CC=arm-none-eabi-gcc
AR=arm-none-eabi-ar
OBJCOPY=arm-none-eabi-objcopy
OBJDUMP=arm-none-eabi-size
NM = arm-none-eabi-nm

ifeq ($(NOISY),1)
AT=
else
AT=@
endif

# Disable --gc-sections in case there is problem with gdb.
LINKER_FLAGS=-nostartfiles -Xlinker -o$(PROJECT_NAME).axf -Xlinker -M -Xlinker -Map=$(PROJECT_NAME).map -Xlinker --gc-sections


# Debugging format.
#DEBUG = stabs
#DEBUG = dwarf-2
DEBUG= gdb

# Optimization level, can be [0, 1, 2, 3, s].
# 0 = turn off optimization. s = optimize for size.
# (Note: 3 is not always the best optimization level. See avr-libc FAQ.)
#OPT = s
#OPT = 2
#OPT = 3
#OPT = 0
OPT = s


# Compiler flag to set the C Standard level.
# c89   - "ANSI" C
# gnu89 - c89 plus GCC extensions
# c99   - ISO C99 standard (not yet fully implemented)
# gnu99 - c99 plus GCC extensions
CSTANDARD = gnu89
FIT_FOR_PM3=n


# Compiler flags definition.
CFLAGS+=-g$(DEBUG) \
		-O$(OPT) \
		-std=$(CSTANDARD) \
		-I $(FREERTOS_INC_DIR) \
		-I ${FREERTOS_INC_DIR}/../../ \
		-I $(LWIP_INC_DIR)/ \
		-I $(LWIP_INC_DIR)/ipv4/ \
		-I $(LWIP_SYSARCH_INC_DIR)/ \
		-D STM32F10X_HD \
		-D USE_STDPERIPH_DRIVER \
		-D VECT_TAB_FLASH \
		-D GCC_ARMCM3 \
		-D inline= \
		-D ALIGN_STRUCT_END=__attribute\(\(aligned\(4\)\)\) \
		-D __LINUX_ERRNO_EXTENSIONS__ \
		-mthumb \
		-MMD \
		-mcpu=cortex-m3 \
		-ffunction-sections \
		-fdata-sections \
		-fno-common \
		-Wall -Wno-unused-function \
		-Wno-address

ifeq (y, $(FIT_FOR_PM3))
	CFLAGS += -DFIT_FOR_PM3
endif

LWIP_SOURCE = \
		$(LWIP_SRC_DIR)/api/api_lib.c \
		$(LWIP_SRC_DIR)/api/api_msg.c \
		$(LWIP_SRC_DIR)/api/err.c \
		$(LWIP_SRC_DIR)/api/netbuf.c \
		$(LWIP_SRC_DIR)/api/netdb.c \
		$(LWIP_SRC_DIR)/api/netifapi.c \
		$(LWIP_SRC_DIR)/api/sockets.c \
		$(LWIP_SRC_DIR)/api/tcpip.c \
		$(LWIP_SRC_DIR)/core/dhcp.c \
		$(LWIP_SRC_DIR)/core/dns.c \
		$(LWIP_SRC_DIR)/core/init.c \
		$(LWIP_SRC_DIR)/core/ipv4/autoip.c \
		$(LWIP_SRC_DIR)/core/ipv4/icmp.c \
		$(LWIP_SRC_DIR)/core/ipv4/igmp.c \
		$(LWIP_SRC_DIR)/core/ipv4/inet.c \
		$(LWIP_SRC_DIR)/core/ipv4/inet_chksum.c \
		$(LWIP_SRC_DIR)/core/ipv4/ip.c \
		$(LWIP_SRC_DIR)/core/ipv4/ip_addr.c \
		$(LWIP_SRC_DIR)/core/ipv4/ip_frag.c \
		$(LWIP_SRC_DIR)/core/mem.c \
		$(LWIP_SRC_DIR)/core/memp.c \
		$(LWIP_SRC_DIR)/core/netif.c \
		$(LWIP_SRC_DIR)/core/pbuf.c \
		$(LWIP_SRC_DIR)/core/raw.c \
		$(LWIP_SRC_DIR)/core/stats.c \
		$(LWIP_SRC_DIR)/core/sys.c \
		$(LWIP_SRC_DIR)/core/tcp.c \
		$(LWIP_SRC_DIR)/core/tcp_in.c \
		$(LWIP_SRC_DIR)/core/tcp_out.c \
		$(LWIP_SRC_DIR)/core/timers.c \
		$(LWIP_SRC_DIR)/core/udp.c \
		$(LWIP_SRC_DIR)/netif/etharp.c \
		$(LWIP_SYSARCH_INC_DIR)/sys_arch.c
#		$(LWIP_SRC_DIR)/netif/stsdionetif.c \

SOURCE+=$(LWIP_SOURCE)

# List of all source files without directory and file-extension.
ALLSRCBASE = $(notdir $(basename $(SOURCE)))


LIBS=

# List of all objects files.
OBJS = $(addprefix $(OUTDIR)/, $(addsuffix .o, $(ALLSRCBASE)))
D_LIST = $(wildcard $(addprefix $(CURDIR)/$(OUTDIR)/, $(addsuffix .dep, $(ALLSRCBASE))))


# Define Messages.
# English
MSG_BEGIN = -------- begin --------
MSG_END = --------  end  --------


# Rules definition. ***********************************************************

FINAL_TARGET = sdk_artifacts

all: begin gccversion $(FINAL_TARGET) end


ifneq ($(D_LIST),)
  include $(D_LIST)
endif

$(OUTDIR)/$(PROJECT_NAME).bin : $(PROJECT_NAME).axf Makefile
	@echo " [flash] $(notdir $@)"
	$(AT)$(OBJCOPY) $(PROJECT_NAME).axf -O binary $(PROJECT_NAME).bin

ifeq ($(STANDALONE),1)
$(PROJECT_NAME).axf : $(OBJS) $(OUTDIR)/startup_stm32f10x.o Makefile
	@echo " [axf] $@"
	$(AT)$(CC) $(CFLAGS) $(OBJS) $(OUTDIR)/startup_stm32f10x.o $(LIBS) $(LINKER_FLAGS)
else
$(PROJECT_NAME).axf : $(LIBS_WM) $(OUTDIR)/startup_stm32f10x.o Makefile
	@echo " [axf] $@"
	$(AT)$(CC) $(CFLAGS)   $(OUTDIR)/startup_stm32f10x.o $(LIBS) -Xlinker --start-group $(OUTDIR)/libblob.a /root/wmsdk/wmsdk/WMSDK/libs/gnu/libcli.a /root/wmsdk/wmsdk/WMSDK/libs/gnu/libjson.a /root/wmsdk/wmsdk/WMSDK/libs/gnu/libc.a /root/wmsdk/wmsdk/WMSDK/libs/gnu/libos.a /root/wmsdk/wmsdk/WMSDK/libs/gnu/libwmstdio.a $(LIBS_WM)  -Xlinker --end-group $(LINKER_FLAGS)
endif

sdk_artifacts: include_dir libs

INC_COPY_FILES_BASENAME = croutine.h FreeRTOS.h list.h mpu_wrappers.h portable.h projdefs.h queue.h semphr.h StackMacros.h task.h timers.h
INC_COPY_FILES = $(addprefix $(RTOS_SOURCE_DIR)/include/,$(INC_COPY_FILES_BASENAME))
include_dir:
	@mkdir -p ./Debug/lwip
	@/bin/cp -pfr $(LWIP_INC_DIR)/* ./Debug/lwip/
	@/bin/cp -pfr $(LWIP_SYSARCH_INC_DIR)/arch ./Debug/lwip/

LIBS_WM =  $(OUTDIR)/liblwip.a 
libs: $(LIBS_WM)

### LWIP
LWIP_SRC_BASE = $(notdir $(basename $(LWIP_SOURCE)))
LWIP_OBJS = $(addprefix $(OUTDIR)/, $(addsuffix .o, $(LWIP_SRC_BASE)))
$(OUTDIR)/liblwip.a: $(LWIP_OBJS)  
	@echo " [ar] $@"
	@echo ""
	$(AT)$(AR) cru $@ $^

# Compile: create object files from C source files.
define COMPILE_C_TEMPLATE
$(OUTDIR)/$(notdir $(basename $(1))).o : $(1)
##	@echo
	@echo " [cc] $$@"
	$(AT)(`echo $(CC) -c  $$(CFLAGS) $$< -o $$@ | sed -e 's/\/cygdrive\/\([a-zA-Z]\)\//\1:\//g'`)
	$(AT)(sed -i -e 's/ \([a-zA-Z]\):/ \/cygdrive\/\1/g' $(OUTDIR)/$(notdir $(basename $(1))).d && mv $(OUTDIR)/$(notdir $(basename $(1))).d $(OUTDIR)/$(notdir $(basename $(1))).dep)
endef
$(foreach src, $(SOURCE), $(eval $(call COMPILE_C_TEMPLATE, $(src))))


clean :
	-@rm -f $(OBJS)
	-@rm -f $(D_LIST)
	-@rm -rf $(OUTDIR)


# Eye candy.
begin:
##	@echo
	@echo $(MSG_BEGIN)
ifeq ($(FREERTOS_INC_DIR),)
  $(error Please Specify path to the FreeRTOS artifacts, using FREERTOS_INC_DIR variable)
endif


end:
	@echo $(MSG_END)
##	@echo

# Display compiler version information.
gccversion :
	@$(CC) --version
