TOPDIR=../../
include $(TOPDIR)Rules.make
include Makefile-static

ifeq ($(STATIC_CORE),1)
all: $(cdfs_so)
else
all: cdfs$(LIB_SUFFIX)
endif

clean:
	rm -f cdfs$(LIB_SUFFIX) *.o

ifeq ($(STATIC_CORE),1)
install:
uninstall:
else
install:
	$(CP) cdfs$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/60-cdfs$(LIB_SUFFIX)"
uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/60-cdfs$(LIB_SUFFIX)"
endif

.PHONY: all clean install uninstall

cdfs$(LIB_SUFFIX): $(cdfs_so)
	$(CC) $(SHARED_FLAGS) $(ICONV_LIBS) -o $@ $^ $(LIBDISCID_LIBS)

audio.o: \
	audio.c \
	../../config.h \
	../../types.h \
	audio.h \
	../musicbrainz.h \
	cdfs.h
	$(CC) $< -o $@ -c $(LIBDISCID_CFLAGS)

cdfs.o: \
	cdfs.c \
	../../config.h \
	../../types.h \
	cdfs.h \
	iso9660.h \
	../dirdb.h \
	../filesystem.h \
	../mdb.h \
	../musicbrainz.h \
	udf.h
	$(CC) $< -o $@ -c

cue.o: \
	cue.c \
	cdfs.h \
	cue.h \
	../filesystem.h \
	wave.h
	$(CC) $< -o $@ -c

iso9660.o: \
	iso9660.c \
	../../config.h \
	../../types.h \
	ElTorito.c \
	susp.c \
		amiga.c \
		rockridge.c \
	cdfs.h \
	iso9660.h \
	main.h
	$(CC) -o $@ $< -c

main.o: \
	main.c \
	../../config.h \
	../../types.h \
	audio.h \
	cdfs.h \
	cue.h \
	../../boot/plinkman.h \
	../dirdb.h \
	../filesystem.h \
	iso9660.h \
	main.h \
	../../stuff/err.h \
	toc.h \
	udf.h
	$(CC) -o $@ $< -c

toc.o: \
	toc.c \
	cdfs.h \
	../filesystem.h \
	toc.h \
	wave.h
	$(CC) $< -o $@ -c


udf.o: \
	udf.c \
	../../config.h \
	../../types.h \
	cdfs.h \
	main.h \
	udf.h
	$(CC) -o $@ $< -c

wave.o: \
	wave.c \
	../../config.h \
	../../types.h \
	../dirdb.h \
	../filesystem.h \
	wave.h
	$(CC) -o $@ $< -c
