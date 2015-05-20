# Copyright 2008-2012 Marvell International Ltd.
# All Rights Reserved.

# Linux/Cygwin?
OS := $(shell uname)

.PHONY: axf2firmware flashprog conf
#
# On Windows/Cygwin, make uses paths that start with
# /cygdrive/c/folder, while the cross toolchain uses paths like
# C:\folder.
# If the variable CYGPATH is available, then toolchains intelligently
# convert between the two path notations
#
ifneq ($(OS), Linux)
ifneq ($(OS), Darwin)
export CYGPATH=cygpath
endif
endif

all: flashprog axf2firmware

flashprog:
	$(MAKE) -C flashprog/

axf2firmware:
	$(MAKE) -C axf2firmware/

conf:
	$(MAKE) -C kconfig conf

install:
	$(MAKE) -C flashprog/ install
	$(MAKE) -C axf2firmware/ install

clean:
	$(MAKE) -C flashprog/ clean
	$(MAKE) -C axf2firmware/ clean
	$(MAKE) -C kconfig/ clean
