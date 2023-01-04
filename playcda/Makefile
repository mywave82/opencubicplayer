TOPDIR=../
include $(TOPDIR)Rules.make

all: playcda$(LIB_SUFFIX)

playcda_so=cdaplay.o cdapplay.o cdatype.o
playcda$(LIB_SUFFIX): $(playcda_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playcda$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/95-playcda$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/95-playcda$(LIB_SUFFIX)"

cdatype.o: cdatype.c cdatype.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	cdatype.h
	$(CC) cdatype.c -o $@ -c

cdaplay.o: cdaplay.c cdaudio.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	cdaudio.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/cdrom.h \
	../filesel/filesystem.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) cdaplay.c -o $@ -c

cdapplay.o: cdapplay.c cdaudio.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/cdrom.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) cdapplay.c -o $@ -c
