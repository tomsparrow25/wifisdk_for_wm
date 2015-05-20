# Selecting Core
CORTEX_M = 3

# Use newlib-nano. To disable it, specify USE_NANO=
USE_NANO=--specs=nano.specs

# Use seimhosting or not
USE_SEMIHOST=--specs=rdimon.specs -lc -lc -lrdimon
USE_NOHOST=-lc -lc -lnosys

CORE=CM$(CORTEX_M)


# Options for specific architecture
ARCH_FLAGS=-mthumb -mcpu=cortex-m$(CORTEX_M)

# -Os -flto -ffunction-sections -fdata-sections to compile for code size

# ARM_GNU macro is added to include a workaround for sscanf with ARM tool chain
CFLAGS=$(ARCH_FLAGS) -Os -flto -ffunction-sections -fdata-sections -DARM_GNU

# Startup code
OBJS = startup_ARM$(CORE).o


# Link for code size
GC=-Wl,--gc-sections


LDSCRIPTS=-L. -T  mc200_flashprog.ld
LDFLAGS= $(USE_SEMIHOST) $(LDSCRIPTS) $(GC) $(USE_NANO)
