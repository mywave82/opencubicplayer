TOPDIR=../
include $(TOPDIR)Rules.make

TARGETS=aytype$(LIB_SUFFIX) playay$(LIB_SUFFIX)

CFLAGS+=-fPIC

all: $(TARGETS)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) aytype$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-aytype$(LIB_SUFFIX)"
	$(CP) playay$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-aytype$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/playay$(LIB_SUFFIX)"

aytype_so=aytype.o
aytype$(LIB_SUFFIX): $(aytype_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

aytype.o: aytype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../stuff/compat.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h
	$(CC) aytype.c -o $@ -c

playay_so=z80.o sound.o aypplay.o ayplay.o
playay$(LIB_SUFFIX): $(playay_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

z80.o: z80.h z80.c z80ops.c edops.c cbops.c main.h
	$(CC) -o $@ z80.c -c

sound.o: sound.c main.h z80.h sound.h \
	../config.h \
	../types.h \
	../dev/player.h
	$(CC) -o $@ sound.c -c

aypplay.o: aypplay.c ayplay.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/player.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h \
	../stuff/timer.h
	$(CC) -o $@ aypplay.c -c

ayplay.o: ayplay.c ayplay.h main.h sound.h z80.h \
	../config.h \
	../types.h \
	../stuff/timer.h \
	../stuff/poll.h \
	../dev/player.h \
	../dev/deviplay.h \
	../dev/ringbuffer.h \
	../stuff/imsrtns.h \
	../dev/plrasm.h
	$(CC) -o $@ ayplay.c -c
