/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2005-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
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

static int timidityLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
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
		-1
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
			cpifaceSession->TogglePauseFade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			cpifaceSession->TogglePause (cpifaceSession);
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
			cpifaceSession->ResetSongTimer (cpifaceSession);
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
	cpifaceSession->cpiDebug (cpifaceSession, "[TiMidity++ MID] loading %s...\n", filename);

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
					cpifaceSession->cpiDebug (cpifaceSession, "[TiMidity++ MID] %s is bigger than 64 Mb - further loading blocked\n", filename);
					free (buffer);
					return errAllocMem;
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

	cpifaceSession->InPause = 0;

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
	if ((err = timidity_config_init (API))) return err;

	return err;
}

static void timidityPluginClose (struct PluginCloseAPI_t *API)
{
	timidity_type_done (API);
	timidity_config_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct timidityPlayer = {"[TiMidity++ MIDI plugin]", timidityOpenFile, timidityCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playtimidity", .desc = "OpenCP TiMidity++ Player (c) 2016-'24 TiMidity++ team & Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit=timidityPluginInit, .PluginClose=timidityPluginClose};
