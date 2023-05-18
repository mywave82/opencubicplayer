TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(HAVE_FLAC),1)
TARGETS=playflac$(LIB_SUFFIX)
endif

CFLAGS+=-fPIC

all: $(TARGETS)

flactest$(EXE_SUFFIX): flactest.c
	$(CC) -o $@ flactest.c $(FLAC_CFLAGS) $(FLAC_LIBS)

clean:
	rm -f *.o *$(LIB_SUFFIX) flactest$(EXE_SUFFIX)

install:
ifeq ($(HAVE_FLAC),1)
	$(CP) playflac$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playflac$(LIB_SUFFIX)"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playflac$(LIB_SUFFIX)"

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
playflac$(LIB_SUFFIX): $(playflac_so)
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
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	flacplay.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h
	$(CC) flacplay.c -o $@ $(FLAC_CFLAGS) -c
