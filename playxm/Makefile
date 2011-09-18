TOPDIR=../
include $(TOPDIR)Rules.make

all: xmpptype.o playxm$(LIB_SUFFIX)

playxm_so=xmlmod.o xmlmxm.o xmload.o xmload.o xmpinst.o xmplay.o xmpplay.o xmptrak.o xmrtns.o xmtime.o
playxm$(LIB_SUFFIX): $(playxm_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) playxm$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playxm$(LIB_SUFFIX)"

xmpptype.o: xmpptype.c \
	../config.h \
	../types.h \
	../filesel/mdb.h
	$(CC) xmpptype.c -o $@ -c

xmlmod.o: xmlmod.c \
	../config.h \
	xmplay.h \
	../types.h \
	../dev/mcp.h \
	../stuff/err.h
	$(CC) xmlmod.c -o $@ -c

xmlmxm.o: xmlmxm.c \
	../config.h \
	xmplay.h \
	../types.h \
	../dev/mcp.h \
	../stuff/err.h
	$(CC) xmlmxm.c -o $@ -c

xmload.o: xmload.c \
	../config.h \
	xmplay.h \
	../types.h \
	../dev/mcp.h \
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
	../dev/mcp.h \
	../dev/deviwave.h \
	../stuff/compat.h \
	../stuff/poutput.h \
	../filesel/pfilesel.h \
	../filesel/mdb.h \
	../stuff/compat.h \
	../stuff/err.h \
	../cpiface/cpiface.h
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
