######################################
# Makefile rules for CyaSSL
######################################


C_OBJS := $(patsubst %.c, %.o,$(C_SRCS))
C_DEPS := $(patsubst %.o, %.d,$(C_OBJS))

C_OBJS := $(addprefix $(OBJDIR)/, $(C_OBJS))
C_DEPS := $(addprefix $(OBJDIR)/, $(C_DEPS))

ASM_OBJS := $(patsubst %.S, %.o,$(ASM_SRCS))
ASM_DEPS := $(patsubst %.o, %.d,$(ASM_OBJS))

ASM_OBJS := $(addprefix $(OBJDIR)/, $(ASM_OBJS))
ASM_DEPS := $(addprefix $(OBJDIR)/, $(ASM_DEPS))

ALL_DEPS := $(patsubst %.d, %.dep,$(C_DEPS) $(ASM_DEPS))

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
