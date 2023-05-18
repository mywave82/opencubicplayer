TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(BUILD_DUMPTOOLS),1)
	DUMPERS=dumpstm$(EXE_SUFFIX) dumps3m$(EXE_SUFFIX)
else
	DUMPERS=
endif

all: playgmd$(LIB_SUFFIX) $(DUMPERS)

playgmd_so=gmdpchan.o gmdpdots.o gmdpinst.o gmdplay.o gmdpplay.o gmdptrak.o gmdrtns.o gmdtime.o gmdtype.o gmdl669.o gmdlams.o gmdldmf.o gmdlmdl.o gmdlmtm.o gmdlokt.o gmdlptm.o gmdls3m.o gmdlstm.o gmdlult.o
playgmd$(LIB_SUFFIX): $(playgmd_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX) dumpstm$(EXE_SUFFIX) dumps3m$(EXE_SUFFIX)

install:
	$(CP) playgmd$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playgmd$(LIB_SUFFIX)"

ifeq ($(BUILD_DUMPTOOLS),1)
	$(CP) dumpstm$(EXE_SUFFIX) "$(DESTDIR)$(BINDIR)/dumpstm$(EXE_SUFFIX)"
	$(CP) dumps3m$(EXE_SUFFIX) "$(DESTDIR)$(BINDIR)/dumps3m$(EXE_SUFFIX)"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playgmd$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(BINDIR)/dumpstm$(EXE_SUFFIX)"
	rm -f "$(DESTDIR)$(BINDIR)/dumps3m$(EXE_SUFFIX)"

dumpstm$(EXE_SUFFIX): dumpstm.o
	$(CC) $(LDFLAGS) -o $@ $^

dumpstm.o: \
	dumpstm.c
	$(CC) -o $@ $< -c

dumps3m$(EXE_SUFFIX): dumps3m.o
	$(CC) $(LDFLAGS) -o $@ $^

dumps3m.o: \
	dumps3m.c
	$(CC) -o $@ $< -c

gmdtype.o: gmdtype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/cp437.h \
	../stuff/err.h \
	gmdtype.h
	$(CC) gmdtype.c -o $@ -c

gmdl669.o: gmdl669.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdl669.c -o $@ -c

gmdlams.o: gmdlams.c \
	gmdlams-v1.c \
	gmdlams-v2.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdlams.c -o $@ -c

gmdldmf.o: gmdldmf.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdldmf.c -o $@ -c

gmdlmdl.o: gmdlmdl.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdlmdl.c -o $@ -c

gmdlmtm.o: gmdlmtm.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdlmtm.c -o $@ -c

gmdlokt.o: gmdlokt.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdlokt.c -o $@ -c

gmdlptm.o: gmdlptm.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdlptm.c -o $@ -c

gmdls3m.o: gmdls3m.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdls3m.c -o $@ -c

gmdlstm.o: gmdlstm.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdlstm.c -o $@ -c

gmdlult.o: gmdlult.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/filesystem.h \
	gmdplay.h \
	gmdinst.h \
	../stuff/err.h
	$(CC) gmdlult.c -o $@ -c

gmdpchan.o: gmdpchan.c \
	../config.h \
	../types.h \
	gmdplay.h \
	gmdpchan.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h
	$(CC) gmdpchan.c -o $@ -c

gmdpdots.o: gmdpdots.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	gmdplay.h \
	gmdpdots.h
	$(CC) gmdpdots.c -o $@ -c

gmdpinst.o: gmdpinst.c \
	../config.h \
	../types.h \
	../dev/mcp.h \
	gmdinst.h \
	../cpiface/cpiface.h
	$(CC) gmdpinst.c -o $@ -c

gmdplay.o: gmdplay.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	gmdplay.h \
	../stuff/imsrtns.h \
	../stuff/err.h
	$(CC) gmdplay.c -o $@ -c

gmdpplay.o: gmdpplay.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	gmdpchan.h \
	gmdpdots.h \
	gmdplay.h \
	gmdptrak.h \
	gmdtype.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) gmdpplay.c -o $@ -c

gmdptrak.o: gmdptrak.c \
	../config.h \
	../types.h \
	gmdplay.h \
	gmdptrak.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h
	$(CC) gmdptrak.c -o $@ -c

gmdrtns.o: gmdrtns.c \
	../config.h \
	../types.h \
	../dev/mcp.h \
	gmdplay.h
	$(CC) gmdrtns.c -o $@ -c

gmdtime.o: gmdtime.c \
	../config.h \
	../types.h \
	gmdplay.h
	$(CC) gmdtime.c -o $@ -c
