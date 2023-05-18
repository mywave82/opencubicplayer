TOPDIR=../
include $(TOPDIR)Rules.make

HAVE_ADPLUG=1
ADPLUG_CXXFLAGS+=-Ilibbinio-git/src -Iadplug-git/src
ADPLUG_LIBS= #$(LIBBINIO_TARGETS) $(LIBADPLUG_TARGETS)

ifeq ($(HAVE_ADPLUG),1)
TARGETS=playopl$(LIB_SUFFIX)
endif

LIBADPLUG_CXXFLAGS=$(ADPLUG_CXXFLAGS)-DVERSION=\"2.3.4-beta\" -Dstricmp=strcasecmp -Ilibbinio-git/src $(ADPLUG_CXX_FLAGS)
ifeq (HAVE_GETOPT_H,1)
LIBADPLUG_CXXFLAGS+=" -DHAVE_GETOPT_H=1"
endif
ifeq (HAVE_SYS_IO_H,1)
LIBADPLUG_CXXFLAGS+=" -DHAVE_SYS_IO_H=1"
endif
ifeq (HAVE_SYS_STAT_H,1)
LIBADPLUG_CXXFLAGS+=" -DHAVE_SYS_STAT_H=1"
endif
ifeq (HAVE_SYS_TYPES_H,1)
LIBADPLUG_CXXFLAGS+=" -DHAVE_SYS_TYPES_H=1"
endif
LIBADPLUG_TARGETS=\
	adplug_a2m.o      adplug_adl.o      adplug_adlibemu.o adplug_adplug.o   adplug_adtrack.o     adplug_amd.o         adplug_analopl.o     adplug_bam.o      adplug_bmf.o      \
	adplug_cff.o      adplug_cmf.o      adplug_coktel.o   adplug_composer.o adplug_cmfmcsop.o    adplug_d00.o         adplug_database.o    adplug_dfm.o      adplug_diskopl.o  \
	adplug_dmo.o      adplug_dro2.o     adplug_dro.o      adplug_dtm.o      adplug_emuopl.o      adplug_flash.o       adplug_fmc.o         adplug_fprovide.o adplug_got.o      \
	adplug_herad.o    adplug_hsc.o      adplug_hsp.o      adplug_hybrid.o   adplug_hyp.o         adplug_imf.o         adplug_jbm.o         adplug_ksm.o      adplug_lds.o      \
	adplug_mad.o      adplug_mdi.o      adplug_mid.o      adplug_mkj.o      adplug_msc.o         adplug_mtk.o         adplug_mtr.o         adplug_mus.o      adplug_nemuopl.o  \
	adplug_pis.o      adplug_player.o   adplug_players.o  adplug_protrack.o adplug_psi.o         adplug_rad2.o        adplug_rat.o         adplug_raw.o      adplug_realopl.o  \
	adplug_rix.o      adplug_rol.o      adplug_s3m.o      adplug_sa2.o      adplug_sng.o         adplug_sop.o         adplug_surroundopl.o adplug_temuopl.o  adplug_u6m.o      \
	adplug_vgm.o      adplug_woodyopl.o adplug_xad.o      adplug_xsm.o      adplug_debug.o       adplug_fmopl.o       adplug_nukedopl.o
#adplugdb_adplugdb.o

LIBBINIO_CXXFLAGS=$(ADPLUG_CXXFLAGS)
LIBBINIO_TARGETS=libbinio_binfile.o libbinio_binio.o libbinio_binstr.o libbinio_binwrap.o

all: $(TARGETS)

clean:
	rm -f *.o *$(LIB_SUFFIX)

install:
ifeq ($(HAVE_ADPLUG),1)
	$(CP) playopl$(LIB_SUFFIX) "$(DESTDIR)$(LIBDIROCP)/autoload/95-playopl$(LIB_SUFFIX)"
ifneq ($(WINDOWS),1)
	mkdir -p "$(DESTDIR)$(DATADIROCP)/data"
	$(CP) adplugdb-git/adplug.db "$(DESTDIR)$(DATADIROCP)/data"
else
	mkdir -p "$(DESTDIR)$(DATADIROCP)"
	$(CP) adplugdb-git/adplug.db "$(DESTDIR)$(DATADIROCP)"
endif
endif

uninstall:
	rm -f "$(DESTDIR)$(LIBDIROCP)/autoload/95-playopl$(LIB_SUFFIX)"
	rm -f "$(DESTDIR)$(DATADIROCP)/data/adplug.db"

oplconfig.o: oplconfig.c \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../filesel/filesystem.h \
	../filesel/filesystem-drive.h \
	../filesel/filesystem-file-dev.h \
	../filesel/filesystem-setup.h \
	../filesel/pfilesel.h \
	../playopl/oplconfig.h \
	../stuff/err.h \
	../stuff/poutput.h
	$(CC) $(CFLAGS) $< -o $@ -c

oplKen.o: oplKen.cpp \
	../config.h \
	../types.h \
	oplKen.h \
	adplug-git/src/opl.h \
	adplug-git/src/adlibemu.h \
	ocpemu.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) $< -o $@ -c

oplNuked.o: oplNuked.cpp \
	oplNuked.h \
	adplug-git/src/opl.h \
	adplug-git/src/fmopl.h \
	ocpemu.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) $< -o $@ -c

oplSatoh.o: oplSatoh.cpp \
	oplSatoh.h \
	adplug-git/src/opl.h \
	adplug-git/src/nukedopl.h \
	ocpemu.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) $< -o $@ -c

oplWoody.o: oplWoody.cpp \
	oplWoody.h \
	adplug-git/src/opl.h \
	adplug-git/src/fmopl.h \
	ocpemu.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) $< -o $@ -c

opltype.o: opltype.cpp \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/mdb.h \
	../filesel/pfilesel.h \
	../stuff/compat.h \
	../stuff/err.h \
	opltype.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) opltype.cpp -o $@ -c

playopl_so=oplconfig.o oplchan.o oplpplay.o oplplay.o oplptrak.o ocpemu.o oplKen.o oplNuked.o oplSatoh.o oplWoody.o opltype.o $(LIBBINIO_TARGETS) $(LIBADPLUG_TARGETS)
playopl$(LIB_SUFFIX): $(playopl_so)
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^ $(ADPLUG_LIBS) $(MATH_LIBS)

oplchan.o: oplchan.cpp \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../playopl/ocpemu.h \
	../playopl/oplplay.h \
	../stuff/poutput.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) $< -o $@ -c


oplpplay.o: oplpplay.cpp \
	../config.h \
	../types.h \
	../boot/plinkman.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/player.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../filesel/pfilesel.h \
	../filesel/mdb.h \
	../stuff/compat.h \
	../stuff/err.h \
	../stuff/poutput.h \
	../stuff/sets.h \
	../playopl/oplconfig.h \
	../playopl/oplplay.h \
	../playopl/opltype.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) oplpplay.cpp -o $@ -c

oplplay.o: oplplay.cpp \
	../config.h \
	../types.h \
	../boot/psetting.h \
	../cpiface/cpiface.h \
	../dev/mcp.h \
	../dev/player.h \
	../dev/ringbuffer.h \
	../filesel/dirdb.h \
	../filesel/filesystem.h \
	../stuff/imsrtns.h \
	oplplay.h \
	ocpemu.h \
	oplKen.h \
	oplNuked.h \
	oplSatoh.h \
	oplWoody.h \
	oplptrak.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) oplplay.cpp -o $@ -c

oplptrak.o: oplptrak.cpp \
	../config.h \
	../types.h \
	../cpiface/cpiface.h \
	../stuff/poutput.h \
	oplplay.h ocpemu.h oplptrak.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) oplptrak.cpp -o $@ -c

ocpemu.o: ocpemu.cpp \
	../config.h \
	../types.h \
	ocpemu.h
	$(ADPLUG_CXX) $(ADPLUG_CXXFLAGS) ocpemu.cpp -o $@ -c

adplugdb_adplugdb.o: \
	adplug-git/adplugdb/adplugdb.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_a2m.o: \
	adplug-git/src/a2m.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_adl.o: \
	adplug-git/src/adl.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_adlibemu.o: \
	adplug-git/src/adlibemu.c
	$(CC) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_adplug.o: \
	adplug-git/src/adplug.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_adtrack.o: \
	adplug-git/src/adtrack.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_amd.o: \
	adplug-git/src/amd.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_analopl.o: \
	adplug-git/src/analopl.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_bam.o: \
	adplug-git/src/bam.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_bmf.o: \
	adplug-git/src/bmf.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_cff.o: \
	adplug-git/src/cff.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_cmf.o: \
	adplug-git/src/cmf.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_cmfmcsop.o: \
	adplug-git/src/cmfmcsop.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_coktel.o: \
	adplug-git/src/coktel.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_composer.o: \
	adplug-git/src/composer.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_d00.o : \
	adplug-git/src/d00.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_database.o : \
	adplug-git/src/database.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_debug.o: \
	adplug-git/src/debug.c
	$(CC) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_dfm.o: \
	adplug-git/src/dfm.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_diskopl.o: \
	adplug-git/src/diskopl.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_dmo.o: \
	adplug-git/src/dmo.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_dro2.o: \
	adplug-git/src/dro2.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_dro.o: \
	adplug-git/src/dro.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_dtm.o: \
	adplug-git/src/dtm.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_emuopl.o: \
	adplug-git/src/emuopl.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_flash.o: \
	adplug-git/src/flash.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_fmc.o: \
	adplug-git/src/fmc.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_fmopl.o: \
	adplug-git/src/fmopl.c
	$(CC) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_fprovide.o: \
	adplug-git/src/fprovide.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_got.o: \
	adplug-git/src/got.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_herad.o: \
	adplug-git/src/herad.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_hsc.o: \
	adplug-git/src/hsc.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_hsp.o: \
	adplug-git/src/hsp.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_hybrid.o: \
	adplug-git/src/hybrid.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_hyp.o: \
	adplug-git/src/hyp.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_imf.o: \
	adplug-git/src/imf.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_jbm.o: \
	adplug-git/src/jbm.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_ksm.o: \
	adplug-git/src/ksm.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_lds.o: \
	adplug-git/src/lds.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_mad.o: \
	adplug-git/src/mad.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_mdi.o: \
	adplug-git/src/mdi.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_mid.o: \
	adplug-git/src/mid.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_mkj.o: \
	adplug-git/src/mkj.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_msc.o: \
	adplug-git/src/msc.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_mtk.o: \
	adplug-git/src/mtk.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_mtr.o: \
	adplug-git/src/mtr.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_mus.o: \
	adplug-git/src/mus.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_nemuopl.o: \
	adplug-git/src/nemuopl.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_nukedopl.o: \
	adplug-git/src/nukedopl.c
	$(CC) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_pis.o: \
	adplug-git/src/pis.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_player.o: \
	adplug-git/src/player.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_players.o: \
	adplug-git/src/players.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_protrack.o: \
	adplug-git/src/protrack.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_psi.o: \
	adplug-git/src/psi.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_rad2.o: \
	adplug-git/src/rad2.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_rat.o: \
	adplug-git/src/rat.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_raw.o: \
	adplug-git/src/raw.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_realopl.o: \
	adplug-git/src/realopl.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_rix.o: \
	adplug-git/src/rix.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_rol.o: \
	adplug-git/src/rol.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_s3m.o: \
	adplug-git/src/s3m.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_sa2.o: \
	adplug-git/src/sa2.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_sng.o: \
	adplug-git/src/sng.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_sop.o: \
	adplug-git/src/sop.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_surroundopl.o: \
	adplug-git/src/surroundopl.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_temuopl.o: \
	adplug-git/src/temuopl.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_u6m.o: \
	adplug-git/src/u6m.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_vgm.o: \
	adplug-git/src/vgm.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_woodyopl.o: \
	adplug-git/src/woodyopl.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_xad.o: \
	adplug-git/src/xad.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

adplug_xsm.o: \
	adplug-git/src/xsm.cpp
	$(ADPLUG_CXX) $(LIBADPLUG_CXXFLAGS) $^ -o $@ -c

libbinio_binfile.o: \
	libbinio-git/src/binfile.cpp
	$(ADPLUG_CXX) $(LIBBINIO_CXXFLAGS) $^ -o $@ -c

libbinio_binio.o: \
	libbinio-git/src/binio.cpp
	$(ADPLUG_CXX) $(LIBBINIO_CXXFLAGS) $^ -o $@ -c

libbinio_binstr.o: \
	libbinio-git/src/binstr.cpp
	$(ADPLUG_CXX) $(LIBBINIO_CXXFLAGS) $^ -o $@ -c

libbinio_binwrap.o: \
	libbinio-git/src/binwrap.cpp
	$(ADPLUG_CXX) $(LIBBINIO_CXXFLAGS) $^ -o $@ -c
