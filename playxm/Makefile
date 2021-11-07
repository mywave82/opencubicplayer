TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(BUILD_DUMPTOOLS),1)
	DUMPERS=dumpmod
else
	DUMPERS=
endif

all: xmpptype.o playxm$(LIB_SUFFIX) $(DUMPERS)

playxm_so=xmlmod.o xmlmxm.o xmload.o xmload.o xmpinst.o xmplay.o xmpplay.o xmptrak.o xmrtns.o xmtime.o
playxm$(LIB_SUFFIX): $(playxm_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX) dumpmod

install:
	$(CP) playxm$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"
ifeq ($(BUILD_DUMPTOOLS),1)
	$(CP) dumpmod "$(DESTDIR)$(BINDIR)/dumpmod"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playxm$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(BINDIR)/dumpmod"

dumpmod: dumpmod.o
	$(CC) $(LDFLAGS) -o $@ $^

dumpmod.o: \
	dumpmod.c
	$(CC) -o $@ $< -c


xmpptype.o: xmpptype.c \
	../config.h \
	../types.h \
	../filesel/filesystem.h \
	../filesel/dirdb.h \
	../filesel/pfilesel.h \
	../filesel/mdb.h \
	../stuff/cp437.h
	$(CC) xmpptype.c -o $@ -c

xmlmod.o: xmlmod.c \
	../config.h \
	xmplay.h \
	../types.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	../stuff/err.h
	$(CC) xmlmod.c -o $@ -c

xmlmxm.o: xmlmxm.c \
	../config.h \
	xmplay.h \
	../types.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	../stuff/err.h
	$(CC) xmlmxm.c -o $@ -c

xmload.o: xmload.c \
	../config.h \
	xmplay.h \
	../types.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	../stuff/err.h
	$(CC) xmload.c -o $@ -c

xmpinst.o: xmpinst.c \
	../config.h \
	xmplay.h \
	../types.h \
	../dev/mcp.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h
	$(CC) xmpinst.c -o $@ -c

xmplay.o: xmplay.c \
	../config.h \
	xmplay.h \
	../types.h \
	../dev/mcp.h \
	../stuff/err.h
	$(CC) xmplay.c -o $@ -c

xmpplay.o: xmpplay.c \
	../config.h \
	xmplay.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/deviwave.h \
	../dev/mcp.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h
	$(CC) xmpplay.c -o $@ -c

xmptrak.o: xmptrak.c \
	../config.h \
	xmplay.h \
	../types.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h
	$(CC) xmptrak.c -o $@ -c

xmrtns.o: xmrtns.c \
	../config.h \
	xmplay.h \
	../types.h \
	../dev/mcp.h \
	../stuff/err.h
	$(CC) xmrtns.c -o $@ -c

xmtime.o: xmtime.c \
	../config.h \
	xmplay.h \
	../types.h \
	../stuff/err.h
	$(CC) xmtime.c -o $@ -c
