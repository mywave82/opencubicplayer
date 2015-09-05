/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay interface routines
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
 *  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
 *    -changed path searching for ULTRASND.INI and patch files
 *    -removed strange error which executed interface even in case of errors
 */


#include "config.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "filesel/pfilesel.h"
#include "filesel/mdb.h"
#include "dev/mcp.h"
#include "boot/psetting.h"
#include "boot/plinkman.h"
#include "gmiplay.h"
#include "stuff/poutput.h"
#include "stuff/err.h"
#include "dev/deviwave.h"
#include "cpiface/cpiface.h"
#include "stuff/compat.h"

#define _MAX_FNAME 8
#define _MAX_EXT 4

#define MAXCHAN 64

static const char *modname;
static const char *composer;

static char currentmodname[_MAX_FNAME+1];
static char currentmodext[_MAX_EXT+1];

static uint32_t starttime;
static uint32_t pausetime;

static struct midifile mid={0,0,0,0,0,{0},0,0,0,0};

static void gmiDrawGStrings(uint16_t (*buf)[CONSOLE_MAX_X])
{
	struct mglobinfo gi;
	uint32_t tim;

	midGetGlobInfo(&gi);

	if (plPause)
		tim=(pausetime-starttime)/DOS_CLK_TCK;
	else
		tim=(dos_clock()-starttime)/DOS_CLK_TCK;

	mcpDrawGStrings(buf);
	if (plScrWidth<128)
	{
		writestring(buf[1],  0, 0x09, " pos: ......../........  spd: ....", 57);
		writenum(buf[1], 6, 0x0F, gi.curtick, 16, 8, 0);
		writenum(buf[1], 15, 0x0F, gi.ticknum-1, 16, 8, 0);
		writenum(buf[1], 30, 0x0F, gi.speed, 16, 4, 1);

		writestring(buf[2],  0, 0x09, " module \372\372\372\372\372\372\372\372.\372\372\372: ...............................               time: ..:.. ", 80);
		writestring(buf[2],  8, 0x0F, currentmodname, 8);
		writestring(buf[2], 16, 0x0F, currentmodext, 4);
		writestring(buf[2], 22, 0x0F, modname, 31);
		if (plPause)
			writestring(buf[2], 58, 0x0C, "paused", 6);
		writenum(buf[2], 74, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 76, 0x0F, ":", 1);
		writenum(buf[2], 77, 0x0F, tim%60, 10, 2, 0);
	} else {
		writestring(buf[1],  0, 0x09, "   position: ......../........  speed: ....", 80);
		writenum(buf[1], 13, 0x0F, gi.curtick, 16, 8, 0);
		writenum(buf[1], 22, 0x0F, gi.ticknum-1, 16, 8, 0);
		writenum(buf[1], 39, 0x0F, gi.speed, 16, 4, 1);

		writestring(buf[2],  0, 0x09, "    module \372\372\372\372\372\372\372\372.\372\372\372: ...............................  composer: ...............................                  time: ..:..    ", 132);
		writestring(buf[2], 11, 0x0F, currentmodname, 8);
		writestring(buf[2], 19, 0x0F, currentmodext, 4);
		writestring(buf[2], 25, 0x0F, modname, 31);
		writestring(buf[2], 68, 0x0F, composer, 31);
		if (plPause)
			writestring(buf[2], 100, 0x0C, "playback paused", 15);
		writenum(buf[2], 123, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 125, 0x0F, ":", 1);
		writenum(buf[2], 126, 0x0F, tim%60, 10, 2, 0);
	}
}

static int gmiProcessKey(unsigned short key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('p', "Start/stop pause");
			cpiKeyHelp('P', "Start/stop pause");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Jump back (big)");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump back (big)");
			cpiKeyHelp('>', "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_UP, "Jump back (small)");
			cpiKeyHelp(KEY_CTRL_DOWN, "Jump forward (small)");
			mcpSetProcessKey(key);
			if (mcpProcessKey)
				mcpProcessKey(key);
			return 0;
		case 'p': case 'P':
		case KEY_CTRL_P:
			if (plPause)
				starttime=starttime+dos_clock()-pausetime;
			else
				pausetime=dos_clock();
			mcpSet(-1, mcpMasterPause, plPause^=1);
			plChanChanged=1;
			break;
/*
		case 0x7700: //ctrl-home TODO keys
			gmiClearInst();
			midSetPosition(0);

			if (plPause)
				starttime=pausetime;
			else
				starttime=dos_clock();
			break;
*/
		case KEY_CTRL_LEFT:
		case '<':
			midSetPosition(midGetPosition()-(mid.ticknum>>5));
			break;
		case KEY_CTRL_RIGHT:
		case '>':
			midSetPosition(midGetPosition()+(mid.ticknum>>5));
			break;
		case KEY_CTRL_UP:
			midSetPosition(midGetPosition()-(mid.ticknum>>8));
			break;
		case KEY_CTRL_DOWN:
			midSetPosition(midGetPosition()+(mid.ticknum>>8));
			break;
		default:
			if (mcpSetProcessKey(key))
				return 1;
			if (mcpProcessKey)
			{
				int ret=mcpProcessKey(key);
				if (ret==2)
					cpiResetScreen();
				if (ret)
					return 1;
			}
			return 0;
	}
	return 1;
}

static void gmiCloseFile(void)
{
	midStopMidi();
	mid_free(&mid);
}

static int gmiLooped(void)
{
	return !fsLoopMods&&midLooped();
}

static void gmiIdle(void)
{
	midSetLoop(fsLoopMods);
	if (mcpIdle)
		mcpIdle();
}

static int gmiOpenFile(const char *path, struct moduleinfostruct *info, FILE *file)
{
	int i;
	char _modname[NAME_MAX+1];
	char _modext[NAME_MAX+1];
	int retval;

	if (!mcpOpenPlayer)
		return errGen;
	if (!file)
		return errFileOpen;

	_splitpath(path, 0, 0, _modname, _modext);

	strncpy(currentmodname, _modname, _MAX_FNAME);
	_modname[_MAX_FNAME]=0;
	strncpy(currentmodext, _modext, _MAX_EXT);
	_modext[_MAX_EXT]=0;

	fseek(file, 0, SEEK_END);
	i=ftell(file);
	fseek(file, 0, SEEK_SET);
	fprintf(stderr, "loading %s%s (%ik)...\n", currentmodname, currentmodext, i>>10);

	retval=midLoadMidi(&mid, file, (info->modtype==mtMIDd)?MID_DRUMCH16:0);

	if (retval==errOk)
	{
		int sampsize=0;
		fprintf(stderr, "preparing samples (");
		for (i=0; i<mid.sampnum; i++)
			sampsize+=(mid.samples[i].length)<<(!!(mid.samples[i].type&mcpSamp16Bit));
		fprintf(stderr, "%ik)...\n",sampsize>>10);

		if (!mid_loadsamples(&mid))
			retval=errAllocSamp;
	} else {
		mid_free(&mid);
		/*fclose(file);*/
		return errGen;
	}

	plNPChan=cfGetProfileInt2(cfSoundSec, "sound", "midichan", 24, 10);
	if (plNPChan<8)
		plNPChan=8;
	if (plNPChan>MAXCHAN)
		plNPChan=MAXCHAN;
	plNLChan=16;
	plPanType=0;
	modname="";
	composer="";

	plIsEnd=gmiLooped;
	plIdle=gmiIdle;
	plProcessKey=gmiProcessKey;
	plDrawGStrings=gmiDrawGStrings;
	plSetMute=midSetMute;
	plGetLChanSample=midGetChanSample;
	plUseDots(gmiGetDots);
	gmiChanSetup(&mid);
	gmiInsSetup(&mid);

	if (!plCompoMode)
	{
		if (!*modname)
			modname=info->modname;
		if (!*composer)
			composer=info->composer;
	} else
		modname=info->comment;

	mcpNormalize(1);
	if (!midPlayMidi(&mid, plNPChan))
		retval=errPlay;
	plNPChan=mcpNChan;

	plGetRealMasterVolume=mcpGetRealMasterVolume;
	plGetMasterSample=mcpGetMasterSample;
	plGetPChanSample=mcpGetChanSample;

	if (retval)
	{
		mid_free(&mid);
		return retval;
	}

	starttime=dos_clock();
	plPause=0;
	mcpSet(-1, mcpMasterPause, 0);

	return errOk;
}

struct cpifaceplayerstruct gmiPlayer = {gmiOpenFile, gmiCloseFile};
struct linkinfostruct dllextinfo = {.name = "playgmi", .desc = "OpenCP GUSPatch Midi Player (c) 1994-08 Niklas Beisert, Tammo Hinrichs", .ver = DLLVERSION, .size = 0};
