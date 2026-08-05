TOPDIR=../
include $(TOPDIR)Rules.make

all: playopus$(LIB_SUFFIX)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playopus$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playopus$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playopus$(LIB_SUFFIX)"

playopus_so=cpiopusinfo.o opusplay.o opuspplay.o opustype.o
playopus$(LIB_SUFFIX): $(playopus_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(OPUS_LIBS) $(OGG_LIBS)

cpiopusinfo.o: cpiopusinfo.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	opusplay.h
	$(CC) $< -o $@ -c

opustype.o: opustype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	opustype.h \
	../stuff/err.h
	$(CC) $< -o $@ -c

opusplay.o: opusplay.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	opusplay.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h
	$(CC) $< -o $@ $(OPUS_CFLAGS) $(OGG_CFLAGS) -c

opuspplay.o: opuspplay.c \
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
	opusplay.h \
	opustype.h \
	../stuff/compat.h  \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) $< -o $@ $(OPSU_CFLAGS) $(OGG_CFLAGS) -c
