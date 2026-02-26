TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(BUILD_DUMPTOOLS),1)
	DUMPSID=dumpsid$(EXE_SUFFIX)
else
	DUMPSID=
endif

TARGETS=playsid$(LIB_SUFFIX)

LIBSIDPLAYCXXFLAGS+= -I./sidplayfp-config -I./libsidplayfp-git/src

LIBSIDPLAYOBJECTS= \
	libsidplayfp-builders-sidlite-builder-sidlite-builder.o \
	libsidplayfp-builders-sidlite-builder-sidlite-emu.o \
	libsidplayfp-builders-sidlite-builder-sidlite-ADSR.o \
	libsidplayfp-builders-sidlite-builder-sidlite-Filter.o \
	libsidplayfp-builders-sidlite-builder-sidlite-SID.o \
	libsidplayfp-builders-sidlite-builder-sidlite-WaveGen.o \
	libsidplayfp-builders-residfp-builder-residfp-builder.o \
	libsidplayfp-builders-residfp-builder-residfp-emu.o \
	libresidfp-Dac.o \
	libresidfp-EnvelopeGenerator.o \
	libresidfp-ExternalFilter.o \
	libresidfp-Filter.o \
	libresidfp-Filter6581.o \
	libresidfp-Filter8580.o \
	libresidfp-FilterModelConfig.o \
	libresidfp-FilterModelConfig6581.o \
	libresidfp-FilterModelConfig8580.o \
	libresidfp-Integrator6581.o\
	libresidfp-Integrator8580.o\
	libresidfp-OpAmp.o \
	libresidfp-residfp.o \
	libresidfp-SID.o \
	libresidfp-Spline.o \
	libresidfp-version.o \
	libresidfp-WaveformCalculator.o \
	libresidfp-WaveformGenerator.o \
	libresidfp-resample-SincResampler.o \
	libsidplayfp-c64-c64.o \
	libsidplayfp-c64-CIA-interrupt.o \
	libsidplayfp-c64-CIA-mos652x.o \
	libsidplayfp-c64-CIA-SerialPort.o \
	libsidplayfp-c64-CIA-timer.o \
	libsidplayfp-c64-CPU-mos6510.o \
	libsidplayfp-c64-CPU-mos6510debug.o \
	libsidplayfp-c64-CIA-tod.o \
	libsidplayfp-c64-mmu.o \
	libsidplayfp-c64-VIC_II-mos656x.o \
	libsidplayfp-EventScheduler.o \
	libsidplayfp-player.o \
	libsidplayfp-psiddrv.o \
	libsidplayfp-reloc65.o \
	libsidplayfp-sidemu.o \
	libsidplayfp-sidplayfp-sidbuilder.o \
	libsidplayfp-sidplayfp-SidConfig.o \
	libsidplayfp-sidplayfp-SidInfo.o \
	libsidplayfp-sidplayfp-SidTune.o \
	libsidplayfp-sidplayfp-SidTuneInfo.o \
	libsidplayfp-sidtune-p00.o \
	libsidplayfp-sidtune-prg.o \
	libsidplayfp-sidtune-MUS.o \
	libsidplayfp-sidtune-PSID.o \
	libsidplayfp-sidtune-SidTuneBase.o \
	libsidplayfp-sidtune-SidTuneTools.o \
	libsidplayfp-simpleMixer.o \
	libsidplayfp-api.o

all: $(TARGETS) $(DUMPSID)

clean:
	rm -f *.o *$(LIB_SUFFIX) libsidplayfp-git/src/sidtune/sidplayer1.bin libsidplayfp-git/libsidplayfp/src/sidtune/sidplayer2.bin libsidplayfp-git/src/psiddrv.bin dumpsid$(EXE_SUFFIX)

install:
	$(CP) playsid$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playsid$(LIB_SUFFIX)"
ifeq ($(BUILD_DUMPTOOLS),1)
	$(CP) dumpsid$(EXE_SUFFIX) "$(DESTDIR)$(BINDIR)/dumpsid$(EXE_SUFFIX)"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-sidplay$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(BINDIR)/dumpsid$(EXE_SUFFIX)"

playsid_so=cpiinfo.o cpisidsetup.o sidplay.o sidpplay.o sidconfig.o sidtype.o $(LIBSIDPLAYOBJECTS)
playsid$(LIB_SUFFIX): $(playsid_so)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^

libsidplayfp-builders-sidlite-builder-sidlite-builder.o: \
	libsidplayfp-git/src/builders/sidlite-builder/sidlite-builder.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libresidfp-git/src -o $@ $< -c
libsidplayfp-builders-sidlite-builder-sidlite-emu.o: \
	libsidplayfp-git/src/builders/sidlite-builder/sidlite-emu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libresidfp-git/src -o $@ $< -c
libsidplayfp-builders-sidlite-builder-sidlite-ADSR.o: \
	libsidplayfp-git/src/builders/sidlite-builder/sidlite/ADSR.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libresidfp-git/src -o $@ $< -c
libsidplayfp-builders-sidlite-builder-sidlite-Filter.o: \
	libsidplayfp-git/src/builders/sidlite-builder/sidlite/Filter.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libresidfp-git/src -o $@ $< -c
libsidplayfp-builders-sidlite-builder-sidlite-SID.o: \
	libsidplayfp-git/src/builders/sidlite-builder/sidlite/SID.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libresidfp-git/src -o $@ $< -c
libsidplayfp-builders-sidlite-builder-sidlite-WaveGen.o: \
	libsidplayfp-git/src/builders/sidlite-builder/sidlite/WavGen.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libresidfp-git/src -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-builder.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp-builder.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libresidfp-git/src -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-emu.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp-emu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -I./libresidfp-git/src -o $@ $< -c
libresidfp-Dac.o: \
	libresidfp-git/src/Dac.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-EnvelopeGenerator.o: \
	libresidfp-git/src/EnvelopeGenerator.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-ExternalFilter.o: \
	libresidfp-git/src/ExternalFilter.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-Filter.o: \
	libresidfp-git/src/Filter.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-Filter6581.o: \
	libresidfp-git/src/Filter6581.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-Filter8580.o: \
	libresidfp-git/src/Filter8580.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-FilterModelConfig.o: \
	libresidfp-git/src/FilterModelConfig.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-FilterModelConfig6581.o: \
	libresidfp-git/src/FilterModelConfig6581.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-FilterModelConfig8580.o: \
	libresidfp-git/src/FilterModelConfig8580.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-Integrator6581.o: \
	libresidfp-git/src/Integrator6581.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-Integrator8580.o: \
	libresidfp-git/src/Integrator8580.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-OpAmp.o: \
	libresidfp-git/src/OpAmp.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-residfp.o: \
	libresidfp-git/src/residfp/residfp.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -I./libresidfp-git/src -o $@ $< -c
libresidfp-SID.o: \
	libresidfp-git/src/SID.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -I./libresidfp-git/src -o $@ $< -c
libresidfp-Spline.o: \
	libresidfp-git/src/Spline.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-version.o: \
	libresidfp-git/src/version.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-WaveformCalculator.o: \
	libresidfp-git/src/WaveformCalculator.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-WaveformGenerator.o: \
	libresidfp-git/src/WaveformGenerator.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -o $@ $< -c
libresidfp-resample-SincResampler.o: \
	libresidfp-git/src/resample/SincResampler.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -ffast-math -fno-unsafe-math-optimizations -I./libresidfp-git/src -o $@ $< -c
libsidplayfp-c64-c64.o: \
	libsidplayfp-git/src/c64/c64.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CIA-interrupt.o: \
	libsidplayfp-git/src/c64/CIA/interrupt.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CIA-mos652x.o: \
	libsidplayfp-git/src/c64/CIA/mos652x.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CIA-SerialPort.o: \
	libsidplayfp-git/src/c64/CIA/SerialPort.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CIA-timer.o: \
	libsidplayfp-git/src/c64/CIA/timer.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CIA-tod.o: \
	libsidplayfp-git/src/c64/CIA/tod.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CPU-mos6510.o: \
	libsidplayfp-git/src/c64/CPU/mos6510.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CPU-mos6510debug.o: \
	libsidplayfp-git/src/c64/CPU/mos6510debug.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-mmu.o: \
	libsidplayfp-git/src/c64/mmu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-VIC_II-mos656x.o: \
	libsidplayfp-git/src/c64/VIC_II/mos656x.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-EventScheduler.o: \
	libsidplayfp-git/src/EventScheduler.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-player.o: \
	libsidplayfp-git/src/player.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-psiddrv.o: \
	libsidplayfp-git/src/psiddrv.cpp \
	libsidplayfp-git/src/psiddrv.bin
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-reloc65.o: \
	libsidplayfp-git/src/reloc65.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidemu.o: \
	libsidplayfp-git/src/sidemu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-sidbuilder.o: \
	libsidplayfp-git/src/sidplayfp/sidbuilder.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-SidConfig.o: \
	libsidplayfp-git/src/sidplayfp/SidConfig.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-SidInfo.o: \
	libsidplayfp-git/src/sidplayfp/SidInfo.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-SidTune.o: \
	libsidplayfp-git/src/sidplayfp/SidTune.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-SidTuneInfo.o: \
	libsidplayfp-git/src/sidplayfp/SidTuneInfo.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-SidTuneTools.o: \
	libsidplayfp-git/src/sidtune/SidTuneTools.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-p00.o: \
	libsidplayfp-git/src/sidtune/p00.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-prg.o: \
	libsidplayfp-git/src/sidtune/prg.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-PSID.o: \
	libsidplayfp-git/src/sidtune/PSID.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-git/src/sidtune/sidplayer1.bin: \
	libsidplayfp-git/src/sidtune/sidplayer1.a65
	$(XA) -R -G $< -o - | $(HEXDUMP) -v -e '1/1 "0x%02X,\n"' > $@
libsidplayfp-git/src/sidtune/sidplayer2.bin: \
	libsidplayfp-git/src/sidtune/sidplayer2.a65
	$(XA) -R -G $< -o - | $(HEXDUMP) -v -e '1/1 "0x%02X,\n"' > $@
libsidplayfp-git/src/psiddrv.bin: \
	libsidplayfp-git/src/psiddrv.a65
	$(XA) -R -G $< -o - | $(HEXDUMP) -v -e '1/1 "0x%02X,\n"' > $@
libsidplayfp-sidtune-MUS.o: \
	libsidplayfp-git/src/sidtune/MUS.cpp \
	libsidplayfp-git/src/sidtune/sidplayer1.bin \
	libsidplayfp-git/src/sidtune/sidplayer2.bin
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-SidTuneBase.o: \
	libsidplayfp-git/src/sidtune/SidTuneBase.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-simpleMixer.o: \
	libsidplayfp-git/src/simpleMixer.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c

libsidplayfp-api.o: \
	libsidplayfp-api.cpp \
	../config.h \
	../types.h \
	libsidplayfp-api.h \
	./libsidplayfp-git/src/sidplayfp/sidbuilder.h \
	./libsidplayfp-git/src/sidplayfp/SidInfo.h \
	libsidplayfp-git/src/sidplayfp/SidTuneInfo.h \
	./libsidplayfp-git/src/builders/residfp-builder/residfp.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I.. -o $@ $< -c

cpiinfo.o: \
	cpiinfo.cpp \
	cpiinfo.h \
	sidplay.h \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h
	$(CXX) $< -o $@ -c

cpisidsetup.o: \
	cpisidsetup.cpp \
	cpisidsetup.h \
	sidplay.h \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../stuff/compat.h \
	../stuff/imsrtns.h \
	../stuff/poutput.h
	$(CXX) $< -o $@ -c

sidconfig.o: sidconfig.c \
	md5.inc.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h \
	../filesel/filesystem-file-dev.h \
	../filesel/filesystem-setup.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	../stuff/utf-8.h \
	../stuff/poutput.h \
	sidconfig.h
	$(CC) $< -o $@ -c

sidptype.o: \
	sidptype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/err.h \
	../stuff/latin1.h \
	sidtype.h
	$(CC) $< -o $@ -c

sidplay.o: \
	sidplay.cpp \
	sidplayfp-config/config.h \
	../config.h \
	../types.h \
	../boot/psetting.h\
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/mixclip.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	../stuff/err.h \
	../stuff/imsrtns.h \
	sidplay.h \
	libsidplayfp-api.h
	$(LIBSIDPLAYCXX) -I.. $(LIBSIDPLAYCXXFLAGS) $< -o $@ -c

sidpplay.o: sidpplay.cpp \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h \
	./libsidplayfp-git/src/sidplayfp/SidTuneInfo.h \
	cpiinfo.h \
	cpisidsetup.h \
	sidplay.h \
	sidtype.h \
	sidconfig.h
	$(CXX) $(LIBSIDPLAYCXXFLAGS) $< -o $@ -c

dumpsid$(EXE_SUFFIX): dumpsid.o
	$(CC) $(LDFLAGS) -o $@ $^

dumpsid.o: \
	dumpsid.c \
	../config.h \
	../types.h \
	dumpsid_6502_dis.c
	$(CC) -o $@ $< -c
