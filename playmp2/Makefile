TOPDIR=../
include $(TOPDIR)Rules.make

all: mpptype.o charset.o playmp2$(LIB_SUFFIX)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playmp2$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playmp2$(LIB_SUFFIX)"

charset.o: charset.c \
	../config.h
	$(CC) charset.c -o $@ -c

mpptype.o: mpptype.c \
	../config.h \
	../types.h \
	../stuff/imsrtns.h \
	../filesel/mdb.h \
	charset.h
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
	../dev/plrasm.h \
	../stuff/imsrtns.h \
	mpplay.h
	$(CC) mpplay.c -o $@ $(MAD_CFLAGS) -c
