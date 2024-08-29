TOPDIR=../../
include $(TOPDIR)Rules.make
include Makefile-static

ifeq ($(STATIC_CORE),1)
all: $(modland_com_so)
else
all: modland_com$(LIB_SUFFIX)
endif

clean:
	rm -f modland_com$(LIB_SUFFIX) *.o

ifeq ($(STATIC_CORE),1)
install:
uninstall:
else
install:
	$(CP) modland_com$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/60-modland_com$(LIB_SUFFIX)"
uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/60-modland_com$(LIB_SUFFIX)"
endif

.PHONY: all clean install uninstall

modland_com$(LIB_SUFFIX): $(modland_com_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

modland-com.o: \
	modland-com.c \
	modland-com-cachedir.c \
	modland-com-dir.c \
	modland-com-file.c \
	modland-com-filedb.c \
	modland-com-filehandle.c \
	modland-com-initialize.c \
	modland-com-mirrors.c \
	modland-com-setup.c \
	../download.h \
	../../config.h \
	../../types.h \
	../../boot/plinkman.h \
	../../boot/psetting.h \
	../../filesel/dirdb.h \
	../../filesel/filesystem.h \
	../../filesel/filesystem-dir-mem.h \
	../../filesel/filesystem-drive.h \
	../../filesel/filesystem-file-dev.h \
	../../filesel/filesystem-textfile.h \
	../../stuff/err.h \
	../../stuff/file.h \
	../../stuff/framelock.h \
	../../stuff/poutput.h
	$(CC) $< -o $@ -c
