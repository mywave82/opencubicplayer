TOPDIR=../
include $(TOPDIR)Rules.make

all: kickload.o compdate.o psetting.o plinkman.o plinkman_end.o pmain.o console.o

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
	../stuff/poutput.h
	$(CC) pmain.c -o $@ -c

psetting.o: psetting.c psetting.h \
	../config.h
	$(CC) psetting.c -o $@ -c

plinkman.o: plinkman.c plinkman.h \
	../config.h \
	../types.h \
	psetting.h
	$(CC) plinkman.c -o $@ -c

plinkman_end.o: plinkman_end.c plinkman.h \
	../config.h \
	../types.h
	$(CC) plinkman_end.c -o $@ -c


kickload.o: kickload.c \
	../config.h \
	../types.h \
	pmain.h \
	../Rules.make
	$(CC) kickload.c -o $@ -c

console.o: console.c console.h \
	../config.h \
	../types.h \
	../stuff/poutput.h
	$(CC) console.c -o $@ -c
