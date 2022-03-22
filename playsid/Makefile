TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(BUILD_DUMPTOOLS),1)
	DUMPSID=dumpsid
else
	DUMPSID=
endif

TARGETS=sidconfig$(LIB_SUFFIX) sidtype$(LIB_SUFFIX) playsid$(LIB_SUFFIX)

LIBSIDPLAYCXXFLAGS+= -I./sidplayfp-config -I./libsidplayfp-git/src

LIBSIDPLAYOBJECTS=libsidplayfp-builders-resid-builder-resid-builder.o \
	libsidplayfp-builders-resid-builder-resid-dac.o \
	libsidplayfp-builders-resid-builder-resid-emu.o \
	libsidplayfp-builders-resid-builder-resid-extfilt.o \
	libsidplayfp-builders-resid-builder-resid-envelope.o \
	libsidplayfp-builders-resid-builder-resid-filter.o \
	libsidplayfp-builders-resid-builder-resid-pot.o \
	libsidplayfp-builders-resid-builder-resid-sid.o \
	libsidplayfp-builders-resid-builder-resid-version.o \
	libsidplayfp-builders-resid-builder-resid-voice.o \
	libsidplayfp-builders-resid-builder-resid-wave.o \
	libsidplayfp-builders-residfp-builder-residfp-builder.o \
	libsidplayfp-builders-residfp-builder-residfp-Dac.o \
	libsidplayfp-builders-residfp-builder-residfp-emu.o \
	libsidplayfp-builders-residfp-builder-residfp-EnvelopeGenerator.o \
	libsidplayfp-builders-residfp-builder-residfp-ExternalFilter.o \
	libsidplayfp-builders-residfp-builder-residfp-Filter.o \
	libsidplayfp-builders-residfp-builder-residfp-Filter6581.o \
	libsidplayfp-builders-residfp-builder-residfp-Filter8580.o \
	libsidplayfp-builders-residfp-builder-residfp-FilterModelConfig.o \
	libsidplayfp-builders-residfp-builder-residfp-FilterModelConfig6581.o \
	libsidplayfp-builders-residfp-builder-residfp-FilterModelConfig8580.o \
	libsidplayfp-builders-residfp-builder-residfp-OpAmp.o \
	libsidplayfp-builders-residfp-builder-residfp-SID.o \
	libsidplayfp-builders-residfp-builder-residfp-SincResampler.o \
	libsidplayfp-builders-residfp-builder-residfp-Spline.o \
	libsidplayfp-builders-residfp-builder-residfp-version.o \
	libsidplayfp-builders-residfp-builder-residfp-WaveformCalculator.o \
	libsidplayfp-builders-residfp-builder-residfp-WaveformGenerator.o \
	libsidplayfp-c64-c64.o \
	libsidplayfp-c64-CIA-interrupt.o \
	libsidplayfp-c64-CIA-mos652x.o \
	libsidplayfp-c64-CIA-SerialPort.o \
	libsidplayfp-c64-CIA-timer.o \
	libsidplayfp-c64-CPU-mos6510.o \
	libsidplayfp-c64-CIA-tod.o \
	libsidplayfp-c64-mmu.o \
	libsidplayfp-c64-VIC_II-mos656x.o \
	libsidplayfp-EventScheduler.o \
	libsidplayfp-mixer.o \
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
	libsidplayfp-utils-md5Factory.o \
	libsidplayfp-utils-MD5-MD5.o \
	libsidplayfp-api.o

all: $(TARGETS) $(DUMPSID)

clean:
	rm -f *.o *$(LIB_SUFFIX) libsidplayfp-git/src/sidtune/sidplayer1.bin sidplayfp-git/libsidplayfp/src/sidtune/sidplayer2.bin dumpsid \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581__ST.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581_P_T.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581_PS_.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581_PST.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580__ST.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580_P_T.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580_PS_.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580_PST.h

install:
	$(CP) playsid$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"
	$(CP) sidtype$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-playsid$(LIB_SUFFIX)"
	$(CP) sidconfig$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-sidconfig$(LIB_SUFFIX)"
ifeq ($(BUILD_DUMPTOOLS),1)
	$(CP) dumpsid "$(DESTDIR)$(BINDIR)/dumpsid"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/sidtype$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-playsid$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(LIBDIR)/autoload/30-timidityconfig$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(BINDIR)/dumpsid"

playsid_so=cpiinfo.o sidplay.o sidpplay.o $(LIBSIDPLAYOBJECTS)
playsid$(LIB_SUFFIX): $(playsid_so)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^

sidconfig$(LIB_SUFFIX): sidconfig.o
	$(CC) $(SHARED_FLAGS) -o $@ $^

sidtype_so=sidptype.o
sidtype$(LIB_SUFFIX): $(sidtype_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^

libsidplayfp-git/src/builders/resid-builder/resid/wave6581__ST.h: \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581__ST.dat
	$(PERL) libsidplayfp-git/src/builders/resid-builder/resid/samp2src.pl $* $< $@

libsidplayfp-git/src/builders/resid-builder/resid/wave6581_P_T.h: \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581_P_T.dat
	$(PERL) libsidplayfp-git/src/builders/resid-builder/resid/samp2src.pl $* $< $@

libsidplayfp-git/src/builders/resid-builder/resid/wave6581_PS_.h: \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581_PS_.dat
	$(PERL) libsidplayfp-git/src/builders/resid-builder/resid/samp2src.pl $* $< $@

libsidplayfp-git/src/builders/resid-builder/resid/wave6581_PST.h: \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581_PST.dat
	$(PERL) libsidplayfp-git/src/builders/resid-builder/resid/samp2src.pl $* $< $@

libsidplayfp-git/src/builders/resid-builder/resid/wave8580__ST.h: \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580__ST.dat
	$(PERL) libsidplayfp-git/src/builders/resid-builder/resid/samp2src.pl $* $< $@

libsidplayfp-git/src/builders/resid-builder/resid/wave8580_P_T.h: \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580_P_T.dat
	$(PERL) libsidplayfp-git/src/builders/resid-builder/resid/samp2src.pl $* $< $@

libsidplayfp-git/src/builders/resid-builder/resid/wave8580_PS_.h: \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580_PS_.dat
	$(PERL) libsidplayfp-git/src/builders/resid-builder/resid/samp2src.pl $* $< $@

libsidplayfp-git/src/builders/resid-builder/resid/wave8580_PST.h: \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580_PST.dat
	$(PERL) libsidplayfp-git/src/builders/resid-builder/resid/samp2src.pl $* $< $@

libsidplayfp-builders-resid-builder-resid-builder.o: \
	libsidplayfp-git/src/builders/resid-builder/resid-builder.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-dac.o: \
	libsidplayfp-git/src/builders/resid-builder/resid/dac.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-emu.o: \
	libsidplayfp-git/src/builders/resid-builder/resid-emu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-envelope.o: \
	libsidplayfp-git/src/builders/resid-builder/resid/envelope.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-extfilt.o: \
	libsidplayfp-git/src/builders/resid-builder/resid/extfilt.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-filter.o: \
	libsidplayfp-git/src/builders/resid-builder/resid/filter.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-pot.o: \
	libsidplayfp-git/src/builders/resid-builder/resid/pot.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-sid.o: \
	libsidplayfp-git/src/builders/resid-builder/resid/sid.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-version.o: \
	libsidplayfp-git/src/builders/resid-builder/resid/version.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-voice.o: \
	libsidplayfp-git/src/builders/resid-builder/resid/voice.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-wave.o: \
	libsidplayfp-git/src/builders/resid-builder/resid/wave.cc \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581__ST.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581_P_T.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581_PS_.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave6581_PST.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580__ST.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580_P_T.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580_PS_.h \
	libsidplayfp-git/src/builders/resid-builder/resid/wave8580_PST.h
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-builder.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp-builder.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libsidplayfp-git/src/builders/residfp-builder/residfp -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Dac.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/Dac.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-emu.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp-emu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libsidplayfp-git/src/builders/residfp-builder/residfp -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-EnvelopeGenerator.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/EnvelopeGenerator.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-ExternalFilter.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/ExternalFilter.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Filter.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/Filter.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Filter6581.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/Filter6581.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Filter8580.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/Filter8580.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-FilterModelConfig.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/FilterModelConfig.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-FilterModelConfig6581.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/FilterModelConfig6581.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-FilterModelConfig8580.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/FilterModelConfig8580.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-OpAmp.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/OpAmp.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-SID.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/SID.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libsidplayfp-git/src/builders/residfp-builder/residfp -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-SincResampler.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/resample/SincResampler.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./libsidplayfp-git/src/builders/residfp-builder/residfp -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Spline.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/Spline.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-version.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/version.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-WaveformCalculator.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/WaveformCalculator.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-WaveformGenerator.o: \
	libsidplayfp-git/src/builders/residfp-builder/residfp/WaveformGenerator.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
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
libsidplayfp-c64-mmu.o: \
	libsidplayfp-git/src/c64/mmu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-VIC_II-mos656x.o: \
	libsidplayfp-git/src/c64/VIC_II/mos656x.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-EventScheduler.o: \
	libsidplayfp-git/src/EventScheduler.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-mixer.o: \
	libsidplayfp-git/src/mixer.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-player.o: \
	libsidplayfp-git/src/player.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-psiddrv.o: \
	libsidplayfp-git/src/psiddrv.cpp
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
libsidplayfp-sidtune-MUS.o: \
	libsidplayfp-git/src/sidtune/MUS.cpp \
	libsidplayfp-git/src/sidtune/sidplayer1.bin \
	libsidplayfp-git/src/sidtune/sidplayer2.bin
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-SidTuneBase.o: \
	libsidplayfp-git/src/sidtune/SidTuneBase.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-utils-md5Factory.o: \
	libsidplayfp-git/src/utils/md5Factory.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-utils-MD5-MD5.o: \
	libsidplayfp-git/src/utils/MD5/MD5.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c

libsidplayfp-api.o: \
	libsidplayfp-api.cpp \
	libsidplayfp-api.h \
	./libsidplayfp-git/src/sidplayfp/sidbuilder.h \
	./libsidplayfp-git/src/sidplayfp/SidInfo.h \
	libsidplayfp-git/src/sidplayfp/SidTuneInfo.h \
	./libsidplayfp-git/src/builders/residfp-builder/residfp.h \
	./libsidplayfp-git/src/builders/resid-builder/resid.h \
	../boot/psetting.h \
	../filesel/dirdb.h \
	../filesel/filesystem-unix.h
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

sidconfig.o: sidconfig.c \
	md5.inc.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h \
	../filesel/filesystem-file-mem.h \
	../filesel/filesystem-setup.h \
	../filesel/filesystem-unix.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/framelock.h \
	../stuff/imsrtns.h \
	../stuff/utf-8.h \
	../stuff/poutput.h
	$(CC) $< -o $@ -c

sidptype.o: \
	sidptype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/latin1.h
	$(CC) $< -o $@ -c

sidplay.o: \
	sidplay.cpp \
	sidplayfp-config/config.h \
	../config.h \
	../types.h \
	../boot/psetting.h\
	../cpiface/cpiface.h \
	../dev/deviplay.h \
	../dev/mcp.h \
	../dev/mixclip.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/ringbuffer.h \
	../filesel/filesystem.h \
	../stuff/imsrtns.h \
	../stuff/poll.h \
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
	../stuff/poutput.h \
	../stuff/sets.h \
	../stuff/timer.h \
	./libsidplayfp-git/src/sidplayfp/SidTuneInfo.h \
	cpiinfo.h \
	sidplay.h
	$(CXX) $(LIBSIDPLAYCXXFLAGS) $< -o $@ -c

dumpsid: dumpsid.o
	$(CC) $(LDFLAGS) -o $@ $^

dumpsid.o: \
	dumpsid.c \
	../config.h \
	../types.h \
	dumpsid_6502_dis.c
	$(CC) -o $@ $< -c
