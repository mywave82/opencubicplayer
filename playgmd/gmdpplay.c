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
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static int gmdActive;

static time_t starttime;
static time_t pausetime;
static char utf8_8_dot_3  [12*4+1];  /* UTF-8 ready */
static char utf8_16_dot_3 [20*4+1]; /* UTF-8 ready */
static struct moduleinfostruct mdbdata;

__attribute__ ((visibility ("internal"))) struct gmdmodule mod;
static char patlock;

static void gmdMarkInsSamp (struct cpifaceSessionAPI_t *cpiSession, uint8_t *ins, uint8_t *samp)
{
	int i;
	/* mod.channum == cpiSessionAPI->LogicalChannelCount */
	for (i=0; i<mod.channum; i++)
	{
		struct chaninfo ci;
		mpGetChanInfo(i, &ci);

		if (!mpGetMute(i)&&mpGetChanStatus(i)&&ci.vol)
		{
			ins[ci.ins]=((cpiSession->SelectedChannel==i)||(ins[ci.ins]==3))?3:2;
			samp[ci.smp]=((cpiSession->SelectedChannel==i)||(samp[ci.smp]==3))?3:2;
		}
	}
}

static int mpLoadGen(struct gmdmodule *m, struct ocpfilehandle_t *file, struct moduletype type, const char *link, const char *name)
{
	int hnd;
	struct gmdloadstruct *loadfn;
	volatile uint8_t retval;

	if ((!link)||(!name))
	{
#ifdef LD_DEBUG
		fprintf (stderr, "ldlink or loader information is missing\n");
#endif
		return errSymMod;
	}

#ifdef LD_DEBUG
	fprintf(stderr, " (%s) Trying to locate \"%s\", func \"%s\"\n", secname, link, name);
#endif

	hnd=lnkLink(link);
	if (hnd<=0)
	{
#ifdef LD_DEBUG
		fprintf(stderr, "Failed to locate ldlink \"%s\"\n", link);
#endif
		return errSymMod;
	}

	loadfn=_lnkGetSymbol(name);
	if (!loadfn)
	{
#ifdef LD_DEBUG
		fprintf(stderr, "Failed to locate loaded \"%s\"\n", name);
#endif
		lnkFree(hnd);
		return errSymSym;
	}
#ifdef LD_DEBUG
	fprintf(stderr, "loading using %s-%s\n", link, name);
#endif
	memset(m->composer, 0, sizeof(m->composer));
	retval=loadfn->load(m, file);

	lnkFree(hnd);

	return retval;
}

static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;

static void startpausefade(void)
{
	if (plPause)
		starttime=starttime+dos_clock()-pausetime;

	if (pausefadedirect)
	{
		if (pausefadedirect<0)
			plPause=1;
		pausefadestart=2*dos_clock()-DOS_CLK_TCK-pausefadestart;
	} else
		pausefadestart=dos_clock();

	if (plPause)
	{
		mcpSet(-1, mcpMasterPause, plPause=0);
		pausefadedirect=1;
	} else
		pausefadedirect=-1;
}

static void dopausefade(void)
{
	int16_t i;
	if (pausefadedirect>0)
	{
		i=((int32_t)dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i<0)
			i=0;
		if (i>=64)
		{
			i=64;
			pausefadedirect=0;
		}
	} else {
		i=64-((int32_t)dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i>=64)
			i=64;
		if (i<=0)
		{
			i=0;
			pausefadedirect=0;
			pausetime=dos_clock();
			mcpSet(-1, mcpMasterPause, plPause=1);
			mcpSetMasterPauseFadeParameters (64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetMasterPauseFadeParameters (i);
}

static void gmdDrawGStrings (void)
{
	struct globinfo gi;

	mcpDrawGStrings ();

	mpGetGlobInfo (&gi);

	mcpDrawGStringsTracked
	(
		utf8_8_dot_3,
		utf8_16_dot_3,
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
		mcpset.amp,
		(set.filter==1)?"AOI":(set.filter==2)?"FOI":"off",
		plPause,
		plPause?((pausetime-starttime)/DOS_CLK_TCK):((dos_clock()-starttime)/DOS_CLK_TCK),
		&mdbdata
	);
}

static int gmdProcessKey (struct cpifaceSessionAPI_t *cpiSession, uint16_t key)
{
	uint16_t pat;
	uint8_t row;
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp(KEY_ALT_L, "Pattern lock toggle");
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_UP, "Jump back (small)");
			cpiKeyHelp(KEY_CTRL_DOWN, "Jump forward (small)");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Jump back (big)");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump back (big)");
			cpiKeyHelp('>', "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_HOME, "Jump start of track");
			mcpSetProcessKey (key);
			return 0;
		case 'p': case 'P':
			startpausefade();
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			if (plPause)
				starttime=starttime+dos_clock()-pausetime;
			else
				pausetime=dos_clock();
			mcpSet(-1, mcpMasterPause, plPause^=1);
			break;
		case KEY_CTRL_HOME:
			gmdInstClear (cpiSession);
			mpSetPosition(0, 0);
			if (plPause)
				starttime=pausetime;
			else
				starttime=dos_clock();
			break;
		case '<':
		case KEY_CTRL_LEFT:
			mpGetPosition(&pat, &row);
			mpSetPosition(pat-1, 0);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			mpGetPosition(&pat, &row);
			mpSetPosition(pat+1, 0);
			break;
		case KEY_CTRL_UP:
			mpGetPosition(&pat, &row);
			mpSetPosition(pat, row-8);
			break;
		case KEY_CTRL_DOWN:
			mpGetPosition(&pat, &row);
			mpSetPosition(pat, row+8);
			break;
		case KEY_ALT_L:
			patlock=!patlock;
			mpLockPat(patlock);
			break;
		default:
			return mcpSetProcessKey (key);
	}
	return 1;
}

static void gmdCloseFile(void)
{
	gmdActive=0;
	mpStopModule();
	mpFree(&mod);
}

static void gmdIdle(void)
{
	mpSetLoop(fsLoopMods);
	if (mcpIdle)
		mcpIdle();
	if (pausefadedirect)
		dopausefade();
}

static int gmdLooped(void)
{
	return (!fsLoopMods&&mpLooped());
}

static int gmdOpenFile (struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader, struct cpifaceSessionAPI_t *cpiSessionAPI)
{
	const char *filename;
	uint64_t i;
	int retval;

	if (!mcpOpenPlayer)
		return errGen;

	if (!file)
		return errFileOpen;

	patlock=0;

	mdbdata = *info;

	i = file->filesize (file);
	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s... (%uk)\n", filename, (unsigned int)(i>>10));
	utf8_XdotY_name ( 8, 3, utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, utf8_16_dot_3, filename);

	retval=mpLoadGen(&mod, file, info->modtype, ldlink, loader);

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
	plPanType=!!(mod.options&MOD_MODPAN);

	plIsEnd=gmdLooped;
	plIdle=gmdIdle;
	plProcessKey=gmdProcessKey;
	plDrawGStrings=gmdDrawGStrings;
	plSetMute=mpMute;
	plGetLChanSample=mpGetChanSample;

	cpiSessionAPI->LogicalChannelCount = mod.channum;
	cpiSessionAPI->PhysicalChannelCount = mcpNChan;

	plUseDots(gmdGetDots);
	if (mod.message)
		plUseMessage(mod.message);
	gmdInstSetup (cpiSessionAPI, mod.instruments, mod.instnum, mod.modsamples, mod.modsampnum, mod.samples, mod.sampnum,
			( (info->modtype.integer.i==MODULETYPE("S3M")) || (info->modtype.integer.i==MODULETYPE("PTM")) )
				?
				1
				:
				( (info->modtype.integer.i==MODULETYPE("DMF")) || (info->modtype.integer.i==MODULETYPE("669")) )
					?
					2
					:
					0, gmdMarkInsSamp);
	gmdChanSetup(&mod);
	gmdTrkSetup(&mod);

	if (!mpPlayModule(&mod, file, cpiSessionAPI))
		retval=errPlay;

	plGetPChanSample=mcpGetChanSample;

	if (retval)
	{
		mpFree(&mod);
		return retval;
	}

	starttime=dos_clock();
	plPause=0;
	mcpSet(-1, mcpMasterPause, 0);
	pausefadedirect=0;

	gmdActive=1;

	return errOk;
}

struct cpifaceplayerstruct gmdPlayer = {"[General module plugin]", gmdOpenFile, gmdCloseFile};

char *dllinfo = "";
struct linkinfostruct dllextinfo = {.name = "playgmd", .desc = "OpenCP General Module Player (c) 1994-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
