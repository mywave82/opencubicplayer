/* OpenCP Module Player
 * copyright (c) 2005-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OPLPlay interface routines
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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
extern "C"
{
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/pfilesel.h"
#include "filesel/mdb.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
}
#include "oplplay.h"
#include "opltype.h"

static time_t starttime;      /* when did the song start, if paused, this is slided if unpaused */
static time_t pausetime;      /* when did the pause start (fully paused) */
static time_t pausefadestart; /* when did the pause fade start, used to make the slide */
static int8_t pausefadedirection; /* 0 = no slide, +1 = sliding from pause to normal, -1 = sliding from normal to pause */

static oplTuneInfo globinfo;
static oplChanInfo ci;

static void togglepausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (pausefadedirection)
	{ /* we are already in a pause-fade, reset the fade-start point */
		pausefadestart = clock_ms() - 1000 + (clock_ms() - pausefadestart);
		pausefadedirection *= -1; /* inverse the direction */
	} else if (cpifaceSession->InPause)
	{ /* we are in full pause already */
		pausefadestart = clock_ms();
		starttime = starttime + pausefadestart - pausetime; /* we are unpausing, so push starttime the amount we have been paused */
		oplPause (cpifaceSession->InPause = 0);
		pausefadedirection = 1;
	} else { /* we were not in pause, start the pause fade */
		pausefadestart = clock_ms();
		pausefadedirection = -1;
	}
}

static void dopausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int16_t i;
	if (pausefadedirection > 0)
	{ /* unpause fade */
		i = ((int_fast32_t)(clock_ms() - pausefadestart)) * 64 / 1000;
		if (i < 1)
		{
			i = 1;
		}
		if (i >= 64)
		{
			i = 64;
			pausefadedirection = 0; /* we reached the end of the slide */
		}
	} else { /* pause fade */
		i = 64 - ((int_fast32_t)(clock_ms() - pausefadestart)) * 64 / 1000;
		if (i >= 64)
		{
			i = 64;
		}
		if (i <= 0)
		{ /* we reached the end of the slide, finish the pause command */
			pausefadedirection = 0;
			pausetime = clock_ms();
			oplPause (cpifaceSession->InPause = 1);
			return;
		}
	}
	cpifaceSession->SetMasterPauseFadeParameters (cpifaceSession, i);
}

static char convnote(long freq)
{
	if (!freq) return 0xff;

	float frfac=(float)freq/220.0;

	float nte=12*(log(frfac)/log(2))+12;

	if (nte<0 || nte>127) nte=0xff;
	return (char)nte;
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

static void drawvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int, unsigned char st)
{
	int l,r;
	l=ci.vol;
	r=ci.vol;
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
		cpifaceSession->console->WriteStringAttr (buf, 9  , right   , r);
	}
}

static void drawlongvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int, unsigned char st)
{
	int l,r;
	l=ci.vol;
	r=ci.vol;
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
		cpifaceSession->console->WriteStringAttr (buf, 17  , right,     r);
	}
}

static void oplDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	oplpGetGlobInfo(globinfo);

	cpifaceSession->drawHelperAPI->GStringsSongXofY
	(
		cpifaceSession,
		globinfo.currentSong,
		globinfo.songs,
		cpifaceSession->InPause ? ((pausetime - starttime) / 1000) : ((clock_ms() - starttime) / 1000)
	);
}

static void drawchannel (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiChanWidth width, int i, int compoMode) /* TODO */
{
        unsigned char st = cpifaceSession->MuteChannel[i];

        unsigned char tcol=st?0x08:0x0F;
        unsigned char tcold=st?0x08:0x07;

	const char *waves4 []= {"sine", "half", "2x  ", "saw "};
	const char *waves16[]= {"sine curves     ", "half sine curves", "positiv sines   ", "sawtooth        "};

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

	oplpGetChanInfo(i,ci);

	if (!ci.vol)
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

	switch(width)
	{
		case cpiChanWidth_36:
			cpifaceSession->console->WriteString (buf, 1, tcol, waves4[ci.wave], 4);
			cpifaceSession->console->WriteString (buf, 6, tcol, nchar, 3);
/*
			cpifaceSession->console->WriteNum (buf, 10, tcol, ci.pulse>>4, 16, 2, 0);
			if (ci.filtenabled && ftype)
				cpifaceSession->console->WriteNum (buf, 13, tcol, ftype, 16, 1, 0);
			if (efx)
				cpifaceSession->console->WriteString (buf, 15, tcol, fx2[efx], 2);*/
			drawvolbar (cpifaceSession, buf+18, i, st);
			break;
		case cpiChanWidth_44:
			cpifaceSession->console->WriteString (buf, 1, tcol, waves4[ci.wave], 4);
/*
			cpifaceSession->console->WriteNum (buf, 6, tcol, ci.ad, 16, 2, 0);
			cpifaceSession->console->WriteNum (buf, 8, tcol, ci.sr, 16, 2, 0);*/
			cpifaceSession->console->WriteString (buf, 11, tcol, nchar, 3);
/*
			cpifaceSession->console->WriteNum (buf, 15, tcol, ci.pulse>>4, 16, 2, 0);
			if (ci.filtenabled && ftype)
				cpifaceSession->console->WriteString (buf, 18, tcol, filters3[ftype], 3);
			if (efx)
				cpifaceSession->console->WriteString (buf, 22, tcol, fx2[efx], 2);*/
			drawvolbar (cpifaceSession, buf+26, i, st);
			break;
		case cpiChanWidth_62:
			cpifaceSession->console->WriteString (buf, 1, tcol, waves16[ci.wave], 16);
/*
			cpifaceSession->console->WriteNum (buf, 18, tcol, ci.ad, 16, 2, 0);
			cpifaceSession->console->WriteNum (buf, 20, tcol, ci.sr, 16, 2, 0);*/
			cpifaceSession->console->WriteString (buf, 23, tcol, nchar, 3);
/*
			cpifaceSession->console->WriteNum (buf, 27, tcol, ci.pulse, 16, 3, 0);
			if (ci.filtenabled && ftype)
				cpifaceSession->console->WriteString (buf, 31, tcol, filters3[ftype], 3);
			if (efx)
				cpifaceSession->console->WriteString (buf, 35, tcol, fx7[efx], 7);*/
			drawvolbar (cpifaceSession, buf+44, i, st);
			break;
		case cpiChanWidth_76:
			cpifaceSession->console->WriteString (buf, 1, tcol, waves16[ci.wave], 16);
/*
			cpifaceSession->console->WriteNum (buf, 20, tcol, ci.ad, 16, 2, 0);
			cpifaceSession->console->WriteNum (buf, 22, tcol, ci.sr, 16, 2, 0);*/
			cpifaceSession->console->WriteString (buf, 27, tcol, nchar, 3);
/*
			cpifaceSession->console->WriteNum (buf, 33, tcol, ci.pulse, 16, 3, 0);
			if (ci.filtenabled && ftype)
				cpifaceSession->console->WriteString (buf, 39, tcol, filters3[ftype], 3);
			cpifaceSession->console->WriteString (buf, 45, tcol, fx11[efx], 11);*/
			drawvolbar (cpifaceSession, buf+59, i, st);
			break;
		case cpiChanWidth_128:
			cpifaceSession->console->WriteString (buf, 1, tcol, waves16[ci.wave], 16);
/*
			cpifaceSession->console->WriteNum (buf, 22, tcol, ci.ad, 16, 2, 0);
			cpifaceSession->console->WriteNum (buf, 24, tcol, ci.sr, 16, 2, 0);*/
			cpifaceSession->console->WriteString (buf, 31, tcol, nchar, 3);
/*
			cpifaceSession->console->WriteNum (buf, 39, tcol, ci.pulse, 16, 3, 0);
			if (ci.filtenabled && ftype)
				cpifaceSession->console->WriteString (buf, 47, tcol, filters12[ftype], 12);
			cpifaceSession->console->WriteString (buf, 64, tcol, fx11[efx], 11);*/
			drawlongvolbar (cpifaceSession, buf+81, i, st);
			break;
	}
}

static int oplProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	struct oplTuneInfo ti;

	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Restart Song");
			cpifaceSession->KeyHelp ('<', "Previous Song");
			cpifaceSession->KeyHelp ('>', "Next song");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			return 0;
		case 'p': case 'P':
			togglepausefade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			/* cancel any pause-fade that might be in progress */
			pausefadedirection = 0;
			cpifaceSession->SetMasterPauseFadeParameters (cpifaceSession, 64);

			if (cpifaceSession->InPause)
			{
				starttime = starttime + clock_ms() - pausetime; /* we are unpausing, so push starttime for the amount we have been paused */
			} else {
				pausetime = clock_ms();
			}
			cpifaceSession->InPause = !cpifaceSession->InPause;
			oplPause (cpifaceSession->InPause);
			break;
		case KEY_CTRL_HOME:
			oplpGetGlobInfo (ti);
			oplSetSong (ti.currentSong);
			break;
		case '<':
			oplpGetGlobInfo (ti);
			oplSetSong (ti.currentSong - 1);
			break;
		case '>':
			oplpGetGlobInfo (ti);
			oplSetSong (ti.currentSong + 1);
			break;
#if 0
		case KEY_CTRL_UP:
		/* case 0x8D00: //ctrl-up */
			oplSetPos(oplGetPos()-oplrate);
			break;
		case KEY_CTRL_DOWN:
		/* case 0x9100: //ctrl-down */
			oplSetPos(oplGetPos()+oplrate);
			break;*/
		case '<':
		case KEY_CTRL_LEFT:
		/* case 0x7300: //ctrl-left */
			{
				int skip=opllen>>5;
				if (skip<128*1024)
					skip=128*1024;
				oplSetPos(oplGetPos()-skip);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
		/* case 0x7400: //ctrl-right */
			{
				int skip=opllen>>5;
				if (skip<128*1024)
					skip=128*1024;
				oplSetPos(oplGetPos()+skip);
			}
			break;
		case 0x7700: //ctrl-home TODO keys
			oplSetPos(0);
			break;
	#endif
		default:
			return 0;
	}
	return 1;
}


static int oplLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	if (pausefadedirection)
	{
		dopausefade (cpifaceSession);
	}
	oplSetLoop (LoopMod);
	oplIdle (cpifaceSession);
	return (!LoopMod) && oplIsLooped();
}

static void oplCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	oplClosePlayer (cpifaceSession);
}

static int oplOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	const char *filename;
	size_t buffersize = 16*1024;
	uint8_t *buffer = (uint8_t *)malloc (buffersize);
	size_t bufferfill = 0;
	int retval;

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	{
		int res;
		while (!file->eof(file))
		{
			if (buffersize == bufferfill)
			{
				if (buffersize >= 16*1024*1024)
				{
					cpifaceSession->cpiDebug (cpifaceSession, "[Adplug OPL] %s is bigger than 16 Mb - further loading blocked\n", filename);
					free (buffer);
					return -1;
				}
				buffersize += 16*1024;
				buffer = (uint8_t *)realloc (buffer, buffersize);
			}
			res = file->read (file, buffer + bufferfill, buffersize - bufferfill);
			if (res<=0)
				break;
			bufferfill += res;
		}
	}
	cpifaceSession->cpiDebug (cpifaceSession, "[Adplug OPL] loading %s\n", filename);

	cpifaceSession->IsEnd = oplLooped;
	cpifaceSession->ProcessKey = oplProcessKey;
	cpifaceSession->DrawGStrings = oplDrawGStrings;

	if ((retval = oplOpenPlayer(filename, buffer, bufferfill, file, cpifaceSession)))
	{
		return retval;
	}
	buffer=0;

	starttime = clock_ms();
	cpifaceSession->InPause = 0;
	pausefadedirection = 0;

	cpifaceSession->LogicalChannelCount = 36;
	cpifaceSession->PhysicalChannelCount = 36;
	cpifaceSession->UseChannels (cpifaceSession, drawchannel);
	cpifaceSession->SetMuteChannel = oplMute;

	oplpGetGlobInfo(globinfo);

	return errOk;
}

static int oplPluginInit (struct PluginInitAPI_t *API)
{
	return opl_type_init (API);
}

static void oplPluginClose (struct PluginCloseAPI_t *API)
{
	opl_type_done (API);
}

extern "C"
{
	const cpifaceplayerstruct oplPlayer = {"[AdPlug OPL plugin]", oplOpenFile, oplCloseFile};
	DLLEXTINFO_PLAYBACK_PREFIX_CPP struct linkinfostruct dllextinfo =
	{ /* c++ historically does not support named initializers, and size needs to be writable... */
		/* .name = */ "playopl",
		/* .desc = */ "OpenCP Adplug (OPL) Detection and Player (c) 2005-'23 Stian Skjelstad",
		/* .ver  = */ DLLVERSION,
		/* .sortindex = */ 95,
		/* .PreInit = */ 0,
		/* .Init = */ 0,
		/* .PluginInit = */ oplPluginInit,
		/* .LateInit = */ 0,
		/* .PreClose = */ 0,
		/* .PluginClose = */ oplPluginClose,
		/* .Close = */ 0,
		/* .LateClose = */ 0,
	};
}
