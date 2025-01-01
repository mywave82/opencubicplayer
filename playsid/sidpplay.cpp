/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2011-'25 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
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
#include "../config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "sidtype.h"
#include "sidconfig.h"
}
#include "sidplayfp/SidTuneInfo.h"
#include "cpiinfo.h"
#include "cpisidsetup.h"
#include "sidplay.h"

static void sidDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->drawHelperAPI->GStringsSongXofY
	(
		cpifaceSession,
		sidGetSong(),
		sidGetSongs()
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



static void drawvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int l, int r, const unsigned char st)
{
	logvolbar(l, r);

	l=(l+4)>>3;
	r=(r+4)>>3;
	if (cpifaceSession->InPause)
	{
		l=r=0;
	}
	if (st)
	{
		cpifaceSession->console->WriteString (buf, 8-l, 0x08, "\376\376\376\376\376\376\376\376", l);
		cpifaceSession->console->WriteString (buf, 9  , 0x08, "\376\376\376\376\376\376\376\376", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0ffe};
		cpifaceSession->console->WriteStringAttr (buf, 8-l, left+8-l, l);
		cpifaceSession->console->WriteStringAttr (buf, 9  , right, r);
	}
}

static void drawlongvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int l, int r, const unsigned char st)
{
	logvolbar(l, r);
	l=(l+2)>>2;
	r=(r+2)>>2;
	if (cpifaceSession->InPause)
	{
		l=r=0;
	}
	if (st)
	{
		cpifaceSession->console->WriteString (buf, 16-l, 0x08, "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376", l);
		cpifaceSession->console->WriteString (buf, 17  , 0x08, "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe};
		cpifaceSession->console->WriteStringAttr (buf, 16-l, left+16-l, l);
		cpifaceSession->console->WriteStringAttr (buf, 17  , right, r);
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

static void drawchannel (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiChanWidth width, int i, int compoMode)
{
	sidChanInfo ci;
	unsigned char st = cpifaceSession->MuteChannel[i];

	unsigned char tcol=st?0x08:0x0F;
	unsigned char tcold=st?0x08:0x07;
/*
	unsigned char tcolr=st?0x08:0x0B;  unused
*/

	switch (width)
	{
		case cpiChanWidth_36:
			cpifaceSession->console->WriteString (buf, 0, tcold, " ---- --- -- - -- \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372 ", 36);
			break;
		case cpiChanWidth_62:
			cpifaceSession->console->WriteString (buf, 0, tcold, " ---------------- ---- --- --- --- -------  \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372 ", 62);
			break;
		case cpiChanWidth_128:
			cpifaceSession->console->WriteString (buf, 0, tcold, "                   \263        \263       \263       \263                \263               \263   \372\372\372\372\372\372\372\372\372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372\372\372\372\372\372\372\372\372", 128);
			break;
		case cpiChanWidth_76:
			cpifaceSession->console->WriteString (buf, 0, tcold, "                  \263      \263     \263     \263     \263             \263 \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372", 76);
			break;
		case cpiChanWidth_44:
			cpifaceSession->console->WriteString (buf, 0, tcold, " ---- ---- --- -- --- --  \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372 ", 44);
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

	switch (width)
	{
		case cpiChanWidth_36:
			cpifaceSession->console->WriteString (buf,  1, tcol, waves4[ci.wave>>4], 4);
			cpifaceSession->console->WriteString (buf,  6, tcol, nchar, 3);
			cpifaceSession->console->WriteNum    (buf, 10, tcol, ci.pulse>>4, 16, 2, 0);
			if (ci.filtenabled)
				cpifaceSession->console->WriteNum    (buf+13, 0, tcol, ftype, 16, 1, 0);
			if (efx)
				cpifaceSession->console->WriteString (buf+15, 0, tcol, fx2[efx], 2);
			drawvolbar (cpifaceSession, buf+18, ci.leftvol, ci.rightvol, st);
			break;

		case cpiChanWidth_44:
			cpifaceSession->console->WriteString (buf,  1, tcol, waves4[ci.wave>>4], 4);
			cpifaceSession->console->WriteNum    (buf,  6, tcol, ci.ad, 16, 2, 0);
			cpifaceSession->console->WriteNum    (buf,  8, tcol, ci.sr, 16, 2, 0);
			cpifaceSession->console->WriteString (buf, 11, tcol, nchar, 3);
			cpifaceSession->console->WriteNum    (buf, 15, tcol, ci.pulse>>4, 16, 2, 0);
			if (ci.filtenabled)
				cpifaceSession->console->WriteString (buf, 18, tcol, filters3[ftype], 3);
			if (efx)
				cpifaceSession->console->WriteString (buf, 22, tcol, fx2[efx], 2);
			drawvolbar (cpifaceSession, buf+26, ci.leftvol, ci.rightvol, st);
			break;

		case cpiChanWidth_62:
			cpifaceSession->console->WriteString (buf,  1, tcol, waves16[ci.wave>>4], 16);
			cpifaceSession->console->WriteNum    (buf, 18, tcol, ci.ad, 16, 2, 0);
			cpifaceSession->console->WriteNum    (buf, 20, tcol, ci.sr, 16, 2, 0);
			cpifaceSession->console->WriteString (buf, 23, tcol, nchar, 3);
			cpifaceSession->console->WriteNum    (buf, 27, tcol, ci.pulse, 16, 3, 0);
			if (ci.filtenabled)
				cpifaceSession->console->WriteString (buf, 31, tcol, filters3[ftype], 3);
			if (efx)
				cpifaceSession->console->WriteString (buf, 35, tcol, fx7[efx], 7);
			drawvolbar (cpifaceSession, buf+44, ci.leftvol, ci.rightvol, st);
			break;

		case cpiChanWidth_76:
			cpifaceSession->console->WriteString (buf,  1, tcol, waves16[ci.wave>>4], 16);
			cpifaceSession->console->WriteNum    (buf, 20, tcol, ci.ad, 16, 2, 0);
			cpifaceSession->console->WriteNum    (buf, 22, tcol, ci.sr, 16, 2, 0);
			cpifaceSession->console->WriteString (buf, 27, tcol, nchar, 3);
			cpifaceSession->console->WriteNum    (buf, 33, tcol, ci.pulse, 16, 3, 0);
			if (ci.filtenabled)
				cpifaceSession->console->WriteString (buf, 39, tcol, filters3[ftype], 3);
			cpifaceSession->console->WriteString         (buf, 45, tcol, fx11[efx], 11);
			drawvolbar (cpifaceSession, buf+59, ci.leftvol, ci.rightvol, st);
			break;

		case cpiChanWidth_128:
			cpifaceSession->console->WriteString (buf,  1, tcol, waves16[ci.wave>>4], 16);
			cpifaceSession->console->WriteNum    (buf, 22, tcol, ci.ad, 16, 2, 0);
			cpifaceSession->console->WriteNum    (buf, 24, tcol, ci.sr, 16, 2, 0);
			cpifaceSession->console->WriteString (buf, 31, tcol, nchar, 3);
			cpifaceSession->console->WriteNum    (buf, 39, tcol, ci.pulse, 16, 3, 0);
			if (ci.filtenabled)
				cpifaceSession->console->WriteString (buf, 47, tcol, filters12[ftype], 12);
			cpifaceSession->console->WriteString (buf,  64, tcol, fx11[efx], 11);
			drawlongvolbar (cpifaceSession, buf+81, ci.leftvol, ci.rightvol, st);
			break;
	}
}

static void sidCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	sidClosePlayer (cpifaceSession);
	SidInfoDone (cpifaceSession);
}

static int sidProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	uint8_t csg;
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('<', "Previous track");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Previous track");
			cpifaceSession->KeyHelp ('>', "Next track");
			cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Next track");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Next to start of song");
			return 0;
		case 'p': case 'P':
			cpifaceSession->TogglePauseFade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			cpifaceSession->TogglePause (cpifaceSession);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			csg=sidGetSong()-1;
			if (csg)
			{
				sidStartSong(csg);
				cpifaceSession->ResetSongTimer (cpifaceSession);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			csg=sidGetSong()+1;
			if (csg<=sidGetSongs())
			{
				sidStartSong(csg);
				cpifaceSession->ResetSongTimer (cpifaceSession);
			}
			break;
		case KEY_CTRL_HOME:
			sidStartSong(csg=sidGetSong());
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}

static int sidLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{ /* We do not detect loops at the moment */
	sidIdle (cpifaceSession);
	return 0;
}

static int sidOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *sidf)
{
	const char *filename;
	int retval;

	if (!sidf)
	{
		return errFormStruc;
	}

	cpifaceSession->dirdb->GetName_internalstr (sidf->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[SID] loading %s...\n", filename);

	if ((retval = sidOpenPlayer(sidf, cpifaceSession)))
	{
		return retval;
	}

	cpifaceSession->LogicalChannelCount = sidNumberOfChips() * 3;
	cpifaceSession->PhysicalChannelCount = sidNumberOfChips() * 4;
	cpifaceSession->UseChannels (cpifaceSession, drawchannel);
	cpifaceSession->SetMuteChannel = sidMute;

	cpifaceSession->IsEnd = sidLooped;
	cpifaceSession->ProcessKey = sidProcessKey;
	cpifaceSession->DrawGStrings = sidDrawGStrings;

	cpifaceSession->GetPChanSample = sidGetPChanSample;
	cpifaceSession->GetLChanSample = sidGetLChanSample;

	cpifaceSession->InPause = 0;

	SidInfoInit (cpifaceSession);
	cpiSidSetupInit (cpifaceSession);

	return errOk;
}

static int sidPluginInit (PluginInitAPI_t *API)
{
	int err;
	if ((err = sid_config_init (API))) return err;
	if ((err = sid_type_init (API))) return err;
	return err;
}

static void sidPluginClose (struct PluginCloseAPI_t *API)
{
	sid_type_done (API);
	sid_config_done (API);
}

extern "C"
{
	const cpifaceplayerstruct sidPlayer = {"[libsidplayfp plugin]", sidOpenFile, sidCloseFile};
	DLLEXTINFO_PLAYBACK_PREFIX_CPP struct linkinfostruct dllextinfo =
	{ /* c++ historically does not support named initializers, and size needs to be writable... */
		/* .name = */ "playsid",
		/* .desc = */ "OpenCP SID Player (c) 1993-'25 Michael Schwendt, Tammo Hinrichs, Stian Skjelstad",
		/* .ver  = */ DLLVERSION,
		/* .sortindex = */ 95,
		/* .PreInit = */ 0,
		/* .Init = */ 0,
		/* .PluginInit = */ sidPluginInit,
		/* .LateInit = */ 0,
		/* .PreClose = */ 0,
		/* .PluginClose = */ sidPluginClose,
		/* .Close = */ 0,
		/* .LateClose = */ 0
	};
}
