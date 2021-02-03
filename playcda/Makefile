TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(CDROM_SUPPORT),1)
PLAYCDA_SO=playcda$(LIB_SUFFIX)
endif

all: $(PLAYCDA_SO)

playcda_so=cdaplay.o cdapplay.o
playcda$(LIB_SUFFIX): $(playcda_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
ifeq ($(CDROM_SUPPORT),1)
	$(CP) $(PLAYCDA_SO) "$(DESTDIR)$(LIBDIR)"
endif

uninstall:
ifeq ($(CDROM_SUPPORT),1)
	rm -f "$(DESTDIR)$(LIBDIR)/$(PLAYCDA_SO)"
endif


cdaplay.o: cdaplay.c cdaudio.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	cdaudio.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/devisamp.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/sampler.h \
	../stuff/imsrtns.h \
	../stuff/poll.h
	$(CC) cdaplay.c -o $@ -c

cdapplay.o: cdapplay.c cdaudio.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/devisamp.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) cdapplay.c -o $@ -c
