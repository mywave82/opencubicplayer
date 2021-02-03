/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '11-'20 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
 *
 * SIDPlay interface routines
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
 *  -kb980717  Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 *  -ryg981219 Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -made max amplification 793% (as in module players)
 *  -ss040709  Stian Skjelstad <stian@nixia.no>
 *    -use compatible timing, and not cputime/clock()
*/

extern "C"
{
#include "config.h"
}
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "types.h"
extern "C"
{
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/player.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "stuff/compat.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "stuff/timer.h"
}
#include "sidplayfp/SidTuneInfo.h"
#include "cpiinfo.h"
#include "sidplay.h"

#define _MAX_FNAME 8
#define _MAX_EXT 4

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
//static int16_t speed
static int16_t pitch;
//static const char finespeed=8;
static const char finepitch=8;

static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;

static void startpausefade (void)
{
	if (plPause)
	{
		starttime = starttime + dos_clock () - pausetime;
		sidSetPitch (0x00010 * pitch * 1 / 4);
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
		sidPause ( plPause = 0 );
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
			sidPause(plPause=1);
			plChanChanged=1;
			//sidSetPitch(0x00010000);
			return;
		}
	}
	pausefaderelspeed=i;
	sidSetPitch (0x00010 * pitch * i / 4);
}

static void sidDrawGStrings(uint16_t (*buf)[CONSOLE_MAX_X])
{
	long tim;
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
		writestring(buf[0], 6, 0x0F, "\376\376\376\376\376\376\376\376", (vol+4)>>3);
		writestring(buf[0], 22, 0x0F, srnd?"x":"o", 1);
		if (((pan+70)>>4)==4)
			writestring(buf[0], 34, 0x0F, "m", 1);
		else {
			writestring(buf[0], 30+((pan+70)>>4), 0x0F, "r", 1);
			writestring(buf[0], 38-((pan+70)>>4), 0x0F, "l", 1);
		}
		writestring(buf[0], 46+((bal+70)>>4), 0x0F, "I", 1);

		writestring(buf[0], 57, 0x09, "filter: ... pitch: ...%", 23);
		_writenum(buf[0], 76, 0x0F, pitch*100/256, 10, 3);
#if 0
		writestring(buf[0], 75, 0x0F, sidGetFilter()?"on":"off", 3);
#endif
		writestring(buf[1],  0, 0x09," song .. of ..    SID:            speed: ....    cpu: ...%",80);
		writenum(buf[1],  6, 0x0F, sidGetSong(), 16, 2, 0);
		writenum(buf[1], 12, 0x0F, sidGetSongs(), 16, 2, 0);
		writestring(buf[1], 41, 0x0F, sidGetVideo()?"PAL":"NTSC", 4);

		_writenum(buf[1], 54, 0x0F, tmGetCpuUsage(), 10, 3);
		writestring(buf[1], 57, 0x0F, "%", 1);


		writestring(buf[2],  0, 0x09, " file \372\372\372\372\372\372\372\372.\372\372\372: ...............................                time: ..:.. ", 80);
		writestring(buf[2],  6, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 14, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 20, 0x0F, modname, 31);
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
		writestring(buf[0], 12, 0x0F, "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376", (vol+2)>>2);
		writestring(buf[0], 41, 0x0F, srnd?"x":"o", 1);
		if (((pan+68)>>3)==8)
			writestring(buf[0], 62, 0x0F, "m", 1);
		else {
			writestring(buf[0], 54+((pan+68)>>3), 0x0F, "r", 1);
			writestring(buf[0], 70-((pan+68)>>3), 0x0F, "l", 1);
		}
		writestring(buf[0], 83+((bal+68)>>3), 0x0F, "I", 1);

		writestring(buf[0], 103, 0x09, "filter: ...  pitch: ...% ", 25);
		_writenum(buf[0], 123, 0x0F, pitch*100/256, 10, 3);
#if 0
		writestring(buf[0], 125, 0x0F, sidGetFilter()?"on":"off", 3);
#endif
		writestring(buf[1],  0, 0x09,"    song .. of ..                    speed: ....    cpu: ...%",132);
		writenum(buf[1],  9, 0x0F, sidGetSong(), 16, 2, 0);
		writenum(buf[1], 15, 0x0F, sidGetSongs(), 16, 2, 0);
		writestring(buf[1], 44, 0x0F, sidGetVideo()?"PAL":"NTSC", 4);

		_writenum(buf[1], 57, 0x0F, tmGetCpuUsage(), 10, 3);
		writestring(buf[1], 60, 0x0F, "%", 1);

		writestring(buf[2],  0, 0x09, "    file \372\372\372\372\372\372\372\372.\372\372\372: ...............................  composer: ...............................                    time: ..:..   ", 132);
		writestring(buf[2],  9, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 17, 0x0F, currentmodext, _MAX_EXT);
		writestring(buf[2], 23, 0x0F, modname, 31);
		writestring(buf[2], 66, 0x0F, composer, 31);
		if (plPause)
			writestring(buf[2], 100, 0x0C, "playback paused", 15);
		writenum(buf[2], 123, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 125, 0x0F, ":", 1);
		writenum(buf[2], 126, 0x0F, tim%60, 10, 2, 0);
	}
}


static void logvolbar(int &l, int &r)
{
	if (l>32)
		l=32+((l-32)>>1);
	if (l>48)
		l=48+((l-48)>>1);
	if (l>56)
		l=56+((l-56)>>1);
	if (l>64)
		l=64;
	if (r>32)
		r=32+((r-32)>>1);
	if (r>48)
		r=48+((r-48)>>1);
	if (r>56)
		r=56+((r-56)>>1);
	if (r>64)
		r=64;
}


static char convnote(long freq)
{
#warning FIXME, frequency does not take VIC-II model / cpu-freqency into account
	if (freq<256) return 0xff;

	float frfac=(float)freq/(float)0x1167;

	float nte=12*(log(frfac)/log(2))+48;

	if (nte<0 || nte>127) nte=0xff;
	return (char)nte;
}



static void drawvolbar(uint16_t *buf, int l, int r, const unsigned char st)
{
	logvolbar(l, r);

	l=(l+4)>>3;
	r=(r+4)>>3;
	if (plPause)
		l=r=0;
	if (st)
	{
		writestring(buf, 8-l, 0x08, "\376\376\376\376\376\376\376\376", l);
		writestring(buf, 9, 0x08, "\376\376\376\376\376\376\376\376", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0ffe};
		writestringattr(buf, 8-l, left+8-l, l);
		writestringattr(buf, 9, right, r);
	}
}

static void drawlongvolbar(uint16_t *buf, int l, int r, const unsigned char st)
{
	logvolbar(l, r);
	l=(l+2)>>2;
	r=(r+2)>>2;
	if (plPause)
		l=r=0;
	if (st)
	{
		writestring(buf, 16-l, 0x08, "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376", l);
		writestring(buf, 17, 0x08, "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe};
		writestringattr(buf, 16-l, left+16-l, l);
		writestringattr(buf, 17, right, r);
	}
}


static const char *waves4[]={"    ","tri ","saw ","trsw","puls","trpu","swpu","tsp ",
                             "nois","????","????","????","????","????","????","????"};

static const char *waves16[]={"                ","triangle        ","sawtooth        ",
                              "tri + saw       ","pulse           ","triangle + pulse",
                              "sawtooth + pulse","tri + saw + puls","noise           ",
                              "invalid         ","invalid         ","invalid         ",
                              "invalid         ","invalid         ","invalid         ",
                              "invalid         "};

static const char *filters3[]={"---","low","bnd","b+l","hgh","h+l","h+b","hbl"};
static const char *filters12[]={"-----","low pass","band pass","low + band","high pass",
                                "band notch","high + band","all pass"};

static const char *fx2[]={"  ","sy","ri","rs"};
static const char *fx7[]={"","sync","ringmod","snc+rng"};
static const char *fx11[]={"","sync","ringmod","sync + ring"};

/*
#### = volume bars.. can be made mono in SID, gives more space
                                                                                                   1         1         1
         1         2         3         4         5         6         7         8         9         0         1         2
12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
                                    $       $                 $             $                                                   $
 ---- --- -- - -- ######## ######## $       $                 $             $                                                   $
 WAVE     Pulsewidth                        $                 $             $                                                   $
      NOTE   filter                         $                 $             $                                                   $
               fx                           $                 $             $                                                   $
                                            $                 $             $                                                   $
 ---- ---- --- -- --- --  ######## ######## $                 $             $                                                   $
 WAVE ADSR NOTE   filter                                      $             $                                                   $
               PulseWidth                                     $             $                                                   $
                      fx                                      $             $                                                   $
                                                              $             $                                                   $
 ---------------- ---- --- --- --- -------  ####### ########  $             $                                                   $
 WAVE             ADSR NOTE    filter                                       $                                                   $
                           PulseWidth                                       $                                                   $
                                   fx                                       $                                                   $
                                                                            $                                                   $
 xxxxxxxxxxxxxxxx | xxxx | xxx | xxx | xxx | xxxxxxxxxxx | ####### ######## $                                                   $
 WAVE               ADSR   NOTE        filter                                                                                   $
                                 Pulsewidth  FX                                                                                 $
                                                                                                                                $
 xxxxxxxxxxxxxxxx  |  xxxx  |  xxx  |  xxx  |  xxxxxxxxxxxx  |  xxxxxxxxxxx  |   ################ ################              $
 WAVE                 ADSR     NOTE    PulseWidth               FX
                                               Filter
*/

static void drawchannel(uint16_t *buf, int len, int i)
{
	sidChanInfo ci;
	unsigned char st=plMuteCh[i];

	unsigned char tcol=st?0x08:0x0F;
	unsigned char tcold=st?0x08:0x07;
/*
	unsigned char tcolr=st?0x08:0x0B;  unused
*/

	switch (len)
	{
		case 36:
			writestring(buf, 0, tcold, " ---- --- -- - -- \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372 ", 36);
			break;
		case 62:
			writestring(buf, 0, tcold, " ---------------- ---- --- --- --- -------  \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372 ", 62);
			break;
		case 128:
			writestring(buf, 0, tcold, "                   \263        \263       \263       \263                \263               \263   \372\372\372\372\372\372\372\372\372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372\372\372\372\372\372\372\372\372", 128);
			break;
		case 76:
			writestring(buf, 0, tcold, "                  \263      \263     \263     \263     \263             \263 \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372", 76);
			break;
		case 44:
			writestring(buf, 0, tcold, " ---- ---- --- -- --- --  \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372 ", 44);
			break;
	}

	sidGetChanInfo(i, ci);

	if (!ci.leftvol && !ci.rightvol)
		return;

	uint8_t nte=convnote(ci.freq);
	char nchar[4];

	if (nte<0xFF)
	{
		nchar[0]="CCDDEFFGGAAB"[nte%12];
		nchar[1]="-#-#--#-#-#-"[nte%12];
		nchar[2]="0123456789ABCDEFGHIJKLMN"[nte/12];
		nchar[3]=0;
	} else
		strcpy(nchar,"   ");

	uint8_t ftype=(ci.filttype>>4)&7;
	uint8_t efx=(ci.wave>>1)&3;

	switch(len)
	{
		case 36:
			writestring(buf+1, 0, tcol, waves4[ci.wave>>4], 4);
			writestring(buf+6, 0, tcol, nchar, 3);
			writenum(buf+10, 0, tcol, ci.pulse>>4, 16, 2, 0);
			if (ci.filtenabled)
				writenum(buf+13, 0, tcol, ftype, 16, 1, 0);
			if (efx)
				writestring(buf+15, 0, tcol, fx2[efx], 2);
			drawvolbar(buf+18, ci.leftvol, ci.rightvol, st);
			break;

		case 44:
			writestring(buf+1, 0, tcol, waves4[ci.wave>>4], 4);
			writenum(buf+6, 0, tcol, ci.ad, 16, 2, 0);
			writenum(buf+8, 0, tcol, ci.sr, 16, 2, 0);
			writestring(buf+11, 0, tcol, nchar, 3);
			writenum(buf+15, 0, tcol, ci.pulse>>4, 16, 2, 0);
			if (ci.filtenabled)
				writestring(buf+18, 0, tcol, filters3[ftype], 3);
			if (efx)
				writestring(buf+22, 0, tcol, fx2[efx], 2);
			drawvolbar(buf+26, ci.leftvol, ci.rightvol, st);
			break;

		case 62:
			writestring(buf+1, 0, tcol, waves16[ci.wave>>4], 16);
			writenum(buf+18, 0, tcol, ci.ad, 16, 2, 0);
			writenum(buf+20, 0, tcol, ci.sr, 16, 2, 0);
			writestring(buf+23, 0, tcol, nchar, 3);
			writenum(buf+27, 0, tcol, ci.pulse, 16, 3, 0);
			if (ci.filtenabled)
				writestring(buf+31, 0, tcol, filters3[ftype], 3);
			if (efx)
				writestring(buf+35, 0, tcol, fx7[efx], 7);
			drawvolbar(buf+44, ci.leftvol, ci.rightvol, st);
			break;

		case 76:
			writestring(buf+1, 0, tcol, waves16[ci.wave>>4], 16);
			writenum(buf+20, 0, tcol, ci.ad, 16, 2, 0);
			writenum(buf+22, 0, tcol, ci.sr, 16, 2, 0);
			writestring(buf+27, 0, tcol, nchar, 3);
			writenum(buf+33, 0, tcol, ci.pulse, 16, 3, 0);
			if (ci.filtenabled)
				writestring(buf+39, 0, tcol, filters3[ftype], 3);
			writestring(buf+45, 0, tcol, fx11[efx], 11);
			drawvolbar(buf+59, ci.leftvol, ci.rightvol, st);
			break;

		case 128:
			writestring(buf+1, 0, tcol, waves16[ci.wave>>4], 16);
			writenum(buf+22, 0, tcol, ci.ad, 16, 2, 0);
			writenum(buf+24, 0, tcol, ci.sr, 16, 2, 0);
			writestring(buf+31, 0, tcol, nchar, 3);
			writenum(buf+39, 0, tcol, ci.pulse, 16, 3, 0);
			if (ci.filtenabled)
				writestring(buf+47, 0, tcol, filters12[ftype], 12);
			writestring(buf+64, 0, tcol, fx11[efx], 11);
			drawlongvolbar(buf+81, ci.leftvol, ci.rightvol, st);
			break;
	}
}


static void normalize(void)
{
	mcpNormalize(0);
	pan    = set.pan;
	bal    = set.bal;
	vol    = set.vol;
	amp    = set.amp;
	srnd   = set.srnd;
	//speed  = set.speed;
	pitch  = set.pitch;
	//sidSetAmplify(1024*amp);
	sidSetVolume(vol, bal, pan, srnd);
	//sidSetSpeed (speed);
	sidSetPitch (pitch * 256);
}

static void sidCloseFile(void)
{
	sidClosePlayer();
	SidInfoDone();
}

static int sidProcessKey(uint16_t key)
{
	uint8_t csg;
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Previous track");
			cpiKeyHelp(KEY_CTRL_LEFT, "Previous track");
			cpiKeyHelp('>', "Next track");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Next track");
			cpiKeyHelp(KEY_BACKSPACE, "Toggle filter");
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
			cpiKeyHelp(KEY_F(11), "Decrease pitch speed");
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
			{
				starttime=starttime+dos_clock()-pausetime;
				sidSetPitch (pitch * 256);
			} else {
				pausetime=dos_clock();
			}
			plPause=!plPause;
			sidPause(plPause);
			plChanChanged=1; /* ? */
			break;
		case '<':
		case KEY_CTRL_LEFT:
		/* case 0x7300: //ctrl-left */
			csg=sidGetSong()-1;
			if (csg)
			{
				sidStartSong(csg);
				starttime=dos_clock();
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
		/* case 0x7400: //ctrl-right */
			csg=sidGetSong()+1;
			if (csg<=sidGetSongs())
			{
				sidStartSong(csg);
				starttime=dos_clock();
			}
			break;
		/* case 0x7700: //ctrl-home TODO KEYS
			sidStartSong(csg=sidGetSong());
			starttime=dos_clock();
			break;*/
		case KEY_BACKSPACE: //backspace
#if 0
			sidToggleFilter();
#endif
			break;
		case '-':
			if (vol>=2)
				vol-=2;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case '+':
			if (vol<=62)
				vol+=2;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case '/':
			if ((bal-=4)<-64)
				bal=-64;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case '*':
			if ((bal+=4)>64)
				bal=64;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case ',':
			if ((pan-=4)<-64)
				pan=-64;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case '.':
			if ((pan+=4)>64)
				pan=64;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(2):
			if ((vol-=8)<0)
				vol=0;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(3):
			if ((vol+=8)>64)
				vol=64;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(4):
			sidSetVolume(vol, bal, pan, srnd=srnd?0:2);
			break;
		case KEY_F(5):
			if ((pan-=16)<-64)
				pan=-64;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(6):
			if ((pan+=16)>64)
				pan=64;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(7):
			if ((bal-=16)<-64)
				bal=-64;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(8):
			if ((bal+=16)>64)
				bal=64;
			sidSetVolume(vol, bal, pan, srnd);
			break;
		case KEY_F(11):
			if ((pitch-=finepitch)<16)
			{
				pitch=16;
			}
			if (!plPause)
			{
				sidSetPitch(pitch*256);
			}
/*
			if (splock)
			{
				speed = pitch;
				sidSetSpeed (speed);
			}
*/
			break;
		case KEY_F(12):
			if ((pitch+=finepitch)>2048)
				pitch=2048;
			if (!plPause)
			{
				sidSetPitch(pitch*256);
			}
/*
			if (splock)
			{
				speed = pitch;
				sidSetSpeed (speed);
			}
*/
			break;
			/*
		case 0x5f00: // ctrl f2 TODO keys
			if ((amp-=4)<4)
				amp=4;
			sidSetAmplify(1024*amp);
			break;
		case 0x6000: // ctrl f3 TODO keys
			if ((amp+=4)>508)
				amp=508;
			sidSetAmplify(1024*amp);
			break;
		case 0x8900: // ctrl f11 TODO keys
			break;
		case 0x6a00: // alt-f3 TODO keys
			normalize();
			break;
		case 0x6900: // alt-f2 TODO keys
			set.pan=pan;
			set.bal=bal;
			set.vol=vol;
			set.amp=amp;
			set.srnd=srnd;
			break;
		case 0x6b00: // alt-f4 TODO keys
			pan=64;
			bal=0;
			vol=64;
			amp=64;
			sidSetVolume(vol, bal, pan, srnd);
			sidSetAmplify(1024*amp);
			break;
			*/
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

static int sidLooped()
{
	if (pausefadedirect)
	{
		dopausefade();
	}
	sidIdle();
	if (plrIdle)
		plrIdle();
	return 0;
}

static int sidOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *sidf)
{
	if (!sidf)
		return -1;

	strncpy(currentmodname, info->name, _MAX_FNAME);
	strncpy(currentmodext, info->name+ + _MAX_FNAME, _MAX_EXT);

	modname=info->modname;
	composer=info->composer;

	fprintf(stderr, "loading %s%s...\n", currentmodname, currentmodext);

	if (!sidOpenPlayer(sidf))
		return -1;

	plNLChan=sidNumberOfChips()*3;
	plNPChan=sidNumberOfChips()*4;
	plIdle=sidIdle;
	plUseChannels(drawchannel);
	plSetMute=sidMute;

	plIsEnd=sidLooped;
	plProcessKey=sidProcessKey;
	plDrawGStrings=sidDrawGStrings;
	plGetMasterSample=plrGetMasterSample;
	plGetRealMasterVolume=plrGetRealMasterVolume;

	plGetPChanSample=sidGetPChanSample;
	plGetLChanSample=sidGetLChanSample;

	starttime=dos_clock();
	plPause=0;
	normalize();
	pausefadedirect=0;

	SidInfoInit();

	return 0;
}

extern "C"
{
	cpifaceplayerstruct sidPlayer = {sidOpenFile, sidCloseFile};
	struct linkinfostruct dllextinfo =
	{
		"playsid" /* name */,
		"OpenCP SID Player (c) 1993-'20 Michael Schwendt, Tammo Hinrichs, Stian Skjelstad" /* desc */,
		DLLVERSION /* ver */
	};
}
