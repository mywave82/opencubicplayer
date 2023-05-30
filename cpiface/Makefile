TOPDIR=../
include $(TOPDIR)Rules.make
include Makefile-static

ifeq ($(STATIC_CORE),1)
all: $(cpiface_so) $(cpiface_libocp_so) $(cpifontdebug_so)
else
all: cpiface$(LIB_SUFFIX) $(cpiface_libocp_so) $(CPIFONTDEBUG_SO)
endif

cpiface$(LIB_SUFFIX): $(cpiface_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(LIBJPEG_LIBS) $(LIBPNG_LIBS) $(MATH_LIBS)

cpifontdebug$(LIB_SUFFIX): $(cpifontdebug_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

ifeq ($(STATIC_CORE),1)
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
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/40-cpifacefontdebug$(LIB_SUFFIX)"
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
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../cpiface/cpiface-private.h \
	../cpiface/cpipic.h \
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
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
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
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../cpiface/cpiface-private.h \
	../cpiface/cpipic.h \
	../dev/mcp.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h
	$(CC) cpiphase.c -o $@ -c

cpipic.o: cpipic.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../cpiface/cpipic.h \
	../cpiface/gif.h \
	../cpiface/tga.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h \
	../stuff/compat.h
	$(CC) cpipic.c -o $@ -c

cpiptype.o: cpiptype.c \
	../config.h \
	../types.h \
	cpiptype.h \
	../filesel/mdb.h
	$(CC) cpiptype.c -o $@ -c

cpiscope.o: cpiscope.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../cpiface/cpiface-private.h \
	../cpiface/cpipic.h \
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
	../cpiface/cpiface.h \
	../cpiface/cpiface-private.h \
	../cpiface/mcpedit.h \
	../dev/deviwave.h \
	../dev/mcp.h \
	../dev/player.h \
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
	../cpiface/cpiface.h \
	../cpiface/cpiface-private.h \
	../cpiface/cpipic.h \
	../cpiface/cpiptype.h \
	../cpiface/mcpedit.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-unix.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	../stuff/framelock.h \
	../stuff/latin1.h \
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
