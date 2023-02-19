TOPDIR=../
include $(TOPDIR)Rules.make
include Makefile-static

ifeq ($(STATIC_CORE),1)
all: $(stuff_libocp_so) $(sets_so) $(poutput_so)
else
all: $(stuff_libocp_so) sets$(LIB_SUFFIX) poutput$(LIB_SUFFIX) poutput-keyboard.o
endif

test: compat-test
	./compat-test.sh

poutput$(LIB_SUFFIX): $(poutput_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(poutput_so_libs)

sets$(LIB_SUFFIX): $(sets_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX) compat-test

ifeq ($(STATIC_CORE),1)
install:
uninstall:
else
install:
	$(CP) poutput$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/12-poutput$(LIB_SUFFIX)"
	$(CP) sets$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/10-sets$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/12-poutput$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/10-sets$(LIB_SUFFIX)"
endif

cp437.o: cp437.c cp437.h \
	../config.h \
	../types.h \
	utf-8.h
	$(CC) $< -o $@ -c

compat-test: compat-test.c \
	compat.c \
	compat.h \
	../config.h \
	../types.h \
	../boot/plinkman.h
	$(CC) $< -o $@

poutput.o: poutput.c \
	../config.h \
	../types.h \
	imsrtns.h \
	framelock.h \
	pfonts.h \
	poutput.h \
	poutput-keyboard.h
	$(CC) poutput.c -o $@ -c

poutput-keyboard.o: poutput-keyboard.c \
	../config.h \
	../types.h \
	poutput.h \
	poutput-keyboard.h
	$(CC) poutput-keyboard.c -o $@ -c

poll.o: poll.c poll.h \
	../config.h \
	../types.h \
	imsrtns.h \
	poll.h
	$(CC) poll.c -o $@ -c

latin1.o: latin1.c latin1.h \
	../config.h \
	../types.h \
	utf-8.h
	$(CC) $< -o $@ -c

poutput-curses.o: poutput-curses.c \
	../config.h \
	../types.h \
	../boot/console.h \
	../boot/psetting.h \
	../stuff/cp437.h \
	../stuff/poll.h \
	../stuff/poutput.h \
	../stuff/poutput-curses.h \
	../stuff/poutput-keyboard.h \
	../stuff/utf-8.h
	$(CC) poutput-curses.c -o $@ -c

poutput-vcsa.o: poutput-vcsa.c \
	../config.h \
	../types.h \
	../boot/console.h \
	../boot/psetting.h \
	../stuff/pfonts.h \
	../stuff/poutput.h \
	../stuff/poutput-keyboard.h \
	../stuff/poutput-vcsa.h \
	../stuff/utf-8.h
	$(CC) poutput-vcsa.c -o $@ -c

poutput-fb.o: poutput-fb.c poutput-fb.h \
	../config.h \
	../types.h \
	../boot/console.h \
	poutput.h
	$(CC) poutput-fb.c -o $@ -c

poutput-fontengine.o: poutput-fontengine.c poutput-fontengine.h \
	../config.h \
	../types.h \
	pfonts.h \
	cp437.h \
	ttf.h \
	utf-8.h
	$(CC) $< -o $@ -c

poutput-sdl.o: poutput-sdl.c \
	ttf.h \
	../config.h \
	../types.h \
	../boot/console.h \
	../boot/psetting.h \
	../stuff/framelock.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h \
	../stuff/poutput-fontengine.h \
	../stuff/poutput-keyboard.h \
	../stuff/poutput-sdl.h \
	../stuff/poutput-swtext.h
	$(CC) $(SDL_CFLAGS) poutput-sdl.c -o $@ -c

poutput-sdl2.o: poutput-sdl2.c \
	ttf.h \
	../config.h \
	../types.h \
	../boot/console.h \
	../boot/psetting.h \
	../stuff/framelock.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h \
	../stuff/poutput-fontengine.h \
	../stuff/poutput-keyboard.h \
	../stuff/poutput-sdl2.h \
	../stuff/poutput-swtext.h
	$(CC) $(SDL2_CFLAGS) poutput-sdl2.c -o $@ -c

poutput-swtext.o: poutput-swtext.c poutput-swtext.h \
	framelock.h \
	latin1.h \
	pfonts.h \
	poutput.h \
	poutput-fontengine.h \
	utf-8.h \
	../config.h \
	../types.h
	$(CC) $(CFLAGS) $< -o $@ -c

poutput-x11.o: poutput-x11.c \
	../config.h \
	../types.h \
	../boot/console.h \
	../boot/psetting.h \
	../desktop/opencubicplayer-48x48.xpm \
	../stuff/framelock.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h \
	../stuff/poutput-keyboard.h \
	../stuff/poutput-swtext.h \
	../stuff/poutput-x11.h \
	../stuff/pfonts.h \
	../stuff/poll.h \
	../stuff/x11-common.h
	$(CC) poutput-x11.c -o $@ -c

x11-common.o: x11-common.c x11-common.h \
	../config.h \
	../types.h \
	poutput.h
	$(CC) x11-common.c -o $@ -c

err.o: err.c err.h \
	../config.h \
	../types.h
	$(CC) err.c -o $@ -c

sets.o: sets.c sets.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	err.h
	$(CC) sets.c -o $@ -c

console.o: console.c \
	../boot/console.h \
	../config.h \
	../boot/psetting.h \
	../stuff/latin1.h \
	poutput-curses.h \
	poutput-sdl.h \
	poutput-sdl2.h \
	poutput-x11.h \
	poutput-vcsa.h \
	utf-8.h
	$(CC) console.c -o $@ -c

compat.o: compat.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	compat.h
	$(CC) compat.c -o $@ -c

pfonts.o: pfonts.c \
	../config.h \
	../types.h \
	pfonts.h
	$(CC) pfonts.c -o $@ -c

framelock.o: framelock.c framelock.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../filesel/pfilesel.h \
	poll.h \
	err.h
	$(CC) framelock.c -o $@ -c

ttf.o: ttf.c ttf.h \
	utf-8.h
	$(CC) $< -o $@ -c $(FREETYPE2_CFLAGS) $(FREETYPE2_CFLAGS)

utf-8.o: utf-8.c \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	poutput.h \
	framelock.h \
	utf-8.h
	$(CC) $< -o $@ -c
