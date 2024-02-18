/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * MPPlay interface routines
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
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "mpplay.h"
#include "mptype.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static uint32_t mpeglen;
static uint32_t mpegrate;

static void mpegDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct mpeginfo inf;

	mpegGetInfo (&inf);

	cpifaceSession->drawHelperAPI->GStringsFixedLengthStream
	(
		cpifaceSession,
		inf.pos,
		inf.len,
		1, /* KB */
		inf.opt25,
		inf.opt50,
		inf.rate / 1000
	);
}

static int mpegProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			mpegSetPos(mpegGetPos()-mpegrate);
			break;
		case KEY_CTRL_DOWN:
			mpegSetPos(mpegGetPos()+mpegrate);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			{
				uint32_t pos = mpegGetPos();
				uint32_t newpos = pos - (mpeglen>>5);
				if (newpos > pos)
				{
					newpos = 0;
				}
				mpegSetPos(newpos);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				uint32_t pos = mpegGetPos();
				uint32_t newpos = pos + (mpeglen>>5);
				if ((newpos < pos) || (newpos > mpeglen))
				{
					newpos = mpeglen - 4;
				}
				mpegSetPos(newpos);
			}
			break;
		case KEY_CTRL_HOME:
			mpegSetPos(0);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}


static int mpegLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	mpegSetLoop (LoopMod);
	mpegIdle (cpifaceSession);
	return (!LoopMod) && mpegIsLooped();
}


static void mpegCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	ID3InfoDone (cpifaceSession);
	ID3PicDone (cpifaceSession);
	mpegClosePlayer (cpifaceSession);
}

static int mpegOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *mpegfile)
{
	int retval;
	const char *filename;
	struct mpeginfo inf;

	if (!mpegfile)
	{
		return errFormStruc;
	}

	cpifaceSession->dirdb->GetName_internalstr (mpegfile->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[MPx] preloading %s...\n", filename);

	cpifaceSession->IsEnd = mpegLooped;
	cpifaceSession->ProcessKey = mpegProcessKey;
	cpifaceSession->DrawGStrings = mpegDrawGStrings;

	if ((retval = mpegOpenPlayer(mpegfile, cpifaceSession)))
	{
		return retval;
	}

	cpifaceSession->InPause = 0;

	mpegGetInfo(&inf);
	mpeglen=inf.len;
	mpegrate=inf.rate;

	ID3InfoInit (cpifaceSession);
	ID3PicInit (cpifaceSession);

	return errOk;
}

static int mpegPluginInit (struct PluginInitAPI_t *API)
{
	return ampeg_type_init (API);
}

static void mpegPluginClose (struct PluginCloseAPI_t *API)
{
	ampeg_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct mpegPlayer = {"[MPEG, libmad plugin]", mpegOpenFile, mpegCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playmp2", .desc = "OpenCP Audio MPEG Player (c) 1994-'24 Stian Skjelstad, Niklas Beisert & Tammo Hinrichs", .ver = DLLVERSION, .sortindex = 95, .PluginInit = mpegPluginInit, .PluginClose = mpegPluginClose};
