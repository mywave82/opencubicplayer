TOPDIR=../
include $(TOPDIR)Rules.make

all: playwav$(LIB_SUFFIX)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playwav$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playwav$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playwav$(LIB_SUFFIX)"

playwav_so=wavplay.o wavpplay.o wavtype.o
playwav$(LIB_SUFFIX): $(playwav_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

wavplay.o: wavplay.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	wave.h \
	wavtype.h
	$(CC) wavplay.c -o $@ -c

wavtype.o: wavtype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	wavtype.h
	$(CC) wavtype.c -o $@ -c

wavpplay.o: wavpplay.c \
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
	wave.h
	$(CC) wavpplay.c -o $@ -c
