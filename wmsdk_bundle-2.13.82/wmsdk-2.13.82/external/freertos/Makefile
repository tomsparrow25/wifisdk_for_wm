#
# Copyright (C) 2011-2012, Marvell International Ltd.
# All Rights Reserved.
#

CC=arm-none-eabi-gcc
AR=arm-none-eabi-ar

ifeq ($(NOISY),1)
AT=
else
AT=@
endif


os:=${shell uname -s}


BUILD_DIR=build
SOURCE_DIR=Source
PORT_DIR=${SOURCE_DIR}/portable/GCC/ARM_CM3
MEMMANG_DIR=${SOURCE_DIR}/portable/MemMang

CFLAGS= -I . -I ${SOURCE_DIR}/include -I ${PORT_DIR} \
	-D inline= -D gcc \
	-mthumb \
	-mcpu=cortex-m3 \
	-fdata-sections \
	-ffunction-sections \
	-fno-common \
	-O2 \
	-MD \
	-g

ifeq ($(DEBUG_HEAP), 1)
CFLAGS+=-DDEBUG_HEAP
endif # DEBUG_HEAP

SOURCE= ${SOURCE_DIR}/list.c \
	${SOURCE_DIR}/queue.c \
	${SOURCE_DIR}/tasks.c \
	${SOURCE_DIR}/timers.c \
	${PORT_DIR}/port.c \
	${MEMMANG_DIR}/heap_4.c

OBJS=$(SOURCE:.c=.o)
DEPS=$(SOURCE:.c=.dep)

all: make_dir

libfreertos.a: ${OBJS}
	@echo " [ar] $@"
	@echo ""
	-@rm -f ${@}
	${AT}${AR} -cr ${@} ${^}

.c.o:
	@echo " [cc] $@"
	${AT}${CC} ${CFLAGS} -o ${@} -c ${<}
ifneq ($(findstring CYGWIN, ${os}), )
	sed -i -e 's/\t/ /;s/ \([a-zA-Z]\):/ \/cygdrive\/\1/g' ${@:.o=.d}
endif
	@mv ${@:.o=.d} ${@:.o=.dep}

make_dir: libfreertos.a
	-@rm -fr ./${BUILD_DIR}
	@mkdir -p ./${BUILD_DIR}/include
	@/bin/cp -pf FreeRTOSConfig.h ${SOURCE_DIR}/include/* ${PORT_DIR}/portmacro.h ./${BUILD_DIR}/include
	@/bin/cp libfreertos.a ${BUILD_DIR}

clean:
	-@rm -rf ${OBJS}
	-@rm -rf ${DEPS}
	-@rm -rf ./${BUILD_DIR}
	-@rm -f libfreertos.a

D_LIST = $(wildcard $(DEPS))
ifneq ($(D_LIST),)
	include $(D_LIST)
endif

