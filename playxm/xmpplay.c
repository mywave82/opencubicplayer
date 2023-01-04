/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * XMPlay interface routines
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -removed all references to gmd structures to make this more flexible
 *    -removed mcp "restricted" flag (theres no point in rendering XM files
 *     to disk in mono if FT is able to do this in stereo anyway ;)
 *    -finally, added all the screen output we all waited for since november
 *     1996 :)
 *  -ss040326   Stian Skjelstad <stian@nixia.no>
 *    -don't length optimize pats if load failed (and memory is freed)
 *  -ss040709   Stian Skjelstad <stian@nixia.no>
 *    -use compatible timing, and not cputime/clock()
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/deviwave.h"
#include "dev/mcp.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "xmchan.h"
#include "xmplay.h"
#include "xmtype.h"

__attribute__ ((visibility ("internal"))) struct xmodule mod;

static time_t starttime;      /* when did the song start, if paused, this is slided if unpaused */
static time_t pausetime;      /* when did the pause start (fully paused) */
static time_t pausefadestart; /* when did the pause fade start, used to make the slide */
static int8_t pausefadedirection; /* 0 = no slide, +1 = sliding from pause to normal, -1 = sliding from normal to pause */

static struct xmpinstrument *insts;
static struct xmpsample *samps;

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


static int xmpProcessKey(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	int row;
	int pat, p;

	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('<', "Jump back (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Jump back (big)");
			cpifaceSession->KeyHelp ('>', "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_UP, "Jump back (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_DOWN, "Jump forward (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Jump to start of track");
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
			xmpInstClear (cpifaceSession);
			xmpSetPos (cpifaceSession, 0, 0);
			if (cpifaceSession->InPause)
			{
				starttime = pausetime;
			} else {
				starttime = clock_ms();
			}
			break;
		case '<':
		case KEY_CTRL_LEFT:
			p=xmpGetPos();
			pat=p>>8;
			xmpSetPos (cpifaceSession, pat-1, 0);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			p=xmpGetPos();
			pat=p>>8;
			xmpSetPos (cpifaceSession, pat+1, 0);
			break;
		case KEY_CTRL_UP:
			p=xmpGetPos();
			pat=p>>8;
			row=p&0xFF;
			xmpSetPos (cpifaceSession, pat, row-8);
			break;
		case KEY_CTRL_DOWN:
			p=xmpGetPos();
			pat=p>>8;
			row=p&0xFF;
			xmpSetPos (cpifaceSession, pat, row+8);
			break;
		default:
			return 0;
	}
	return 1;
}

static int xmpLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	if (pausefadedirection)
	{
		dopausefade (cpifaceSession);
	}
	xmpSetLoop (LoopMod);
	cpifaceSession->mcpDevAPI->Idle (cpifaceSession);

	return (!LoopMod) && xmpLoop();
}

static void xmpDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int pos = xmpGetRealPos (cpifaceSession);
	int gvol,bpm,tmp;
	struct xmpglobinfo gi;

	xmpGetGlobInfo(&tmp, &bpm, &gvol);
	xmpGetGlobInfo2(&gi);

	cpifaceSession->drawHelperAPI->GStringsTracked
	(
		cpifaceSession,
		0,          /* song X */
		0,          /* song Y */
		(pos>>8)&0xFF,/* row X */
		mod.patlens[mod.orders[(pos>>16)&0xFF]]-1,/* row Y */
		(pos>>16)&0xFF,/* order X */
		mod.nord-1, /* order Y */
		tmp,        /* speed */
		bpm,        /* tempo */
		gvol,
		(gi.globvolslide==xfxGVSUp)?1:(gi.globvolslide==xfxGVSDown)?-1:0,
		0,          /* chan X */
		0,          /* chan Y */
		cpifaceSession->InPause ? ((pausetime - starttime) / 1000) : ((clock_ms() - starttime) / 1000)
	);
}

static void xmpCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	xmpStopModule (cpifaceSession);
	xmpFreeModule(&mod);
}

/***********************************************************************/

static void xmpMarkInsSamp (struct cpifaceSessionAPI_t *cpifaceSession, char *ins, char *smp)
{
	int i;
	int in, sm;

	/* mod.nchan == cpifaceSession->LogicalChannelCount */
	for (i=0; i<mod.nchan; i++)
	{
		if (!xmpChanActive (cpifaceSession, i) || cpifaceSession->MuteChannel[i])
			continue;
		in=xmpGetChanIns(i);
		sm=xmpGetChanSamp(i);
		ins[in-1]=((cpifaceSession->SelectedChannel==i)||(ins[in-1]==3))?3:2;
		smp[sm]=((cpifaceSession->SelectedChannel==i)||(smp[sm]==3))?3:2;
	}
}

/*************************************************************************/

static int xmpGetDots (struct cpifaceSessionAPI_t *cpifaceSession, struct notedotsdata *d, int max)
{
	int pos=0;
	int i;

	int smp,frq,voll,volr,sus;

	/* mod.nchan == cpifaceSession->LogicalChannelCount */
	for (i=0; i<mod.nchan; i++)
	{
		if (pos>=max)
			break;
		if (!xmpGetDotsData (cpifaceSession, i, &smp, &frq, &voll, &volr, &sus))
			continue;
		d[pos].voll=voll;
		d[pos].volr=volr;
		d[pos].chan=i;
		d[pos].note=frq;
		d[pos].col=(sus?32:16)+(smp&15);
		pos++;
	}
	return pos;
}

static int xmpOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	const char *filename;
	int (*loader)(struct xmodule *, struct ocpfilehandle_t *)=0;
	int retval;

	if (!cpifaceSession->mcpDevAPI->OpenPlayer)
		return errGen;

	if (!file)
		return errFileOpen;

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s (%uk)...\n", filename, (unsigned int)(file->filesize (file) >> 10));

	     if (info->modtype.integer.i == MODULETYPE("XM"))   loader=xmpLoadModule;
	else if (info->modtype.integer.i == MODULETYPE("MOD"))  loader=xmpLoadMOD;
	else if (info->modtype.integer.i == MODULETYPE("MODt")) loader=xmpLoadMODt;
	else if (info->modtype.integer.i == MODULETYPE("MODd")) loader=xmpLoadMODd;
	else if (info->modtype.integer.i == MODULETYPE("M31"))  loader=xmpLoadM31;
	else if (info->modtype.integer.i == MODULETYPE("M15"))  loader=xmpLoadM15;
	else if (info->modtype.integer.i == MODULETYPE("M15t")) loader=xmpLoadM15t;
	else if (info->modtype.integer.i == MODULETYPE("WOW"))  loader=xmpLoadWOW;
	else if (info->modtype.integer.i == MODULETYPE("MXM"))  loader=xmpLoadMXM;
	else if (info->modtype.integer.i == MODULETYPE("MODf")) loader=xmpLoadMODf;

	if (!loader)
		return errFormStruc;

	if (retval=loader(&mod, file))
	{
		xmpFreeModule(&mod);
		return retval;
	}
	if (!xmpLoadSamples (cpifaceSession, &mod))
	{
		xmpFreeModule(&mod);
		return errAllocMem;
	}

	xmpOptimizePatLens(&mod);

	if (!xmpPlayModule(&mod, file, cpifaceSession))
	{
		xmpFreeModule(&mod);
		return errPlay;
	}

	insts=mod.instruments;
	samps=mod.samples;

	cpifaceSession->IsEnd = xmpLooped;
	cpifaceSession->ProcessKey = xmpProcessKey;
	cpifaceSession->DrawGStrings = xmpDrawGStrings;
	cpifaceSession->SetMuteChannel = xmpMute;
	cpifaceSession->GetLChanSample = cpifaceSession->mcpGetChanSample;
	cpifaceSession->GetPChanSample = cpifaceSession->mcpGetChanSample;

	cpifaceSession->LogicalChannelCount = mod.nchan;

	cpifaceSession->UseDots(xmpGetDots);

	xmChanSetup (cpifaceSession, insts, samps);

	xmpInstSetup (cpifaceSession, mod.instruments, mod.ninst, mod.samples, mod.nsamp, mod.sampleinfos, mod.nsampi, 0, xmpMarkInsSamp);
	xmTrkSetup (cpifaceSession, &mod);

	starttime = clock_ms();
	cpifaceSession->InPause = 0;
	cpifaceSession->mcpSet(-1, mcpMasterPause, 0);
	pausefadedirection = 0;

	return errOk;
}

static int xmPluginInit (struct PluginInitAPI_t *API)
{
	return xm_type_init (API);
}

static void xmPluginClose (struct PluginCloseAPI_t *API)
{
	xm_type_done (API);
}

const struct cpifaceplayerstruct __attribute__((visibility ("internal"))) xmpPlayer = {"[FastTracker II plugin]", xmpOpenFile, xmpCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playxm", .desc = "OpenCP XM/MOD Player (c) 1995-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = xmPluginInit, .PluginClose = xmPluginClose};
