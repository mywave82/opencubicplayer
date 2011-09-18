TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(HAVE_OSS),1)
OSS_SO=devsoss$(LIB_SUFFIX)
endif


all: devsnone$(LIB_SUFFIX) $(OSS_SO)

devsnone_so=devsnone.o
devsnone$(LIB_SUFFIX): $(devsnone_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

devsoss_so=devsoss.o
devsoss$(LIB_SUFFIX): $(devsoss_so)
	$(CC) $(SHARED_FLAGS) $(OSS_LIBS) -o $@ $^

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
	$(CP) devsnone$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"
ifeq ($(HAVE_OSS),1)
	$(CP) $(OSS_SO) "$(DESTDIR)$(LIBDIR)"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/devsnone$(LIB_SUFFIX)"
ifeq ($(HAVE_OSS),1)
	rm -f "$(DESTDIR)$(LIBDIR)/$(OSS_SO)"
endif

devsnone.o: devsnone.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../dev/imsdev.h \
	../dev/sampler.h \
	../stuff/imsrtns.h
	$(CC) devsnone.c -o $@ -c

devsoss.o: devsoss.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../dev/devigen.h \
	../dev/imsdev.h \
	../dev/sampler.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h
	$(CC) $(OSS_CFLAGS) devsoss.c -o $@ -c
