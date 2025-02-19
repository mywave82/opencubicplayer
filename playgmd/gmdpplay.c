/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay interface routines
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -ss040709   Stian Skjelstad <stian@nixia.no>
 *    -use compatible timing, and now cputime/clock()
 */

#include "config.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "gmdpchan.h"
#include "gmdpdots.h"
#include "gmdplay.h"
#include "gmdptrak.h"
#include "gmdtype.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static int gmdActive;

OCP_INTERNAL struct gmdmodule mod;
static char patlock;

static void gmdMarkInsSamp (struct cpifaceSessionAPI_t *cpifaceSession, uint8_t *ins, uint8_t *samp)
{
	int i;
	/* mod.channum == cpifaceSession->LogicalChannelCount */
	for (i=0; i<mod.channum; i++)
	{
		struct chaninfo ci;
		mpGetChanInfo(i, &ci);

		if (!cpifaceSession->MuteChannel[i] && mpGetChanStatus (cpifaceSession, i) && ci.vol)
		{
			ins[ci.ins]=((cpifaceSession->SelectedChannel==i)||(ins[ci.ins]==3))?3:2;
			samp[ci.smp]=((cpifaceSession->SelectedChannel==i)||(samp[ci.smp]==3))?3:2;
		}
	}
}

static void gmdDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct globinfo gi;

	mpGetGlobInfo (&gi);

	cpifaceSession->drawHelperAPI->GStringsTracked
	(
		cpifaceSession,
		0,          /* song X */
		0,          /* song Y */
		gi.currow,  /* row X */
		gi.patlen-1,/* row Y */
		gi.curpat,  /* order X */
		gi.patnum-1,/* order Y */
		gi.tempo,   /* speed - do not ask.. */
		gi.speed,   /* tempo - do not ask.. */
		gi.globvol, /* gvol */
		(gi.globvolslide==fxGVSUp)?1:(gi.globvolslide==fxGVSDown)?-1:0,
		0,          /* chan X */
		0           /* chan Y */
	);
}

static int gmdProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	uint16_t pat;
	uint8_t row;
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp (KEY_ALT_L, "Pattern lock toggle");
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_UP, "Jump back (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_DOWN, "Jump forward (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('<', "Jump back (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Jump back (big)");
			cpifaceSession->KeyHelp ('>', "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Jump start of track");
			return 0;
		case 'p': case 'P':
			cpifaceSession->TogglePauseFade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			cpifaceSession->TogglePause (cpifaceSession);
			break;
		case KEY_CTRL_HOME:
			gmdInstClear (cpifaceSession);
			mpSetPosition (cpifaceSession, 0, 0);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			mpGetPosition(&pat, &row);
			mpSetPosition (cpifaceSession, pat-1, 0);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			mpGetPosition(&pat, &row);
			mpSetPosition (cpifaceSession, pat+1, 0);
			break;
		case KEY_CTRL_UP:
			mpGetPosition(&pat, &row);
			mpSetPosition (cpifaceSession, pat, row-8);
			break;
		case KEY_CTRL_DOWN:
			mpGetPosition(&pat, &row);
			mpSetPosition (cpifaceSession, pat, row+8);
			break;
		case KEY_ALT_L:
			patlock=!patlock;
			mpLockPat(patlock);
			break;
		default:
			return 0;
	}
	return 1;
}

static void gmdCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	gmdActive=0;
	mpStopModule (cpifaceSession);
	mpFree(&mod);
}

static int gmdLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	mpSetLoop (LoopMod);
	cpifaceSession->mcpDevAPI->Idle (cpifaceSession);

	return (!LoopMod) && mpLooped();
}

static int gmdOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file, int (*loader) (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file))
{
	const char *filename;
	uint64_t i;
	int retval;

	if (!cpifaceSession->mcpDevAPI->OpenPlayer)
		return errPlay;

	if (!file)
		return errFileOpen;

	patlock=0;

	i = file->filesize (file);
	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD] loading %s... (%uk)\n", filename, (unsigned int)(i>>10));

	memset (info->composer, 0, sizeof (info->composer));
	if ((retval = loader (cpifaceSession, &mod, file)))
	{
		mpFree(&mod);
		return retval;
	}

	{
		unsigned int sampsize=0;
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD] preparing samples (");
		for (i=0; i<mod.sampnum; i++)
			sampsize+=(mod.samples[i].length)<<(!!(mod.samples[i].type&mcpSamp16Bit));
		cpifaceSession->cpiDebug (cpifaceSession, "%uk)...\n", sampsize>>10);
	}
	if (!mpReduceSamples(&mod))
	{
		mpFree(&mod);
		return errAllocMem;
	}
	if (!mpLoadSamples (cpifaceSession, &mod))
	{
		mpFree(&mod);
		return errAllocSamp;
	}
	mpReduceMessage(&mod);
	mpReduceInstruments(&mod);
	mpOptimizePatLens(&mod);

	if ((retval = mpPlayModule(&mod, file, cpifaceSession)))
	{
		mpFree(&mod);
		return retval;
	}

	cpifaceSession->PanType = !!(mod.options & MOD_MODPAN);

	cpifaceSession->IsEnd = gmdLooped;
	cpifaceSession->ProcessKey = gmdProcessKey;
	cpifaceSession->DrawGStrings = gmdDrawGStrings;
	cpifaceSession->SetMuteChannel = mpMute;
	cpifaceSession->GetLChanSample = mpGetChanSample;

	cpifaceSession->LogicalChannelCount = mod.channum;

	cpifaceSession->UseDots(gmdGetDots);
	if (mod.message)
	{
		cpifaceSession->UseMessage(mod.message);
	}
	gmdInstSetup (cpifaceSession, mod.instruments, mod.instnum, mod.modsamples, mod.modsampnum, mod.samples, mod.sampnum,
			( (info->modtype.integer.i==MODULETYPE("S3M")) || (info->modtype.integer.i==MODULETYPE("PTM")) )
				?
				1
				:
				( (info->modtype.integer.i==MODULETYPE("DMF")) || (info->modtype.integer.i==MODULETYPE("669")) )
					?
					2
					:
					0, gmdMarkInsSamp);
	gmdChanSetup (cpifaceSession, &mod);
	gmdTrkSetup (cpifaceSession, &mod);

	cpifaceSession->GetPChanSample = cpifaceSession->mcpGetChanSample;

	cpifaceSession->InPause = 0;
	cpifaceSession->mcpSet (cpifaceSession, -1, mcpMasterPause, 0);

	gmdActive=1;

	return errOk;
}

static int gmdOpenFile669 (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, Load669);
}

static int gmdOpenFileAMS (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, LoadAMS);
}

static int gmdOpenFileDMF (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, LoadDMF);
}

static int gmdOpenFileMDL (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, LoadMDL);
}

static int gmdOpenFileMTM (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, LoadMTM);
}

static int gmdOpenFileOKT (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, LoadOKT);
}

static int gmdOpenFilePTM (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, LoadPTM);
}

static int gmdOpenFileS3M (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, LoadS3M);
}

static int gmdOpenFileSTM (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, LoadSTM);
}

static int gmdOpenFileULT (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	return gmdOpenFile (cpifaceSession, info, file, LoadULT);
}

static int gmdPluginInit (struct PluginInitAPI_t *API)
{
	return gmd_type_init (API);
}

static void gmdPluginClose (struct PluginCloseAPI_t *API)
{
	gmd_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayer669 = {"[General module plugin: 669]", gmdOpenFile669, gmdCloseFile};
OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerAMS = {"[General module plugin: AMS]", gmdOpenFileAMS, gmdCloseFile};
OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerDMF = {"[General module plugin: DMF]", gmdOpenFileDMF, gmdCloseFile};
OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerMDL = {"[General module plugin: MDL]", gmdOpenFileMDL, gmdCloseFile};
OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerMTM = {"[General module plugin: MTM]", gmdOpenFileMTM, gmdCloseFile};
OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerOKT = {"[General module plugin: OKT]", gmdOpenFileOKT, gmdCloseFile};
OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerPTM = {"[General module plugin: PTM]", gmdOpenFilePTM, gmdCloseFile};
OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerS3M = {"[General module plugin: S3M]", gmdOpenFileS3M, gmdCloseFile};
OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerSTM = {"[General module plugin: STM]", gmdOpenFileSTM, gmdCloseFile};
OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerULT = {"[General module plugin: ULT]", gmdOpenFileULT, gmdCloseFile};

DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playgmd", .desc = "OpenCP General Module Player (c) 1994-'25 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = gmdPluginInit, .PluginClose = gmdPluginClose};
