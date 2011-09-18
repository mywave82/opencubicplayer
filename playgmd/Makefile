TOPDIR=../
include $(TOPDIR)Rules.make

all: gmdptype.o load669$(LIB_SUFFIX) loadams$(LIB_SUFFIX) loaddmf$(LIB_SUFFIX) loadmdl$(LIB_SUFFIX) loadmtm$(LIB_SUFFIX) loadokt$(LIB_SUFFIX) loadptm$(LIB_SUFFIX) loads3m$(LIB_SUFFIX) loadult$(LIB_SUFFIX) playgmd$(LIB_SUFFIX)

playgmd_so=gmdpchan.o gmdpdots.o gmdpinst.o gmdplay.o gmdpplay.o gmdptrak.o gmdrtns.o gmdtime.o
playgmd$(LIB_SUFFIX): $(playgmd_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

load669_so=gmdl669.o
load669$(LIB_SUFFIX): $(load669_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

loadams_so=gmdlams.o
loadams$(LIB_SUFFIX): $(loadams_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

loaddmf_so=gmdldmf.o
loaddmf$(LIB_SUFFIX): $(loaddmf_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

loadmdl_so=gmdlmdl.o
loadmdl$(LIB_SUFFIX): $(loadmdl_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

loadmtm_so=gmdlmtm.o
loadmtm$(LIB_SUFFIX): $(loadmtm_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

loadokt_so=gmdlokt.o
loadokt$(LIB_SUFFIX): $(loadokt_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

loadptm_so=gmdlptm.o
loadptm$(LIB_SUFFIX): $(loadptm_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

loads3m_so=gmdls3m.o
loads3m$(LIB_SUFFIX): $(loads3m_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

loadult_so=gmdlult.o
loadult$(LIB_SUFFIX): $(loadult_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) load669$(LIB_SUFFIX) loadams$(LIB_SUFFIX) loaddmf$(LIB_SUFFIX) loadmdl$(LIB_SUFFIX) loadmtm$(LIB_SUFFIX) loadokt$(LIB_SUFFIX) loadptm$(LIB_SUFFIX) loads3m$(LIB_SUFFIX) loadult$(LIB_SUFFIX) playgmd$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/load669$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/loadams$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/loaddmf$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/loadmdl$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/loadmtm$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/loadokt$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/loadptm$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/loads3m$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/loadult$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/playgmd$(LIB_SUFFIX)"


gmdptype.o: gmdptype.c \
	../config.h \
	../types.h \
	../filesel/mdb.h
	$(CC) gmdptype.c -o $@ -c

gmdl669.o: gmdl669.c \
	../config.h \
	gmdplay.h \
	gmdinst.h \
	../boot/plinkman.h \
	../dev/mcp.h \
	../stuff/err.h \
	../types.h
	$(CC) gmdl669.c -o $@ -c

gmdlams.o: gmdlams.c \
	../config.h \
	gmdplay.h \
	gmdinst.h \
	../boot/plinkman.h \
	../dev/mcp.h \
	../stuff/err.h \
	../types.h
	$(CC) gmdlams.c -o $@ -c

gmdldmf.o: gmdldmf.c \
	../config.h \
	gmdplay.h \
	gmdinst.h \
	../boot/plinkman.h \
	../dev/mcp.h \
	../stuff/err.h \
	../types.h
	$(CC) gmdldmf.c -o $@ -c

gmdlmdl.o: gmdlmdl.c \
	../config.h \
	gmdplay.h \
	gmdinst.h \
	../boot/plinkman.h \
	../dev/mcp.h \
	../stuff/err.h \
	../types.h
	$(CC) gmdlmdl.c -o $@ -c

gmdlmtm.o: gmdlmtm.c \
	../config.h \
	gmdplay.h \
	gmdinst.h \
	../boot/plinkman.h \
	../dev/mcp.h \
	../stuff/err.h \
	../types.h
	$(CC) gmdlmtm.c -o $@ -c

gmdlokt.o: gmdlokt.c \
	../config.h \
	gmdplay.h \
	gmdinst.h \
	../boot/plinkman.h \
	../dev/mcp.h \
	../stuff/err.h \
	../types.h
	$(CC) gmdlokt.c -o $@ -c

gmdlptm.o: gmdlptm.c \
	../config.h \
	gmdplay.h \
	gmdinst.h \
	../boot/plinkman.h \
	../dev/mcp.h \
	../stuff/err.h \
	../types.h
	$(CC) gmdlptm.c -o $@ -c

gmdls3m.o: gmdls3m.c \
	../config.h \
	gmdplay.h \
	gmdinst.h \
	../boot/plinkman.h \
	../dev/mcp.h \
	../stuff/err.h \
	../types.h
	$(CC) gmdls3m.c -o $@ -c

gmdlult.o: gmdlult.c \
	../config.h \
	gmdplay.h \
	gmdinst.h \
	../boot/plinkman.h \
	../dev/mcp.h \
	../stuff/err.h \
	../types.h
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
	../dev/mcp.h \
	gmdplay.h \
	gmdpdots.h \
	../cpiface/cpiface.h
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
	../dev/mcp.h \
	gmdplay.h \
	../stuff/imsrtns.h \
	../cpiface/cpiface.h
	$(CC) gmdplay.c -o $@ -c

gmdpplay.o: gmdpplay.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/mdb.h \
	../dev/mcp.h \
	../boot/psetting.h \
	gmdplay.h \
	gmdpchan.h \
	gmdpdots.h \
	gmdptrak.h \
	../stuff/compat.h \
	../stuff/poutput.h \
	../stuff/err.h \
	../boot/plinkman.h \
	../dev/deviwave.h \
	../cpiface/cpiface.h \
	../stuff/compat.h \
	../filesel/pfilesel.h
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
