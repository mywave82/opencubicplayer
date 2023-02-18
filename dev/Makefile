TOPDIR=../
include $(TOPDIR)Rules.make
include Makefile-static

ifeq ($(STATIC_CORE),1)
all: $(plrbase_so) $(mcpbase_so) $(mchasm_so)
else
all: plrbase$(LIB_SUFFIX) mcpbase$(LIB_SUFFIX) mchasm$(LIB_SUFFIX)
endif

#mixclip$(LIB_SUFFIX): $(mixclip_so)
#	$(CC) $(SHARED_FLAGS) -o $@ $^

plrbase$(LIB_SUFFIX): $(plrbase_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

mcpbase$(LIB_SUFFIX): $(mcpbase_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

mchasm$(LIB_SUFFIX): $(mchasm_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX) mchasm_test smpman_asminctest ringbuffer-unit-test

ifeq ($(STATIC_CORE),1)
install:
uninstall:
else
install:
	$(CP) mcpbase$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/20-mcpbase$(LIB_SUFFIX)"
	$(CP) plrbase$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/20-plrbase$(LIB_SUFFIX)"
	$(CP) mchasm$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/10-mchasm$(LIB_SUFFIX)"


uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/20-mcpbase$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/20-plrbase$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/10-mchasm$(LIB_SUFFIX)"
endif

test: ringbuffer-unit-test mchasm_test smpman_asminctest
	./ringbuffer-unit-test
	./mchasm_test
	./smpman_asminctest

ringbuffer-unit-test: \
	ringbuffer.c \
	../config.h \
	../types.h
	$(CC) ringbuffer.c -o $@ -DUNIT_TEST

deviplay.o: deviplay.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h \
	../filesel/filesystem-file-dev.h \
	../filesel/filesystem-setup.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h
	$(CC) deviplay.c -o $@ -c

deviwave.o: deviwave.c deviwave.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../dev/deviwave.h \
	../dev/mcp.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h \
	../filesel/filesystem-file-dev.h \
	../filesel/filesystem-setup.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h
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
	../cpiface/cpiface.h \
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
	../cpiface/cpiface.h \
	mchasm.h \
	mcp.h \
	../stuff/imsrtns.h
	$(CC) player.c -o $@ -c

plrasm.o: plrasm.c plrasm.h \
	../config.h \
	../types.h
	$(CC) plrasm.c -o $@ -c

ringbuffer.o: ringbuffer.c ringbuffer.h \
	../config.h \
	../types.h
	$(CC) ringbuffer.c -o $@ -c

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
