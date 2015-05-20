# Makefile to build lxdialog package
#

check-lxdialog  := $(srctree)/$(src)/check-lxdialog.sh

# Use reursively expanded variables so we do not call gcc unless
# we really need to do so. (Do not call gcc as part of make mrproper)
CFLAGS += -I/usr/include/ncurses -DCURSES_LOC="<ncurses.h>" -DLOCALE

all: lxdialog

lxdialog: checklist.o menubox.o textbox.o yesno.o inputbox.o \
		 util.o lxdialog.o msgbox.o
	$(CC) -o $@ $^ -lncursesw

clean:
	rm -f lxdialog *.o
