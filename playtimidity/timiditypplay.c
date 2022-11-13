/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Timidity front-end interface
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "cpitimiditysetup.h"
#include "timidityplay.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "timidityconfig.h"
#include "timiditytype.h"

static time_t starttime;      /* when did the song start, if paused, this is slided if unpaused */
static time_t pausetime;      /* when did the pause start (fully paused) */
static time_t pausefadestart; /* when did the pause fade start, used to make the slide */
static int8_t pausefadedirection; /* 0 = no slide, +1 = sliding from pause to normal, -1 = sliding from normal to pause */

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
		timidityPause (cpifaceSession->InPause = 0);
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
			timidityPause (cpifaceSession->InPause = 1);
			return;
		}
	}
	cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, i);
}

static int timidityLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	if (pausefadedirection)
	{
		dopausefade (cpifaceSession);
	}
	timiditySetLoop (LoopMod);
	timidityIdle (cpifaceSession);
	return (!LoopMod) && timidityIsLooped();
}

static void timidityDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct mglobinfo gi;

	timidityGetGlobInfo(&gi);

	cpifaceSession->drawHelperAPI->GStringsFixedLengthStream
	(
		cpifaceSession,
		gi.curtick,
		gi.ticknum,
		0, /* events */
		"",//gi.opt25,
		"",//gi.opt50,
		-1,
		cpifaceSession->InPause ? ((pausetime - starttime) / 1000) : ((clock_ms() - starttime) / 1000)
	);
}

static int timidityProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('<', "Jump back (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Jump back (big)");
			cpifaceSession->KeyHelp ('>', "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_UP, "Jump back (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_DOWN, "Jump forward (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Jump to start of track");
			return 0;
		case 'p': case 'P':
			togglepausefade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			/* cancel any pause-fade that might be in progress */
			pausefadedirection = 0;
			cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, 64);

			if (cpifaceSession->InPause)
			{
				starttime = starttime + clock_ms() - pausetime; /* we are unpausing, so push starttime for the amount we have been paused */
			} else {
				pausetime = clock_ms();
			}
			cpifaceSession->InPause = !cpifaceSession->InPause;
			timidityPause (cpifaceSession->InPause);
			break;
		case KEY_CTRL_UP:
			timiditySetRelPos(-1); /* seconds */
			break;
		case KEY_CTRL_DOWN:
			timiditySetRelPos(1);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			timiditySetRelPos(-10);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			timiditySetRelPos(10);
			break;
		case KEY_CTRL_HOME:
			timidityRestart ();
			break;
		default:
			return 0;
	}
	return 1;

}

static int timidityOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	const char *filename;
	int err;

	if (!file)
		return errGen;

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s...\n", filename);

	cpifaceSession->IsEnd = timidityLooped;
	cpifaceSession->ProcessKey = timidityProcessKey;
	cpifaceSession->DrawGStrings = timidityDrawGStrings;
	cpifaceSession->UseDots(timidityGetDots);

	cpifaceSession->LogicalChannelCount = 16;
	cpifaceSession->SetMuteChannel = timidityMute;
	timidityChanSetup (cpifaceSession);

	{
		size_t buffersize = 64*1024;
		uint8_t *buffer = (uint8_t *)malloc (buffersize);
		size_t bufferfill = 0;

		int res;

		while (!file->eof (file))
		{
			if (buffersize == bufferfill)
			{
				if (buffersize >= 64*1024*1024)
				{
					fprintf (stderr, "timidityOpenFile: %s is bigger than 64 Mb - further loading blocked\n", filename);
					free (buffer);
					return -1;
				}
				buffersize += 64*1024;
				buffer = (uint8_t *)realloc (buffer, buffersize);
			}
			res = file->read (file, buffer + bufferfill, buffersize - bufferfill);
			if (res<=0)
				break;
			bufferfill += res;
		}

		err = timidityOpenPlayer (filename, buffer, bufferfill, file, cpifaceSession); /* buffer will be owned by the player */
		if (err)
		{
			free (buffer);
			return err;
		}
	}

	starttime = clock_ms();
	cpifaceSession->InPause = 0;
	pausefadedirection = 0;

	cpiTimiditySetupInit (cpifaceSession);

	return errOk;
}

static void timidityCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	timidityClosePlayer (cpifaceSession);
	cpiTimiditySetupDone (cpifaceSession);
}

static int timidityPluginInit (struct PluginInitAPI_t *API)
{
	int err;
	if ((err = timidity_type_init (API))) return err;
	if ((err = timidity_config_init ())) return err;

	return err;
}

static void timidityPluginClose (struct PluginCloseAPI_t *API)
{
	timidity_config_done ();
	timidity_type_done (API);
}

const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) timidityPlayer = {"[TiMidity++ MIDI plugin]", timidityOpenFile, timidityCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playtimidity", .desc = "OpenCP TiMidity++ Player (c) 2016-'22 TiMidity++ team & Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit=timidityPluginInit, .PluginClose=timidityPluginClose};
