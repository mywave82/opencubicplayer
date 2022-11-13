/* OpenCP Module Player
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "aytype.h"
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
		ayPause (cpifaceSession->InPause = 0);
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
			ayPause (cpifaceSession->InPause = 1);
			return;
		}
	}
	cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, i);
}

static void ayCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	ayClosePlayer (cpifaceSession);
}

static int ayLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	if (pausefadedirection)
	{
		dopausefade (cpifaceSession);
	}
	aySetLoop (LoopMod);
	ayIdle (cpifaceSession);
	return (!LoopMod) && ayIsLooped();
}

static void ayDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct ayinfo globinfo;

	ayGetInfo (&globinfo);

	cpifaceSession->drawHelperAPI->GStringsSongXofY
	(
		cpifaceSession,
		globinfo.track,
		globinfo.numtracks,
		cpifaceSession->InPause?((pausetime - starttime) / 1000):((clock_ms() - starttime) / 1000)
	);
#warning TODO: globinfo.trackname, each track can have unique names.....
}

static int ayProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	int csg;
	struct ayinfo globinfo;
	ayGetInfo(&globinfo);

	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('<', "Jump to previous track");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Jump to previous track");
			cpifaceSession->KeyHelp ('>', "Jump to next track");
			cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Jump to next track");
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
			ayPause (cpifaceSession->InPause);
			break;
		case '<':
		case KEY_CTRL_LEFT: /* curses.h can't do these */
			csg=globinfo.track-1;
			if (csg)
			{
				ayStartSong (cpifaceSession, csg);
				starttime = clock_ms(); /* reset the starttime */
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT: /* curses.h can't do these */
			csg=globinfo.track+1;
			if (csg<=globinfo.numtracks)
			{
				ayStartSong (cpifaceSession, csg);
				starttime = clock_ms(); /* reset the starttime */
			}
			break;

		default:
			return 0;
	}
	return 1;
}

static int ayOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	const char *filename;

	if (!file)
		return -1;

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s...\n", filename);

	cpifaceSession->IsEnd = ayLooped;
	cpifaceSession->ProcessKey = ayProcessKey;
	cpifaceSession->DrawGStrings = ayDrawGStrings;

	cpifaceSession->SetMuteChannel = aySetMute;
	cpifaceSession->LogicalChannelCount = 6;

	ayChanSetup (cpifaceSession);

	if (!ayOpenPlayer(file, cpifaceSession))
	{
#ifdef INITCLOSE_DEBUG
		fprintf(stderr, "ayOpenPlayer FAILED\n");
#endif
		return -1;
	}

	starttime = clock_ms(); /* initialize starttime */
	cpifaceSession->InPause = 0;

	pausefadedirection = 0;

	return errOk;
}

static int ayPluginInit (struct PluginInitAPI_t *API)
{
	return ay_type_init (API);
}

static void ayPluginClose (struct PluginCloseAPI_t *API)
{
	ay_type_done (API);
}

const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) ayPlayer = {"[Aylet plugin]", ayOpenFile, ayCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playay", .desc = "OpenCP aylet Player (c) 2005-'22 Russell Marks, Ian Collier & Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = ayPluginInit, .PluginClose = ayPluginClose};
