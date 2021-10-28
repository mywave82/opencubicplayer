TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(HAVE_ADPLUG),1)
TARGETS=opltype$(LIB_SUFFIX) playopl$(LIB_SUFFIX)
endif

all: $(TARGETS)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
ifeq ($(HAVE_ADPLUG),1)
	$(CP) playopl$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"
	$(CP) opltype$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-opltype$(LIB_SUFFIX)"
endif

uninstall:
ifeq ($(HAVE_ADPLUG),1)
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-opltype$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/playopl$(LIB_SUFFIX)"
endif


opltype_so=opltype.o
opltype$(LIB_SUFFIX): $(opltype_so)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(ADPLUG_LIBS)

opltype.o: opltype.cpp \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h
	$(CXX) $(ADPLUG_CXXFLAGS) opltype.cpp -o $@ -c

playopl_so=oplpplay.o oplplay.o ocpemu.o
playopl$(LIB_SUFFIX): $(playopl_so)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(ADPLUG_LIBS) $(MATH_LIBS)

oplpplay.o: oplpplay.cpp \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/pfilesel.h \
	../filesel/mdb.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h \
	../stuff/timer.h \
	oplplay.h
	$(CXX) $(ADPLUG_CXXFLAGS) oplpplay.cpp -o $@ -c

oplplay.o: oplplay.cpp \
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
	oplplay.h ocpemu.h
	$(CXX) $(ADPLUG_CXXFLAGS) oplplay.cpp -o $@ -c

ocpemu.o: ocpemu.cpp \
	ocpemu.h
	$(CXX) $(ADPLUG_CXXFLAGS) ocpemu.cpp -o $@ -c
