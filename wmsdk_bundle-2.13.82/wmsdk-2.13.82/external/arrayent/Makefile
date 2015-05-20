#
# All Rights Reserved.
#

CC=arm-none-eabi-gcc
AR=arm-none-eabi-ar

ifeq ($(NOISY),1)
AT=
else
AT=@
endif

BUILD_DIR=build
SOURCE_DIR=source

all: make_dir

make_dir:
	-@rm -fr ./${BUILD_DIR}
	@mkdir -p ./${BUILD_DIR}/include
	@/bin/cp ${SOURCE_DIR}/include/aca.h ${BUILD_DIR}/include
	@/bin/cp ${SOURCE_DIR}/libaca.a ${BUILD_DIR}

clean:
	-@rm -rf ./${BUILD_DIR}
	-@touch libaca.a

