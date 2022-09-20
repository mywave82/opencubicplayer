TOPDIR=../
include $(TOPDIR)Rules.make

all: playogg$(LIB_SUFFIX)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playogg$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/95-playogg$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/95-playogg$(LIB_SUFFIX)"

playogg_so=cpiogginfo.o cpioggpic.o oggplay.o oggpplay.o oggtype.o
playogg$(LIB_SUFFIX): $(playogg_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(OGG_LIBS) $(MATH_LIBS) $(VORBIS_LIBS) $(VORBISFILE_LIBS) $(LIBJPEG_LIBS) $(LIBPNG_LIBS)

cpiogginfo.o: cpiogginfo.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	oggplay.h
	$(CC) $< -o $@ $(VORBISFILE_CFLAGS) -c

cpioggpic.o: cpioggpic.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	oggplay.h
	$(CC) $< -o $@ $(VORBISFILE_CFLAGS) -c

oggtype.o: oggtype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	oggtype.h \
	../stuff/err.h
	$(CC) oggtype.c -o $@ -c

oggplay.o: oggplay.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../cpiface/gif.h \
	../cpiface/jpeg.h \
	../cpiface/png.h \
	../dev/deviplay.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	oggplay.h \
	../stuff/imsrtns.h
	$(CC) oggplay.c -o $@ $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(VORBISFILE_CFLAGS) -c

oggpplay.o: oggpplay.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	oggplay.h \
	oggtype.h \
	../stuff/compat.h  \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) oggpplay.c -o $@ $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(VORBISFILE_CFLAGS) -c
