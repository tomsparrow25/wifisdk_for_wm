

CFLAGS=-mcpu=cortex-m3 -g -Os -mthumb -Wall -fdata-sections -ffunction-sections
ASFLAGS=-mcpu=cortex-m3 --gdwarf-2 -mthumb-interwork

STACK_BASE=     -Wl,--defsym -Wl,__cs3_stack=0x0015FFFC
HEAP_START=     -Wl,--defsym -Wl,__cs3_heap_start=0x20000000
HEAP_END=       -Wl,--defsym -Wl,__cs3_heap_end=0x20016000

LDFLAGS=-s -T mc200-m-hosted.ld $(STACK_BASE) $(HEAP_START) $(HEAP_END) -Xlinker --gc-sections

OBJS += reset.o
