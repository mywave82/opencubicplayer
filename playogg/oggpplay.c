/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OGGPlay interface routines
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
#include "oggplay.h"
#include "oggtype.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static ogg_int64_t ogglen;
static uint32_t oggrate;

static void oggDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct ogginfo inf;

	oggGetInfo (cpifaceSession, &inf);

	cpifaceSession->drawHelperAPI->GStringsFixedLengthStream
	(
		cpifaceSession,
		inf.pos,
		inf.len,
		1, /* KB */
		inf.opt25,
		inf.opt50,
		inf.bitrate / 1000
	);
}

static int oggProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			oggSetPos (cpifaceSession, oggGetPos (cpifaceSession) - oggrate);
			break;
		case KEY_CTRL_DOWN:
			oggSetPos (cpifaceSession, oggGetPos (cpifaceSession) + oggrate);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			{
				ogg_int64_t pos = oggGetPos (cpifaceSession);
				ogg_int64_t newpos = pos -(ogglen>>5);
				if (newpos > pos)
				{
					newpos = 0;
				}
				oggSetPos (cpifaceSession, newpos);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				ogg_int64_t pos = oggGetPos (cpifaceSession);
				ogg_int64_t newpos = pos + (ogglen>>5);
				if ((newpos < pos) || (newpos > ogglen)) /* catch both wrap around (not likely), and overshots */
				{
					newpos = ogglen - 4;
				}
				oggSetPos (cpifaceSession, newpos);
			}
			break;
		case KEY_CTRL_HOME:
			oggSetPos (cpifaceSession, 0);
			break;
		default:
			return 0;
	}
	return 1;
}

static int oggIsLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	oggSetLoop (LoopMod);
	oggIdle (cpifaceSession);
	return (!LoopMod) && oggLooped();
}


static void oggCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	oggClosePlayer (cpifaceSession);

	OggInfoDone (cpifaceSession);
	OggPicDone (cpifaceSession);
}

static int oggOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *oggf)
{
	const char *filename;
	struct ogginfo inf;
	int retval;

	if (!oggf)
		return errFormStruc;

	cpifaceSession->dirdb->GetName_internalstr (oggf->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[OGG] preloading %s...\n", filename);

	cpifaceSession->IsEnd = oggIsLooped;
	cpifaceSession->ProcessKey = oggProcessKey;
	cpifaceSession->DrawGStrings = oggDrawGStrings;

	if ((retval = oggOpenPlayer(oggf, cpifaceSession)))
	{
		return retval;
	}

	cpifaceSession->InPause = 0;

	oggGetInfo (cpifaceSession, &inf);
	ogglen=inf.len;
	oggrate=inf.rate;

	OggInfoInit (cpifaceSession);
	OggPicInit (cpifaceSession);

	return errOk;
}

static int oggPluginInit (struct PluginInitAPI_t *API)
{
	return ogg_type_init (API);
}

static void oggPluginClose (struct PluginCloseAPI_t *API)
{
	ogg_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct oggPlayer = {"[OGG Vorbis plugin]", oggOpenFile, oggCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playogg", .desc = "OpenCP Ogg Vorbis Player (c) 1994-'23 Stian Skjelstad, Niklas Beisert & Tammo Hinrichs", .ver = DLLVERSION, .sortindex = 95, .PluginInit = oggPluginInit, .PluginClose = oggPluginClose};
