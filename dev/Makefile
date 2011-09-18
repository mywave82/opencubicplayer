TOPDIR=../
include $(TOPDIR)Rules.make

mixclip_so=mixclip.o

plrbase_so=deviplay.o plrasm.o player.o

devi_so=devigen.o

mcpbase_so=deviwave.o mcp.o mix.o mixasm.o smpman.o

mchasm_so=mchasm.o

smpbase_so=devisamp.o sampler.o

ifeq ($(STATIC_BUILD),1)
all: $(mixclip_so) $(plrbase_so) $(devi_so) $(mcpbase_so) $(mchasm_so) $(smpbase_so)
else
all: devi$(LIB_SUFFIX) mcpbase$(LIB_SUFFIX) plrbase$(LIB_SUFFIX) smpbase$(LIB_SUFFIX) mchasm$(LIB_SUFFIX) mixclip$(LIB_SUFFIX)
endif

mixclip$(LIB_SUFFIX): $(mixclip_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

plrbase$(LIB_SUFFIX): $(plrbase_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

devi$(LIB_SUFFIX): $(devi_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

mcpbase$(LIB_SUFFIX): $(mcpbase_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

mchasm$(LIB_SUFFIX): $(mchasm_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

smpbase$(LIB_SUFFIX): $(smpbase_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX) mchasm_test smpman_asminctest

ifeq ($(STATIC_BUILD),1)
install:
uninstall:
else
install:
	$(CP) mcpbase$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-mcpbase$(LIB_SUFFIX)"
	$(CP) plrbase$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-plrbase$(LIB_SUFFIX)"
	$(CP) smpbase$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-smpbase$(LIB_SUFFIX)"
	$(CP) mixclip$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/10-mixclip$(LIB_SUFFIX)"
	$(CP) mchasm$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/10-mchasm$(LIB_SUFFIX)"
	$(CP) devi$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/10-devi$(LIB_SUFFIX)"


uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-mcpbase$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-plrbase$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-smpbase$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/10-mixclip$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/10-mchasm$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/10-devi$(LIB_SUFFIX)"
endif

devigen.o: devigen.c devigen.h \
	../config.h \
	../types.h \
	imsdev.h \
	devigen.h \
	../boot/psetting.h \
	../boot/plinkman.h
	$(CC) devigen.c -o $@ -c

deviplay.o: deviplay.c deviplay.h \
	../config.h \
	../types.h \
	../filesel/dirdb.h \
	../filesel/mdb.h \
	../filesel/modlist.h \
	../filesel/pfilesel.h \
	imsdev.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	../stuff/err.h \
	devigen.h \
	player.h \
	../stuff/compat.h
	$(CC) deviplay.c -o $@ -c

devisamp.o: devisamp.c devisamp.h \
	../config.h \
	../types.h \
	../filesel/dirdb.h \
	../filesel/mdb.h \
	../filesel/modlist.h \
	../filesel/pfilesel.h \
	imsdev.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	../stuff/err.h \
	devigen.h \
	sampler.h \
	../stuff/compat.h
	$(CC) devisamp.c -o $@ -c

deviwave.o: deviwave.c deviwave.h \
	../config.h \
	../types.h \
	../filesel/dirdb.h \
	../filesel/mdb.h \
	../filesel/modlist.h \
	../filesel/pfilesel.h \
	imsdev.h \
	player.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	../stuff/err.h \
	devigen.h \
	mcp.h \
	../stuff/compat.h
	$(CC) deviwave.c -c -o $@ -c

mchasm.o: mchasm.c mchasm.h \
	../config.h \
	../types.h \
	../boot/plinkman.h
	$(CC) mchasm.c -o $@ -c

mchasm_test.o: mchasm_test.c mchasm.h \
	../config.h \
	../types.h
	$(CC) mchasm_test.c -o $@ -c

mchasm_test: mchasm.o mchasm_test.o
	$(CC) -o $@ $^

mcp.o: mcp.c mcp.h \
	../config.h \
	../types.h
	$(CC) mcp.c -o $@ -c

mix.o: mix.c mix.h \
	../config.h \
	../types.h \
	mcp.h \
	../stuff/imsrtns.h \
	../stuff/pagesize.inc.c \
	mixasm.h
	$(CC) mix.c -o $@ -c

mixasm.o: mixasm.c mixasm.h \
	../config.h \
	../types.h \
	mix.h
	$(CC) -O mixasm.c -o $@ -c

player.o: player.c player.h \
	../config.h \
	../types.h \
	mchasm.h \
	../stuff/imsrtns.h
	$(CC) player.c -o $@ -c

plrasm.o: plrasm.c plrasm.h \
	../config.h \
	../types.h
	$(CC) plrasm.c -o $@ -c

sampler.o: sampler.c sampler.h \
	../config.h \
	../types.h \
	../stuff/imsrtns.h \
	mchasm.h
	$(CC) sampler.c -o $@ -c

smpman.o: smpman.c \
	../config.h \
	../types.h \
	smpman_asminc.c \
	mcp.h
	$(CC) smpman.c -o $@ -c

smpman_asminctest.o: smpman_asminctest.c smpman_asminc.c ../config.h
	$(CC) smpman_asminctest.c -o $@ -c

smpman_asminctest: smpman_asminctest.o
	$(CC) -o $@ $^

mixclip.o: mixclip.c mixclip.h \
	../config.h \
	../types.h \
	../boot/plinkman.h
	$(CC) mixclip.c -o $@ -c
