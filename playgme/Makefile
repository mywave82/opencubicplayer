TOPDIR=../
include $(TOPDIR)Rules.make

TARGETS=playgme$(LIB_SUFFIX) $(DUMPAY)

all: $(TARGETS)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playgme$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playgme$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playgme$(LIB_SUFFIX)"

cpiinfo.o: cpiinfo.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../playgme/gmeplay.h \
	../stuff/poutput.h
	$(CC) $(LIBGME_CFLAGS) $< -o $@ -c

gmeplay.o: gmeplay.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	../playgme/gmeplay.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) $(LIBGME_CFLAGS) $< -o $@ -c

gmepplay.o: gmepplay.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../playgme/gmeplay.h \
	../playgme/gmetype.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) $< -o $@ -c

gmetype.o: gmetype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../playgme/gmetype.h \
	../stuff/compat.h \
	../stuff/cp437.h \
	../stuff/err.h
	$(CC) $< -o $@ -c

playgme_so=cpiinfo.o gmetype.o gmeplay.o gmepplay.o
playgme$(LIB_SUFFIX): $(playgme_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(LIBGME_LIBS) -lz
