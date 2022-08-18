TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(HAVE_LZW),1)
GIF_O=gif.o
endif

cpiface_so=fft.o cpianal.o cpichan.o cpidots.o cpiface.o cpigraph.o cpiinst.o cpikube.o cpilinks.o cpimsg.o cpimvol.o cpiphase.o cpipic.o cpiptype.o cpiscope.o cpitext.o cpitrack.o jpeg.o mcpedit.o $(GIF_O) png.o tga.o volctrl.o

ifeq ($(HAVE_X11),1)
NEED_CPIFONTDEBUG=1
endif
ifeq ($(HAVE_SDL),1)
NEED_CPIFONTDEBUG=1
endif
ifeq ($(HAVE_SDL2),1)
NEED_CPIFONTDEBUG=1
endif
ifeq ($(NEED_CPIFONTDEBUG),1)
ifeq ($(STATIC_BUILD),1)
 HAVE_CPIFONTDEBUG=cpifontdebug.o
else
 HAVE_CPIFONTDEBUG=cpifontdebug$(LIB_SUFFIX)
endif
endif

ifeq ($(STATIC_BUILD),1)
all: $(cpiface_so) cpikeyhelp.o $(HAVE_CPIFONTDEBUG)
else
all: cpiface$(LIB_SUFFIX) $(HAVE_CPIFONTDEBUG) cpikeyhelp.o
endif

cpiface$(LIB_SUFFIX): $(cpiface_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(LIBJPEG_LIBS) $(LIBPNG_LIBS) $(MATH_LIBS)

cpifontdebug$(LIB_SUFFIX): cpifontdebug.o
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

ifeq ($(STATIC_BUILD),1)
install:
uninstall:
else
install:
	$(CP) cpiface$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/35-cpiface$(LIB_SUFFIX)"
ifeq ($(NEED_CPIFONTDEBUG),1)
	$(CP) cpifontdebug$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/40-cpifontdebug$(LIB_SUFFIX)"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/35-cpiface$(LIB_SUFFIX)"
endif

cpikeyhelp.o: cpikeyhelp.c cpiface.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../stuff/poutput.h \
	../stuff/framelock.h
	$(CC) cpikeyhelp.c -o $@ -c

fft.o: fft.c fft.h \
	../config.h
	$(CC) fft.c -o $@ -c

cpianal.o: cpianal.c \
	../config.h \
	../types.h \
	cpiface.h \
	cpiface-private.h \
	fft.h \
	../boot/psetting.h \
	../dev/mcp.h \
	../stuff/poutput.h
	$(CC) cpianal.c -o $@ -c

cpichan.o: cpichan.c \
	cpiface.h \
	cpiface-private.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/poutput.h
	$(CC) cpichan.c -o $@ -c

cpidots.o: cpidots.c \
	cpiface.h \
	cpiface-private.h \
	cpipic.h \
	../config.h \
	../types.h \
	../stuff/poutput.h
	$(CC) cpidots.c -o $@ -c

cpifontdebug.o: cpifontdebug.c \
	cpiface.h \
	../config.h \
	../types.h \
	../boot/console.h \
	../boot/plinkman.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/poutput-fontengine.h \
	../stuff/poutput-swtext.h
	$(CC) $< -o $@ -c

cpigraph.o: cpigraph.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	cpiface.h \
	cpiface-private.h \
	cpipic.h \
	fft.h \
	../dev/mcp.h
	$(CC) cpigraph.c -o $@ -c

cpiinst.o: cpiinst.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/poutput.h \
	cpiface.h \
	cpiface-private.h
	$(CC) cpiinst.c -o $@ -c

cpikube.o: cpikube.c \
	../config.h \
	cpiface.h \
	../types.h \
	../boot/psetting.h \
	../stuff/compat.h \
	../stuff/poutput.h
	$(CC) cpikube.c -o $@ -c

cpilinks.o: cpilinks.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../filesel/pfilesel.h \
	cpiface.h \
	../boot/plinkman.h
	$(CC) cpilinks.c -o $@ -c

cpimsg.o: cpimsg.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	cpiface.h
	$(CC) cpimsg.c -o $@ -c

cpimvol.o: cpimvol.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/poutput.h \
	cpiface.h \
	cpiface-private.h
	$(CC) cpimvol.c -o $@ -c

cpiphase.o: cpiphase.c \
	../config.h \
	../types.h \
	cpiface.h \
	cpiface-private.h \
	cpipic.h \
	../dev/mcp.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h
	$(CC) cpiphase.c -o $@ -c

cpipic.o: cpipic.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	cpiface.h \
	gif.h \
	tga.h \
	../stuff/compat.h
	$(CC) cpipic.c -o $@ -c

cpiptype.o: cpiptype.c \
	../config.h \
	../types.h \
	../filesel/mdb.h
	$(CC) cpiptype.c -o $@ -c

cpiscope.o: cpiscope.c \
	../config.h \
	../types.h \
	cpiface.h \
	cpiface-private.h \
	cpipic.h \
	../dev/mcp.h \
	../stuff/poutput.h
	$(CC) cpiscope.c -o $@ -c

cpitext.o: cpitext.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../filesel/pfilesel.h \
	cpiface.h \
	cpiface-private.h \
	../boot/psetting.h \
	../boot/plinkman.h
	$(CC) cpitext.c -o $@ -c

cpitrack.o: cpitrack.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../boot/psetting.h \
	cpiface.h \
	cpiface-private.h
	$(CC) cpitrack.c -o $@ -c

mcpedit.o: mcpedit.c \
	../config.h \
	../types.h \
	cpiface.h \
	cpiface-private.h \
	../dev/deviplay.h \
	../dev/deviwave.h \
	../dev/mcp.h \
	../boot/psetting.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) mcpedit.c -o $@ -c

gif.o: gif.c \
	gif.h \
	../config.h \
	../types.h
	$(CC) gif.c -o $@ -c

jpeg.o: jpeg.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	cpiface.h \
	jpeg.h
	$(CC) $< -o $@ -c $(LIBJPEG_CFLAGS)

png.o: png.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	cpiface.h \
	png.h
	$(CC) $< -o $@ -c $(LIBPNG_CFLAGS)

tga.o: tga.c \
	tga.h \
	../config.h \
	../types.h
	$(CC) tga.c -o $@ -c

cpiface.o: cpiface.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	cpiface.h \
	cpiface-private.h \
	cpipic.h \
	../dev/ringbuffer.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	../stuff/framelock.h \
	../stuff/poutput.h \
	../stuff/poll.h \
	../stuff/sets.h
	$(CC) cpiface.c -o $@ -c

volctrl.o: volctrl.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	cpiface.h \
	cpiface-private.h \
	../stuff/imsrtns.h \
	vol.h
	$(CC) volctrl.c -o $@ -c
