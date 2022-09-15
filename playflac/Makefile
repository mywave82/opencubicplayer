TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(HAVE_FLAC),1)
TARGETS=40-playflac$(LIB_SUFFIX)
endif

CFLAGS+=-fPIC

all: $(TARGETS)

flactest: flactest.c
	$(CC) -o $@ flactest.c -lFLAC

clean:
	rm -f *.o *$(LIB_SUFFIX) flactest

install:
ifeq ($(HAVE_FLAC),1)
	$(CP) 40-playflac$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/"
endif

uninstall:
ifeq ($(HAVE_FLAC),1)
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/40-playflac$(LIB_SUFFIX)"
endif

cpiflacinfo.o: cpiflacinfo.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	flacplay.h
	$(CC) $< -o $@ -c

cpiflacpic.o: cpiflacpic.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	flacplay.h
	$(CC) $< -o $@ -c

flactype.o: flactype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	flactype.h
	$(CC) flactype.c -o $@ -c

playflac_so=cpiflacinfo.o cpiflacpic.o flacpplay.o flacplay.o flactype.o
40-playflac$(LIB_SUFFIX): $(playflac_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(FLAC_LIBS) $(LIBJPEG_LIBS) $(LIBPNG_LIBS)

flacpplay.o: flacpplay.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/pfilesel.h \
	../filesel/mdb.h \
	flacplay.h \
	flactype.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) flacpplay.c -o $@ $(FLAC_CFLAGS) -c

flacplay.o: flacplay.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../cpiface/gif.h \
	../cpiface/jpeg.h \
	../cpiface/png.h \
	../dev/deviplay.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	flacplay.h \
	../stuff/imsrtns.h
	$(CC) flacplay.c -o $@ $(FLAC_CFLAGS) -c
