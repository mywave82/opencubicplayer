/* OpenCP Module Player
 * copyright (c) '94-'21 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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

#define _MAX_FNAME 8
#define _MAX_EXT 4

static int gmdActive;

static const char *modname;
static const char *composer;

static char currentmodname[_MAX_FNAME+1];
static char currentmodext[_MAX_EXT+1];

static time_t starttime;
static time_t pausetime;

static struct gmdmodule mod;
static char patlock;

static void gmdMarkInsSamp(uint8_t *ins, uint8_t *samp)
{
	int i;
	for (i=0; i<plNLChan; i++)
	{
		struct chaninfo ci;
		mpGetChanInfo(i, &ci);

		if (!mpGetMute(i)&&mpGetChanStatus(i)&&ci.vol)
		{
			ins[ci.ins]=((plSelCh==i)||(ins[ci.ins]==3))?3:2;
			samp[ci.smp]=((plSelCh==i)||(samp[ci.smp]==3))?3:2;
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
		plChanChanged=1;
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
			plChanChanged=1;
			mcpSetFadePars(64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetFadePars(i);
}

static void gmdDrawGStrings(unsigned short (*buf)[CONSOLE_MAX_X])
{
	struct globinfo gi;
	uint32_t tim;

	mcpDrawGStrings(buf);

	mpGetGlobInfo(&gi);

	if (plPause)
		tim=(pausetime-starttime)/DOS_CLK_TCK;
	else
		tim=(dos_clock()-starttime)/DOS_CLK_TCK;

	if (plScrWidth<128)
	{
		memset(buf[2]+80, 0, (plScrWidth-80)*sizeof(uint16_t));

		writestring(buf[1],  0, 0x09, " row: ../..  ord: .../...  tempo: ..  bpm: ...  gvol: ..\xfa ", 58);
		writenum(buf[1],  6, 0x0F, gi.currow, 16, 2, 0);
		writenum(buf[1],  9, 0x0F, gi.patlen-1, 16, 2, 0);
		writenum(buf[1], 18, 0x0F, gi.curpat, 16, 3, 0);
		writenum(buf[1], 22, 0x0F, gi.patnum-1, 16, 3, 0);
		writenum(buf[1], 34, 0x0F, gi.tempo, 16, 2, 1);
		writenum(buf[1], 43, 0x0F, gi.speed, 10, 3, 1);
		writenum(buf[1], 54, 0x0F, gi.globvol, 16, 2, 0);
		writestring(buf[1], 56, 0x0F, (gi.globvolslide==fxGVSUp)?"\x18":(gi.globvolslide==fxGVSDown)?"\x19":" ", 1);

		writestring(buf[2],  0, 0x09, " module \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................               time: ..:.. ", 80);
		writestring(buf[2],  8, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 16, 0x0F, currentmodext, _MAX_EXT);
#warning modname is not UTF-8...
		writestring(buf[2], 22, 0x0F, modname, 31);
		if (plPause)
			writestring(buf[2], 58, 0x0C, "paused", 6);
		writenum(buf[2], 74, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 76, 0x0F, ":", 1);
		writenum(buf[2], 77, 0x0F, tim%60, 10, 2, 0);
	} else {
		memset(buf[2]+128, 0, (plScrWidth-128)*sizeof(uint16_t));

		writestring(buf[1],  0, 0x09, "    row: ../..  order: .../...   tempo: ..  speed/bpm: ...   global volume: ..\xfa  ", 81);
		writenum(buf[1],  9, 0x0F, gi.currow, 16, 2, 0);
		writenum(buf[1], 12, 0x0F, gi.patlen-1, 16, 2, 0);
		writenum(buf[1], 23, 0x0F, gi.curpat, 16, 3, 0);
		writenum(buf[1], 27, 0x0F, gi.patnum-1, 16, 3, 0);
		writenum(buf[1], 40, 0x0F, gi.tempo, 16, 2, 1);
		writenum(buf[1], 55, 0x0F, gi.speed, 10, 3, 1);
		writenum(buf[1], 76, 0x0F, gi.globvol, 16, 2, 0);
		writestring(buf[1], 78, 0x0F, (gi.globvolslide==fxGVSUp)?"\x18":(gi.globvolslide==fxGVSDown)?"\x19":" ", 1);

		writestring(buf[2],  0, 0x09, "    module \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................  composer: ...............................                  time: ..:..    ", 132);
		writestring(buf[2], 11, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 19, 0x0F, currentmodext, _MAX_EXT);
#warning modname and title are now UTF-8
		writestring(buf[2], 25, 0x0F, modname, 31);
		writestring(buf[2], 68, 0x0F, composer, 31);
		if (plPause)
			writestring(buf[2], 100, 0x0C, "playback paused", 15);
		writenum(buf[2], 123, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 125, 0x0F, ":", 1);
		writenum(buf[2], 126, 0x0F, tim%60, 10, 2, 0);
	}
}

#define dgetch() egetch()
#define dkbhit() ekbhit()
#define releaseslice() {} /* we don't have this feature one generic unixes other than nice stuff */

static int gmdProcessKey(unsigned short key)
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
			plChanChanged=1;
			break;
		case KEY_CTRL_HOME:
			gmdInstClear();
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

static int gmdOpenFile (struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader)
{
	const char *filename;
	uint64_t i;
	int retval;

	if (!mcpOpenPlayer)
		return errGen;

	if (!file)
		return errFileOpen;

	patlock=0;

#warning replace currentmodname currentmodext
	//strncpy(currentmodname, info->name, _MAX_FNAME);
	//strncpy(currentmodext, info->name + _MAX_FNAME, _MAX_EXT);

	i = file->filesize (file);
	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s... (%uk)\n", filename, (unsigned int)(i>>10));

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
	plNLChan=mod.channum;
	modname=mod.name;
	composer=mod.composer;
	plPanType=!!(mod.options&MOD_MODPAN);

	plIsEnd=gmdLooped;
	plIdle=gmdIdle;
	plProcessKey=gmdProcessKey;
	plDrawGStrings=gmdDrawGStrings;
	plSetMute=mpMute;
	plGetLChanSample=mpGetChanSample;
	plUseDots(gmdGetDots);
	if (mod.message)
		plUseMessage(mod.message);
	gmdInstSetup(mod.instruments, mod.instnum, mod.modsamples, mod.modsampnum, mod.samples, mod.sampnum,
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

	if (!plCompoMode)
	{
		if (!*modname)
			modname=info->title;
		if (!*composer)
			composer=info->composer;
	} else
		modname=info->comment;

	if (!mpPlayModule(&mod, file))
		retval=errPlay;
	plNPChan=mcpNChan;

	plGetRealMasterVolume=mcpGetRealMasterVolume;
	plGetMasterSample=mcpGetMasterSample;
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

struct cpifaceplayerstruct gmdPlayer = {gmdOpenFile, gmdCloseFile};

char *dllinfo = "";
struct linkinfostruct dllextinfo = {.name = "playgmd", .desc = "OpenCP General Module Player (c) 1994-21 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
