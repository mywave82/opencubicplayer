/* OpenCP Module Player
 * copyright (c) 2007-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * FLACPlay interface routines
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
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/pfilesel.h"
#include "filesel/mdb.h"
#include "flacplay.h"
#include "flactype.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static uint32_t flaclen;
static uint32_t flacrate;

static void flacDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct flacinfo inf;

	flacGetInfo (&inf);

	cpifaceSession->drawHelperAPI->GStringsFixedLengthStream
	(
		cpifaceSession,
		inf.pos, // this is in samples :-(
		inf.len, // this is in samples :-(
		1, /* KB */
		inf.opt25,
		inf.opt50,
		inf.bitrate / 1000
	);
}

static int flacProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			flacSetPos (flacGetPos (cpifaceSession) - flacrate);
			break;
		case KEY_CTRL_DOWN:
			flacSetPos (flacGetPos (cpifaceSession) + flacrate);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			{
				uint64_t oldpos = flacGetPos (cpifaceSession);
				unsigned int skip=flaclen>>5;
				if (skip<128*1024)
					skip=128*1024;
				if (oldpos<skip)
					flacSetPos(0);
				else
					flacSetPos(oldpos-skip);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				int skip=flaclen>>5;
				if (skip<128*1024)
					skip=128*1024;
				flacSetPos (flacGetPos (cpifaceSession) + skip);
			}
			break;
		case KEY_CTRL_HOME:
			flacSetPos(0);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}


static int flacLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	flacSetLoop (LoopMod);
	flacIdle (cpifaceSession);
	return (!LoopMod) && flacIsLooped();
}


static void flacCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	flacClosePlayer (cpifaceSession);

	FlacInfoDone (cpifaceSession);
	FlacPicDone (cpifaceSession);
}

static int flacOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *flacf)
{
	int retval;
	const char *filename;
	struct flacinfo inf;

	if (!flacf)
		return -1;

	cpifaceSession->dirdb->GetName_internalstr (flacf->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] preloading %s...\n", filename);

	cpifaceSession->IsEnd = flacLooped;
	cpifaceSession->ProcessKey = flacProcessKey;
	cpifaceSession->DrawGStrings = flacDrawGStrings;

	if ((retval = flacOpenPlayer(flacf, cpifaceSession)))
		return retval;

	cpifaceSession->InPause = 0;

	flacGetInfo(&inf);
	flaclen=inf.len;
	flacrate=inf.rate;

	FlacInfoInit (cpifaceSession);
	FlacPicInit (cpifaceSession);

	return errOk;
}

static int flacPluginInit (struct PluginInitAPI_t *API)
{
	return flac_type_init (API);
}

static void flacPluginClose (struct PluginCloseAPI_t *API)
{
	flac_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct flacPlayer = {"[FLAC plugin]", flacOpenFile, flacCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playflac", .desc = "OpenCP FLAC Player (c) 2007-'24 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = flacPluginInit, .PluginClose = flacPluginClose};
