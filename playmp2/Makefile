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
	$(CC) $(LDFLAGS) -o $@ $^

dumpid3.o: dumpid3.c
	$(CC) -o $@ $< -c

id3.o: id3.c \
	../config.h \
	../types.h \
	id3.h
	$(CC) $< -o $@ -c

mpptype.o: mpptype.c \
	../config.h \
	../types.h \
	id3.h \
	../stuff/imsrtns.h \
	../stuff/utf-8.h \
	../filesel/mdb.h
	$(CC) mpptype.c -o $@ -c

playmp2_so=mppplay.o mpplay.o
playmp2$(LIB_SUFFIX): $(playmp2_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(MAD_LIBS) $(MATH_LIBS)

mppplay.o: mppplay.c \
	../config.h \
	../types.h \
	../filesel/pfilesel.h \
	../filesel/mdb.h \
	../stuff/poutput.h \
	../dev/player.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	mpplay.h \
	../stuff/err.h \
	../stuff/sets.h \
	../stuff/compat.h \
	../dev/deviplay.h \
	../cpiface/cpiface.h
	$(CC) mppplay.c -o $@ $(MAD_CFLAGS) -c

mpplay.o: mpplay.c \
	../config.h \
	../types.h \
	../stuff/poll.h \
	../stuff/timer.h \
	../dev/player.h \
	../dev/deviplay.h \
	../dev/ringbuffer.h \
	../dev/plrasm.h \
	../stuff/imsrtns.h \
	mpplay.h
	$(CC) mpplay.c -o $@ $(MAD_CFLAGS) -c
