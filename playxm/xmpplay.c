/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
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

OCP_INTERNAL struct xmodule mod;

static struct xmpinstrument *insts;
static struct xmpsample *samps;

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
			cpifaceSession->TogglePauseFade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			cpifaceSession->TogglePause (cpifaceSession);
			break;
		case KEY_CTRL_HOME:
			xmpInstClear (cpifaceSession);
			xmpSetPos (cpifaceSession, 0, 0);
			if (cpifaceSession->InPause)
			{
#warning reset display time
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
		0           /* chan Y */
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
	int (*loader)(struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *, struct ocpfilehandle_t *)=0;
	int retval;

	if (!cpifaceSession->mcpDevAPI)
	{
		return errPlay;
	}

	if (!file)
		return errFileOpen;

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[XM] loading %s (%uk)...\n", filename, (unsigned int)(file->filesize (file) >> 10));

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
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[XM] no loader found\n");
		return errGen;
	}

	if ((retval = loader (cpifaceSession, &mod, file)))
	{
		xmpFreeModule(&mod);
		return retval;
	}
	if (!xmpLoadSamples (cpifaceSession, &mod))
	{
		xmpFreeModule(&mod);
		return errAllocSamp;
	}

	xmpOptimizePatLens(&mod);

	if ((retval = xmpPlayModule (&mod, file, cpifaceSession)))
	{
		xmpFreeModule(&mod);
		return retval;
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

	cpifaceSession->InPause = 0;
	cpifaceSession->mcpSet (cpifaceSession, -1, mcpMasterPause, 0);

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

OCP_INTERNAL const struct cpifaceplayerstruct xmpPlayer = {"[FastTracker II plugin]", xmpOpenFile, xmpCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playxm", .desc = "OpenCP XM/MOD Player (c) 1995-'23 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = xmPluginInit, .PluginClose = xmPluginClose};
