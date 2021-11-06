/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Export _dllinfo for FSTYPES.DLL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * revision history: (please note changes here)
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 */

#include "config.h"
#include "types.h"
#include "boot/plinkman.h"
#include "pfilesel.h"
#include "mdb.h"

extern struct mdbreadinforegstruct
#ifdef HAVE_MAD
	ampegpReadInfoReg,
#endif
	itpReadInfoReg,
	oggReadInfoReg,
	gmdReadInfoReg,
	hvlReadInfoReg,
	xmpReadInfoReg,
	gmiReadInfoReg,
	wavReadInfoReg;

extern struct interfaceparameters AMS_p,
                                  DMF_p,
                                  HVL_p,
                                  IT_p,
                                  MDL_p,
                                  MIDI_p,
                                  MOD_p,
                                  MPx_p,
                                  MTM_p,
                                  OGG_p,
                                  OKT_p,
                                  PTM_p,
                                  S3M_p,
                                  STM_p,
                                  ULT_p,
                                  WAV_p,
                                 _669_p;

extern const char **AMS_description,
                  **DMF_description,
                  **HVL_description,
                  **IT_description,
                  **M15_description,
                  **M15t_description,
                  **M31_description,
                  **MDL_description,
                  **MIDI_description,
                  **MOD_description,
                  **MODd_description,
                  **MODf_description,
                  **MODt_description,
                  **MPx_description,
                  **MTM_description,
                  **MXM_description,
                  **OGG_description,
                  **OKT_description,
                  **PTM_description,
                  **S3M_description,
                  **STM_description,
                  **ULT_description,
                  **WAV_description,
                  **WOW_description,
                  **XM_description,
                 **_669_description;

static void __attribute__((constructor))init(void)
{
	struct moduletype mt;

#ifdef HAVE_MAD
	mdbRegisterReadInfo(&ampegpReadInfoReg);
#endif
	mdbRegisterReadInfo(&itpReadInfoReg);
	mdbRegisterReadInfo(&oggReadInfoReg);
	mdbRegisterReadInfo(&gmdReadInfoReg);
	mdbRegisterReadInfo(&hvlReadInfoReg);
	mdbRegisterReadInfo(&xmpReadInfoReg);
	mdbRegisterReadInfo(&gmiReadInfoReg);
	mdbRegisterReadInfo(&wavReadInfoReg);

	fsRegisterExt ("AMS");
	mt.integer.i = MODULETYPE("AMS");
	fsTypeRegister (mt, AMS_description, "plOpenCP", &AMS_p);

	fsRegisterExt ("DMF");
	mt.integer.i = MODULETYPE("DMF");
	fsTypeRegister (mt, DMF_description, "plOpenCP", &DMF_p);

	fsRegisterExt ("HVL");
	fsRegisterExt ("AHX");
	mt.integer.i = MODULETYPE("HVL");
	fsTypeRegister (mt, HVL_description, "plOpenCP", &HVL_p);

	fsRegisterExt ("IT");
	mt.integer.i = MODULETYPE("IT");
	fsTypeRegister (mt, IT_description, "plOpenCP", &IT_p);

	fsRegisterExt ("MDL");
	mt.integer.i = MODULETYPE("MDL");
	fsTypeRegister (mt, MDL_description, "plOpenCP", &MDL_p);

	fsRegisterExt ("MID");
	fsRegisterExt ("MIDI");
	fsRegisterExt ("RMI");
	mt.integer.i = MODULETYPE("MIDI");
	fsTypeRegister (mt, MIDI_description, "plOpenCP", &MIDI_p);

	fsRegisterExt ("NST");
	fsRegisterExt ("MOD");
	fsRegisterExt ("MXM");
	fsRegisterExt ("XM");
	mt.integer.i = MODULETYPE("M15");
	fsTypeRegister (mt, M15_description, "plOpenCP", &MOD_p);
	mt.integer.i = MODULETYPE("M15t");
	fsTypeRegister (mt, M15t_description, "plOpenCP", &MOD_p);
	mt.integer.i = MODULETYPE("M31");
	fsTypeRegister (mt, M31_description, "plOpenCP", &MOD_p);
	mt.integer.i = MODULETYPE("MOD");
	fsTypeRegister (mt, MOD_description, "plOpenCP", &MOD_p);
	mt.integer.i = MODULETYPE("MODd");
	fsTypeRegister (mt, MODd_description, "plOpenCP", &MOD_p);
	mt.integer.i = MODULETYPE("MODf");
	fsTypeRegister (mt, MODf_description, "plOpenCP", &MOD_p);
	mt.integer.i = MODULETYPE("MODt");
	fsTypeRegister (mt, MODt_description, "plOpenCP", &MOD_p);
	mt.integer.i = MODULETYPE("MXM");
	fsTypeRegister (mt, MXM_description, "plOpenCP", &MOD_p);
	mt.integer.i = MODULETYPE("XM");
	fsTypeRegister (mt, XM_description, "plOpenCP", &MOD_p);

	fsRegisterExt ("MP1");
	fsRegisterExt ("MP2");
	fsRegisterExt ("MP3");
	mt.integer.i = MODULETYPE("MPx");
	fsTypeRegister (mt, MPx_description, "plOpenCP", &MPx_p);

	fsRegisterExt ("MTM");
	mt.integer.i = MODULETYPE("MTM");
	fsTypeRegister (mt, MTM_description, "plOpenCP", &MTM_p);

	fsRegisterExt ("OGA");
	fsRegisterExt ("OGG");
	mt.integer.i = MODULETYPE("OGG");
	fsTypeRegister (mt, OGG_description, "plOpenCP", &OGG_p);

	fsRegisterExt ("OKT");
	fsRegisterExt ("OKTA");
	mt.integer.i = MODULETYPE("OKT");
	fsTypeRegister (mt, OKT_description, "plOpenCP", &OKT_p);

	fsRegisterExt ("PTM");
	mt.integer.i = MODULETYPE("PTM");
	fsTypeRegister (mt, PTM_description, "plOpenCP", &PTM_p);

	fsRegisterExt ("S3M");
	mt.integer.i = MODULETYPE("S3M");
	fsTypeRegister (mt, S3M_description, "plOpenCP", &S3M_p);

	fsRegisterExt ("STM");
	mt.integer.i = MODULETYPE("STM");
	fsTypeRegister (mt, STM_description, "plOpenCP", &STM_p);

	fsRegisterExt ("ULT");
	mt.integer.i = MODULETYPE("ULT");
	fsTypeRegister (mt, ULT_description, "plOpenCP", &ULT_p);

	fsRegisterExt ("WAV");
	fsRegisterExt ("WAVE");
	mt.integer.i = MODULETYPE("WAV");
	fsTypeRegister (mt, WAV_description, "plOpenCP", &WAV_p);

	fsRegisterExt ("WOW");
	mt.integer.i = MODULETYPE("WOW");
	fsTypeRegister (mt, WOW_description, "plOpenCP", &MOD_p);

	fsRegisterExt ("669");
	mt.integer.i = MODULETYPE("669");
	fsTypeRegister (mt, _669_description, "plOpenCP", &_669_p);
}

static void __attribute__((destructor))done(void)
{
#ifdef HAVE_MAD
	mdbUnregisterReadInfo(&ampegpReadInfoReg);
#endif
	mdbUnregisterReadInfo(&itpReadInfoReg);
	mdbUnregisterReadInfo(&oggReadInfoReg);
	mdbUnregisterReadInfo(&gmdReadInfoReg);
	mdbUnregisterReadInfo(&hvlReadInfoReg);
	mdbUnregisterReadInfo(&xmpReadInfoReg);
	mdbUnregisterReadInfo(&gmiReadInfoReg);
	mdbUnregisterReadInfo(&wavReadInfoReg);
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "fstypes", .desc = "OpenCP Module Detection (c) 1994-2021 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
