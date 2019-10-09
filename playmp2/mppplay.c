/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * MPPlay interface routines
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
 *  -ss040910   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -added fade pause
 */

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "filesel/pfilesel.h"
#include "filesel/mdb.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "dev/player.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "stuff/sets.h"
#include "stuff/compat.h"
#include "dev/deviplay.h"
#include "cpiface/cpiface.h"
#include "mpplay.h"

#define _MAX_FNAME 8
#define _MAX_EXT 4

/* #define MPEG_DEBUG 1 */

static FILE *mpegfile;
static uint32_t mpeglen;
static uint32_t mpegrate;
static time_t starttime;
static time_t pausetime;
static char currentmodname[_MAX_FNAME+1];
static char currentmodext[_MAX_EXT+1];
static char *modname;
static char *composer;
static int16_t vol;
static int16_t bal;
static int16_t pan;
static char srnd;

static uint32_t amp;
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
		mpegPause(plPause=0);
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
			mpegPause(plPause=1);
			plChanChanged=1;
			mpegSetSpeed(speed);
			return;
		}
	}
	pausefaderelspeed=i;
	mpegSetSpeed(speed*i/64);
}

static void mpegDrawGStrings(uint16_t (*buf)[CONSOLE_MAX_X])
{
	struct mpeginfo inf;
	uint32_t tim, tim2;
	int l;
	int p;

	mpegGetInfo(&inf);

/*
	tim=inf.len*8/(mpeg_Bitrate*1000);*/
	tim=inf.timelen;
	l=(inf.len>>(10/*-inf.stereo-inf.bit16*/)); /* these now refer to offset in file */
	if (!l) l=1;
	p=(inf.pos>>(10/*-inf.stereo-inf.bit16*/)); /* these now refer to offset in file */

	if (plPause)
		tim2=(pausetime-starttime)/DOS_CLK_TCK;
	else
		tim2=(dos_clock()-starttime)/DOS_CLK_TCK;

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

		writestring(buf[1], 0, 0x09, "  pos: ...% / ......k  size: ......k  len: ..:..", 57);
		_writenum(buf[1], 7, 0x0F, p*100/l, 10, 3);
		writenum(buf[1], 43, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[1], 45, 0x0F, ":", 1);
		writenum(buf[1], 46, 0x0F, tim%60, 10, 2, 0);
		writenum(buf[1], 29, 0x0F, l, 10, 6, 1);
		writenum(buf[1], 14, 0x0F, p, 10, 6, 1);

		writestring(buf[2],  0, 0x09, "   mpeg \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................               time: ..:.. ", 80);
		writestring(buf[2],  8, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 16, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 22, 0x0F, modname, 31);
		if (plPause)
			writestring(buf[2], 57, 0x0C, " paused ", 8);
		else {
			writestring(buf[2], 57, 0x09, "kbps: ", 6);
			writenum(buf[2], 63, 0x0F, mpeg_Bitrate, 10, 3, 1);
		}
		writenum(buf[2], 74, 0x0F, (tim2/60)%60, 10, 2, 1);
		writestring(buf[2], 76, 0x0F, ":", 1);
		writenum(buf[2], 77, 0x0F, tim2%60, 10, 2, 0);
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
			tim2=(pausetime-starttime)/DOS_CLK_TCK;
		else
			tim2=(dos_clock()-starttime)/DOS_CLK_TCK;

		writestring(buf[2],  0, 0x09, "      mpeg \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................  composer: ...............................                  time: ..:..    ", 132);
		writestring(buf[2], 11, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 19, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 25, 0x0F, modname, 31);
		writestring(buf[2], 68, 0x0F, composer, 31);
		if (plPause)
			writestring(buf[2], 100, 0x0C, "playback paused", 15);
		else {
			writestring(buf[2], 100, 0x09, "kbps: ", 6);
			writenum(buf[2], 106, 0x0F, mpeg_Bitrate, 10, 3, 1);
		}
		writenum(buf[2], 123, 0x0F, (tim2/60)%60, 10, 2, 1);
		writestring(buf[2], 125, 0x0F, ":", 1);
		writenum(buf[2], 126, 0x0F, tim2%60, 10, 2, 0);
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
	mpegSetAmplify(1024*amp);
	mpegSetVolume(vol, bal, pan, srnd);
	mpegSetSpeed(speed);
	/*  mpegSetMasterReverbChorus(reverb, chorus); */
}

static int mpegProcessKey(uint16_t key)
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
			cpiKeyHelp(KEY_CTRL_DOWN, "Jump forward (small(");
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
			mpegPause(plPause);
			break;
		case KEY_CTRL_UP:
		/* case 0x8D00: //ctrl-up */
			mpegSetPos(mpegGetPos()-mpegrate);
			break;
		case KEY_CTRL_DOWN:
		/* case 0x9100: //ctrl-down */
			mpegSetPos(mpegGetPos()+mpegrate);
			break;
		case '<':
		case KEY_CTRL_LEFT:
		/* case 0x7300: //ctrl-left  */
			{
				uint32_t pos = mpegGetPos();
				uint32_t newpos = pos - (mpeglen>>5);
				if (newpos > pos)
				{
					newpos = 0;
				}
				mpegSetPos(newpos);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
		/* case 0x7400: //ctrl-right */
			{
				uint32_t pos = mpegGetPos();
				uint32_t newpos = pos + (mpeglen>>5);
				if ((newpos < pos) || (newpos > mpeglen))
				{
					newpos = mpeglen - 4;
				}
				mpegSetPos(newpos);
			}
			break;
/*
		case 0x7700: //ctrl-home TODO keys
			mpegSetPos(0);
			break;
*/
		case '-':
			if (vol>=2)
				vol-=2;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case '+':
			if (vol<=62)
				vol+=2;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case '/':
			if ((bal-=4)<-64)
				bal=-64;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case '*':
			if ((bal+=4)>64)
				bal=64;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case ',':
			if ((pan-=4)<-64)
				pan=-64;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case '.':
			if ((pan+=4)>64)
				pan=64;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(2):
			if ((vol-=8)<0)
				vol=0;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(3):
			if ((vol+=8)>64)
				vol=64;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(4):
			mpegSetVolume(vol, bal, pan, srnd=srnd?0:2);
			break;
		case KEY_F(5):
			if ((pan-=16)<-64)
				pan=-64;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(6):
			if ((pan+=16)>64)
				pan=64;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(7):
			if ((bal-=16)<-64)
				bal=-64;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(8):
			if ((bal+=16)>64)
				bal=64;
			mpegSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(9):
		case KEY_F(11):
			if ((speed-=finespeed)<16)
				speed=16;
			mpegSetSpeed(speed);
			break;
		case KEY_F(10):
		case KEY_F(12):
			if ((speed+=finespeed)>2048)
				speed=2048;
			mpegSetSpeed(speed);
			break;
			/* TODO keys
	case 0x5f00: // ctrl f2
    if ((amp-=4)<4)
      amp=4;
    mpegSetAmplify(1024*amp);
    break;
  case 0x6000: // ctrl f3
    if ((amp+=4)>508)
      amp=508;
    mpegSetAmplify(1024*amp);
    break;
  case 0x8900: // ctrl f11
    finespeed=(finespeed==8)?1:8;
    break;
  case 0x6a00:
    normalize();
    break;
  case 0x6900:
    set.pan=pan;
    set.bal=bal;
    set.vol=vol;
    set.speed=speed;
    set.amp=amp;
    set.srnd=srnd;
    break;
  case 0x6b00:
    pan=64;
    bal=0;
    vol=64;
    speed=256;
    amp=64;
    mpegSetVolume(vol, bal, pan, srnd);
    mpegSetSpeed(speed);
    mpegSetAmplify(1024*amp);
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


static int mpegLooped(void)
{
	if (pausefadedirect)
		dopausefade();
	mpegSetLoop(fsLoopMods);
	mpegIdle();
	if (plrIdle)
		plrIdle();
	return !fsLoopMods&&mpegIsLooped();
}


static void mpegCloseFile(void)
{
	mpegClosePlayer();
}

static int mpegOpenFile(const uint32_t dirdbref, struct moduleinfostruct *info, FILE *mpegf)
{
	unsigned char sig[4];
	uint32_t fl;
	int ofs=0;
	struct mpeginfo inf;

	if (!mpegf)
		return -1;

	strncpy(currentmodname, info->name, _MAX_FNAME);
	strncpy(currentmodext, info->name + _MAX_FNAME, _MAX_EXT);

	modname=info->modname;
	composer=info->composer;

	fprintf(stderr, "loading %s%s...\n", currentmodname, currentmodext);

	mpegfile=mpegf;

	if (!(fseek(mpegfile, 0, SEEK_SET))) /* seek will fail on streams */
	{
		if (fread(sig, 4, 1, mpegfile)!=1)
		{
			fprintf(stderr, __FILE__ ": fread failed #1\n");
			return errFileRead;
		}
		fseek(mpegfile, 0, SEEK_SET);
		if (!memcmp(sig, "RIFF", 4))
		{
#ifdef MPEG_DEBUG
			fprintf(stderr, "[mppplay.c]: container RIFF\n");
#endif

			fseek(mpegfile, 12, SEEK_SET);
			fl=0;
			while (1)
			{
				if (fread(sig, 1, 4, mpegfile)!=4)
					break;
#ifdef MPEG_DEBUG
				fprintf(stderr, "[mppplay.c]: chunk: %c%c%c%c\n", sig[0], sig[1], sig[2], sig[3]);
#endif
				if (fread(&fl, sizeof(uint32_t), 1, mpegfile)!=1)
				{
					fprintf(stderr, __FILE__ ": fread failed #3\n");
					return errFileRead;
				}
				fl = uint32_little (fl);
#ifdef MPEG_DEBUG
				fprintf(stderr, "[mppplay.c]: length: %d\n", (int)fl);
#endif
				if (!memcmp(sig, "data", 4))
				{
					ofs=ftell(mpegfile);
					break;
				}
				fseek(mpegfile, fl, SEEK_CUR);
			}
		} else {
			char tag[3];

			if (!memcmp(sig, "ID3", 3))
			{
				char *ofs2;
				char buffer[1024*10];
				char needle[2]={'\377','\175'};
				fseek(mpegfile, 0, SEEK_SET);
				if (fread(buffer, 1024*10, 1, mpegfile)!=1)
				{
					fprintf(stderr, __FILE__ ": fread failed #4\n");
				} else {
					if ((ofs2=(char *)memmem(buffer, 1024*10, needle, 2)))
						ofs=ofs2-buffer;
				}
			}

			fseek(mpegfile, 0, SEEK_END);
			fl=ftell(mpegfile);

			fseek(mpegfile, -128, SEEK_END);
			if (fread(tag, 3, 1, mpegfile)!=1)
			{
				fprintf(stderr, __FILE__ ": fread failed #5\n");
			} else {
				if (!memcmp(tag, "TAG", 3))
					fl-=128;
				fseek(mpegfile, ofs, SEEK_SET);
			}
		}
	} else
		fl=0xffffffff; /* stream */

	plIsEnd=mpegLooped;
	plProcessKey=mpegProcessKey;
	plDrawGStrings=mpegDrawGStrings;
	plGetMasterSample=plrGetMasterSample;
	plGetRealMasterVolume=plrGetRealMasterVolume;

	if (!mpegOpenPlayer(mpegfile, ofs, fl))
		return -1;

	starttime=dos_clock();
	plPause=0;
	normalize();
	pausefadedirect=0;

	mpegGetInfo(&inf);
	mpeglen=inf.len;
	mpegrate=inf.rate;

	return errOk;
}

struct cpifaceplayerstruct mpegPlayer = {mpegOpenFile, mpegCloseFile};
struct linkinfostruct dllextinfo = {.name = "playmp2", .desc = "OpenCP Audio MPEG Player (c) 1994-09 Stian Skjelstad, Niklas Beisert & Tammo Hinrichs", .ver = DLLVERSION, .size = 0};
