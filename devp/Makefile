TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(HAVE_ALSA),1)
ALSA_SO=devpalsa$(LIB_SUFFIX)
endif

ifeq ($(HAVE_OSS),1)
OSS_SO=devposs$(LIB_SUFFIX)
endif

ifeq ($(HAVE_COREAUDIO),1)
COREAUDIO_SO=devpcoreaudio$(LIB_SUFFIX)
endif

ifeq ($(HAVE_SDL),1)
SDL_SO=devpsdl$(LIB_SUFFIX)
endif

ifeq ($(HAVE_SDL2),1)
SDL2_SO=devpsdl2$(LIB_SUFFIX)
endif

all: devpnone$(LIB_SUFFIX) devpdisk$(LIB_SUFFIX) $(OSS_SO) $(ALSA_SO) $(COREAUDIO_SO) $(SDL_SO) $(SDL2_SO)

devpnone_so=devpnone.o
devpnone$(LIB_SUFFIX): $(devpnone_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

devposs_so=devposs.o
devposs$(LIB_SUFFIX):$(devposs_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) $(OSS_LIBS) -o $@ $^

devpdisk_so=devpdisk.o
devpdisk$(LIB_SUFFIX):$(devpdisk_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

devpalsa_so=devpalsa.o
devpalsa$(LIB_SUFFIX):$(devpalsa_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(ALSA_LIBS)

devpcoreaudio_so=devpcoreaudio.o
devpcoreaudio$(LIB_SUFFIX):$(devpcoreaudio_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(COREAUDIO_LIBS) $(PTHREAD_LIBS)

devpsdl_so=devpsdl.o
devpsdl$(LIB_SUFFIX):$(devpsdl_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(SDL_LIBS)

devpsdl2_so=devpsdl2.o
devpsdl2$(LIB_SUFFIX):$(devpsdl2_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^ $(SDL2_LIBS)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) devpnone$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/35-devpnone$(LIB_SUFFIX)"
	$(CP) devpdisk$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/35-devpdisk$(LIB_SUFFIX)"
ifeq ($(HAVE_ALSA),1)
	$(CP) $(ALSA_SO) "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(ALSA_SO)"
endif
ifeq ($(HAVE_OSS),1)
	$(CP) $(OSS_SO) "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(OSS_SO)"
endif
ifeq ($(HAVE_COREAUDIO),1)
	$(CP) $(COREAUDIO_SO) "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(COREAUDIO_SO)"
endif
ifeq ($(HAVE_SDL),1)
	$(CP) $(SDL_SO) "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(SDL_SO)"
endif
ifeq ($(HAVE_SDL2),1)
	$(CP) $(SDL2_SO) "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(SDL2_SO)"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-devpnone$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-devpdisk$(LIB_SUFFIX)"
ifeq ($(HAVE_ALSA),1)
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(ALSA_SO)"
endif
ifeq ($(HAVE_OSS),1)
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(OSS_SO)"
endif
ifeq ($(HAVE_COREAUDIO),1)
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(COREAUDIO_SO)"
endif
ifeq ($(HAVE_SDL),1)
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(SDL_SO)"
endif
ifeq ($(HAVE_SDL2),1)
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/35-$(SDL2_SO)"
endif

devposs.o: devposs.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../cpiface/vol.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../boot/psetting.h  \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) devposs.c $(OSS_CFLAGS) -o $@ -c

devpnone.o: devpnone.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) devpnone.c -o $@ -c

devpdisk.o: devpdisk.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) devpdisk.c -o $@ -c

devpalsa.o: devpalsa.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../cpiface/vol.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h \
	../filesel/filesystem-file-dev.h \
	../filesel/filesystem-setup.h \
	../filesel/mdb.h \
	../filesel/modlist.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h \
	../stuff/utf-8.h
	$(CC) devpalsa.c -o $@ -c $(ALSA_CFLAGS)

devpcoreaudio.o: devpcoreaudio.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) devpcoreaudio.c -o $@ -c $(DEVPCOREAUDIO_CFLAGS)

devpsdl.o: devpsdl.c \
	../config.h \
	../types.h \
	devpsdl-common.c \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) devpsdl.c -o $@ -c $(SDL_CFLAGS)

devpsdl2.o: devpsdl2.c \
	../config.h \
	../types.h \
	devpsdl-common.c \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../stuff/err.h \
	../stuff/imsrtns.h
	$(CC) devpsdl2.c -o $@ -c $(SDL2_CFLAGS)
