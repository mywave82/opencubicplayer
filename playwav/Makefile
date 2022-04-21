TOPDIR=../
include $(TOPDIR)Rules.make

all: wavptype.o playwav$(LIB_SUFFIX)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playwav$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playwav$(LIB_SUFFIX)"

playwav_so=wavplay.o wavpplay.o
playwav$(LIB_SUFFIX): $(playwav_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

wavplay.o: wavplay.c \
	../config.h \
	../types.h \
	../dev/deviplay.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	../stuff/imsrtns.h \
	../stuff/poll.h \
	wave.h
	$(CC) wavplay.c -o $@ -c

wavptype.o: wavptype.c \
	../config.h \
	../types.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h
	$(CC) wavptype.c -o $@ -c

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
	../dev/devigen.h \
	wave.h
	$(CC) wavpplay.c -o $@ -c
