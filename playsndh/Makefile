TOPDIR=../
include $(TOPDIR)Rules.make

TARGETS=playsndh$(LIB_SUFFIX)

CFLAGS+=-fPIC

all: $(TARGETS)

clean:
	rm -f *.o *$(LIB_SUFFIX) psgplay-git/include/tos/tos.h
	rm -f psgplay-git/lib/m68k/m68kmake
	rm -f psgplay-git/lib/m68k/m68kops.c
	rm -f psgplay-git/include/m68k/m68kops.h

install:
	$(CP) playsndh$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playsndh$(LIB_SUFFIX)"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playsndh$(LIB_SUFFIX)"

sndhtype.o: sndhtype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/dirdb.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	sndhtype.h \
	psgplay-git/lib/toslibc/include/toslibc/unicode/atari.h \
	psgplay-git/include/ice/ice.h
	$(CC) $< -o $@ -c

playsndh_so=sndhtype.o sndhplay.o sndhpplay.o sndhtracks.o \
	atari_bus.o \
	atari_cpu.o \
	atari_dac.o \
	atari_device.o \
	atari_exception-vector.o \
	atari_fdc.o \
	atari_glue.o \
	atari_machine.o \
	atari_mixer.o \
	atari_mfp.o \
	atari_mmu.o \
	atari_mmu-trace.o \
	atari_psg.o \
	atari_ram.o \
	atari_rom.o \
	atari_shifter.o \
	atari_sound.o \
	atari_system_variable.o \
	cf2149_module_cf2149.o \
	cf300588_module_cf300588-sound.o \
	cf68901_module_cf68901.o \
	internal_string.o \
	m68k_m68kcpu.o \
	m68k_m68kops.o \
	psgplay_psgplay.o \
	psgplay_sndh.o \
	toslibc_unicode-atari.o \
	toslibc_unicode-utf8.o
ifeq ($(SNDH_MUSTPROVIDE_ICE),1)
playsndh_so+=ice_ice.o
endif

playsndh$(LIB_SUFFIX): $(playsndh_so)
	$(CC) $(SHARED_FLAGS) -o $@ $^

#z80.o: z80.h z80.c z80ops.c edops.c cbops.c main.h
#	$(CC) -o $@ z80.c -c
#
#sound.o: sound.c main.h z80.h sound.h \
#	../config.h \
#	../types.h
#	$(CC) -o $@ sound.c -c

#aychan.o: aychan.c \
#	ayplay.h \
#	sound.h \
#	../config.h \
#	../types.h \
#	../cpiface/cpiface.h \
#	../dev/mcp.h \
#	../stuff/poutput.h
#	$(CC) -o $@ aychan.c -c

sndhpplay.o: sndhpplay.c sndhplay.h \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h
	$(CC) $< -o $@ -c

sndhplay.o: sndhplay.c sndhplay.h sndhtype.h \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	psgplay-git/include/system/unix/file.h \
	psgplay-git/include/system/unix/option.h \
	psgplay-git/include/psgplay/psgplay.h \
	psgplay-git/include/psgplay/sndh.h \
	psgplay-git/include/ice/ice.h
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

sndhtracks.o: sndhtracks.c \
	sndhtype.h \
	sndhplay.h \
	../config.h \
	../types.h \
	../stuff/poutput.h \
	../cpiface/cpiface.h
	$(CC) $< -o $@ -c

psgplay-git/include/tos/tos.h: ./psgplay-git/lib/tos/tos
	cd psgplay-git; script/tos lib/tos/tos include/tos/tos.h

atari_bus.o: psgplay-git/lib/atari/bus.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_cpu.o: psgplay-git/lib/atari/cpu.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_dac.o: psgplay-git/lib/atari/dac.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_device.o: psgplay-git/lib/atari/device.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_exception-vector.o: psgplay-git/lib/atari/exception-vector.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_fdc.o: psgplay-git/lib/atari/fdc.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_glue.o: psgplay-git/lib/atari/glue.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_machine.o: psgplay-git/lib/atari/machine.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_mfp.o: psgplay-git/lib/atari/mfp.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_mixer.o: psgplay-git/lib/atari/mixer.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_mmu.o: psgplay-git/lib/atari/mmu.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_mmu-trace.o: psgplay-git/lib/atari/mmu-trace.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_ram.o: psgplay-git/lib/atari/ram.c psgplay-git/include/tos/tos.h
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_psg.o: psgplay-git/lib/atari/psg.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_rom.o: psgplay-git/lib/atari/rom.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_shifter.o: psgplay-git/lib/atari/shifter.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_sound.o: psgplay-git/lib/atari/sound.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

atari_system_variable.o: psgplay-git/lib/atari/system-variable.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

cf2149_module_cf2149.o: psgplay-git/lib/cf2149/module/cf2149.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/cf2149/include

cf300588_module_cf300588-sound.o: psgplay-git/lib/cf300588/module/cf300588-sound.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/cf300588/include

cf68901_module_cf68901.o: psgplay-git/lib/cf68901/module/cf68901.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/cf68901/include

ice_ice.o: psgplay-git/lib/ice/ice.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include

internal_string.o: psgplay-git/lib/internal/string.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include

m68k_m68kcpu.o: psgplay-git/lib/m68k/m68kcpu.c psgplay-git/include/m68k/m68kops.h
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

m68k_m68kops.o: psgplay-git/lib/m68k/m68kops.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/cf300588/include

psgplay_sndh.o: psgplay-git/lib/psgplay/sndh.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/toslibc/include

psgplay_psgplay.o: psgplay-git/lib/psgplay/psgplay.c
	$(CC) $< -o $@ -c -Ipsgplay-git/include -Ipsgplay-git/lib/cf2149/include -Ipsgplay-git/lib/toslibc/include -Ipsgplay-git/lib/cf68901/include -Ipsgplay-git/lib/cf300588/include

psgplay-git/lib/m68k/m68kops.c psgplay-git/include/m68k/m68kops.h: psgplay-git/lib/m68k/m68k_in.c psgplay-git/lib/m68k/m68kmake
	cd psgplay-git; lib/m68k/m68kmake . lib/m68k/m68k_in.c; cd ..

psgplay-git/lib/m68k/m68kmake: psgplay-git/lib/m68k/m68kmake.c
	$(CC_FOR_BUILD) $< -o $@

toslibc_unicode-atari.o: psgplay-git/lib/toslibc/lib/unicode-atari.c
	$(CC) $< -o $@ -c -Ipsgplay-git/lib/toslibc/include

toslibc_unicode-utf8.o: psgplay-git/lib/toslibc/lib/unicode-utf8.c
	$(CC) $< -o $@ -c -Ipsgplay-git/lib/toslibc/include
