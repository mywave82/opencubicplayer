TOPDIR=../
include $(TOPDIR)Rules.make

PLAYCDA_SO=playcda$(LIB_SUFFIX)

all: $(PLAYCDA_SO)

playcda_so=cdaplay.o cdapplay.o
playcda$(LIB_SUFFIX): $(playcda_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) $(PLAYCDA_SO) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/$(PLAYCDA_SO)"


cdaplay.o: cdaplay.c cdaudio.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	cdaudio.h \
	../dev/deviplay.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/cdrom.h \
	../filesel/filesystem.h \
	../stuff/imsrtns.h \
	../stuff/poll.h
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
