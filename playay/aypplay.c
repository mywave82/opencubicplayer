/* OpenCP Module Player
 * copyright (c) '94-'10 Stian Skjelstad <stian@nixia.no>
 *
 * AYPlay interface routines
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
 *  -sss051202   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */


#include "config.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "ayplay.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/player.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "stuff/timer.h"

#define _MAX_FNAME 8
#define _MAX_EXT 4

static time_t starttime;
static time_t pausetime;
static char currentmodname[_MAX_FNAME+1];
static char currentmodext[_MAX_EXT+1];
//static char *modname;
static char *composer;
static int16_t vol;
static int16_t bal;
static int16_t pan;
static int srnd;
static int16_t amp;
static int16_t speed;
static int16_t reverb;
static int16_t chorus;
static char finespeed=8;

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
		ayPause(plPause=0);
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
			ayPause(plPause=1);
			plChanChanged=1;
			aySetSpeed(speed);
			return;
		}
	}
	pausefaderelspeed=i;
	aySetSpeed(speed*i/64);
}

static void normalize(void)
{
	mcpNormalize(0);
	speed=set.speed;
	pan=set.pan;
	bal=set.bal;
	vol=set.vol;
	amp=set.amp;
	srnd=set.srnd;
	reverb=set.reverb;
	chorus=set.chorus;
/*	aySetAmplify(1024*amp);*/
	aySetVolume(vol, bal, pan, srnd);
	aySetSpeed(speed);
/*
	wpSetMasterReverbChorus(reverb, chorus);
*/
}

static void ayCloseFile()
{
	ayClosePlayer();
}

static int ayLooped(void)
{
	if (pausefadedirect)
		dopausefade();
	aySetLoop(fsLoopMods);
	ayIdle();
	if (plrIdle)
		plrIdle();
	return !fsLoopMods&&ayIsLooped();
}

static void ayDrawGStrings(uint16_t (*buf)[CONSOLE_MAX_X])
{
	long tim;
	struct ayinfo globinfo;

	ayGetInfo(&globinfo);

	if (plPause)
		tim=(pausetime-starttime)/DOS_CLK_TCK;
	else
		tim=(dos_clock()-starttime)/DOS_CLK_TCK;

	if (plScrWidth<128)
	{
		memset(buf[0]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[1]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[2]+80, 0, (plScrWidth-80)*sizeof(uint16_t));

		writestring(buf[0], 0, 0x09, " vol: \372\372\372\372\372\372\372\372 ", 15);
		writestring(buf[0], 15, 0x09, " srnd: \372  pan: l\372\372\372m\372\372\372r  bal: l\372\372\372m\372\372\372r ", 41);
		writestring(buf[0], 56, 0x09, " spd: ---% \x1D ptch: ---% ", 24);
		writestring(buf[0], 6, 0x0F, "\376\376\376\376\376\376\376\376", (vol+4)>>3);
		writestring(buf[0], 22, 0x0F, srnd?"x":"o", 1);
		if (((pan+70)>>4)==4)
			writestring(buf[0], 34, 0x0F, "m", 1);
		else {
			writestring(buf[0], 30+((pan+70)>>4), 0x0F, "r", 1);
			writestring(buf[0], 38-((pan+70)>>4), 0x0F, "l", 1);
		}
		writestring(buf[0], 46+((bal+70)>>4), 0x0F, "I", 1);
		_writenum(buf[0], 62, 0x0F, speed*100/256, 10, 3);
		_writenum(buf[0], 75, 0x0F, speed*100/256, 10, 3);


		writestring(buf[1],  0, 0x09," song .. of ..                                 cpu: ...% amp: ...% filter: ...  ",80);
		writenum(buf[1],  6, 0x0F, globinfo.track, 16, 2, 0);
		writenum(buf[1], 12, 0x0F, globinfo.numtracks, 16, 2, 0);
		_writenum(buf[1], 52, 0x0F, tmGetCpuUsage(), 10, 3);
		_writenum(buf[1], 62, 0x0F, amp*100/64, 10, 3);
/*
		writestring(buf[1], 75, 0x0F, sidpGetFilter()?"on":"off", 3);
*/
		writestring(buf[1], 75, 0x0F, "off", 3);

		writestring(buf[2],  0, 0x09, " file \372\372\372\372\372\372\372\372.\372\372\372: .............................................  time: ..:.. ", 80);
		writestring(buf[2],  6, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 14, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 20, 0x0F, globinfo.trackname, 45);
		if (plPause)
			writestring(buf[2], 58, 0x0C, "paused", 6);
		writenum(buf[2], 73, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 75, 0x0F, ":", 1);
		writenum(buf[2], 76, 0x0F, tim%60, 10, 2, 0);

	} else {
		memset(buf[0]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[1]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[2]+128, 0, (plScrWidth-128)*sizeof(uint16_t));

		writestring(buf[0], 0, 0x09, "    volume: \372\372\372\372\372\372\372\372\372\372\372\372\372\372\372\372  ", 30);
		writestring(buf[0], 30, 0x09, " surround: \372   panning: l\372\372\372\372\372\372\372m\372\372\372\372\372\372\372r   balance: l\372\372\372\372\372\372\372m\372\372\372\372\372\372\372r  ", 72);
		writestring(buf[0], 102, 0x09,  " speed: ---% \x1D pitch: ---%    ", 30);
		writestring(buf[0], 12, 0x0F, "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376", (vol+2)>>2);
		writestring(buf[0], 41, 0x0F, srnd?"x":"o", 1);
		if (((pan+68)>>3)==8)
			writestring(buf[0], 62, 0x0F, "m", 1);
		else {
			writestring(buf[0], 54+((pan+68)>>3), 0x0F, "r", 1);
			writestring(buf[0], 70-((pan+68)>>3), 0x0F, "l", 1);
		}
		writestring(buf[0], 83+((bal+68)>>3), 0x0F, "I", 1);
		_writenum(buf[0], 110, 0x0F, speed*100/256, 10, 3);
		_writenum(buf[0], 124, 0x0F, speed*100/256, 10, 3);

		writestring(buf[1],  0, 0x09,"    song .. of ..                                   cpu: ...%",132);
		writenum(buf[1],  9, 0x0F, globinfo.track, 16, 2, 0);
		writenum(buf[1], 15, 0x0F, globinfo.numtracks, 16, 2, 0);

		_writenum(buf[1], 57, 0x0F, tmGetCpuUsage(), 10, 3);
		writestring(buf[1], 60, 0x0F, "%", 1);

		writestring(buf[1], 61, 0x00, "", 128-61);
		writestring(buf[1], 92, 0x09, "   amplification: ...%  filter: ...     ", 40);
		_writenum(buf[1], 110, 0x0F, amp*100/64, 10, 3);
		writestring(buf[1], 124, 0x0F, "off", 3); /* <80 also misses this */

		writestring(buf[2],  0, 0x09, "    file \372\372\372\372\372\372\372\372.\372\372\372: ........................................  composer: ........................................  time: ..:..   ", 132);
		writestring(buf[2],  9, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 17, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 23, 0x0F, globinfo.trackname, 40);
		writestring(buf[2], 75, 0x0F, composer, 40);
		if (plPause)
			writestring(buf[2], 100, 0x0C, "playback paused", 15);
		writenum(buf[2], 123, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 125, 0x0F, ":", 1);
		writenum(buf[2], 126, 0x0F, tim%60, 10, 2, 0);
	}

}

static int ayProcessKey(uint16_t key)
{
	int csg;
	struct ayinfo globinfo;
	ayGetInfo(&globinfo);

	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Jump to previous track");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump to previous track");
			cpiKeyHelp('>', "Jump to next track");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Jump to next track");
			cpiKeyHelp('-', "Decrease volume (small)");
			cpiKeyHelp('+', "Increase volume (small)");
			cpiKeyHelp('/', "Move balance left (small)");
			cpiKeyHelp('*', "Move balance right (small)");
			cpiKeyHelp(',', "Move panning against normal (small)");
			cpiKeyHelp('.', "Move panning against reverse (small)");
			cpiKeyHelp(KEY_F(2), "Decrease volume");
			cpiKeyHelp(KEY_F(3), "Increase volume");
			cpiKeyHelp(KEY_F(4), "Toggle surround on/off");
			cpiKeyHelp(KEY_F(5), "Move panning against normal");
			cpiKeyHelp(KEY_F(6), "Move panning against reverse");
			cpiKeyHelp(KEY_F(7), "Move balance left");
			cpiKeyHelp(KEY_F(8), "Move balance right");
			cpiKeyHelp(KEY_F(9), "Decrease pitch speed");
			cpiKeyHelp(KEY_F(11), "Decrease pitch speed");
			cpiKeyHelp(KEY_F(10), "Increase pitch speed");
			cpiKeyHelp(KEY_F(12), "Increase pitch speed");
			if (plrProcessKey)
				plrProcessKey(key);
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
			plPause=!plPause;
			ayPause(plPause);
			break;
		case '<':
		case KEY_CTRL_LEFT: /* curses.h can't do these */
		/* case 0x7300: //ctrl-left */
			csg=globinfo.track-1;
			if (csg)
			{
				ayStartSong(csg);
				starttime=dos_clock();
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT: /* curses.h can't do these */
		/* case 0x7400: //ctrl-right*/
			csg=globinfo.track+1;
			if (csg<=globinfo.numtracks)
			{
				ayStartSong(csg);
				starttime=dos_clock();
			}
			break;
		case '-':
			if (vol>=2)
				vol-=2;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case '+':
			if (vol<=62)
				vol+=2;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case '/':
			if ((bal-=4)<-64)
				bal=-64;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case '*':
			if ((bal+=4)>64)
				bal=64;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case ',':
			if ((pan-=4)<-64)
				pan=-64;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case '.':
			if ((pan+=4)>64)
				pan=64;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(2):
			if ((vol-=8)<0)
				vol=0;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(3):
			if ((vol+=8)>64)
				vol=64;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(4):
			aySetVolume(vol, bal, pan, srnd=srnd?0:2);
			break;
		case KEY_F(5):
			if ((pan-=16)<-64)
				pan=-64;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(6):
			if ((pan+=16)>64)
				pan=64;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(7):
			if ((bal-=16)<-64)
				bal=-64;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(8):
			if ((bal+=16)>64)
				bal=64;
			aySetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(9):
		case KEY_F(11):
			if ((speed-=finespeed)<16)
				speed=16;
			aySetSpeed(speed);
			break;
		case KEY_F(10):
		case KEY_F(12):
			if ((speed+=finespeed)>2048)
				speed=2048;
			aySetSpeed(speed);
			break;

		default:
			if (plrProcessKey)
			{
				int ret=plrProcessKey(key);
				if (ret==2)
					cpiResetScreen();
				if (ret)
					return 1;
			}
			return 0;
	}
	return 1;

}

static int ayOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
{
/*
	struct ayinfo ay;
*/
	if (!file)
		return -1;

#warning TODO replace currentmodname currentmodext
	//strncpy(currentmodname, info->name, _MAX_FNAME);
	//strncpy(currentmodext, info->name + _MAX_FNAME, _MAX_EXT);

	//modname=info->modname;
	composer=info->composer;

	fprintf(stderr, "Loading %s%s...\n", currentmodname, currentmodext);

	plIsEnd=ayLooped;
	plProcessKey=ayProcessKey;
	plDrawGStrings=ayDrawGStrings;
	plSetMute=aySetMute;
	plGetMasterSample=plrGetMasterSample;
	plGetRealMasterVolume=plrGetRealMasterVolume;

	plNLChan=6;
	ayChanSetup();

	if (!ayOpenPlayer(file))
	{
#ifdef INITCLOSE_DEBUG
		fprintf(stderr, "ayOpenPlayer FAILED\n");
#endif
		return -1;
	}

	starttime=dos_clock();
	plPause=0;
	normalize();
	pausefadedirect=0;


/*
	ayGetInfo(&ay);
	aylen=inf.len;
	ayrate=inf.rate;
*/

	return errOk;
}

struct cpifaceplayerstruct ayPlayer = {ayOpenFile, ayCloseFile};
struct linkinfostruct dllextinfo = {.name = "playay", .desc = "OpenCP aylet Player (c) 2005-09 Russell Marks, Ian Collier & Stian Skjelstad", .ver = DLLVERSION, .size = 0};
