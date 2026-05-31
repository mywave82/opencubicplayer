TOPDIR=../
include $(TOPDIR)Rules.make

all: playwavpack$(LIB_SUFFIX) $(DUMPID3)

playwavpack_so=cpiwavpackinfo.o cpiwavpackpic.o wavpacktype.o wavpackplay.o wavpackpplay.o
playwavpack$(LIB_SUFFIX): $(playwavpack_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(WAVPACK_LIBS) -lm

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playwavpack$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playwavpack$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playwavpack$(LIB_SUFFIX)"

cpiwavpackinfo.o: cpiwavpackinfo.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	wavpackplay.h
	$(CC) $< -o $@ -c

cpiwavpackpic.o: cpiwavpackpic.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	wavpackplay.h
	$(CC) $< -o $@ -c

wavpackplay.o: wavpackplay.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h \
	wavpackplay.h
	$(CC) $< -o $@ -c

wavpackpplay.o: wavpackpplay.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../stuff/err.h \
	wavpacktype.h \
	wavpackplay.h
	$(CC) $< -o $@ -c

wavpacktype.o: wavpacktype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	wavpacktype.h
	$(CC) $< -o $@ -c
