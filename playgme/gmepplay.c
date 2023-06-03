/* OpenCP Module Player
 * copyright (c) 2023 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Game Music Emulator Play interface routines
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
#include <string.h>
#include <time.h>
#include "types.h"
#include "gmeplay.h"
#include "gmetype.h"
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

OCP_INTERNAL struct moduletype gme_mt;
OCP_INTERNAL const char *gme_filename;
static struct ocpfilehandle_t *gme_file;

static void gmeCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (gme_file)
	{
		gme_file->unref (gme_file);
		gme_file = 0;
		gme_filename = 0;
	}
	gmeClosePlayer (cpifaceSession);
	gmeInfoDone (cpifaceSession);
}

static int gmeLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	gmeSetLoop (LoopMod);
	gmeIdle (cpifaceSession);
	return (!LoopMod) && gmeIsLooped();
}

static void gmeDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct gmeinfo globinfo;

	gmeGetInfo (&globinfo);

	cpifaceSession->drawHelperAPI->GStringsSongXofY
	(
		cpifaceSession,
		globinfo.track + 1,
		globinfo.numtracks
	);
}

static int gmeProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	int csg;
	struct gmeinfo globinfo;
	gmeGetInfo(&globinfo);

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
			gmeStartSong (cpifaceSession, csg);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		case '<':
		case KEY_CTRL_LEFT: /* curses.h can't do these */
			csg=globinfo.track-1;
			if (csg)
			{
				gmeStartSong (cpifaceSession, csg);
				cpifaceSession->ResetSongTimer (cpifaceSession);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT: /* curses.h can't do these */
			csg=globinfo.track+1;
			if (csg<=globinfo.numtracks)
			{
				gmeStartSong (cpifaceSession, csg);
				cpifaceSession->ResetSongTimer (cpifaceSession);
			}
			break;

		default:
			return 0;
	}
	return 1;
}

static int gmeOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	int retval;

	if (!file)
		return -1;

	gme_mt = info->modtype;

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &gme_filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[GME] loading %s...\n", gme_filename);

	cpifaceSession->IsEnd = gmeLooped;
	cpifaceSession->ProcessKey = gmeProcessKey;
	cpifaceSession->DrawGStrings = gmeDrawGStrings;

	if ((retval = gmeOpenPlayer(file, cpifaceSession)))
	{
		return retval;
	}

	gmeInfoInit (cpifaceSession);

	cpifaceSession->InPause = 0;

	gme_file = file;
	file->ref (file);

	return errOk;
}

static int gmePluginInit (struct PluginInitAPI_t *API)
{
	return gme_type_init (API);
}

static void gmePluginClose (struct PluginCloseAPI_t *API)
{
	gme_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct gmePlayer = {"[Game Music Emulator plugin]", gmeOpenFile, gmeCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playgme", .desc = "OpenCP GME Player (c) 2023 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = gmePluginInit, .PluginClose = gmePluginClose};
