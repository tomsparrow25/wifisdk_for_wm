######################################
# Makefile rules for CyaSSL
######################################


C_OBJS1 := $(patsubst %.c, %.o,$(C_SRCS1))
C_DEPS1 := $(patsubst %.o, %.d,$(C_OBJS1))

C_OBJS1 := $(addprefix $(OBJDIR)/, $(C_OBJS1))
C_DEPS1 := $(addprefix $(OBJDIR)/, $(C_DEPS1))

ASM_OBJS1 := $(patsubst %.S, %.o,$(ASM_SRCS1))
ASM_DEPS1 := $(patsubst %.o, %.d,$(ASM_OBJS1))

ASM_OBJS1 := $(addprefix $(OBJDIR)/, $(ASM_OBJS1))
ASM_DEPS1 := $(addprefix $(OBJDIR)/, $(ASM_DEPS1))

ALL_DEPS1 := $(patsubst %.d, %.dep,$(C_DEPS1) $(ASM_DEPS1))

C_OBJS2 := $(patsubst %.c, %.o,$(C_SRCS2))
C_DEPS2 := $(patsubst %.o, %.d,$(C_OBJS2))

C_OBJS2 := $(addprefix $(OBJDIR)/, $(C_OBJS2))
C_DEPS2 := $(addprefix $(OBJDIR)/, $(C_DEPS2))

ASM_OBJS2 := $(patsubst %.S, %.o,$(ASM_SRCS2))
ASM_DEPS2 := $(patsubst %.o, %.d,$(ASM_OBJS2))

ASM_OBJS2 := $(addprefix $(OBJDIR)/, $(ASM_OBJS2))
ASM_DEPS2 := $(addprefix $(OBJDIR)/, $(ASM_DEPS2))

ALL_DEPS2 := $(patsubst %.d, %.dep,$(C_DEPS2) $(ASM_DEPS2))

ifeq ($(OS), Linux)
define fix_paths_and_run
	($(1))
endef
else
define fix_paths_and_run
	(thecmd=`echo $(1) | sed -e 's/\/cygdrive\/\([a-zA-Z]\)\//\1:\//g'`; \
	$$thecmd)
endef
endif

$(OBJDIR)/%.o : %.c
	@echo " [cc] $<"
	${AT}$(call fix_paths_and_run,$(CC) -MD $(CFLAGS) -c -o $@  $<)
ifneq ($(OS), Linux)
	@sed -i -e 's/\t/ /;s/ \([a-zA-Z]\):/ \/cygdrive\/\1/g' ${@:.o=.d}
endif
	@mv ${@:.o=.d} ${@:.o=.dep}

$(OBJDIR)/%.o : %.S
	@echo " [asm] $<"
	${AT}$(CC) -MD $(ASFLAGS) -c -o $@ $<
