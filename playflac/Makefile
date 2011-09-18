TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(HAVE_FLAC),1)
TARGETS=flacptype$(LIB_SUFFIX) playflac$(LIB_SUFFIX)
endif

CFLAGS+=-fPIC

all: $(TARGETS)

flactest: flactest.c
	$(CC) -o $@ flactest.c -lFLAC

clean:
	rm -f *.o *$(LIB_SUFFIX) flactest

install:
ifeq ($(HAVE_FLAC),1)
	$(CP) playflac$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"
	$(CP) flacptype$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-flacptype$(LIB_SUFFIX)"
endif

uninstall:
ifeq ($(HAVE_FLAC),1)
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-flacptype$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/playflac$(LIB_SUFFIX)"
endif

flacptype_so=flacptype.o
flacptype$(LIB_SUFFIX): $(flacptype_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

flacptype.o: flacptype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h
	$(CC) flacptype.c -o $@ -c

playflac_so=flacpplay.o flacplay.o
playflac$(LIB_SUFFIX): $(playflac_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(FLAC_LIBS)

flacpplay.o: flacpplay.c \
	../config.h \
	../types.h \
	../filesel/pfilesel.h \
	../filesel/mdb.h \
	../stuff/poutput.h \
	../dev/player.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	flacplay.h \
	../stuff/sets.h \
	../stuff/compat.h \
	../dev/deviplay.h \
	../cpiface/cpiface.h
	$(CC) flacpplay.c -o $@ $(FLAC_CFLAGS) -c

flacplay.o: flacplay.c \
	../config.h \
	../types.h \
	../stuff/poll.h \
	../stuff/timer.h \
	../dev/player.h \
	../dev/deviplay.h \
	../dev/plrasm.h \
	../stuff/imsrtns.h \
	flacplay.h
	$(CC) flacplay.c -o $@ $(FLAC_CFLAGS) -c
