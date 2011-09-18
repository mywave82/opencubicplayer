TOPDIR=../
include $(TOPDIR)/Rules.make

ifeq ($(HAVE_LZH),1)
LZH_LIB:=-llzh
else
LZH_LIB:=
endif

all: lzh_dir stsoundlib_dir ymtype$(LIB_SUFFIX) playym$(LIB_SUFFIX)

.PHONY: lzh_dir stsoundlib_dir

lzh_dir:
ifeq ($(HAVE_LZH),1)
	$(MAKE) -C lzh all TOPDIR="../$(TOPDIR)"
endif

stsoundlib_dir:
	$(MAKE) -C stsoundlib all TOPDIR="../$(TOPDIR)"

install:
	$(CP) ymtype$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-ymtype$(LIB_SUFFIX)"
	$(CP) playym$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:

clean:
	$(MAKE) -C lzh clean TOPDIR="../$(TOPDIR)"
	$(MAKE) -C stsoundlib clean TOPDIR="../$(TOPDIR)"
	rm -f *.o *$(LIB_SUFFIX)

ymtype_so=ymtype.o
ymtype$(LIB_SUFFIX): $(ymtype_so)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(LZH_LIB) -Llzh

playym_so=ympplay.o ymplay.o
playym$(LIB_SUFFIX): $(playym_so) lzh_dir stsoundlib_dir
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $(playym_so) $(LZH_LIB) -lym -Llzh -Lstsoundlib

ymtype.o: ymtype.cpp \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	lzh/lzh.h
	$(CXX) ymtype.cpp -o $@ -c

ympplay.o: ympplay.cpp \
	../config.h \
	../types.h \
	../filesel/pfilesel.h \
	../filesel/mdb.h \
	../dev/player.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	ymplay.h \
	../stuff/compat.h \
	../stuff/sets.h \
	../stuff/poutput.h \
	../stuff/timer.h \
	../dev/deviplay.h \
	../cpiface/cpiface.h \
	stsoundlib/StSoundLibrary.h
	$(CXX) ympplay.cpp -o $@ -c

ymplay.o: ymplay.cpp \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../stuff/poll.h \
	../stuff/timer.h \
	../dev/player.h \
	../dev/mcp.h \
	../dev/deviplay.h \
	../dev/plrasm.h \
	../stuff/imsrtns.h \
	ymplay.h
	$(CXX) ymplay.cpp -o $@ -c
