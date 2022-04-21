/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2011-'22 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
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
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "stuff/compat.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
}
#include "sidplayfp/SidTuneInfo.h"
#include "cpiinfo.h"
#include "sidplay.h"

static time_t starttime;
static time_t pausetime;
static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;
static char utf8_8_dot_3  [12*4+1];  /* UTF-8 ready */
static char utf8_16_dot_3 [20*4+1]; /* UTF-8 ready */
static struct moduleinfostruct mdbdata;

static void startpausefade (void)
{
	if (plPause)
	{
		starttime = starttime + dos_clock () - pausetime;
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
			mcpSetFadePars(64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetFadePars(i);
}

static void sidDrawGStrings (void)
{
	mcpDrawGStrings ();

	mcpDrawGStringsSongXofY
	(
		utf8_8_dot_3,
		utf8_16_dot_3,
		sidGetSong(),
		sidGetSongs(),
		plPause,
		plPause?((pausetime-starttime)/DOS_CLK_TCK):((dos_clock()-starttime)/DOS_CLK_TCK),
		&mdbdata
	);
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
			cpiKeyHelp(KEY_CTRL_HOME, "Next to start of song");
			mcpSetProcessKey (key);
			return 0;
		case 'p': case 'P':
			startpausefade();
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			if (plPause)
			{
				starttime=starttime+dos_clock()-pausetime;
			} else {
				pausetime=dos_clock();
			}
			plPause=!plPause;
			sidPause(plPause);
			//plChanChanged=1; /* ? */
			break;
		case '<':
		case KEY_CTRL_LEFT:
			csg=sidGetSong()-1;
			if (csg)
			{
				sidStartSong(csg);
				starttime=dos_clock();
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			csg=sidGetSong()+1;
			if (csg<=sidGetSongs())
			{
				sidStartSong(csg);
				starttime=dos_clock();
			}
			break;
		case KEY_CTRL_HOME:
			sidStartSong(csg=sidGetSong());
			starttime=dos_clock();
			break;
		default:
			return mcpSetProcessKey (key);
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
	return 0;
}

static int sidOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *sidf, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
{
	const char *filename;

	if (!sidf)
		return -1;

	mdbdata = *info;
	dirdbGetName_internalstr (sidf->dirdb_ref, &filename);
	fprintf(stderr, "loading %s...\n", filename);
	utf8_XdotY_name ( 8, 3, utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, utf8_16_dot_3, filename);

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

	pausefadedirect=0;

	SidInfoInit();

	return 0;
}

extern "C"
{
	cpifaceplayerstruct sidPlayer = {"[libsidplayfp plugin]", sidOpenFile, sidCloseFile};
	struct linkinfostruct dllextinfo =
	{
		"playsid" /* name */,
		"OpenCP SID Player (c) 1993-'22 Michael Schwendt, Tammo Hinrichs, Stian Skjelstad" /* desc */,
		DLLVERSION /* ver */
	};
}
