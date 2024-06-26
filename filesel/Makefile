TOPDIR=../
include $(TOPDIR)Rules.make
include Makefile-static

ifeq ($(STATIC_CORE),1)
all: $(cdrom_so) $(pfilesel_so)
else
all: $(CDROM_SO) pfilesel$(LIB_SUFFIX)
endif
	$(MAKE) -C cdfs TOPDIR=../$(TOPDIR)

test: adbmeta-test$(EXE_SUFFIX) dirdb-test$(EXE_SUFFIX) filesystem-bzip2-test$(EXE_SUFFIX) filesystem-filehandle-cache-test$(EXE_SUFFIX) filesystem-gzip-test$(EXE_SUFFIX) filesystem-tar-test$(EXE_SUFFIX) filesystem-textfile-test$(EXE_SUFFIX) mdb-test$(EXE_SUFFIX)
	@echo "" && echo "adbmeta-test:"                       && ./adbmeta-test
	@echo "" && echo "dirdb-test:"                         && ./dirdb-test
	@echo "" && echo "filesystem-bzip2-test:"              && ./filesystem-bzip2-test
	@echo "" && echo "filesystem-filehandle-cache-test:"   && ./filesystem-filehandle-cache-test
	@echo "" && echo "filesystem-gzip-test:"               && ./filesystem-gzip-test
	@echo "" && echo "filesystem-tar-test:"                && ./filesystem-tar-test
	@echo "" && echo "filesystem-textfile-test:"           && ./filesystem-textfile-test
	@echo "" && echo "mdb-test:"                           && ./mdb-test

cdrom$(LIB_SUFFIX): $(cdrom_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(PTHREAD_LIBS) $(LIBDISCID_LIBS)

pfilesel$(LIB_SUFFIX): $(pfilesel_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ -lbz2 -lz $(MATH_LIBS) $(ICONV_LIBS) $(LIBCJSON_LIBS) $(LIBANCIENT_LIBS)

clean:
	$(MAKE) -C cdfs TOPDIR=../$(TOPDIR) clean
	rm -f *.o *$(LIB_SUFFIX) adbmeta-test$(EXE_SUFFIX) dirdb-test$(EXE_SUFFIX) filesystem-bzip2-test$(EXE_SUFFIX) filesystem-filehandle-cache-test$(EXE_SUFFIX) filesystem-gzip-test$(EXE_SUFFIX) filesystem-tar-test$(EXE_SUFFIX) filesystem-textfile-test$(EXE_SUFFIX) mdb-test$(EXE_SUFFIX)

ifeq ($(STATIC_CORE),1)
install:
uninstall:
else
install:
	$(CP) pfilesel$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/25-pfilesel$(LIB_SUFFIX)"
ifeq ($(CDROM_SUPPORT),1)
	$(CP) $(CDROM_SO) "$(DESTDIR)$(LIBDIROCP)/autoload/30-cdrom$(LIB_SUFFIX)"
endif
	$(MAKE) -C cdfs TOPDIR=../$(TOPDIR) install
uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/25-pfilesel$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/30-cdrom$(LIB_SUFFIX)"
	$(MAKE) -C cdfs TOPDIR=../$(TOPDIR) uninstall
endif

adbmeta.o: adbmeta.c adbmeta.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../filesel/dirdb.h \
	../stuff/file.h
	$(CC) $< -o $@ -c

adbmeta-test$(EXE_SUFFIX): adbmeta-test.c adbmeta.c adbmeta.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../filesel/dirdb.h \
	../stuff/file.c \
	../stuff/file.h
	$(CC) $< -o $@

charsets.o: charsets.c \
	../config.h \
	../types.h \
	charsets.h
	$(CC) $< -o $@ -c

dirdb.o: dirdb.c \
	../config.h \
	../types.h \
	../boot/console.h \
	../boot/psetting.h \
	../stuff/compat.h \
	../stuff/file.h \
	../stuff/poutput.h \
	../stuff/utf-8.h \
	dirdb.h \
	mdb.h
	$(CC) $< -o $@ -c

dirdb-test$(EXE_SUFFIX): dirdb-test.c \
	dirdb.c \
	dirdb.h \
	../stuff/compat.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/compat.h \
	../stuff/file.h \
	../stuff/file.h
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
	musicbrainz.h \
	pfilesel.h \
	../stuff/err.h
	$(CC) $< -o $@ -c $(LIBDISCID_CFLAGS)

filesystem.o: filesystem.c \
	../config.h \
	../types.h \
	filesystem.h
	$(CC) $< -o $@ -c

filesystem-ancient.o: filesystem-ancient.cpp \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	filesystem.h \
	filesystem-ancient.h \
	filesystem-file-mem.h
	$(LIBANCIENT_CXX) $< -o $@ -c $(LIBANCIENT_CXXFLAGS)

filesystem-bzip2.o: filesystem-bzip2.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-bzip2.h \
	../stuff/framelock.h
	$(CC) $< -o $@ -c

filesystem-bzip2-test$(EXE_SUFFIX): filesystem-bzip2-test.c \
	filesystem-bzip2.c \
	filesystem-file-mem.h \
	filesystem-dir-mem.h \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem-bzip2.h \
	filesystem-file-mem-nocache.o \
	filesystem-dir-mem-nocache.o
	$(CC) $< -o $@ filesystem-file-mem-nocache.o filesystem-dir-mem-nocache.o -lbz2

filesystem-dir-mem-nocache.o: filesystem-dir-mem.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-dir-mem.h
	$(CC) $< -o $@ -c -DFILEHANDLE_CACHE_DISABLE

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

filesystem-file-dev.o: filesystem-file-dev.c \
	../config.h \
	../types.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-file-dev.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h
	$(CC) $< -o $@ -c

filesystem-file-mem-nocache.o: filesystem-file-mem.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-file-mem.h
	$(CC) $< -o $@ -c -DFILEHANDLE_CACHE_DISABLE

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

filesystem-gzip-test$(EXE_SUFFIX): filesystem-gzip-test.c \
	filesystem-gzip.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem-file-mem.h \
	filesystem-dir-mem.h \
	filesystem-gzip.h \
	filesystem-dir-mem-nocache.o \
	filesystem-file-mem-nocache.o
	$(CC) $< -o $@ filesystem-file-mem-nocache.o filesystem-dir-mem-nocache.o -lz

filesystem-filehandle-cache.o: filesystem-filehandle-cache.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-filehandle-cache.h
	$(CC) $< -o $@ -c

filesystem-filehandle-cache-test$(EXE_SUFFIX): filesystem-filehandle-cache-test.c \
	filesystem-filehandle-cache.c \
	filesystem-filehandle-cache.h \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h
	$(CC) $< -o $@

filesystem-pak.o: filesystem-pak.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-pak.h
	$(CC) $< -o $@ -c

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

filesystem-rpg.o: filesystem-rpg.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-rpg.h \
	../stuff/cp437.h
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

filesystem-tar-test$(EXE_SUFFIX): filesystem-tar-test.c \
	filesystem-tar.c \
	../config.h \
	../types.h \
	adbmeta.h \
	dirdb.h \
	filesystem.h \
	filesystem-tar.h \
	filesystem-dir-mem.h \
	filesystem-file-mem.h \
	filesystem-dir-mem-nocache.o \
	filesystem-file-mem-nocache.o
	$(CC) $< filesystem-dir-mem-nocache.o filesystem-file-mem-nocache.o -o $@

filesystem-textfile.o: filesystem-textfile.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-textfile.h
	$(CC) $< -o $@ -c

filesystem-textfile-test$(EXE_SUFFIX): filesystem-textfile-test.c \
	filesystem-textfile.c \
	../config.h \
	../types.h \
	dirdb.h \
	filesystem.h \
	filesystem-textfile.h \
	filesystem-dir-mem.h \
	filesystem-file-mem.h \
	filesystem-dir-mem-nocache.o \
	filesystem-file-mem-nocache.o
	$(CC) $< filesystem-dir-mem-nocache.o filesystem-file-mem-nocache.o -o $@

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

filesystem-windows.o: filesystem-windows.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h \
	../filesel/filesystem-setup.h \
	../filesel/filesystem-windows.h \
	../filesel/pfilesel.h \
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
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-ancient.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/cp437.h \
	../stuff/compat.h \
	../stuff/file.h \
	../stuff/imsrtns.h \
	../stuff/latin1.h
	$(CC) $< -o $@ -c

mdb-test$(EXE_SUFFIX): mdb-test.c \
	mdb.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/file.h \
	../stuff/file.c \
	../stuff/imsrtns.h
	$(CC) $< -o $@

musicbrainz.o: musicbrainz.c \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h \
	../filesel/filesystem-file-dev.h \
	../filesel/filesystem-setup.h \
	../filesel/musicbrainz.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/file.h \
	../stuff/piperun.h \
	../stuff/poutput.h \
	../stuff/utf-8.h
	$(CC) $< -o $@ -c $(LIBCJSON_CFLAGS)

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
	fsptype.h \
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
	filesystem-ancient.h \
	filesystem-bzip2.h \
	filesystem-drive.h \
	filesystem-file-dev.h \
	filesystem-gzip.h \
	filesystem-pak.h \
	filesystem-playlist.h \
	filesystem-playlist-m3u.h \
	filesystem-playlist-pls.h \
	filesystem-setup.h \
	filesystem-rpg.h \
	filesystem-tar.h \
	filesystem-unix.h \
	filesystem-windows.h \
	filesystem-z.h \
	filesystem-zip.h \
	mdb.h \
	modlist.h \
	musicbrainz.h \
	pfilesel.h \
	../stuff/compat.h \
	../stuff/framelock.h \
	../stuff/imsrtns.h \
	../stuff/piperun.h \
	../stuff/poutput.h \
	../stuff/utf-8.h
	$(CC) $< -o $@ -c

pfsmain.o: pfsmain.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/pmain.h \
	../boot/psetting.h \
	../dev/deviplay.h \
	../dev/deviwave.h \
	../dev/postproc.h \
	dirdb.h \
	filesystem.h \
	filesystem-file-dev.h \
	filesystem-setup.h \
	mdb.h \
	pfilesel.h \
	fsptype.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h
	$(CC) $< -o $@ -c
