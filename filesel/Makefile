TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(CDROM_SUPPORT),1)
CDROM_SO=cdrom$(LIB_SUFFIX)
endif

arctar_so=arctar.o
arczip_so=arczip.o
cdrom_so=cdrom.o
pfilesel_so=adb.o cphlpfs.o dirdb.o pfilesel.o pfsmain.o modlist.o fsptype.o mdb.o pfspls.o pfsm3u.o gendir.o playlist.o

ifeq ($(STATIC_BUILD),1)
all: $(arctar_so) $(arczip_so) $(cdrom_so) $(pfilesel_so) fstypes.o
else
all: arctar$(LIB_SUFFIX) arczip$(LIB_SUFFIX) $(CDROM_SO) fstypes.o pfilesel$(LIB_SUFFIX)
endif

arctar$(LIB_SUFFIX): $(arctar_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

arczip$(LIB_SUFFIX): $(arczip_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ -lz

cdrom$(LIB_SUFFIX): $(cdrom_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

pfilesel$(LIB_SUFFIX): $(pfilesel_so)
	$(CC) $(SHARED_FLAGS) $(MATH_LIBS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX) modlist_test

ifeq ($(STATIC_BUILD),1)
install:
uninstall:
else
install:
	$(CP) arctar$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-arctar$(LIB_SUFFIX)"
	$(CP) arczip$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-arczip$(LIB_SUFFIX)"
	$(CP) pfilesel$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/25-pfilesel$(LIB_SUFFIX)"
ifeq ($(CDROM_SUPPORT),1)
	$(CP) $(CDROM_SO) "$(DESTDIR)$(LIBDIR)/autoload/30-cdrom$(LIB_SUFFIX)"
endif
uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-arctar$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-arczip$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/25-pfilesel$(LIB_SUFFIX)"
ifeq ($(CDROM_SUPPORT),1)
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-cdrom$(LIB_SUFFIX)"
endif
endif

adb.o: adb.c adb.h \
	../config.h \
	../types.h \
	dirdb.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../stuff/poutput.h \
	modlist.h \
	mdb.h \
	pfilesel.h \
	../stuff/compat.h
	$(CC) adb.c -o $@ -c

dirdb.o: dirdb.c dirdb.h \
	../config.h \
	../types.h \
	../boot/psetting.h
	$(CC) dirdb.c -o $@ -c

cdrom.o: cdrom.c \
	../config.h \
	../types.h \
	dirdb.h \
	../boot/plinkman.h \
	modlist.h \
	pfilesel.h \
	../stuff/err.h \
	mdb.h
	$(CC) cdrom.c -o $@ -c

mdb.o: mdb.c mdb.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	modlist.h \
	pfilesel.h \
	../stuff/imsrtns.h
	$(CC) mdb.c -o $@ -c

cphlpfs.o: cphlpfs.c cphlpfs.h \
	../config.h \
	../types.h \
	../stuff/framelock.h \
	../stuff/poutput.h \
	../help/cphelper.h
	$(CC) cphlpfs.c -o $@ -c

fsptype.o: fsptype.c \
	../config.h \
	../types.h \
	dirdb.h \
	mdb.h
	$(CC) fsptype.c -o $@ -c

modlist.o: modlist.c modlist.h \
	../config.h \
	../types.h \
	../stuff/compat.h \
	dirdb.h
	$(CC) modlist.c -o $@ -c

modlist_test.o: modlist_test.c \
	../config.h
	modlist.h
	$(CC) modlist_test.c -o $@ -c

modlist_test: modlist.o modlist_test.o
	$(CC) -o $@ $^ -lefence

pfilesel.o: pfilesel.c pfilesel.h \
	../config.h \
	../types.h \
	adb.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	cphlpfs.h \
	dirdb.h \
	gendir.h \
	mdb.h \
	../stuff/compat.h \
	../stuff/framelock.h \
	../stuff/poutput.h \
	modlist.h
	$(CC) pfilesel.c -o $@ -c

fstypes.o: fstypes.c \
	../config.h \
	../boot/plinkman.h \
	mdb.h
	$(CC) fstypes.c -o $@ -c

pfsmain.o: pfsmain.c \
	../config.h \
	../types.h \
	dirdb.h \
	mdb.h \
	pfilesel.h \
	../boot/plinkman.h \
	../boot/pmain.h \
	../stuff/poutput.h \
	../stuff/err.h \
	../boot/psetting.h
	$(CC) pfsmain.c -o $@ -c

arctar.o: arctar.c \
	../config.h \
	../types.h \
	adb.h \
	mdb.h \
	mif.h \
	modlist.h \
	../boot/plinkman.h \
	../stuff/compat.h \
	pfilesel.h
	$(CC) arctar.c -o $@ -c

arczip.o: arczip.c \
	../config.h \
	../types.h \
	adb.h \
	mdb.h \
	mif.h \
	modlist.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../stuff/compat.h \
	../stuff/pagesize.inc.c \
	pfilesel.h
	$(CC) arczip.c -o $@ -c

pfspls.o: pfspls.c \
	../config.h \
	../types.h \
	adb.h \
	dirdb.h \
	gendir.h \
	mdb.h \
	modlist.h \
	playlist.h \
	pfilesel.h \
	../stuff/compat.h
	$(CC) pfspls.c -o $@ -c

pfsm3u.o: pfsm3u.c \
	../config.h \
	../types.h \
	adb.h \
	dirdb.h \
	gendir.h \
	mdb.h \
	modlist.h \
	playlist.h \
	pfilesel.h \
	../stuff/compat.h
	$(CC) pfsm3u.c -o $@ -c

gendir.o: gendir.c \
	../config.h \
	../types.h \
	gendir.h
	$(CC) gendir.c -o $@ -c

playlist.o: playlist.c \
	../config.h \
	../types.h \
	adb.h \
	dirdb.h \
	gendir.h \
	mdb.h \
	modlist.h \
	pfilesel.h \
	playlist.h \
	../stuff/compat.h
	$(CC) playlist.c -o $@ -c
