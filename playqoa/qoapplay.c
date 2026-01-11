/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * QOAPlay interface routines
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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
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
#include "qoaplay.h"
#include "qoatype.h"

static void qoaDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct qoainfo inf;

	qoaGetInfo (cpifaceSession, &inf);

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

static int qoaProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			{
				struct qoainfo inf;
				qoaGetInfo (cpifaceSession, &inf);
				if (inf.pos >= inf.rate)
				{
					qoaSetPos (cpifaceSession, inf.pos - inf.rate);
				} else {
					qoaSetPos (cpifaceSession, 0);
				}
			}
			break;
		case KEY_CTRL_DOWN:
			{
				struct qoainfo inf;
				qoaGetInfo (cpifaceSession, &inf);
				if ((inf.pos + inf.rate) < inf.len)
				{
					qoaSetPos (cpifaceSession, inf.pos + inf.rate);
				}
			}
			break;
		case '<':
			{
				struct qoainfo inf;
				uint32_t pos, newpos;

				qoaGetInfo (cpifaceSession, &inf);
				pos    = inf.pos;
				newpos = pos - (inf.rate * 2); /* we buffer half a second, so need more when rewinding */
				if (newpos > pos)
				{
					newpos = 0;
				}
				qoaSetPos (cpifaceSession, newpos);
			}
			break;
		case KEY_CTRL_LEFT:
			{
				struct qoainfo inf;
				uint32_t pos, newpos;

				qoaGetInfo (cpifaceSession, &inf);
				pos    = inf.pos;
				newpos = pos - (inf.len >> 5);
				if (newpos > pos)
				{
					newpos = 0;
				}
				qoaSetPos (cpifaceSession, newpos);
			}
			break;
		case '>':
			{
				struct qoainfo inf;
				uint32_t pos, newpos;

				qoaGetInfo (cpifaceSession, &inf);
				pos    = inf.pos;
				newpos = pos + (inf.rate);
				if ((newpos < pos) || (newpos > inf.len)) /* catch both wrap around (not likely), and overshots */
				{
					newpos = inf.len - 4;
				}
				qoaSetPos (cpifaceSession, newpos);
			}
			break;
		case KEY_CTRL_RIGHT:
			{
				struct qoainfo inf;
				uint32_t pos, newpos;

				qoaGetInfo (cpifaceSession, &inf);
				pos    = inf.pos;
				newpos = pos + (inf.len >> 5);
				if ((newpos < pos) || (newpos > inf.len)) /* catch both wrap around (not likely), and overshots */
				{
					newpos = inf.len - 4;
				}
				qoaSetPos (cpifaceSession, newpos);
			}
			break;
		case KEY_CTRL_HOME:
			qoaSetPos (cpifaceSession, 0);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}


static int qoaLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	qoaSetLoop (LoopMod);
	qoaIdle (cpifaceSession);
	return (!LoopMod) && qoaIsLooped();
}

static void qoaCloseFile(struct cpifaceSessionAPI_t *cpifaceSession)
{
	qoaClosePlayer(cpifaceSession);
}

static int qoaOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *f)
{
	const char *filename;
	int retval;

	if (!f)
		return errFormStruc;

	cpifaceSession->dirdb->GetName_internalstr (f->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[QOA preloading %s...\n", filename);

	cpifaceSession->IsEnd = qoaLooped;
	cpifaceSession->ProcessKey = qoaProcessKey;
	cpifaceSession->DrawGStrings = qoaDrawGStrings;

	if ((retval = qoaOpenPlayer(f, cpifaceSession)))
	{
		return retval;
	}

	cpifaceSession->InPause = 0;

	return errOk;
}

static int qoaPluginInit (struct PluginInitAPI_t *API)
{
	return qoa_type_init (API);
}

static void qoaPluginClose (struct PluginCloseAPI_t *API)
{
	qoa_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct qoaPlayer = {"[QOA plugin]", qoaOpenFile, qoaCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playqoa", .desc = "OpenCP QOAPlayer (c) 2026 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit=qoaPluginInit, .PluginClose=qoaPluginClose};
