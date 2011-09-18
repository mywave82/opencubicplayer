TOPDIR=../
include $(TOPDIR)Rules.make

all: itpptype.o playit$(LIB_SUFFIX)

playit_so=itload.o itpinst.o itplay.o itpplay.o itptrack.o itrtns.o itsex.o ittime.o
playit$(LIB_SUFFIX): $(playit_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playit$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playit$(LIB_SUFFIX)"

itpptype.o: itpptype.c \
	../config.h \
	../types.h \
	../filesel/mdb.h
	$(CC) itpptype.c -o $@ -c

itload.o: itload.c \
	../config.h \
	../types.h \
	itplay.h \
	../dev/mcp.h \
	../stuff/compat.h \
	../stuff/err.h
	$(CC) itload.c -o $@ -c

itpinst.o: itpinst.c \
	../config.h \
	../types.h \
	itplay.h \
	../dev/mcp.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h
	$(CC) itpinst.c -o $@ -c

itplay.o: itplay.c \
	../config.h \
	../types.h \
	itplay.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../stuff/imsrtns.h
	$(CC) itplay.c -o $@ -c

itpplay.o: itpplay.c \
	../config.h \
	../types.h \
	itplay.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	../stuff/compat.h \
	../stuff/poutput.h \
	../stuff/err.h \
	../dev/deviwave.h \
	../cpiface/cpiface.h
	$(CC) itpplay.c -o $@ -c

itptrack.o: itptrack.c \
	../config.h \
	../types.h \
	itplay.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h \
	../dev/mcp.h
	$(CC) itptrack.c -o $@ -c

itrtns.o: itrtns.c \
	../config.h \
	../types.h \
	itplay.h
	$(CC) itrtns.c -o $@ -c

# kb, how did you come up with this name? - stian
itsex.o: itsex.c \
	../config.h \
	../types.h \
	itplay.h \
	../stuff/imsrtns.h
	$(CC) itsex.c -o $@ -c

ittime.o: ittime.c \
	../config.h \
	../types.h \
	itplay.h
	$(CC) ittime.c -o $@ -c
