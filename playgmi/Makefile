TOPDIR=../
include $(TOPDIR)Rules.make

all: gmiptype.o playgmi$(LIB_SUFFIX)

playgmi_so=gmiinit.o gmiload.o gmipchan.o gmipdots.o gmipinst.o gmipplay.o gmiplay.o gmipat.o gmifff.o gmiultra.o gmifreepats.o gmitimidity.o
playgmi$(LIB_SUFFIX): $(playgmi_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX) dumppat

install:
	$(CP) playgmi$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/playgmi$(LIB_SUFFIX)"

gmipat.o: gmipat.c gmipat.h gmiplay.h \
	../config.h \
	../types.h \
	../dev/mcp.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) gmipat.c -o $@ -c

gmiultra.o: gmiultra.c gmipat.h gmiplay.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/compat.h \
	../stuff/err.h
	$(CC) gmiultra.c -o $@ -c

gmifreepats.o: gmifreepats.c gmipat.h gmiplay.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../stuff/err.h
	$(CC) gmifreepats.c -o $@ -c

gmitimidity.o: gmitimidity.c gmipat.h gmiplay.h \
	../config.h \
	../types.h \
	../stuff/err.h
	$(CC) gmitimidity.c -o $@ -c

gmifff.o: gmifff.c gmiplay.h \
	../config.h \
	../types.h
	$(CC) gmifff.c -o $@ -c

gmiinit.o: gmiinit.c gmiplay.h \
	../config.h \
	../types.h \
	../dev/mcp.h \
	../boot/psetting.h \
	../stuff/compat.h
	$(CC) gmiinit.c -o $@ -c

gmiload.o: gmiload.c gmiplay.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../dev/mcp.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) gmiload.c -o $@ -c

gmipchan.o: gmipchan.c gmiplay.h \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h
	$(CC) gmipchan.c -o $@ -c

gmipdots.o: gmipdots.c gmiplay.h \
	../config.h \
	../types.h \
	../dev/mcp.h \
	../cpiface/cpiface.h
	$(CC) gmipdots.c -o $@ -c

gmipinst.o: gmipinst.c gmiplay.h \
	../config.h \
	../types.h \
	../dev/mcp.h \
	../cpiface/cpiface.h
	$(CC) gmipinst.c -o $@ -c

gmipplay.o: gmipplay.c gmiplay.h \
	../config.h \
	../types.h \
	../filesel/pfilesel.h \
	../dev/mcp.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	../stuff/poutput.h \
	../filesel/mdb.h \
	../stuff/err.h \
	../dev/deviwave.h \
	../stuff/compat.h \
	../cpiface/cpiface.h
	$(CC) gmipplay.c -o $@ -c

gmiplay.o: gmiplay.c gmiplay.h \
	../config.h \
	../types.h \
	../dev/mcp.h
	$(CC) gmiplay.c -o $@ -c

gmiptype.o: gmiptype.c \
	../config.h \
	../types.h \
	../filesel/mdb.h \
	../stuff/compat.h
	$(CC) gmiptype.c -o $@ -c

dumppat.o: dumppat.c \
	../config.h \
	../types.h \
	gmipat.h
	$(CC) dumppat.c -o $@ -c

dumppat: dumppat.o
	$(CC) -o $@ $^
