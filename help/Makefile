TOPDIR=../
include $(TOPDIR)Rules.make
include Makefile-static

ifeq ($(STATIC_CORE),1)
all: $(cphelper_so) $(cphlpif_so)
else
all: cphelper$(LIB_SUFFIX) cphlpif$(LIB_SUFFIX)
endif

clean:
	rm -f *$(LIB_SUFFIX) *.o

ifeq ($(STATIC_CORE),1)
install:
uninstall:
else
install:
	$(CP) cphlpif$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/40-cphlpif$(LIB_SUFFIX)"
	$(CP) cphelper$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/20-cphelper$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/40-cphlpif$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/20-cphelper$(LIB_SUFFIX)"
endif

cphelper.o: cphelper.c cphelper.h \
	../config.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../stuff/err.h \
	../stuff/compat.h \
	../stuff/poutput.h \
	../types.h
	$(CC) cphelper.c -o $@ -c

cphelper$(LIB_SUFFIX): $(cphelper_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ -lz

cphlpif.o: cphlpif.c cphelper.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../stuff/poutput.h \
	../stuff/framelock.h \
	../cpiface/cpiface.h \
	../stuff/err.h
	$(CC) cphlpif.c -o $@ -c

cphlpif$(LIB_SUFFIX): $(cphlpif_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^
