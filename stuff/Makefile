TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(LINUX),1)
LINUX_OBJ=poutput-vcsa.o poutput-fb.o
LINUX_LIBS=$(ICONV_LIBS)
endif

compat_so=compat.o
framelock_so=framelock.o
hardware_so=timer.o irq.o hardware.o freq.o
sets_so=sets.o
poutput_so=console.o poutput-curses.o $(LINUX_OBJ) poutput-keyboard.o pfonts.o poutput.o
poutput_so_libs=
ifeq ($(HAVE_X11),1)
poutput_so+=poutput-x11.o x11-common.o
poutput_so_libs+=$(X11_LIBS)
endif
ifeq ($(HAVE_SDL),1)
poutput_so+=poutput-sdl.o
poutput_so_libs+=$(SDL_LIBS)
endif

ifeq ($(STATIC_BUILD),1)
all: err.o $(compat_so) $(framelock_so) $(hardware_so) $(sets_so) $(poutput_so)
else
all: hardware$(LIB_SUFFIX) err.o sets$(LIB_SUFFIX) poutput$(LIB_SUFFIX) compat$(LIB_SUFFIX) framelock$(LIB_SUFFIX)
endif

compat$(LIB_SUFFIX): $(compat_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

framelock$(LIB_SUFFIX): $(framelock_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

poutput$(LIB_SUFFIX): $(poutput_so)
	$(CC) $(SHARED_FLAGS) $(LINUX_LIBS) -o $@ $^ -lncurses $(poutput_so_libs)

hardware$(LIB_SUFFIX): $(hardware_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

sets$(LIB_SUFFIX): $(sets_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

ifeq ($(STATIC_BUILD),1)
install:
uninstall:
else
install:
	$(CP) poutput$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/12-poutput$(LIB_SUFFIX)"
	$(CP) framelock$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/11-framelock$(LIB_SUFFIX)"
	$(CP) compat$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/10-compat$(LIB_SUFFIX)"
	$(CP) sets$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/10-sets$(LIB_SUFFIX)"
	$(CP) hardware$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/10-hardware$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/hardware$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/autoload/12-poutput$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/autoload/10-compat$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/autoload/11-framelock$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/autoload/10-sets$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/autoload/10-hardware$(LIB_SUFFIX)"
endif

poutput.o: poutput.h poutput.c \
	../config.h \
	../types.h \
	imsrtns.h \
	pfonts.h
	$(CC) poutput.c -o $@ -c

poutput-keyboard.o: poutput-keyboard.c \
	poutput.h \
	../config.h \
	../types.h
	$(CC) poutput-keyboard.c -o $@ -c

timer.o: timer.c timer.h irq.h \
	../config.h \
	../types.h \
	compat.h \
	imsrtns.h \
	poll.h
	$(CC) timer.c -o $@ -c

irq.o: irq.c irq.h \
	../config.h \
	../types.h
	$(CC) irq.c -o $@ -c

poutput-curses.o: poutput-curses.c poutput-curses.h \
	../config.h \
	../types.h \
	poutput.h \
	../boot/console.h \
	../boot/psetting.h
	$(CC) poutput-curses.c -o $@ -c

poutput-vcsa.o: poutput-vcsa.c poutput-vcsa.h \
	../config.h \
	../types.h \
	poutput.h \
	../boot/console.h \
	../boot/psetting.h \
	pfonts.h
	$(CC) poutput-vcsa.c -o $@ -c

poutput-fb.o: poutput-fb.c poutput-fb.h \
	../config.h \
	../types.h \
	../boot/console.h \
	poutput.h \
	pfonts.h
	$(CC) poutput-fb.c -o $@ -c

poutput-sdl.o: poutput-sdl.c poutput-sdl.h \
	../config.h \
	../types.h \
	../boot/console.h \
	../boot/psetting.h \
	../stuff/framelock.h \
	poutput.h \
	pfonts.h
	$(CC) $(SDL_CFLAGS) poutput-sdl.c -o $@ -c

poutput-x11.o: poutput-x11.c poutput-x11.h \
	../config.h \
	../types.h \
	../boot/console.h \
	../boot/psetting.h \
	../desktop/opencubicplayer.xpm \
	../stuff/framelock.h \
	poutput.h \
	pfonts.h \
	x11-common.h
	$(CC) poutput-x11.c -o $@ -c

x11-common.o: x11-common.c x11-common.h \
	../config.h \
	../types.h \
	poutput.h
	$(CC) x11-common.c -o $@ -c

freq.o: freq.c freq.h imsrtns.h \
	../config.h \
	../types.h
	$(CC) freq.c -o $@ -c

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

hardware.o: hardware.c \
	../config.h \
	../types.h \
	../boot/plinkman.h
	$(CC) hardware.c -o $@ -c

console.o: console.c \
	../boot/console.h \
	../config.h \
	../boot/psetting.h \
	poutput-curses.h \
	poutput-fb.h \
	poutput-sdl.h \
	poutput-x11.h \
	poutput-vcsa.h
	$(CC) console.c -o $@ -c

compat.o: compat.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	compat.h
	$(CC) compat.c -o $@ -c

pfonts.o: pfonts.c \
	../config.h \
	pfonts.h
	$(CC) pfonts.c -o $@ -c

framelock.o: framelock.c framelock.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../filesel/pfilesel.h \
	timer.h \
	err.h
	$(CC) framelock.c -o $@ -c

