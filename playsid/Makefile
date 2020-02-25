TOPDIR=../
include $(TOPDIR)Rules.make

ifeq ($(BUILD_DUMPTOOLS),1)
	DUMPSID=dumpsid
else
	DUMPSID=
endif

TARGETS=sidtype$(LIB_SUFFIX) playsid$(LIB_SUFFIX)

LIBSIDPLAYCXXFLAGS+= -I./sidplayfp-config -I./sidplayfp-git/libsidplayfp/src

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
	libsidplayfp-builders-residfp-builder-residfp-FilterModelConfig8580.o \
	libsidplayfp-builders-residfp-builder-residfp-OpAmp.o \
	libsidplayfp-builders-residfp-builder-residfp-SID.o \
	libsidplayfp-builders-residfp-builder-residfp-SincResampler.o \
	libsidplayfp-builders-residfp-builder-residfp-Spline.o \
	libsidplayfp-builders-residfp-builder-residfp-version.o \
	libsidplayfp-builders-residfp-builder-residfp-WaveformCalculator.o \
	libsidplayfp-builders-residfp-builder-residfp-WaveformGenerator.o \
	libsidplayfp-c64-c64.o \
	libsidplayfp-c64-CIA-mos6526.o \
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
	rm -f *.o *$(LIB_SUFFIX) dumpsid

install:
	$(CP) playsid$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)"
	$(CP) sidtype$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIR)/autoload/30-playsid$(LIB_SUFFIX)"
ifeq ($(BUILD_DUMPTOOLS),1)
	$(CP) dumpsid "$(DESTDIR)$(BINDIR)/dumpsid"
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/sidtype$(LIB_SUFFIX)" "$(DESTDIR)$(LIBDIR)/autoload/30-playsid$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(BINDIR)/dumpsid"

playsid_so=cpiinfo.o sidplay.o sidpplay.o $(LIBSIDPLAYOBJECTS)
playsid$(LIB_SUFFIX): $(playsid_so)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^

sidtype_so=sidptype.o
sidtype$(LIB_SUFFIX): $(sidtype_so)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^

libsidplayfp-builders-resid-builder-resid-builder.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid-builder.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-dac.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid/dac.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-emu.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid-emu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-envelope.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid/envelope.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-extfilt.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid/extfilt.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-filter.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid/filter.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-pot.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid/pot.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-sid.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid/sid.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-version.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid/version.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-voice.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid/voice.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-resid-builder-resid-wave.o: \
	sidplayfp-git/libsidplayfp/src/builders/resid-builder/resid/wave.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-builder.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp-builder.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Dac.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/Dac.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-emu.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp-emu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-EnvelopeGenerator.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/EnvelopeGenerator.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-ExternalFilter.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/ExternalFilter.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Filter.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/Filter.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Filter6581.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/Filter6581.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Filter8580.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/Filter8580.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-FilterModelConfig.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/FilterModelConfig.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-FilterModelConfig8580.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/FilterModelConfig8580.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-OpAmp.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/OpAmp.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-SID.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/SID.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-SincResampler.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/resample/SincResampler.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -I./sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-Spline.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/Spline.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-version.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/version.cc
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-WaveformCalculator.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/WaveformCalculator.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-builders-residfp-builder-residfp-WaveformGenerator.o: \
	sidplayfp-git/libsidplayfp/src/builders/residfp-builder/residfp/WaveformGenerator.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-c64.o: \
	sidplayfp-git/libsidplayfp/src/c64/c64.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CIA-mos6526.o: \
	sidplayfp-git/libsidplayfp/src/c64/CIA/mos6526.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CIA-SerialPort.o: \
	sidplayfp-git/libsidplayfp/src/c64/CIA/SerialPort.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CIA-timer.o: \
	sidplayfp-git/libsidplayfp/src/c64/CIA/timer.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CIA-tod.o: \
	sidplayfp-git/libsidplayfp/src/c64/CIA/tod.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-CPU-mos6510.o: \
	sidplayfp-git/libsidplayfp/src/c64/CPU/mos6510.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-mmu.o: \
	sidplayfp-git/libsidplayfp/src/c64/mmu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-c64-VIC_II-mos656x.o: \
	sidplayfp-git/libsidplayfp/src/c64/VIC_II/mos656x.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-EventScheduler.o: \
	sidplayfp-git/libsidplayfp/src/EventScheduler.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-mixer.o: \
	sidplayfp-git/libsidplayfp/src/mixer.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-player.o: \
	sidplayfp-git/libsidplayfp/src/player.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-psiddrv.o: \
	sidplayfp-git/libsidplayfp/src/psiddrv.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-reloc65.o: \
	sidplayfp-git/libsidplayfp/src/reloc65.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidemu.o: \
	sidplayfp-git/libsidplayfp/src/sidemu.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-sidbuilder.o: \
	sidplayfp-git/libsidplayfp/src/sidplayfp/sidbuilder.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-SidConfig.o: \
	sidplayfp-git/libsidplayfp/src/sidplayfp/SidConfig.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-SidInfo.o: \
	sidplayfp-git/libsidplayfp/src/sidplayfp/SidInfo.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-SidTune.o: \
	sidplayfp-git/libsidplayfp/src/sidplayfp/SidTune.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidplayfp-SidTuneInfo.o: \
	sidplayfp-git/libsidplayfp/src/sidplayfp/SidTuneInfo.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-SidTuneTools.o: \
	sidplayfp-git/libsidplayfp/src/sidtune/SidTuneTools.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-p00.o: \
	sidplayfp-git/libsidplayfp/src/sidtune/p00.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-prg.o: \
	sidplayfp-git/libsidplayfp/src/sidtune/prg.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-PSID.o: \
	sidplayfp-git/libsidplayfp/src/sidtune/PSID.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
sidplayfp-git/libsidplayfp/src/sidtune/sidplayer1.bin: \
	sidplayfp-git/libsidplayfp/src/sidtune/sidplayer1.a65
	o65file=`echo $@ | sed 's/bin/o65/'`; \
	xa -R -G $< -o $o65file && \
	od -v -An -w8 -tx1 $o65file | sed -re 's/[[:alnum:]]+/0x&,/g' > $@ && \
	rm -f $o65file
sidplayfp-git/libsidplayfp/src/sidtune/sidplayer2.bin: \
	sidplayfp-git/libsidplayfp/src/sidtune/sidplayer2.a65
	o65file=`echo $@ | sed 's/bin/o65/'`; \
	xa -R -G $< -o $o65file && \
	od -v -An -w8 -tx1 $o65file | sed -re 's/[[:alnum:]]+/0x&,/g' > $@ && \
	rm -f $o65file
libsidplayfp-sidtune-MUS.o: \
	sidplayfp-git/libsidplayfp/src/sidtune/MUS.cpp \
	sidplayfp-git/libsidplayfp/src/sidtune/sidplayer1.bin \
	sidplayfp-git/libsidplayfp/src/sidtune/sidplayer2.bin
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-sidtune-SidTuneBase.o: \
	sidplayfp-git/libsidplayfp/src/sidtune/SidTuneBase.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-utils-md5Factory.o: \
	sidplayfp-git/libsidplayfp/src/utils/md5Factory.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c
libsidplayfp-utils-MD5-MD5.o: \
	sidplayfp-git/libsidplayfp/src/utils/MD5/MD5.cpp
	$(LIBSIDPLAYCXX) $(LIBSIDPLAYCXXFLAGS) -o $@ $< -c

libsidplayfp-api.o: \
	libsidplayfp-api.cpp \
	libsidplayfp-api.h \
	../boot/psetting.h \
	../filesel/gendir.h
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

sidptype.o: \
	sidptype.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../filesel/mdb.h \
	../stuff/latin1.h
	$(CC) $< -o $@ -c

sidplay.o: \
	sidplay.cpp \
	../config.h \
	../boot/psetting.h\
	../cpiface/cpiface.h \
	../dev/player.h \
	../dev/plrasm.h \
	../dev/deviplay.h \
	../dev/mixclip.h \
	../dev/ringbuffer.h \
	../stuff/imsrtns.h \
	../stuff/poll.h \
	../types.h \
	libsidplayfp-api.h \
	sidplay.h \
	sidplayfp-config/config.h	
	$(LIBSIDPLAYCXX) -I.. $(LIBSIDPLAYCXXFLAGS) $< -o $@ -c

sidpplay.o: sidpplay.cpp \
	../config.h \
	../types.h \
	../filesel/mdb.h \
	../stuff/poutput.h \
	../dev/player.h \
	../boot/psetting.h \
	../boot/plinkman.h \
	../stuff/compat.h \
	../stuff/sets.h \
	../dev/deviplay.h \
	../cpiface/cpiface.h \
	../stuff/compat.h \
	../stuff/timer.h \
	cpiinfo.h \
	sidplay.h
	$(CXX) $< -o $@ -c

dumpsid: dumpsid.o
	$(CC) $(LDFLAGS) -o $@ $^

dumpsid.o: \
	dumpsid.c \
	../config.h \
	../types.h \
	dumpsid_6502_dis.c
	$(CC) -o $@ $< -c
