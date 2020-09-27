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

test-dwmixqa.o: test-dwmixqa.c ../config.h ../dev/mix.h devwmix.h dwmix.h dwmixqa.h ../stuff/pagesize.inc.c
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
	$(CP) devwnone$(LIB_SUFFIX) devwmix$(LIB_SUFFIX) devwmixf$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/devwnone$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/devwmix$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/devwmixf$(LIB_SUFFIX)"

devwnone.o: devwnone.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../dev/imsdev.h \
	../dev/mcp.h \
	../stuff/timer.h \
	../dev/mix.h \
	../stuff/imsrtns.h \
	dwnone_asminc.c
	$(CC) -O devwnone.c -o $@ -c

devwmix.o: devwmix.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../dev/devigen.h \
	../dev/mcp.h \
	../dev/mix.h \
	../stuff/poll.h \
	../dev/player.h \
	devwmix.h \
	../stuff/imsrtns.h \
	../stuff/pagesize.inc.c \
	dwmix.h \
	dwmixa.h \
	dwmixqa.h
	$(CC) devwmix.c -o $@ -c

dwmixa.o: dwmixa.c dwmixa.h \
	dwmixa_x86.c \
	dwmixa_x86_emu.c \
	dwmixa_c.c \
	dwmix.h \
	../config.h \
	../types.h \
	../dev/mix.h \
	../asm_emu/x86*.h
	$(CC) -O dwmixa.c -o $@ -c

dwmixqa.o: dwmixqa.c dwmixqa.h \
	dwmixqa_x86.c \
	dwmixqa_c.c \
	dwmix.h \
	../config.h \
	../types.h
	$(CC) -O dwmixqa.c -o $@ -c

devwmixf.o: devwmixf.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../dev/imsdev.h \
	../dev/mcp.h \
	../dev/mix.h \
	../dev/player.h \
	../stuff/imsrtns.h \
	../stuff/pagesize.inc.c \
	devwmixf.h \
	../stuff/poll.h \
	dwmixfa.h \
	../dev/devigen.h \
	../boot/psetting.h \
	../dev/deviplay.h \
	../boot/plinkman.h
	$(CC) devwmixf.c -o $@ -c

dwmixfa.o: dwmixfa.c \
	dwmixfa_8087.c \
	dwmixfa_8087_emu.c \
	dwmixfa_c.c \
	../config.h \
	../types.h \
	dwmixfa.h \
	../asm_emu/x86*.h
	$(CC) dwmixfa.c -o $@ -c
