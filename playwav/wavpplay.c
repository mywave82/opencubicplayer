/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * WAVPlay interface routines
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
 *  -ryg981219 Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -made max amplification 793% (as in module players)
 */

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "types.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/poutput.h"
#include "boot/plinkman.h"
#include "dev/player.h"
#include "boot/psetting.h"
#include "wave.h"
#include "stuff/sets.h"
#include "dev/deviplay.h"
#include "cpiface/cpiface.h"
#include "stuff/compat.h"
#include "stuff/err.h"

#define _MAX_FNAME 8
#define _MAX_EXT 4

extern char plPause;

static FILE *wavefile;
static unsigned long wavelen;
static unsigned long waverate;

static time_t starttime;
static time_t pausetime;
static char currentmodname[_MAX_FNAME+1];
static char currentmodext[_MAX_EXT+1];
static char *modname;
static char *composer;
static short vol;
static short bal;
static short pan;
static char srnd;
static long amp;
static short speed;
static short reverb;
static short chorus;
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
		wpPause(plPause=0);
		pausefadedirect=1;
	} else
		pausefadedirect=-1;
}

static void dopausefade(void)
{
	int16_t i;
	if (pausefadedirect>0)
	{
		i=(dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i<0)
			i=0;
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
			wpPause(plPause=1);
			plChanChanged=1;
			wpSetSpeed(speed);
			return;
		}
	}
	pausefaderelspeed=i;
	wpSetSpeed(speed*i/64);
}
static void wavDrawGStrings(unsigned short (*buf)[CONSOLE_MAX_X])
{
	struct waveinfo inf;
	long tim;
	int l;
	int p;

	wpGetInfo(&inf);
	tim=inf.len/inf.rate;

	if (plScrWidth<128)
	{
		memset(buf[0]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[1]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[2]+80, 0, (plScrWidth-80)*sizeof(uint16_t));

		writestring(buf[0], 0, 0x09, " vol: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 15);
		writestring(buf[0], 15, 0x09, " srnd: \xfa  pan: l\xfa\xfa\xfam\xfa\xfa\xfar  bal: l\xfa\xfa\xfam\xfa\xfa\xfar ", 41);
		writestring(buf[0], 56, 0x09, " spd: ---% \x1D ptch: ---% ", 24);
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
		_writenum(buf[0], 75, 0x0F, speed*100/256, 10, 3);

		writestring(buf[1], 57, 0x09, "amp: ...% filter: ...  ", 23);
		_writenum(buf[1], 62, 0x0F, amp*100/64, 10, 3);
		writestring(buf[1], 75, 0x0F, "off", 3);

		l=(inf.len>>(10-inf.stereo-inf.bit16));
		p=(inf.pos>>(10-inf.stereo-inf.bit16));
		writestring(buf[1], 0, 0x09, "  pos: ...% / ......k  size: ......k  len: ..:..", 57);
		_writenum(buf[1], 7, 0x0F, p*100/l, 10, 3);
		writenum(buf[1], 43, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[1], 45, 0x0F, ":", 1);
		writenum(buf[1], 46, 0x0F, tim%60, 10, 2, 0);
		writenum(buf[1], 29, 0x0F, l, 10, 6, 1);
		writenum(buf[1], 14, 0x0F, p, 10, 6, 1);

		if (plPause)
			tim=(pausetime-starttime);
		else
			 tim=(time(NULL)-starttime);

		writestring(buf[2],  0, 0x09, "   wave \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................               time: ..:.. ", 80);
		writestring(buf[2],  8, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 16, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 22, 0x0F, modname, 31);
		if (plPause)
			writestring(buf[2], 58, 0x0C, "paused", 6);
		writenum(buf[2], 74, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 76, 0x0F, ":", 1);
		writenum(buf[2], 77, 0x0F, tim%60, 10, 2, 0);
	} else {
		memset(buf[0]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[1]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[2]+128, 0, (plScrWidth-128)*sizeof(uint16_t));

		writestring(buf[0], 0, 0x09, "    volume: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa  ", 30);
		writestring(buf[0], 30, 0x09, " surround: \xfa   panning: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar   balance: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar  ", 72);
		writestring(buf[0], 102, 0x09,  " speed: ---% \x1D pitch: ---%    ", 30);
		writestring(buf[0], 12, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+2)>>2);
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

		l=(inf.len>>(10-inf.stereo-inf.bit16));
		p=(inf.pos>>(10-inf.stereo-inf.bit16));
		writestring(buf[1], 0, 0x09, "    position: ...% / ......k  size: ......k  length: ..:..  opt: .....Hz, .. bit, ......", 92);
		_writenum(buf[1], 14, 0x0F, p*100/l, 10, 3);
		writenum(buf[1], 53, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[1], 55, 0x0F, ":", 1);
		writenum(buf[1], 56, 0x0F, tim%60, 10, 2, 0);
		writenum(buf[1], 36, 0x0F, l, 10, 6, 1);
		writenum(buf[1], 21, 0x0F, p, 10, 6, 1);
		writenum(buf[1], 65, 0x0F, inf.rate, 10, 5, 1);
		writenum(buf[1], 74, 0x0F, 8<<inf.bit16, 10, 2, 1);
		writestring(buf[1], 82, 0x0F, inf.stereo?"stereo":"mono", 6);

		writestring(buf[1], 92, 0x09, "   amplification: ...%  filter: ...     ", 40);
		_writenum(buf[1], 110, 0x0F, amp*100/64, 10, 3);
		writestring(buf[1], 124, 0x0F, "off", 3);

		if (plPause)
			tim=(pausetime-starttime);
		else
			tim=(time(NULL)-starttime);

		writestring(buf[2],  0, 0x09, "      wave \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................  composer: ...............................                  time: ..:..    ", 132);
		writestring(buf[2], 11, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 19, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 25, 0x0F, modname, 31);
		writestring(buf[2], 68, 0x0F, composer, 31);
		if (plPause)
			writestring(buf[2], 100, 0x0C, "playback paused", 15);
		writenum(buf[2], 123, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 125, 0x0F, ":", 1);
		writenum(buf[2], 126, 0x0F, tim%60, 10, 2, 0);
	}
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
	wpSetAmplify(1024*amp);
	wpSetVolume(vol, bal, pan, srnd);
	wpSetSpeed(speed);
/*
	wpSetMasterReverbChorus(reverb, chorus);
*/
}

static void wavCloseFile()
{
	wpClosePlayer();
/*
	fclose(wavefile);
*/
}

static int wavProcessKey(unsigned short key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Jump back (big)");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump back (big)");
			cpiKeyHelp('>', "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_UP, "Jump back (small)");
			cpiKeyHelp(KEY_CTRL_DOWN, "Jump forward (small)");
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
				starttime=starttime+time(NULL)-pausetime;
			else
				pausetime=time(NULL);
			plPause=!plPause;
			wpPause(plPause);
			break;
		case KEY_CTRL_UP:
		/* case 0x8D00: //ctrl-up */
			wpSetPos(wpGetPos()-waverate);
			break;
		case KEY_CTRL_DOWN:
		/* case 0x9100: //ctrl-down */
			wpSetPos(wpGetPos()+waverate);
			break;
		case '<':
		case KEY_CTRL_LEFT:
		/* case 0x7300: //ctrl-left */
			{
				uint32_t pos = wpGetPos();
				uint32_t newpos = pos -(wavelen>>5);
				if (newpos > pos)
				{
					newpos = 0;
				}
				wpSetPos(newpos);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
		/* case 0x7400: //ctrl-right */
			{
				uint32_t pos = wpGetPos();
				uint32_t newpos = pos + (wavelen>>5);
				if ((newpos < pos) || (newpos > wavelen)) /* catch both wrap around (not likely), and overshots */
				{
					newpos = wavelen - 4;
				}
				wpSetPos(newpos);
			}
			break;
/*
		case 0x7700: //ctrl-home TODO keys
			wpSetPos(0);
			break;
*/
		case '-':
			if (vol>=2)
				vol-=2;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case '+':
			if (vol<=62)
				vol+=2;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case '/':
			if ((bal-=4)<-64)
				bal=-64;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case '*':
			if ((bal+=4)>64)
				bal=64;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case ',':
			if ((pan-=4)<-64)
				pan=-64;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case '.':
			if ((pan+=4)>64)
				pan=64;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(2): /*f2*/
			if ((vol-=8)<0)
				vol=0;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(3): /*f3*/
			if ((vol+=8)>64)
				vol=64;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(4): /*f4*/
			wpSetVolume(vol, bal, pan, srnd=srnd?0:2);
			break;
		case KEY_F(5): /*f5*/
			if ((pan-=16)<-64)
				pan=-64;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(6): /*f6*/
			if ((pan+=16)>64)
				pan=64;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(7): /*f7*/
			if ((bal-=16)<-64)
				bal=-64;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(8): /*f8*/
			if ((bal+=16)>64)
				bal=64;
			wpSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(9): /*f9*/
		case KEY_F(11): /*f11*/
			if ((speed-=finespeed)<16)
				speed=16;
			wpSetSpeed(speed);
			break;
		case KEY_F(10): /*f10*/
		case KEY_F(12): /*f12*/
			if ((speed+=finespeed)>2048)
				speed=2048;
			wpSetSpeed(speed);
			break;
/*
		case 0x5f00: // ctrl f2 TODO keys
			if ((amp-=4)<4)
				amp=4;
			wpSetAmplify(1024*amp);
			break;
		case 0x6000: // ctrl f3 TODO keys
			if ((amp+=4)>508)
				amp=508;
			wpSetAmplify(1024*amp);
			break;
		case 0x8900: // ctrl f11 TODO keys
			finespeed=(finespeed==8)?1:8;
			break;
		case 0x6a00: // alt-f3 TODO keys
			normalize();
			break;
		case 0x6900: // alt-f2 TODO keys
			set.pan=pan;
			set.bal=bal;
			set.vol=vol;
			set.speed=speed;
			set.amp=amp;
			set.srnd=srnd;
			break;
		case 0x6b00: // alt-f4 TODO keys
			pan=64;
			bal=0;
			vol=64;

			speed=256;
			amp=64;
			wpSetVolume(vol, bal, pan, srnd);
			wpSetSpeed(speed);
			wpSetAmplify(1024*amp);
			break;*/
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


static int wavLooped(void)
{
	if (pausefadedirect)
		dopausefade();
	wpSetLoop(fsLoopMods);
	wpIdle();
	if (plrIdle)
		plrIdle();
	return !fsLoopMods&&wpLooped();
}


static int wavOpenFile(const char *path, struct moduleinfostruct *info, FILE *wavf)
{
	struct waveinfo inf;
	char _modname[NAME_MAX+1];
	char _modext[NAME_MAX+1];

	if (!wavf)
		return -1;

	_splitpath(path, 0, 0, _modname, _modext);

	strncpy(currentmodname, _modname, _MAX_FNAME);
	_modname[_MAX_FNAME]=0;
	strncpy(currentmodext, _modext, _MAX_EXT);
	_modext[_MAX_EXT]=0;

	modname=info->modname;
	composer=info->composer;

	fprintf(stderr, "preloading %s%s...\n", currentmodname, currentmodext);

	wavefile=wavf;

	plIsEnd=wavLooped;
	plProcessKey=wavProcessKey;
	plDrawGStrings=wavDrawGStrings;
	plGetMasterSample=plrGetMasterSample;
	plGetRealMasterVolume=plrGetRealMasterVolume;

	if (!wpOpenPlayer(wavefile))
	{
#ifdef INITCLOSE_DEBUG
		fprintf(stderr, "wpOpenPlayer FAILED\n");
#endif
		return -1;
	}

	starttime=time(NULL);
	plPause=0;
	normalize();
	pausefadedirect=0;

	wpGetInfo(&inf);
	wavelen=inf.len;
	waverate=inf.rate;

	return errOk;
}

struct cpifaceplayerstruct wavPlayer = {wavOpenFile, wavCloseFile};
struct linkinfostruct dllextinfo = {.name = "playwav", .desc = "OpenCP Wave Player (c) 1994-09 Niklas Beisert, Tammo Hinrichs", .ver = DLLVERSION, .size = 0};
