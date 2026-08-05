/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OpusPlay interface routines
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
 *  -ss040911   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -added fade pause
 */
#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "opusplay.h"
#include "opustype.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static void opusDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct opusinfo inf;

	opusGetInfo (cpifaceSession, &inf);

	cpifaceSession->drawHelperAPI->GStringsFixedLengthStream
	(
		cpifaceSession,

		inf.filepos,
		inf.filelen,
		1, /* KB */
		inf.opt25,
		inf.opt50,
		inf.bitrate / 1000
	);
}

static int opusProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			opusSeekReverse (cpifaceSession, 96000);
			break;
		case KEY_CTRL_DOWN:
			opusSeekForward (cpifaceSession, 48000);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			{
				opusSeekReverse (cpifaceSession, 528000);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				opusSeekForward (cpifaceSession, 480000);
			}
			break;
		case KEY_CTRL_HOME:
			opusSeekHome (cpifaceSession);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}

static int opusIsLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	opusSetLoop (LoopMod);
	opusIdle (cpifaceSession);
	return (!LoopMod) && opusLooped();
}

static void opusCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	opusClosePlayer (cpifaceSession);

	opusInfoDone (cpifaceSession);
#if 0
	opusPicDone (cpifaceSession);
#endif
}

static int opusOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *f)
{
	const char *filename;
//	struct opusinfo inf;
	int retval;

	if (!f)
		return errFormStruc;

	cpifaceSession->dirdb->GetName_internalstr (f->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[OPUS] preloading %s...\n", filename);

	cpifaceSession->IsEnd = opusIsLooped;
	cpifaceSession->ProcessKey = opusProcessKey;
	cpifaceSession->DrawGStrings = opusDrawGStrings;

	if ((retval = opusOpenPlayer(f, cpifaceSession)))
	{
		return retval;
	}

	cpifaceSession->InPause = 0;
	opusInfoInit (cpifaceSession);
#if 0
	opusPicInit (cpifaceSession);
#endif
	return errOk;
}

static int opusPluginInit (struct PluginInitAPI_t *API)
{
	return opus_type_init (API);
}

static void opusPluginClose (struct PluginCloseAPI_t *API)
{
	opus_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct opusPlayer = {"[Opus plugin]", opusOpenFile, opusCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playopus", .desc = "OpenCP Opus Player (c) 2026 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = opusPluginInit, .PluginClose = opusPluginClose};
