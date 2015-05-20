# Copyright (C) 2008-2012 Marvell International Ltd.
# All Rights Reserved.

# rules.mak provides the default rules to generate
#   - .o (object) files from .c files
#   - .a (archive) files from .o files
#   - clean generated artifacts
#
# This file takes care that proper handling of paths is done when
# Cygwin is used as the development host.
#

ifneq "$(INCLUDE_RULES_MAK)" "1"
INCLUDE_RULES_MAK = 1

#
# Makefile rules
# Please make sure that all the variables that this file refers to exist
# as environment variables, i.e. if they are defined in a Makefile,
# then be sure to export them.
#

# The path to be replaced for prettier output
ZAP_PATH ?= $(abspath $(SDK_PATH))
# Some make version do not have abspath implementation
ifeq ($(ZAP_PATH),)
  ZAP_PATH = $(SDK_PATH)
endif

os:=${shell uname -s}
#### .c to .o rule
$(OBJ_DIR)/%.o: %.c
	$(AT)echo " [cc] $(strip $(subst $(ZAP_PATH)/, ,$(MOD)/$<))"; \
	$(CC) -c -o $(OBJ_DIR)/$*.o $(CFLAGS) $(EXTRACFLAGS) $<
ifneq ($(findstring CYGWIN, ${os}), )
	sed -i -e 's/\t/ /;s/ \([a-zA-Z]\):/ \/cygdrive\/\1/g' ${@:.o=.d}
endif

#### .s to .o rule
$(OBJ_DIR)/%.o: %.s
	$(AT)echo " [cc] $(strip $(subst $(ZAP_PATH)/, ,$(MOD)/$<))"; \
	$(AS) -o $(OBJ_DIR)/$(notdir $*.o) $(ASFLAGS) $<

lib: $(DST_NAME)

#### Dependency management
D_LIST = $(wildcard $(addprefix $(OBJ_DIR)/, $(addsuffix .d,$(basename $(SRCS)))))
ifneq ($(D_LIST),)
	include $(D_LIST)
endif

#### Library Creation
OBJ_LIST = $(addprefix $(OBJ_DIR)/, $(addsuffix .o,$(basename $(SRCS))))
$(DST_NAME): $(OBJ_LIST) $(FTFS_FILES) $(ADDOBJ_LIST)
	$(AT)rm -f $(OBJ_DIR)/$@
	$(AT)$(AR) $(ARFLAGS) $(OBJ_DIR)/$@ $(OBJ_LIST) $(ADDOBJ_LIST) $(ARQUIET)

$(OBJ_DIR):
	$(AT)-mkdir -p $@

#### Clean artifacts
clean::
	rm -f $(OBJ_DIR)/*.o $(OBJ_LIST) $(D_LIST) $(OBJ_DIR)/*.d $(OBJ_DIR)/*.dep $(OBJ_DIR)/*.a $(OBJ_DIR)/make.txt $(CLEAN_FILES) *.layout

# Define a variable which contains just space. We use this to escape spaces in the
# path of a filename incase of ld (gcc) invocation under cygwin
space :=
space +=

# Let the target decide how to create the images
include $(TARGET-y)/Makefile.include


##### end INCLUDE_RULES_MAK 
##
endif
