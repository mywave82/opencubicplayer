TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(BUILD_DUMPTOOLS),1)
	DUMPAY=dumpay
else
	DUMPAY=
endif

TARGETS=playay$(LIB_SUFFIX) $(DUMPAY)

CFLAGS+=-fPIC

all: $(TARGETS)

clean:
	rm -f *.o *$(LIB_SUFFIX) dumpay

install:
	$(CP) playay$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/95-playay$(LIB_SUFFIX)"
ifeq ($(BUILD_DUMPTOOLS),1)
	$(CP) dumpay "$(DESTDIR)$(BINDIR)/dumpay"
endif


uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/95-playay$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(BINDIR)/dumpay"

aytype.o: aytype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/cp437.h \
	../stuff/err.h \
	aytype.h
	$(CC) aytype.c -o $@ -c

playay_so=z80.o sound.o aychan.o aypplay.o ayplay.o aytype.o
playay$(LIB_SUFFIX): $(playay_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

z80.o: z80.h z80.c z80ops.c edops.c cbops.c main.h
	$(CC) -o $@ z80.c -c

sound.o: sound.c main.h z80.h sound.h \
	../config.h \
	../types.h
	$(CC) -o $@ sound.c -c

aychan.o: aychan.c \
	ayplay.h \
	sound.h \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../stuff/poutput.h
	$(CC) -o $@ aychan.c -c

aypplay.o: aypplay.c ayplay.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) -o $@ aypplay.c -c

ayplay.o: ayplay.c ayplay.h main.h sound.h z80.h \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	../stuff/imsrtns.h
	$(CC) -o $@ ayplay.c -c

dumpay: dumpay.o
	$(CC) $(LDFLAGS) -o $@ $^

dumpay.o: \
	dumpay.c \
	dumpay_z80_dis.c \
	z80.c \
	z80ops.c
	$(CC) -o $@ $< -c
