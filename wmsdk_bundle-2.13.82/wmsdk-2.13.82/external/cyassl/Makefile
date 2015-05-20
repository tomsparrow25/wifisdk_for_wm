# This is a custom Makefile developed for WMSDK.
# CyaSSL pristine folder has the GNU autoconf build system. 
# We have replaced that as we cannot build it cleanly on 
# systems where autoconf is not easily available.

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(SDK_PATH), )
$(error You need to give SDK_PATH)
endif
ifeq ($(LWIP_INC_DIR), )
$(error You need to give LWIP_INC_DIR)
endif
endif # ifneq ($(MAKECMDGOALS),clean)

OS := $(shell uname)

ifeq ($(NOISY),1)
AT=
else
AT=@
endif


CROSS_COMPILE := arm-none-eabi
LIBDIR                    := lib
OBJDIR                   := obj

LIBNAME1 := libctaocrypt.a
LIBNAME2 := libcyassl.a

CC=$(CROSS_COMPILE)-gcc
AR=$(CROSS_COMPILE)-ar

CFLAGS=-mthumb -g -Os -ffunction-sections -ffreestanding -MMD \
	-Wall -fno-strict-aliasing -fno-common \
	-mcpu=cortex-m3 -fdata-sections -DFREERTOS -DCYASSL_LWIP \
	-DNO_FILESYSTEM  -DNO_DEV_RANDOM -DNDEBUG -DNO_PSK \
	-DWMSDK_BUILD -DDEBUG_CYSSL

ifneq ($(USE_SOFTWARE_AES_CRYPTO), y)
CFLAGS += -DMC200_AES_HW_ACCL
endif # USE_SOFTWARE_AES_CRYPTO

LWIP_INCLUDE_DIRS := $(addprefix -I, $(LWIP_INC_DIR))
CFLAGS += $(LWIP_INCLUDE_DIRS)
CFLAGS += -I$(CURDIR)/cyassl/ctaocrypt \
	-I$(CURDIR)\
	-I$(CURDIR)/cyassl \
	-I$(CURDIR)/include \
	-I$(CURDIR)/ctaocrypt/test

CFLAGS += -I$(SDK_PATH)/src/incl/libc \
	-I$(SDK_PATH)/src/incl/platform/os/freertos \
	-I$(SDK_PATH)/src/incl/WLAN -I$(SDK_PATH)/src/incl/platform/net/lwip \
	-I$(SDK_PATH)/src/incl/sdk -I$(SDK_PATH)/src/incl/sdk/base \
	-I$(SDK_PATH)/sample_apps/shared/incl \
	-I$(SDK_PATH)/src/incl/platform/arch

VPATH= ctaocrypt/src : src : ctaocrypt/test

C_SRCS1 := aes.c arc4.c asm.c asn.c coding.c des3.c dh.c dsa.c ecc.c ecc_fp.c \
	hc128.c hmac.c integer.c logging.c md4.c md5.c memory.c pwdbased.c \
	rabbit.c random.c ripemd.c rsa.c sha256.c sha512.c sha.c tfm.c

C_SRCS2 := internal.c io.c keys.c sniffer.c ssl.c tls.c crl.c ocsp.c

ifeq ($(INCLUDE_TEST_CODE),y)
C_SRCS2 += test.c
endif # INCLUDE_TEST_CODE

ifeq ($(ENABLE_AES_CCM), y)
CFLAGS += -DHAVE_AESCCM -DCYASSL_SHA384 -DCYASSL_SHA512
endif # ENABLE_AES_CCM

include build/rules.mk

all: $(LIBDIR)/$(LIBNAME1) $(LIBDIR)/$(LIBNAME2) license_notice

$(LIBDIR)/$(LIBNAME1):  $(C_OBJS1) $(ASM_OBJS1)
	${AT}$(AR) rc $@ $(C_OBJS1) $(ASM_OBJS1)
	@echo " [ar] $@"

$(LIBDIR)/$(LIBNAME2):  $(C_OBJS2) $(ASM_OBJS2)
	${AT}$(AR) rc $@ $(C_OBJS2) $(ASM_OBJS2)
	@echo " [ar] $@"
	@echo ""

clean:
	-@rm -f $(OBJDIR)/*.o $(OBJDIR)/*.d \
	$(OBJDIR)/*.dep $(LIBDIR)/$(LIBNAME1) \
	$(LIBDIR)/$(LIBNAME2)

license_notice:
	@echo "---- CyaSSL License ----"
	@echo -e "Note: You have enabled CyaSSL in your WMSDK \n\
	configuration. The CyaSSL software is bundled with the SDK \n\
	under license terms that allow you to use it for internal \n\
	development, testing, and evaluation purposes only. You must \n\
	enter into a separate agreement with Sawtooth Consulting \n\
	Ltd. to use the software in any other way. Please refer to the \n\
	SDK License for more details on the License terms. Please \n\
	contact your Marvell sales representative for additional \n\
	information."
	@echo "----  ----"


ifneq ($(MAKECMDGOALS),clean)
-include $(ALL_DEPS1)
-include $(ALL_DEPS2)
endif # ! ($(MAKECMDGOALS),clean)
