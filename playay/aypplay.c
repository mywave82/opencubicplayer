/* OpenCP Module Player
 * copyright (c) 2005-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
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

static void ayCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	ayClosePlayer (cpifaceSession);
}

static int ayLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
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
		globinfo.numtracks
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
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Restart Song");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('<', "Jump to previous track");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Jump to previous track");
			cpifaceSession->KeyHelp ('>', "Jump to next track");
			cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Jump to next track");
			return 0;
		case 'p': case 'P':
			cpifaceSession->TogglePauseFade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			cpifaceSession->TogglePause (cpifaceSession);
			break;
		case KEY_CTRL_HOME:
			csg=globinfo.track;
			ayStartSong (cpifaceSession, csg);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		case '<':
		case KEY_CTRL_LEFT: /* curses.h can't do these */
			csg=globinfo.track-1;
			if (csg)
			{
				ayStartSong (cpifaceSession, csg);
				cpifaceSession->ResetSongTimer (cpifaceSession);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT: /* curses.h can't do these */
			csg=globinfo.track+1;
			if (csg<=globinfo.numtracks)
			{
				ayStartSong (cpifaceSession, csg);
				cpifaceSession->ResetSongTimer (cpifaceSession);
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
	int retval;

	if (!file)
		return -1;

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[AY] loading %s...\n", filename);

	cpifaceSession->IsEnd = ayLooped;
	cpifaceSession->ProcessKey = ayProcessKey;
	cpifaceSession->DrawGStrings = ayDrawGStrings;

	cpifaceSession->SetMuteChannel = aySetMute;
	cpifaceSession->LogicalChannelCount = 6;

	if ((retval = ayOpenPlayer(file, cpifaceSession)))
	{
		return retval;
	}

	ayChanSetup (cpifaceSession);

	cpifaceSession->InPause = 0;

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

OCP_INTERNAL const struct cpifaceplayerstruct ayPlayer = {"[Aylet plugin]", ayOpenFile, ayCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playay", .desc = "OpenCP aylet Player (c) 2005-'24444ssell Marks, Ian Collier & Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = ayPluginInit, .PluginClose = ayPluginClose};
