/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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

static time_t starttime;      /* when did the song start, if paused, this is slided if unpaused */
static time_t pausetime;      /* when did the pause start (fully paused) */
static time_t pausefadestart; /* when did the pause fade start, used to make the slide */
static int8_t pausefadedirection; /* 0 = no slide, +1 = sliding from pause to normal, -1 = sliding from normal to pause */

__attribute__ ((visibility ("internal"))) struct gmdmodule mod;
static char patlock;

static void togglepausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (pausefadedirection)
	{ /* we are already in a pause-fade, reset the fade-start point */
		pausefadestart = clock_ms() - 1000 + (clock_ms() - pausefadestart);
		pausefadedirection *= -1; /* inverse the direction */
	} else if (cpifaceSession->InPause)
	{ /* we are in full pause already */
		pausefadestart = clock_ms();
		starttime = starttime + pausefadestart - pausetime; /* we are unpausing, so push starttime the amount we have been paused */
		cpifaceSession->mcpSet (-1, mcpMasterPause, cpifaceSession->InPause = 0);
		pausefadedirection = 1;
	} else { /* we were not in pause, start the pause fade */
		pausefadestart = clock_ms();
		pausefadedirection = -1;
	}
}

static void dopausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int16_t i;
	if (pausefadedirection > 0)
	{ /* unpause fade */
		i = ((int_fast32_t)(clock_ms() - pausefadestart)) * 64 / 1000;
		if (i < 1)
		{
			i = 1;
		}
		if (i >= 64)
		{
			i = 64;
			pausefadedirection = 0; /* we reached the end of the slide */
		}
	} else { /* pause fade */
		i = 64 - ((int_fast32_t)(clock_ms() - pausefadestart)) * 64 / 1000;
		if (i >= 64)
		{
			i = 64;
		}
		if (i <= 0)
		{ /* we reached the end of the slide, finish the pause command */
			pausefadedirection = 0;
			pausetime = clock_ms();
			cpifaceSession->mcpSet (-1, mcpMasterPause, cpifaceSession->InPause = 1);
			return;
		}
	}
	cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, i);
}



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
		0,          /* chan Y */
		cpifaceSession->InPause ? ((pausetime - starttime) / 1000) : ((clock_ms() - starttime) / 1000)
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
			togglepausefade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			/* cancel any pause-fade that might be in progress */
			pausefadedirection = 0;
			cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, 64);

			if (cpifaceSession->InPause)
			{
				starttime = starttime + clock_ms() - pausetime; /* we are unpausing, so push starttime for the amount we have been paused */
			} else {
				pausetime = clock_ms();
			}
			cpifaceSession->InPause = !cpifaceSession->InPause;
			cpifaceSession->mcpSet (-1, mcpMasterPause, cpifaceSession->InPause);
			break;
		case KEY_CTRL_HOME:
			gmdInstClear (cpifaceSession);
			mpSetPosition (cpifaceSession, 0, 0);
			if (cpifaceSession->InPause)
			{
				starttime = pausetime;
			} else {
				starttime = clock_ms();
			}
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
	if (pausefadedirection)
	{
		dopausefade (cpifaceSession);
	}
	mpSetLoop (LoopMod);
	mcpDevAPI->mcpIdle (cpifaceSession);

	return (!LoopMod) && mpLooped();
}

static int gmdOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file, int (*loader) (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file))
{
	const char *filename;
	uint64_t i;
	int retval;

	if (!mcpDevAPI->mcpOpenPlayer)
		return errGen;

	if (!file)
		return errFileOpen;

	patlock=0;

	i = file->filesize (file);
	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s... (%uk)\n", filename, (unsigned int)(i>>10));

	bzero (info->composer, sizeof (info->composer));
	retval = loader (cpifaceSession, &mod, file);

	if (!retval)
	{
		int sampsize=0;
		fprintf(stderr, "preparing samples (");
		for (i=0; i<mod.sampnum; i++)
			sampsize+=(mod.samples[i].length)<<(!!(mod.samples[i].type&mcpSamp16Bit));
		fprintf(stderr, "%ik)...\n", sampsize>>10);

		if (!mpReduceSamples(&mod))
			retval=errAllocMem;
		else if (!mpLoadSamples(&mod))
			retval=errAllocSamp;
		else {
			mpReduceMessage(&mod);
			mpReduceInstruments(&mod);
			mpOptimizePatLens(&mod);
		}
	} else {
		fprintf(stderr, "mpLoadGen failed\n");
		mpFree(&mod);
		return retval;
	}

	if (retval)
		mpFree(&mod);

	if (retval)
		return retval;

	if (plCompoMode)
		mpRemoveText(&mod);
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

	if (!mpPlayModule(&mod, file, cpifaceSession))
		retval=errPlay;

	cpifaceSession->GetPChanSample = cpifaceSession->mcpGetChanSample;

	if (retval)
	{
		mpFree(&mod);
		return retval;
	}

	starttime = clock_ms(); /* initialize starttime */
	cpifaceSession->InPause = 0;
	cpifaceSession->mcpSet(-1, mcpMasterPause, 0);

	pausefadedirection = 0;

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

static int gmdInit (void)
{
	return gmd_type_init ();
}

static void gmdClose (void)
{
	gmd_type_done ();
}

const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayer669 = {"[General module plugin: 669]", gmdOpenFile669, gmdCloseFile};
const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerAMS = {"[General module plugin: AMS]", gmdOpenFileAMS, gmdCloseFile};
const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerDMF = {"[General module plugin: DMF]", gmdOpenFileDMF, gmdCloseFile};
const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerMDL = {"[General module plugin: MDL]", gmdOpenFileMDL, gmdCloseFile};
const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerMTM = {"[General module plugin: MTM]", gmdOpenFileMTM, gmdCloseFile};
const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerOKT = {"[General module plugin: OKT]", gmdOpenFileOKT, gmdCloseFile};
const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerPTM = {"[General module plugin: PTM]", gmdOpenFilePTM, gmdCloseFile};
const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerS3M = {"[General module plugin: S3M]", gmdOpenFileS3M, gmdCloseFile};
const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerSTM = {"[General module plugin: STM]", gmdOpenFileSTM, gmdCloseFile};
const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerULT = {"[General module plugin: ULT]", gmdOpenFileULT, gmdCloseFile};

const struct linkinfostruct dllextinfo = {.name = "playgmd", .desc = "OpenCP General Module Player (c) 1994-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .Init = gmdInit, .Close = gmdClose};
