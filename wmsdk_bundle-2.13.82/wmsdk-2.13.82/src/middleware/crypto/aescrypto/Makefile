#                Copyright 2008-2013, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.
#
# Makefile to build the HTTP client module
#

include $(SDK_PATH)/build/env/common.mak

DST_NAME = libaescrypto.a
#SRCS =	key_algorim.c libtommath.c crypto_internal-modexp.c bignum.c dh.c encrypt.c sha256.c aes.c aes_wrap.c sha1-internal.c sha256-internal.c sha1.c
SRCS =	aes.c aes_wrap.c 


# Compiler option for debugging
#CFLAGS += -DDBG_PRINT_FUNC
CFLAGS += -Os -Wall 

# Compiler option for sha256.o
CFLAGS += -DINTERNAL_SHA256

# Compiler option for not including unused function in aes_wrap.o
CFLAGS += -DCONFIG_NO_AES_WRAP
CFLAGS += -DCONFIG_NO_AES_OMAC1
CFLAGS += -DCONFIG_NO_AES_CTR
CFLAGS += -DCONFIG_NO_AES_EAX

# Compiler option for DH big number math
CFLAGS += -DCONFIG_CRYPTO_INTERNAL
CFLAGS += -DCONFIG_INTERNAL_LIBTOMMATH
CFLAGS += -DLTM_FAST
CFLAGS += -DCONFIG_WPS

# Compiler option for Embedded Supplicant
# CFLAGS += -DINTERNAL_SHA1 -DINTERNAL_MD5

all:  lib


include $(SDK_PATH)/build/env/rules.mak
