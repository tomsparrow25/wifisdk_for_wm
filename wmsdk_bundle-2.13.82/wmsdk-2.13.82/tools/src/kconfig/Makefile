.PHONY: conf mconf

OS := $(shell uname)

ifneq ($(OS), Darwin)
STATIC = -static
endif

CFLAGS = -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -DKBUILD_NO_NLS

export CFLAGS

ifeq ($(OS), Darwin)
all: conf
else
all: conf mconf
	$(MAKE) -C lxdialog/ all
endif

conf: conf.o kxgettext.o zconf.tab.o 
	gcc $(STATIC) -o conf conf.o zconf.tab.o

mconf: mconf.o kxgettext.o zconf.tab.o
	gcc $(STATIC) -o mconf mconf.o zconf.tab.o

zconf.tab.o: zconf.tab.c lex.zconf.c zconf.hash.c

zconf.tab.c:
	cat zconf.tab.c_shipped > zconf.tab.c

lex.zconf.c:
	cat lex.zconf.c_shipped > lex.zconf.c

zconf.hash.c:
	cat zconf.hash.c_shipped > zconf.hash.c

clean:
	rm -f zconf.tab.c lex.zconf.c zconf.hash.c conf mconf *.o
	$(MAKE) -C lxdialog/ clean
