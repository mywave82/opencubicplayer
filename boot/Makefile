TOPDIR=../
include $(TOPDIR)Rules.make
include Makefile-static

all: $(boot_libocp_start_so) $(boot_libocp_so) $(boot_libocp_end_so) kickload.o

clean:
	rm -f *.o *.a *$(LIB_SUFFIX)

install:

uninstall:

compdate.o: compdate.c \
	../config.h
	$(CC) compdate.c -o $@ -c

pmain.o: pmain.c pmain.h \
	../config.h \
	../types.h \
	plinkman.h \
	psetting.h \
	../stuff/err.h \
	../stuff/framelock.h \
	../stuff/poutput.h
	$(CC) pmain.c -o $@ -c

psetting.o: psetting.c psetting.h \
	../config.h \
	../stuff/compat.h
	$(CC) psetting.c -o $@ -c

plinkman.o: plinkman.c plinkman.h \
	../config.h \
	../types.h \
	psetting.h \
	../stuff/compat.h
	$(CC) plinkman.c -o $@ -c

plinkman_end.o: plinkman_end.c plinkman.h \
	../config.h \
	../types.h
	$(CC) plinkman_end.c -o $@ -c

ifeq ($(WINDOWS),1)
KICKLOAD_C=kickload-windows.c
else
KICKLOAD_C=kickload-unix.c
endif

kickload.o: $(KICKLOAD_C) \
	../config.h \
	../types.h \
	../boot/pmain.h \
	../boot/console.h \
	../stuff/poutput.h \
	../Rules.make
	$(CC) $< -o $@ -c

console.o: console.c console.h \
	../config.h \
	../types.h \
	../cpiface/gif.h \
	../cpiface/jpeg.h \
	../cpiface/png.h \
	../stuff/framelock.h \
	../stuff/poutput.h \
	../stuff/poutput-keyboard.h \
	../stuff/utf-8.h
	$(CC) console.c -o $@ -c
