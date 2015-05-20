#                Copyright 2008-2013, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.
#
# Makefile to build the HTTP server module
#

include $(SDK_PATH)/build/env/common.mak

ifeq (y,$(CONFIG_HTTPD_TESTS))
FAKEFS_DIRS = testfiles
endif

DST_NAME = libhttpd.a
SRCS =  httpd.c http_parse.c httpd_handle.c httpd_sys.c http-strings.c httpd_ssi.c \
	httpd_wsgi.c httpd_test.c httpd_handle_file.c

all:  lib


include $(SDK_PATH)/build/env/rules.mak
