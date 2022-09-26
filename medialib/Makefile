TOPDIR=../
include $(TOPDIR)Rules.make
include Makefile-static

ifeq ($(STATIC_CORE),1)
all: $(medialib_so)
else
all: medialib$(LIB_SUFFIX)
endif

medialib$(LIB_SUFFIX): $(medialib_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

ifeq ($(STATIC_CORE),1)
install:
uninstall:
else
install:
	$(CP) medialib$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/90-medialib$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/90-medialib$(LIB_SUFFIX)"
endif

medialib.o: medialib.c \
	medialib-add.c \
	medialib-listall.c \
	medialib-refresh.c \
	medialib-remove.c \
	medialib-scan.c \
	medialib-search.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../filesel/adbmeta.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-dir-mem.h \
	../filesel/filesystem-drive.h \
	../filesel/filesystem-file-dev.h \
	../filesel/filesystem-unix.h \
	../filesel/modlist.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/framelock.h \
	../stuff/poutput.h \
	../stuff/utf-8.h
	$(CC) medialib.c -o $@ -c
