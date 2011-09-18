TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(HAVE_SIDPLAY),1)
TARGETS=sidtype$(LIB_SUFFIX) playsid$(LIB_SUFFIX)
endif

all: $(TARGETS)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
ifeq ($(HAVE_SIDPLAY),1)
	$(CP) playsid$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"
	$(CP) sidtype$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-playsid$(LIB_SUFFIX)"
endif

uninstall:
ifeq ($(HAVE_SIDPLAY),1)
	rm -f "$(DESTDIR)$(LIBDIR)/sidtype$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/autoload/30-playsid$(LIB_SUFFIX)"
endif

playsid_so=sidplay.o sidpplay.o
playsid$(LIB_SUFFIX): $(playsid_so)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ -lsidplay

sidtype_so=sidptype.o
sidtype$(LIB_SUFFIX): $(sidtype_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^

sidptype.o: sidptype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/mdb.h
	$(CC) sidptype.c -o $@ -c

sidplay.o: sidplay.cpp \
	../config.h \
	../types.h \
	../stuff/poll.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/deviplay.h \
	../dev/mixclip.h \
	../cpiface/cpiface.h \
	../boot/psetting.h\
	sid.h \
	opstruct.h
	$(CXX) sidplay.cpp -o $@ -c

sidpplay.o: sidpplay.cpp \
	../config.h \
	../types.h \
	../filesel/mdb.h \
	../stuff/poutput.h \
	../dev/player.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	../stuff/compat.h \
	../stuff/sets.h \
	../dev/deviplay.h \
	../cpiface/cpiface.h \
	../stuff/compat.h \
	../stuff/timer.h \
	sid.h
	$(CXX) sidpplay.cpp -o $@ -c
