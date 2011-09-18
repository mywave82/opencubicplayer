TOPDIR=../
include $(TOPDIR)Rules.make

all: medialib$(LIB_SUFFIX)

medialib_so=medialib.o
medialib$(LIB_SUFFIX): $(medialib_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) medialib$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/medialib$(LIB_SUFFIX)"

medialib.o: medialib.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../filesel/dirdb.h \
	../filesel/modlist.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/framelock.h \
	../filesel/mdb.h \
	../filesel/adb.h
	$(CC) medialib.c -o $@ -c
