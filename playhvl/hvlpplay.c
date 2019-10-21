/* OpenCP Module Player
 * copyright (c) 2019 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * HVLPlay interface routines
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
 */

#include "config.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "types.h"
#include "boot/plinkman.h"
//#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "dev/deviplay.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "hvlpchan.h"
#include "hvlpinst.h"
#include "hvlplay.h"
#include "hvlptrak.h"
#include "player.h"

#define _MAX_FNAME 8
#define _MAX_EXT 4

static time_t starttime;
static time_t pausetime;

static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;

static int16_t vol;
static int16_t bal;
static int16_t pan;
static char srnd;
static uint32_t amp;
static int16_t speed, pitch;
static int     splock=1;
static int16_t reverb;
static int16_t chorus;
static const char finespeed=8;
static const char finepitch=8;

static char currentmodname[_MAX_FNAME+1];
static char currentmodext[_MAX_EXT+1];

static void startpausefade (void)
{
	if (plPause)
	{
		starttime = starttime + dos_clock () - pausetime;
		hvlSetPausePitch (0x00010000 * 1 / 64);
	}

	if (pausefadedirect)
	{
		if (pausefadedirect < 0)
		{
			plPause = 1;
		}
		pausefadestart = 2 * dos_clock () - DOS_CLK_TCK - pausefadestart;
	} else {
		pausefadestart = dos_clock ();
	}

	if (plPause)
	{
		plChanChanged = 1;
		hvlPause ( plPause = 0 );
		pausefadedirect = 1;
	} else
		pausefadedirect = -1;
}

static void dopausefade (void)
{
	int16_t i;
	if (pausefadedirect>0)
	{
		i=(dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i<0)
			i=1;
		if (i>=64)
		{
			i=64;
			pausefadedirect=0;
		}
	} else {
		i=64-(dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i>=64)
			i=64;
		if (i<=0)
		{
			i=0;
			pausefadedirect=0;
			pausetime=dos_clock();
			hvlPause(plPause=1);
			plChanChanged=1;
			//hvlSetPausePitch(0x00010000);
			return;
		}
	}
	pausefaderelspeed=i;
	hvlSetPausePitch (0x00010000 * i / 64);
}

static void hvlDrawGStrings (uint16_t (*buf)[CONSOLE_MAX_X])
{
	int     row,     rows;
	int   order,   orders;
	int subsong, subsongs;
	int tempo;
	int speedmult;

	int32_t tim;

	hvlGetStats (&row, &rows, &order, &orders, &subsong, &subsongs, &tempo, &speedmult);

	if (plPause)
		tim=(pausetime-starttime)/DOS_CLK_TCK;
	else
		tim=(dos_clock()-starttime)/DOS_CLK_TCK;

	if (plScrWidth<128)
	{
		memset(buf[0]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[1]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[2]+80, 0, (plScrWidth-80)*sizeof(uint16_t));

		/* basically cloning mcpDrawGStrings */
		writestring(buf[0], 0, 0x09, " vol: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 15);
		writestring(buf[0], 15, 0x09, " srnd: \xfa  pan: l\xfa\xfa\xfam\xfa\xfa\xfar  bal: l\xfa\xfa\xfam\xfa\xfa\xfar ", 41);
		writestring(buf[0], 56, 0x09, " spd: ---%   ptch: ---% ", 24);
		if (splock)
		{
			writestring (buf[0], 67, 0x09, "\x1D", 1);
		}
		writestring(buf[0], 6, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+4)>>3);
		writestring(buf[0], 22, 0x0F, srnd?"x":"o", 1);
		if (((pan+70)>>4)==4)
			writestring(buf[0], 34, 0x0F, "m", 1);
		else {
			writestring(buf[0], 30+((pan+70)>>4), 0x0F, "r", 1);
			writestring(buf[0], 38-((pan+70)>>4), 0x0F, "l", 1);
		}
		writestring(buf[0], 46+((bal+70)>>4), 0x0F, "I", 1);
		_writenum(buf[0], 62, 0x0F, speed*100/256, 10, 3);
		_writenum(buf[0], 75, 0x0F, pitch*100/256, 10, 3);

		writestring(buf[1],  0, 0x09, " row: ../..  ord: ..../....  speed: ..  bpm: ...  subsong: ../..    amp: ...%   ", 80);
                writenum(buf[1],  6, 0x0F, row, 16, 2, 0);
		writenum(buf[1],  9, 0x0F, rows-1, 16, 2, 0);
		writenum(buf[1], 18, 0x0F, order, 16, 4, 0);
		writenum(buf[1], 23, 0x0F, orders-1, 16, 4, 0);
		writenum(buf[1], 36, 0x0F, tempo, 16, 2, 1);
		writenum(buf[1], 45, 0x0F, 125*speedmult*4/tempo, 10, 3, 1); // 125, is assumed tempo is 4
		writenum(buf[1], 59, 0x0F, subsong, 10, 2, 0);
		writenum(buf[1], 62, 0x0F, subsongs, 10, 2, 0);
		_writenum(buf[1], 73, 0x0F, amp*100/64, 10, 3);

		writestring(buf[2],  0, 0x09, "    HVL \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ............................................  time: ..:.. ", 80);
		writestring(buf[2],  8, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 16, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 22, 0x0F, current_hvl_tune?current_hvl_tune->ht_Name:"", 44);
		if (plPause)
		{
			writestring(buf[2], 57, 0x0C, " paused ", 8);
		}
		writenum(buf[2], 74, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 76, 0x0F, ":", 1);
		writenum(buf[2], 77, 0x0F, tim%60, 10, 2, 0);
	} else {
		memset(buf[0]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[1]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[2]+128, 0, (plScrWidth-128)*sizeof(uint16_t));

		/* basically cloning mcpDrawGStrings */
		writestring(buf[0], 0, 0x09, "    volume: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa  ", 30);
		writestring(buf[0], 30, 0x09, " surround: \xfa   panning: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar   balance: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar  ", 72);
		writestring(buf[0], 102, 0x09,  " speed: ---%   pitch: ---%    ", 30);
		if (splock)
		{
			writestring (buf[0], 115, 0x09, "\x1D", 1);
		}
		writestring(buf[0], 12, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+2)>>2);
		writestring(buf[0], 41, 0x0F, srnd?"x":"o", 1);
		if (((pan+68)>>3)==8)
			writestring(buf[0], 62, 0x0F, "m", 1);
		else {
			writestring(buf[0], 54+((pan+68)>>3), 0x0F, "r", 1);
			writestring(buf[0], 70-((pan+68)>>3), 0x0F, "l", 1);
		}
		writestring(buf[0], 83+((bal+68)>>3), 0x0F, "I", 1);
		_writenum(buf[0], 110, 0x0F, speed*100/256, 10, 3);
		_writenum(buf[0], 124, 0x0F, pitch*100/256, 10, 3);

		writestring(buf[1],  0, 0x09, "     row: ../..  ord: ..../....  speed: ..  tempo: ...  subsong: ../..                                   amp: ...%  filter: ... ", 128);
                writenum(buf[1], 10, 0x0F, row, 16, 2, 0);
		writenum(buf[1], 13, 0x0F, rows-1, 16, 2, 0);
		writenum(buf[1], 22, 0x0F, order, 16, 4, 0);
		writenum(buf[1], 27, 0x0F, orders-1, 16, 4, 0);
		writenum(buf[1], 40, 0x0F, tempo, 16, 2, 1);
		writenum(buf[1], 51, 0x0F, 125*speedmult*4/tempo, 10, 3, 1); // 125 is assumed that tempo is 4
		writenum(buf[1], 65, 0x0F, subsong, 10, 2, 0);
		writenum(buf[1], 68, 0x0F, subsongs, 10, 2, 0);
		_writenum(buf[1], 110, 0x0F, amp*100/64, 10, 3);
		writestring(buf[1], 124, 0x0F, "off", 3);

		writestring(buf[2],  0, 0x09, "       HVL \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ......................................................................................... time: ..:.. ", 128);
		writestring(buf[2], 11, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 19, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 25, 0x0F, current_hvl_tune?current_hvl_tune->ht_Name:"", 89);
		if (plPause)
		{
			writestring(buf[2], 100, 0x0C, "playback paused", 15);
		}
		writenum(buf[2], 121, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 123, 0x0F, ":", 1);
		writenum(buf[2], 124, 0x0F, tim%60, 10, 2, 0);
	}
}


static void normalize(void)
{
	speed  = set.speed;
	pitch  = set.pitch;
	pan    = set.pan;
	bal    = set.bal;
	vol    = set.vol;
	amp    = set.amp;
	srnd   = set.srnd;
	reverb = set.reverb;
	chorus = set.chorus;
	hvlSetAmplify (1024*amp);
	hvlSetVolume (vol, bal, pan, srnd);
	hvlSetSpeed (speed);
	hvlSetPitch (pitch);
}

static int hvlProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Previous sub-song");
			cpiKeyHelp('>', "Next sub-song");
			cpiKeyHelp(KEY_F(2), "Decrease volume");
			cpiKeyHelp(KEY_F(3), "Increase volume");
			cpiKeyHelp(KEY_F(4), "Toggle surround on/off");
			cpiKeyHelp(KEY_F(5), "Move panning against normal");
			cpiKeyHelp(KEY_F(6), "Move panning against reverse");
			cpiKeyHelp(KEY_F(7), "Move balance left");
			cpiKeyHelp(KEY_F(8), "Move balance right");
			cpiKeyHelp(KEY_F(9), "Decrease song speed");
			cpiKeyHelp(KEY_F(11), "Decrease song speed");
			cpiKeyHelp(KEY_F(10), "Increase pitch speed");
			cpiKeyHelp(KEY_F(12), "Increase pitch speed");
			cpiKeyHelp('\\', "Toggle lock between pitch/speed");
			return 0;
		case 'p': case 'P':
			startpausefade();
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			if (plPause)
			{
				starttime=starttime+dos_clock()-pausetime;
				hvlSetPausePitch (0x00010000);
			} else {
				pausetime=dos_clock();
			}
			plPause=!plPause;
			hvlPause(plPause);
			plChanChanged=1; /* ? */
			break;
/*
		case 0x7700: //ctrl-home TODO keys
			itpInstClear();
			itplayer.setpos(0, 0);
			if (plPause)
				starttime=pausetime;
			else
				starttime=dos_clock();
			break;
*/
		case '<':
			hvlPrevSubSong();	
			break;
		case '>':
			hvlNextSubSong();
			break;
		case KEY_F(2):
			if ((vol-=8)<0)
				vol=0;
			hvlSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(3):
			if ((vol+=8)>64)
				vol=64;
			hvlSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(4):
			hvlSetVolume(vol, bal, pan, srnd=srnd?0:2);
			break;
		case KEY_F(5):
			if ((pan-=16)<-64)
				pan=-64;
			hvlSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(6):
			if ((pan+=16)>64)
				pan=64;
			hvlSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(7):
			if ((bal-=16)<-64)
				bal=-64;
			hvlSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(8):
			if ((bal+=16)>64)
				bal=64;
			hvlSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(9):
			if ((speed-=finespeed)<16)
			{
				speed=16;
			}
			hvlSetSpeed(speed);
			if (splock)
			{
				pitch = speed;
				hvlSetPitch (pitch);
			}
			break;
		case KEY_F(10):
			if ((speed+=finespeed)>2048)
			{
				speed=2048;
			}
			hvlSetSpeed (speed);
			if (splock)
			{
				pitch = speed;
				hvlSetPitch (pitch);
			}
			break;
		case KEY_F(11):
			if ((pitch-=finepitch)<16)
			{
				pitch=16;
			}
			hvlSetPitch(pitch);
			if (splock)
			{
				speed = pitch;
				hvlSetSpeed (speed);
			}
			break;
		case KEY_F(12):
			if ((pitch+=finepitch)>2048)
				pitch=2048;
			hvlSetPitch(pitch);
			if (splock)
			{
				speed = pitch;
				hvlSetSpeed (speed);
			}
			break;
		case '\\':
			splock^=1;
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

static int hvlIsLooped(void)
{
	if (pausefadedirect)
	{
		dopausefade();
	}
	hvlSetLoop(fsLoopMods);
	hvlIdle();
	if (plrIdle)
	{
		plrIdle();
	}
	return !fsLoopMods&&hvlLooped();
}

static void hvlCloseFile(void)
{
	hvlClosePlayer();
}

static int hvlOpenFile(const uint32_t dirdbref, struct moduleinfostruct *info, FILE *file)
{
	uint8_t *filebuf;
	size_t   filelen;

	if (!file)
	{
		return errFileOpen;
	}

	strncpy(currentmodname, info->name, _MAX_FNAME);
	strncpy(currentmodext, info->name + _MAX_FNAME, _MAX_EXT);

	fseek (file, 0, SEEK_END);
	filelen = ftell (file);
	fseek (file, 0, SEEK_SET);

	if (filelen < 14)
	{
		fprintf (stderr, "hvlOpenFile: file too small\n");
		return errGen;
	}

	filebuf = malloc (filelen);
	if (!filebuf)
	{
		fprintf (stderr, "hvlOpenFile: malloc(%ld) failed\n", (long)filelen);
		return errAllocMem;
	}
	if (fread (filebuf, filelen, 1, file) != 1)
	{
		fprintf (stderr, "hvlOpenFile: error reading file: %s\n", strerror(errno));
		free (filebuf);
		return errGen;	
	}

	hvlOpenPlayer (filebuf, filelen);
	free (filebuf);
	if (!current_hvl_tune)
	{
		return errGen;
	}

	plIsEnd=hvlIsLooped;
	plProcessKey=hvlProcessKey;
	plDrawGStrings=hvlDrawGStrings;
	plGetMasterSample=plrGetMasterSample;
	plGetRealMasterVolume=plrGetRealMasterVolume;

	starttime=dos_clock();
	plPause=0;
	normalize();
	pausefadedirect=0;
	plNLChan=ht->ht_Channels;
	plIdle=hvlIdle;
	plSetMute=hvlMute;
#if 0
	plGetLChanSample=mpGetChanSample;
	plUseDots(gmdGetDots);
#endif
	hvlInstSetup ();
	hvlChanSetup ();
	hvlTrkSetup ();
#if 0
	plGetPChanSample=mcpGetChanSample;
#endif
	return errOk;
}

struct cpifaceplayerstruct hvlPlayer = {hvlOpenFile, hvlCloseFile};
struct linkinfostruct dllextinfo = {.name = "playhvl", .desc = "OpenCP HVL Player (c) 2019 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
