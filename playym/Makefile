TOPDIR=../
include $(TOPDIR)/Rules.make

ifeq ($(HAVE_LZH),1)
LZH_LIB:=-llzh
else
LZH_LIB:=
endif

all: lzh_dir stsoundlib_dir ymtype$(LIB_SUFFIX) playym$(LIB_SUFFIX)

.PHONY: all clean install uninstall lzh_dir stsoundlib_dir


lzh_dir:
ifeq ($(HAVE_LZH),1)
	$(MAKE) -C lzh all TOPDIR="../$(TOPDIR)"
endif
lzh/liblzh.a: lzh_dir


stsoundlib_dir:
	$(MAKE) -C stsoundlib all TOPDIR="../$(TOPDIR)"
stsoundlib/libym.a: stsoundlib_dir


install:
	$(CP) ymtype$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-ymtype$(LIB_SUFFIX)"
	$(CP) playym$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:

clean:
	$(MAKE) -C lzh clean TOPDIR="../$(TOPDIR)"
	$(MAKE) -C stsoundlib clean TOPDIR="../$(TOPDIR)"
	rm -f *.o *$(LIB_SUFFIX)


ymtype_so=ymtype.o
ifeq ($(HAVE_LZH),1)
ymtype_so+=lzh/liblzh.a
endif
ymtype$(LIB_SUFFIX): $(ymtype_so)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $(ymtype_so) $(LZH_LIB) -Llzh


playym_so=ympplay.o ymplay.o stsoundlib/libym.a
ifeq ($(HAVE_LZH),1)
playym_so+=lzh/liblzh.a
endif
playym$(LIB_SUFFIX): $(playym_so)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $(playym_so) $(LZH_LIB) -lym -Llzh -Lstsoundlib

ymtype.o: ymtype.cpp \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	lzh/lzh.h
	$(CXX) ymtype.cpp -o $@ -c

ympplay.o: ympplay.cpp \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/poutput.h \
	../stuff/sets.h \
	stsoundlib/StSoundLibrary.h \
	ymplay.h
	$(CXX) ympplay.cpp -o $@ -c

ymplay.o: ymplay.cpp \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	../stuff/imsrtns.h \
	ymplay.h \
	stsoundlib/YmMusic.h
	$(CXX) ymplay.cpp -o $@ -c
