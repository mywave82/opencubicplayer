TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(BUILD_DUMPTOOLS),1)
	DUMPAHX=dumpahx$(EXE_SUFFIX)
else
	DUMPAHX=
endif

all: playhvl$(LIB_SUFFIX) $(DUMPAHX)

playhvl_so=loader.o player.o hvlpchan.o hvlpdots.o hvlpinst.o hvlpplay.o hvlplay.o hvlptrak.o hvltype.o
playhvl$(LIB_SUFFIX): $(playhvl_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(MATH_LIBS)

clean:
	rm -f *.o *$(LIB_SUFFIX) dumpahx$(EXE_SUFFIX)

install:
	$(CP) playhvl$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playhvl$(LIB_SUFFIX)"
ifeq ($(BUILD_DUMPTOOLS),1)
	$(CP) dumpahx$(EXE_SUFFIX) "$(DESTDIR)$(BINDIR)/dumpahx$(EXE_SUFFIX)"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playhvl$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(BINDIR)/dumpahx$(EXE_SUFFIX)"

dumpahx$(EXE_SUFFIX): dumpahx.o
	$(CC) $(LDFLAGS) -o $@ $^

dumpahx.o: \
	dumpahx.c
	$(CC) -o $@ $< -c

hvltype.o: \
	hvltype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/cp437.h \
	../stuff/err.h \
	hvltype.h
	$(CC) -c -o $@ $<

hvlplay.o: \
	hvlplay.c \
	../config.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/player.h \
	../types.h \
	hvlplay.h \
	loader.h \
	player.h \
	../stuff/err.h
	$(CC) -c -o $@ $<

hvlpchan.o: \
	hvlpchan.c \
	../config.h \
	../cpiface/cpiface.h \
	../dev/mix.h \
	../stuff/poutput.h \
	../types.h \
	hvlpchan.h \
	hvlplay.h \
	hvltype.h
	$(CC) -c -o $@ $<

hvlpdots.o: \
	hvlpdots.c \
	../config.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	../types.h \
	hvlpdots.h \
	hvlplay.h \
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
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	hvlplay.h \
	hvlpchan.h \
	hvlpdots.h \
	hvlpinst.h \
	hvlptrak.h \
	player.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) -c -o $@ $<

hvlptrak.o: \
	hvlptrak.c \
	../config.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	../types.h \
	hvlplay.h \
	hvlptrak.h \
	player.h
	$(CC) -c -o $@ $<

loader.o: \
	loader.c \
	loader.h \
	player.h \
	../config.h \
	../types.h \
	../cpiface/cpiface.h
	$(CC) -c -o $@ $<

player.o: \
	player.c \
	player.h  \
	../config.h \
	../types.h
	$(CC) -c -o $@ $<
