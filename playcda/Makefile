TOPDIR=../
include $(TOPDIR)Rules.make

PLAYCDA_SO=40-playcda$(LIB_SUFFIX)

all: $(PLAYCDA_SO)

playcda_so=cdaplay.o cdapplay.o cdatype.o
40-playcda$(LIB_SUFFIX): $(playcda_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) $(PLAYCDA_SO) "$(DESTDIR)$(LIBDIR)/autoload/"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/$(PLAYCDA_SO)"

cdatype.o: cdatype.c cdatype.h \
	../config.h \
	../types.h \
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
