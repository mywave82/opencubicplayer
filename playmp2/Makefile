TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(BUILD_DUMPTOOLS),1)
	DUMPID3=dumpid3
else
	DUMPID3=
endif

all: mpptype.o id3.o playmp2$(LIB_SUFFIX) $(DUMPID3)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playmp2$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playmp2$(LIB_SUFFIX)"

dumpid3: dumpid3.o
	$(CC) $(LDFLAGS) -o $@ $^ -lz

dumpid3.o: dumpid3.c
	$(CC) -o $@ $< -c

cpiid3info.o: cpiid3info.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	id3.h \
	mpplay.h
	$(CC) $< -o $@ -c

cpiid3pic.o: cpiid3pic.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../cpiface/jpeg.h \
	../cpiface/png.h \
	../stuff/poutput.h \
	id3.h \
	mpplay.h
	$(CC) $< -o $@ -c

id3.o: id3.c \
	../config.h \
	../types.h \
	id3.h
	$(CC) $< -o $@ -c

mpptype.o: mpptype.c \
	../config.h \
	../types.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	id3.h \
	../stuff/cp437.h \
	../stuff/imsrtns.h \
	../stuff/utf-8.h
	$(CC) mpptype.c -o $@ -c

playmp2_so=cpiid3info.o cpiid3pic.o mppplay.o mpplay.o
playmp2$(LIB_SUFFIX): $(playmp2_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(MAD_LIBS) $(MATH_LIBS)
mppplay.o: mppplay.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/player.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	mpplay.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) mppplay.c -o $@ $(MAD_CFLAGS) -c

mpplay.o: mpplay.c \
	../config.h \
	../types.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	id3.h \
	mpplay.h \
	../stuff/imsrtns.h \
	../stuff/poll.h \
	../stuff/timer.h
	$(CC) mpplay.c -o $@ $(MAD_CFLAGS) -c
