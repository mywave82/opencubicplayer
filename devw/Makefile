TOPDIR=../
include $(TOPDIR)Rules.make

all: devwnone$(LIB_SUFFIX) devwmix$(LIB_SUFFIX) devwmixf$(LIB_SUFFIX)

test: test-dwmixa test-dwmixqa test-dwmixfa
	./test-dwmixa
	./test-dwmixqa
	./test-dwmixfa

test-dwmixa.o: test-dwmixa.c ../config.h dwmix.h dwmixa.h ../stuff/pagesize.inc.c
	$(CC) -c -o $@ test-dwmixa.c

test-dwmixa: test-dwmixa.o dwmixa.o
	$(CC) -o $@ $^

test-dwmixqa.o: test-dwmixqa.c ../config.h ../dev/mix.h dwmix.h dwmixqa.h ../stuff/pagesize.inc.c
	$(CC) -c -o $@ test-dwmixqa.c

test-dwmixqa: test-dwmixqa.o dwmixqa.o
	$(CC) -o $@ $^

test-dwmixfa.o: test-dwmixfa.c ../config.h dwmixfa.h ../stuff/pagesize.inc.c ../dev/mcp.h
	$(CC) -c -o $@ test-dwmixfa.c

test-dwmixfa: test-dwmixfa.o dwmixfa.o
	$(CC) -o $@ $^

devwnone_so=devwnone.o
devwnone$(LIB_SUFFIX): $(devwnone_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

devwmix_so=devwmix.o dwmixa.o dwmixqa.o
devwmix$(LIB_SUFFIX): $(devwmix_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

devwmixf_so=devwmixf.o dwmixfa.o
devwmixf$(LIB_SUFFIX): $(devwmixf_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(MATH_LIBS)

clean:
	rm -f *.o *$(LIB_SUFFIX) test-dwmixqa test-dwmixa test-dwmixfa

install:
	$(CP) devwnone$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/35-devwnone$(LIB_SUFFIX)"
	$(CP) devwmix$(LIB_SUFFIX)  "$(DESTDIR)$(LIBDIROCP)/autoload/35-devwmix$(LIB_SUFFIX)"
	$(CP) devwmixf$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/35-devwmixf$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-devwnone$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-devwmix$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-devwmixf$(LIB_SUFFIX)"

devwnone.o: devwnone.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/deviwave.h \
	../dev/mcp.h \
	../dev/mix.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	dwnone_asminc.c
	$(CC) -O devwnone.c -o $@ -c

devwmix.o: devwmix.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/deviwave.h \
	../dev/mcp.h \
	../dev/mix.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/postproc.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	../stuff/pagesize.inc.c \
	dwmix.h \
	dwmixa.h \
	dwmixqa.h
	$(CC) devwmix.c -o $@ -c

dwmixa.o: dwmixa.c dwmixa.h \
	dwmixa_c.c \
	dwmix.h \
	../config.h \
	../types.h \
	../dev/mix.h
	$(CC) -O dwmixa.c -o $@ -c

dwmixqa.o: dwmixqa.c dwmixqa.h \
	dwmixqa_c.c \
	dwmix.h \
	../config.h \
	../types.h
	$(CC) -O dwmixqa.c -o $@ -c

devwmixf.o: devwmixf.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/deviwave.h \
	../dev/mcp.h \
	../dev/mix.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/postproc.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	../stuff/pagesize.inc.c \
	dwmixfa.h
	$(CC) devwmixf.c -o $@ -c

dwmixfa.o: dwmixfa.c \
	dwmixfa_c.c \
	../config.h \
	../types.h \
        ../dev/mcp.h \
	../dev/postproc.h \
	dwmixfa.h
	$(CC) dwmixfa.c -o $@ -c
