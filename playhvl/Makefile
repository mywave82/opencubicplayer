TOPDIR=../
include $(TOPDIR)Rules.make
all: hvlptype.o playhvl$(LIB_SUFFIX)

playhvl_so=loader.o player.o hvlpinst.o hvlpplay.o hvlplay.o
playhvl$(LIB_SUFFIX): $(playhvl_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playhvl$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playhvl$(LIB_SUFFIX)"

hvlptype.o: \
	hvlptype.c \
	../config.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/ringbuffer.h \
	../filesel/mdb.h \
	../stuff/imsrtns.h \
	../stuff/poll.h \
	../types.h
	$(CC) -c -o $@ $<

hvlplay.o: \
	hvlplay.c \
	../config.h \
	../dev/deviplay.h \
	../dev/player.h \
	../types.h \
	hvlplay.h \
	loader.h \
	player.h
	$(CC) -c -o $@ $<

hvlpinst.o: \
	hvlpinst.c \
	../config.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	../types.h \
	hvlpinst.h \
	hvlplay.h \
	player.h
	$(CC) -c -o $@ $<

hvlpplay.o: \
	hvlpplay.c \
	../config.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../dev/deviplay.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h \
	../types.h \
	hvlplay.h \
	player.h
	$(CC) -c -o $@ $<

loader.o: \
	loader.c \
	loader.h \
	player.h \
	../config.h \
	../types.h
	$(CC) -c -o $@ $<

player.o: \
	player.c \
	player.h  \
	../config.h \
	../types.h
	$(CC) -c -o $@ $<
