TOPDIR=../
include $(TOPDIR)Rules.make

all: freverb$(LIB_SUFFIX) ireverb$(LIB_SUFFIX)

freverb_so=freverb.o
freverb$(LIB_SUFFIX): $(freverb_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(MATH_LIBS)

ireverb_so=ireverb.o
ireverb$(LIB_SUFFIX): $(ireverb_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(MATH_LIBS)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	mkdir -p "$(DESTDIR)$(LIBDIROCP)/autoload"
	$(CP) freverb$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload"
	$(CP) ireverb$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/freverb$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/ireverb$(LIB_SUFFIX)"

freverb.o: freverb.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../cpiface/vol.h \
	../dev/mcp.h \
	../dev/postproc.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) -O $< -o $@ -c

ireverb.o: ireverb.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../cpiface/vol.h \
	../dev/mcp.h \
	../dev/postproc.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) -O $< -o $@ -c
