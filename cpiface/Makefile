TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(HAVE_LZW),1)
GIF_O=gif.o
endif

cpiface_so=fft.o cpianal.o cpichan.o cpidots.o cpiface.o cpigraph.o cpiinst.o cpikube.o cpilinks.o cpimsg.o cpimvol.o cpiphase.o cpipic.o cpiptype.o cpiscope.o cpitext.o cpitrack.o mcpedit.o $(GIF_O) tga.o volctrl.o

keyhelp_so=cpikeyhelp.o

ifeq ($(STATIC_BUILD),1)
all: $(cpiface_so) $(keyhelp_so)
else
all: cpiface$(LIB_SUFFIX) keyhelp$(LIB_SUFFIX)
endif

cpiface$(LIB_SUFFIX): $(cpiface_so)
	$(CC) $(SHARED_FLAGS) $(MATH_LIBS) -o $@ $^

keyhelp$(LIB_SUFFIX): $(keyhelp_so)
	$(CC) $(SHARED_FLAGS) $(MATH_LIBS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

ifeq ($(STATIC_BUILD),1)
install:
uninstall:
else
install:
	$(CP) cpiface$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/35-cpiface$(LIB_SUFFIX)"
	$(CP) keyhelp$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/15-keyhelp$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/35-cpiface$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/15-keyhelp$(LIB_SUFFIX)"
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
	fft.h \
	cpiface.h \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../boot/psetting.h
	$(CC) cpianal.c -o $@ -c

cpichan.o: cpichan.c \
	cpiface.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/poutput.h
	$(CC) cpichan.c -o $@ -c

cpidots.o: cpidots.c \
	cpiface.h \
	cpipic.h \
	../config.h \
	../types.h \
	../stuff/poutput.h
	$(CC) cpidots.c -o $@ -c

cpigraph.o: cpigraph.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	cpiface.h \
	cpipic.h \
	fft.h
	$(CC) cpigraph.c -o $@ -c

cpiinst.o: cpiinst.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/poutput.h \
	cpiface.h
	$(CC) cpiinst.c -o $@ -c

cpikube.o: cpikube.c \
	../config.h \
	cpiface.h \
	../types.h \
	../boot/psetting.h \
	../stuff/compat.h \
	../stuff/poutput.h \
	../stuff/timer.h
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
	cpiface.h
	$(CC) cpimvol.c -o $@ -c

cpiphase.o: cpiphase.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	cpiface.h \
	../stuff/imsrtns.h \
	cpipic.h
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
	../stuff/poutput.h \
	cpiface.h \
	cpipic.h
	$(CC) cpiscope.c -o $@ -c

cpitext.o: cpitext.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../filesel/pfilesel.h \
	cpiface.h \
	../boot/psetting.h \
	../boot/plinkman.h
	$(CC) cpitext.c -o $@ -c

cpitrack.o: cpitrack.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../boot/psetting.h \
	cpiface.h
	$(CC) cpitrack.c -o $@ -c

mcpedit.o: mcpedit.c \
	../config.h \
	../types.h \
	cpiface.h \
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

tga.o: tga.c \
	tga.h \
	../config.h \
	../types.h
	$(CC) tga.c -o $@ -c

cpiface.o: cpiface.c \
	../config.h \
	../types.h \
	cpiface.h \
	cpipic.h \
	../stuff/compat.h \
	../stuff/err.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../boot/plinkman.h \
	../stuff/framelock.h \
	../stuff/poutput.h \
	../boot/psetting.h \
	../boot/plinkman.h
	$(CC) cpiface.c -o $@ -c

volctrl.o: volctrl.c \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	cpiface.h \
	../stuff/imsrtns.h \
	vol.h
	$(CC) volctrl.c -o $@ -c
