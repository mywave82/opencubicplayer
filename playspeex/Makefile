TOPDIR=../
include $(TOPDIR)Rules.make

all: playspeex$(LIB_SUFFIX)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playspeex$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playspeex$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playspeex$(LIB_SUFFIX)"

playspeex_so=cpispeexinfo.o speexpplay.o speexplay.o speextype.o
playspeex$(LIB_SUFFIX): $(playspeex_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(SPEEX_LIBS) $(OGG_LIBS)

cpispeexinfo.o: cpispeexinfo.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	speexplay.h
	$(CC) $< -o $@ -c

speextype.o: speextype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	speextype.h \
	../stuff/err.h
	$(CC) $< -o $@ -c

speexplay.o: speexplay.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	speexplay.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h
	$(CC) $< -o $@ $(SPEEX_CFLAGS) $(OGG_CFLAGS) -c

speexpplay.o: speexpplay.c \
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
	speexplay.h \
	speextype.h \
	../stuff/compat.h  \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) $< -o $@ $(SPEEX_CFLAGS) $(OGG_CFLAGS) -c
