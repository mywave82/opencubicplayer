TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(BUILD_DUMPTOOLS),1)
	DUMPMIDI=dumpmidi
else
	DUMPMIDI=
endif

all: gmiptype.o playtimidity$(LIB_SUFFIX) $(DUMPMIDI)

clean:
	rm -f *.o *$(LIB_SUFFIX) dumpmidi timidity-git/timidity/newton_table.c timidity-git/interface/*.o timidity-git/libarc/*.o timidity-git/timidity/*.o timidity-git/utils/*.o

install:
	$(CP) playtimidity$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"
ifeq ($(BUILD_DUMPTOOLS),1)
	$(CP) dumpmidi "$(DESTDIR)$(BINDIR)/dumpmidi"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playtimidity$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(BINDIR)/dumpmidi"

gmitimidity.o: gmitimidity.c gmipat.h gmiplay.h \
	../config.h \
	../types.h \
	../stuff/err.h
	$(CC) gmitimidity.c -o $@ -c

gmiptype.o: gmiptype.c \
	../config.h \
	../types.h \
	../filesel/mdb.h \
	../stuff/compat.h
	$(CC) gmiptype.c -o $@ -c

dumpmidi.o: dumpmidi.c \
	../config.h \
	../types.h
	$(CC) -o $@ $< -c

dumpmidi: dumpmidi.o
	$(CC) $(LDFLAGS) -o $@ $^

CFLAGS+= -Itimidity-git/interface -Itimidity-git/libarc -Itimidity-git/libunimod -Itimidity-git/timidity -Itimidity-git/utils -I. -DHAVE_CONFIG_H

%.o: \
	%.c \
	../config.h \
	../types.h
	$(CC) $< -o $@ -c -DNO_CURSES

libarc_obj=\
	timidity-git/libarc/arc.o \
	timidity-git/libarc/arc_mime.o \
	timidity-git/libarc/arc_lzh.o \
	timidity-git/libarc/arc_tar.o \
	timidity-git/libarc/arc_zip.o \
	timidity-git/libarc/deflate.o \
	timidity-git/libarc/explode.o \
	timidity-git/libarc/inflate.o \
	timidity-git/libarc/unlzh.o \
	timidity-git/libarc/url.o \
	timidity-git/libarc/url_b64decode.o \
	timidity-git/libarc/url_cache.o \
	timidity-git/libarc/url_dir.o \
	timidity-git/libarc/url_file.o \
	timidity-git/libarc/url_hqxdecode.o \
	timidity-git/libarc/url_inflate.o \
	timidity-git/libarc/url_mem.o \
	timidity-git/libarc/url_pipe.o \
	timidity-git/libarc/url_qsdecode.o \
	timidity-git/libarc/url_uudecode.o

interface_obj=\
	timidity-git/interface/wrdt_dumb.o

timidity_obj=\
	timidity-git/timidity/aq.o \
	timidity-git/timidity/audio_cnv.o \
	timidity-git/timidity/common.o \
	timidity-git/timidity/effect.o \
	timidity-git/timidity/filter.o \
	timidity-git/timidity/freq.o \
	timidity-git/timidity/instrum.o \
	timidity-git/timidity/loadtab.o \
	timidity-git/timidity/mfi.o \
	timidity-git/timidity/miditrace.o \
	timidity-git/timidity/mix.o \
	timidity-git/timidity/mt19937ar.o \
	timidity-git/timidity/optcode.o \
	timidity-git/timidity/quantity.o \
	timidity-git/timidity/rcp.o \
	timidity-git/timidity/readmidi.o \
	timidity-git/timidity/recache.o \
	timidity-git/timidity/resample.o \
	timidity-git/timidity/reverb.o \
	timidity-git/timidity/sbkconv.o \
	timidity-git/timidity/sffile.o \
	timidity-git/timidity/sfitem.o \
	timidity-git/timidity/smfconv.o \
	timidity-git/timidity/smplfile.o \
	timidity-git/timidity/sndfont.o \
	timidity-git/timidity/tables.o \
	timidity-git/timidity/timidity.o \
	timidity-git/timidity/version.o \
	timidity-git/timidity/wrd_read.o \
	timidity-git/timidity/wrdt.o
#	timidity-git/timidity/playmidi.o

utils_obj=\
	timidity-git/utils/mblock.o \
	timidity-git/utils/fft4g.o \
	timidity-git/utils/memb.o \
	timidity-git/utils/nkflib.o \
	timidity-git/utils/strtab.o \
	timidity-git/utils/timer.o

ocp-output.o: ocp-output.c \
	../config.h
	$(CC) $< -o $@ -c

calcnewt: timidity-git/timidity/calcnewt.c
	$(CC_FOR_BUILD) $< -o $@ -lm

timidity-git/timidity/newton_table.c: calcnewt
	./calcnewt > $@

#these ones are special
timidity-git/timidity/resample.o: \
	timidity-git/timidity/resample.c \
	timidity-git/timidity/newton_table.c \
	../config.h \
	../types.h
	$(CC) $< -o $@ -c

timidity-git/timidity/timidity.o: \
	timidity-git/timidity/timidity.c \
	../config.h \
	../types.h
	$(CC) $< -o $@ -c -DANOTHER_MAIN -Dmain=timidity_main -DDEFAULT_PATH=TIMIDITY_DEFAULT_PATH -DPKGDATADIR=TIMIDITYPKGDATADIR

timidity-git/timidity/wrdt.o: \
	timidity-git/timidity/wrdt.c \
	../config.h \
	../types.h
	$(CC) $< -o $@ -c -Dtty_wrdt_mode=dumb_wrdt_mode

timidityplay.o: timidityplay.c \
	../config.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	timidityplay.h
	$(CC) $< -o $@ -c -DANOTHER_MAIN -Dmain=timidity_main -DDEFAULT_PATH=TIMIDITY_DEFAULT_PATH -DPKGDATADIR=TIMIDITYPKGDATADIR

timiditypplay.o: timiditypplay.c \
	../config.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../filesel/dirdb.h \
	timidityplay.h
	$(CC) $< -o $@ -c

timiditypchan.o: timiditypchan.c \
	../config.h \
	../dev/mcp.h \
	timidityplay.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h
	$(CC) $< -o $@ -c

timiditypdots.o: \
	timiditypdots.c \
	../config.h \
	../types.h \
	../dev/mcp.h \
	timidityplay.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h
	$(CC) $< -o $@ -c

playtimidity_so=timiditypchan.o timidityplay.o timiditypplay.o timiditypdots.o ocp-output.o $(interface_obj) $(libarc_obj) $(timidity_obj) $(utils_obj)
playtimidity$(LIB_SUFFIX): $(playtimidity_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ -lm
