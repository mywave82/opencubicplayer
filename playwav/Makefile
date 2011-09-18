TOPDIR=../
include $(TOPDIR)Rules.make

all: wavptype.o playwav$(LIB_SUFFIX)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playwav$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playwav$(LIB_SUFFIX)"

playwav_so=wavplay.o wavpplay.o
playwav$(LIB_SUFFIX): $(playwav_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

wavplay.o: wavplay.c wave.h \
	../config.h \
	../types.h \
	../dev/deviplay.h \
	../dev/mixclip.h \
	../stuff/imsrtns.h \
	../stuff/poll.h \
	../dev/player.h \
	../dev/plrasm.h
	$(CC) wavplay.c -o $@ -c

wavptype.o: wavptype.c \
	../config.h \
	../types.h \
	../filesel/mdb.h
	$(CC) wavptype.c -o $@ -c

wavpplay.o: wavpplay.c wave.h\
	../config.h \
	../types.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../stuff/sets.h \
	../stuff/compat.h \
	../dev/devigen.h \
	../cpiface/cpiface.h
	$(CC) wavpplay.c -o $@ -c
