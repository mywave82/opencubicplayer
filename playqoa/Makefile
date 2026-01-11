TOPDIR=../
include $(TOPDIR)Rules.make

all: playqoa$(LIB_SUFFIX)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playqoa$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playqoa$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playqoa$(LIB_SUFFIX)"

playqoa_so=qoaplay.o qoapplay.o qoatype.o
playqoa$(LIB_SUFFIX): $(playqoa_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

qoatype.o: qoatype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	qoatype.h
	$(CC) $< -o $@ -c

qoaplay.o: qoaplay.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	qoaplay.h \
	qoa-git/qoa.h
	$(CC) $< -o $@ -c

qoapplay.o: qoapplay.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h \
	qoaplay.h \
	qoatype.h
	$(CC) $< -o $@ -c
