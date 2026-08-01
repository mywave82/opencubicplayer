/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * SpeexPlay interface routines
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
#include "speexplay.h"
#include "speextype.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static void speexDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct speexinfo inf;

	speexGetInfo (cpifaceSession, &inf);

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

static int speexProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			speexSeekReverse (cpifaceSession, 50);
			break;
		case KEY_CTRL_DOWN:
			speexSeekForward (cpifaceSession, 50);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			{
				speexSeekReverse (cpifaceSession, 500);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				speexSeekForward (cpifaceSession, 500);
			}
			break;
		case KEY_CTRL_HOME:
			speexSeekHome (cpifaceSession);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}

static int speexIsLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	speexSetLoop (LoopMod);
	speexIdle (cpifaceSession);
	return (!LoopMod) && speexLooped();
}

static void speexCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	speexClosePlayer (cpifaceSession);

	speexInfoDone (cpifaceSession);
#if 0
	speexPicDone (cpifaceSession);
#endif
}

static int speexOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *f)
{
	const char *filename;
//	struct speexinfo inf;
	int retval;

	if (!f)
		return errFormStruc;

	cpifaceSession->dirdb->GetName_internalstr (f->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[SPEEX] preloading %s...\n", filename);

	cpifaceSession->IsEnd = speexIsLooped;
	cpifaceSession->ProcessKey = speexProcessKey;
	cpifaceSession->DrawGStrings = speexDrawGStrings;

	if ((retval = speexOpenPlayer(f, cpifaceSession)))
	{
		return retval;
	}

	cpifaceSession->InPause = 0;
	speexInfoInit (cpifaceSession);
#if 0
	speexPicInit (cpifaceSession);
#endif
	return errOk;
}

static int speexPluginInit (struct PluginInitAPI_t *API)
{
	return speex_type_init (API);
}

static void speexPluginClose (struct PluginCloseAPI_t *API)
{
	speex_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct speexPlayer = {"[Speex plugin]", speexOpenFile, speexCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playspeex", .desc = "OpenCP Speex Player (c) 2026 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = speexPluginInit, .PluginClose = speexPluginClose};
