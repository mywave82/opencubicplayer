TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(CDROM_SUPPORT),1)
CDROM_SO=cdrom$(LIB_SUFFIX)
cdrom_so=cdrom.o
endif

pfilesel_so=\
adbmeta.o                     \
charsets.o                    \
cphlpfs.o                     \
dirdb.o                       \
filesystem.o                  \
filesystem-bzip2.o            \
filesystem-dir-mem.o          \
filesystem-drive.o            \
filesystem-gzip.o             \
filesystem-file-mem.o         \
filesystem-filehandle-cache.o \
filesystem-playlist.o         \
filesystem-playlist-m3u.o     \
filesystem-playlist-pls.o     \
filesystem-setup.o            \
filesystem-tar.o              \
filesystem-unix.o             \
filesystem-z.o                \
filesystem-zip.o              \
fsptype.o                     \
modlist.o                     \
mdb.o                         \
pfilesel.o                    \
pfsmain.o

ifeq ($(STATIC_BUILD),1)
all: $(cdrom_so) $(pfilesel_so) fstypes.o
else
all: $(CDROM_SO) fstypes.o pfilesel$(LIB_SUFFIX)
endif

test: adbmeta-test dirdb-test filesystem-bzip2-test filesystem-filehandle-cache-test filesystem-gzip-test filesystem-tar-test
	@echo "" && echo "adbmeta-test:"                       && ./adbmeta-test
	@echo "" && echo "dirdb-test:"                         && ./dirdb-test
	@echo "" && echo "filesystem-bzip2-test:"              && ./filesystem-bzip2-test
	@echo "" && echo "filesystem-filehandle-cache-test:"   && ./filesystem-filehandle-cache-test
	@echo "" && echo "filesystem-gzip-test:"               && ./filesystem-gzip-test
	@echo "" && echo "filesystem-tar-test:"                && ./filesystem-tar-test

cdrom$(LIB_SUFFIX): $(cdrom_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

pfilesel$(LIB_SUFFIX): $(pfilesel_so)
	$(CC) $(SHARED_FLAGS) $(MATH_LIBS) $(ICONV_LIBS) -o $@ $^ -lbz2 -lz

clean:
	rm -f *.o *$(LIB_SUFFIX) adbmeta-test dirdb-test filesystem-bzip2-test filesystem-filehandle-cache-test filesystem-gzip-test filesystem-tar-test

ifeq ($(STATIC_BUILD),1)
install:
uninstall:
else
install:
	$(CP) pfilesel$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/25-pfilesel$(LIB_SUFFIX)"
ifeq ($(CDROM_SUPPORT),1)
	$(CP) $(CDROM_SO) "$(DESTDIR)$(LIBDIR)/autoload/30-cdrom$(LIB_SUFFIX)"
endif
uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/25-pfilesel$(LIB_SUFFIX)"
ifeq ($(CDROM_SUPPORT),1)
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-cdrom$(LIB_SUFFIX)"
endif
endif

adbmeta.o: adbmeta.c adbmeta.h \
	../config.h \
	../types.h \
	dirdb.h \
	../boot/psetting.h
	$(CC) $< -o $@ -c

adbmeta-test: adbmeta-test.c adbmeta.c adbmeta.h \
	../config.h \
	../types.h \
	dirdb.h \
	../boot/psetting.h
	$(CC) $< -o $@

charsets.o: charsets.c \
	../config.h \
	../types.h \
	charsets.h
	$(CC) $< -o $@ -c

dirdb.o: dirdb.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/compat.c \
	dirdb.h
	$(CC) $< -o $@ -c

dirdb-test: dirdb-test.c \
	dirdb.c \
	dirdb.h \
	../stuff/compat.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/compat.h
	$(CC) $< -o $@

cdrom.o: cdrom.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	cdrom.h \
	dirdb.h \
	filesystem.h \
	filesystem-drive.h \
	filesystem-file-mem.h \
	mdb.h \
	pfilesel.h \
	../stuff/err.h
	$(CC) $< -o $@ -c

filesystem.o: filesystem.c \
	../config.h \
	../types.h \
	filesystem.h
	$(CC) $< -o $@ -c

filesystem-bzip2.o: filesystem-bzip2.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-bzip2.h \
	../stuff/framelock.h
	$(CC) $< -o $@ -c

filesystem-bzip2-test: filesystem-bzip2-test.c \
	filesystem-bzip2.c \
	filesystem-file-mem.h \
	filesystem-dir-mem.h \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem-bzip2.h \
	filesystem-file-mem.o \
	filesystem-dir-mem.o
	$(CC) $< -o $@ filesystem-file-mem.o filesystem-dir-mem.o -lbz2

filesystem-dir-mem.o: filesystem-dir-mem.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-dir-mem.h
	$(CC) $< -o $@ -c

filesystem-drive.o: filesystem-drive.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-drive.h \
	../stuff/compat.h
	$(CC) $< -o $@ -c

filesystem-file-mem.o: filesystem-file-mem.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-file-mem.h
	$(CC) $< -o $@ -c

filesystem-gzip.o: filesystem-gzip.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-gzip.h \
	../stuff/framelock.h
	$(CC) $< -o $@ -c

filesystem-gzip-test: filesystem-gzip-test.c \
	filesystem-gzip.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem-file-mem.h \
	filesystem-dir-mem.h \
	filesystem-gzip.h \
	filesystem-dir-mem.o \
	filesystem-file-mem.o
	$(CC) $< -o $@ filesystem-file-mem.o filesystem-dir-mem.o -lz

filesystem-filehandle-cache.o: filesystem-filehandle-cache.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-filehandle-cache.h
	$(CC) $< -o $@ -c

filesystem-filehandle-cache-test: filesystem-filehandle-cache-test.c \
	filesystem-filehandle-cache.c \
	filesystem-filehandle-cache.h \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-dir-mem.h \
	filesystem-file-mem.h \
	filesystem-dir-mem.o \
	filesystem-file-mem.o
	$(CC) $< filesystem-file-mem.o filesystem-dir-mem.o -o $@

filesystem-playlist.o: filesystem-playlist.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-drive.h \
	filesystem-playlist.h \
	modlist.h \
	pfilesel.h \
	../stuff/compat.h
	$(CC) $< -o $@ -c

filesystem-playlist-m3u.o: filesystem-playlist-m3u.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-playlist.h \
	filesystem-playlist-m3u.h \
	mdb.h \
	modlist.h \
	pfilesel.h \
	../stuff/compat.h
	$(CC) $< -o $@ -c

filesystem-playlist-pls.o: filesystem-playlist-pls.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-playlist.h \
	filesystem-playlist-pls.h \
	mdb.h \
	modlist.h \
	pfilesel.h \
	../stuff/compat.h
	$(CC) $< -o $@ -c

filesystem-setup.o: filesystem-setup.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-dir-mem.h \
	filesystem-drive.h \
	filesystem-setup.h \
	pfilesel.h
	$(CC) $< -o $@ -c

filesystem-tar.o: filesystem-tar.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-tar.h
	$(CC) $< -o $@ -c

filesystem-tar-test: filesystem-tar-test.c \
	filesystem-tar.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-tar.h \
	filesystem-dir-mem.h \
	filesystem-file-mem.h \
	filesystem-dir-mem.o \
	filesystem-file-mem.o
	$(CC) $< filesystem-dir-mem.o filesystem-file-mem.o -o $@

filesystem-unix.o: filesystem-unix.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	dirdb.h \
	filesystem.h \
	filesystem-drive.h \
	filesystem-unix.h \
	../stuff/compat.h
	$(CC) $< -o $@ -c

filesystem-zip.o: filesystem-zip.c \
	filesystem-zip-headers.c \
	zip-bzip2.c \
	zip-expand.c \
	zip-explode.c \
	zip-inflate.c \
	zip-unshrink.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-zip.h
	$(CC) $< -o $@ -c

filesystem-z.o: filesystem-z.c \
	z-unlzw.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-z.h \
	../stuff/framelock.h
	$(CC) $< -o $@ -c

mdb.o: mdb.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	dirdb.h \
	filesystem.h \
	mdb.h \
	pfilesel.h \
	../stuff/compat.h \
	../stuff/imsrtns.h
	$(CC) $< -o $@ -c

cphlpfs.o: cphlpfs.c \
	../config.h \
	../types.h \
	cphlpfs.h \
	../help/cphelper.h \
	../stuff/framelock.h \
	../stuff/poutput.h
	$(CC) $< -o $@ -c

fsptype.o: fsptype.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	mdb.h
	$(CC) $< -o $@ -c

modlist.o: modlist.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-drive.h \
	mdb.h \
	modlist.h \
	../stuff/compat.h \
	../stuff/poutput.h \
	../stuff/utf-8.h
	$(CC) $< -o $@ -c

pfilesel.o: pfilesel.c \
	pfilesel-charset.c \
	../config.h \
	../types.h \
	adbmeta.h \
	../boot/psetting.h \
	charsets.h \
	cphlpfs.h \
	../cpiface/cpiface.h \
	dirdb.h \
	filesystem.h \
	filesystem-bzip2.h \
	filesystem-drive.h \
	filesystem-gzip.h \
	filesystem-playlist.h \
	filesystem-playlist-m3u.h \
	filesystem-playlist-pls.h \
	filesystem-setup.h \
	filesystem-tar.h \
	filesystem-unix.h \
	filesystem-z.h \
	filesystem-zip.h \
	mdb.h \
	modlist.h \
	pfilesel.h \
	../stuff/compat.h \
	../stuff/framelock.h \
	../stuff/poutput.h \
	../stuff/utf-8.h
	$(CC) $< -o $@ -c

fstypes.o: fstypes.c \
	../config.h \
	../boot/plinkman.h \
	mdb.h
	$(CC) $< -o $@ -c

pfsmain.o: pfsmain.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/pmain.h \
	../boot/psetting.h \
	dirdb.h \
	filesystem.h \
	mdb.h \
	pfilesel.h \
	../stuff/err.h \
	../stuff/poutput.h
	$(CC) $< -o $@ -c
