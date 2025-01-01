/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ITPlayer interface routines
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
 *  -kb980717   Tammo Hinrichs <kb@nwn.de>
 *    -added many many things to provide channel display and stuff
 *    -removed some bugs which caused crashing in some situations
 *  -ss040709   Stian Skjelstad <stian@nixia.no>
 *    -use compatible timing instead of cputime/clock()
 */

#include "config.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/deviwave.h"
#include "dev/mcp.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "itchan.h"
#include "itplay.h"
#include "ittype.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

OCP_INTERNAL struct itplayer itplayer;
static struct it_module mod;

static struct it_instrument *insts;
static struct it_sample *samps;

static int itpProcessKey(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			itpInstClear (cpifaceSession);
			setpos (&itplayer, 0, 0);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			p=getpos(&itplayer);
			pat=p>>16;
			setpos(&itplayer, pat-1, 0);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			p=getpos(&itplayer);
			pat=p>>16;
			setpos(&itplayer, pat+1, 0);
			break;
		case KEY_CTRL_UP:
			p=getpos(&itplayer);
			pat=p>>16;
			row=(p>>8)&0xFF;
			setpos(&itplayer, pat, row-8);
			break;
		case KEY_CTRL_DOWN:
			p=getpos(&itplayer);
			pat=p>>16;
			row=(p>>8)&0xFF;
			setpos(&itplayer, pat, row+8);
			break;
		default:
			return 0;
	}
	return 1;
}

static int itpLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	setloop(&itplayer, LoopMod);
	cpifaceSession->mcpDevAPI->Idle (cpifaceSession);

	return (!LoopMod) && getloop(&itplayer);
}

static void itpDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int pos = getrealpos (cpifaceSession, &itplayer)>>8;
	int gvol, bpm, tmp, gs;
	int i, nch = 0;

	getglobinfo (cpifaceSession, &itplayer, &tmp, &bpm, &gvol, &gs);

	for (i=0; i < cpifaceSession->PhysicalChannelCount; i++)
	{
		if (cpifaceSession->mcpGet (cpifaceSession, i, mcpCStatus))
		{
			nch++;
		}
	}

	cpifaceSession->drawHelperAPI->GStringsTracked
	(
		cpifaceSession,
		0,          /* song X */
		0,          /* song Y */
		pos&0xFF,   /* row X */
		mod.patlens[mod.orders[pos>>8]]-1, /* row Y */
		pos>>8,     /* order X */
		mod.nord-1, /* order Y */
		tmp,        /* speed */
		bpm,        /* tempo */
		gvol,
		(gs==ifxGVSUp)?1:(gs==ifxGVSDown)?-1:0,
		nch,
		cpifaceSession->PhysicalChannelCount
	);
}

static void itpCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	itstop (cpifaceSession, &itplayer);
	it_free(&mod);
}

/**********************************************************************/

static void itpMarkInsSamp(struct cpifaceSessionAPI_t *cpifaceSession, uint8_t *ins, uint8_t *smp)
{
	int i;

	/* mod.nchan == cpifaceSession->LogicalChannelCount */
	for (i=0; i<mod.nchan; i++)
	{
		int j;
		if (cpifaceSession->MuteChannel[i])
			continue;
		for (j=0; j<mod.nchan; j++)
		{
			int lc, in, sm;
			if (!chanactive (cpifaceSession, &itplayer, j, &lc))
				continue;
			if (lc!=i)
				continue;
			in=getchanins(&itplayer, j);
			sm=getchansamp(&itplayer, j);
			ins[in-1] = ((cpifaceSession->SelectedChannel==i)||(ins[in-1]==3))?3:2;
			smp[sm] = ((cpifaceSession->SelectedChannel==i)||(smp[sm]==3))?3:2;
		}
	}
}

/************************************************************************/

static int itpGetDots (struct cpifaceSessionAPI_t *cpifaceSession, struct notedotsdata *d, int max)
{
	int i,j;
	int pos=0;
	/* mod.nchan == cpifaceSession->LogicalChannelCount */
	for (i=0; i<mod.nchan; i++)
	{
		if (pos>=max)
			break;
		j=0;
		while (pos<max)
		{
			int smp, voll, volr, note, sus;
			j = getdotsdata (cpifaceSession, &itplayer, i, j, &smp, &note, &voll, &volr, &sus);
			if (j==-1)
				break;
			d[pos].voll=voll;
			d[pos].volr=volr;
			d[pos].chan=i;
			d[pos].note=note;
			d[pos].col=(smp&15)+(sus?32:16);
			pos++;
		}
	}
  return pos;
}

static void itpMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m)
{
	cpifaceSession->MuteChannel[i] = m;
	mutechan(&itplayer, i, m);
}

static int itpGetLChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *buf, unsigned int len, uint32_t rate, int opt)
{
	return getchansample (cpifaceSession, &itplayer, ch, buf, len, rate, opt);
}

static int itpOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	const char *filename;
	int retval;
	int nch;

	if (!file)
		return errFileOpen;

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[IT] loading %s (%uk)...\n", filename, (unsigned int)(file->filesize(file)>>10));

	memset (&itplayer, 0, sizeof (itplayer));
	memset (&mod, 0, sizeof (mod));

	if (!(retval=it_load(cpifaceSession, &mod, file)))
		if (!loadsamples (cpifaceSession, &mod))
			retval=errAllocMem;

	if (retval)
	{
		it_free(&mod);
		return retval;
	}

	it_optimizepatlens(&mod);

	nch = cpifaceSession->configAPI->GetProfileInt2 (cpifaceSession->configAPI->SoundSec, "sound", "itchan", 64, 10);
	if ((retval = itplay(&itplayer, &mod, nch, file, cpifaceSession)))
	{
		it_free(&mod);
		return retval;
	}

	insts=mod.instruments;
	samps=mod.samples;
	cpifaceSession->IsEnd = itpLooped;
	cpifaceSession->ProcessKey = itpProcessKey;
	cpifaceSession->DrawGStrings = itpDrawGStrings;
	cpifaceSession->SetMuteChannel = itpMute;
	cpifaceSession->GetLChanSample = itpGetLChanSample;
	cpifaceSession->LogicalChannelCount = mod.nchan;
	cpifaceSession->UseDots(itpGetDots);
	itChanSetup (cpifaceSession, insts, samps);
	itpInstSetup (cpifaceSession, mod.instruments, mod.ninst, mod.samples, mod.nsamp, mod.sampleinfos, /*mod.nsampi,*/ 0, itpMarkInsSamp);
	itTrkSetup (cpifaceSession, &mod);
	if (mod.message)
	{
		cpifaceSession->UseMessage(mod.message);
	}

	cpifaceSession->GetPChanSample = cpifaceSession->mcpGetChanSample;

	cpifaceSession->InPause = 0;
	cpifaceSession->mcpSet (cpifaceSession, -1, mcpMasterPause, 0);

	return errOk;
}

static int itPluginInit (struct PluginInitAPI_t *API)
{
	return it_type_init (API);
}

static void itPluginClose (struct PluginCloseAPI_t *API)
{
	it_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct itPlayer = {"[ImpulseTracker plugin]", itpOpenFile, itpCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playit", .desc = "OpenCP IT Player (c) 1997-'25 Tammo Hinrichs, Niklas Beisert, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = itPluginInit, .PluginClose = itPluginClose};
