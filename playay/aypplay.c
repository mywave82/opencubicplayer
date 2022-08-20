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

static time_t starttime;
static time_t pausetime;
static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;
static char utf8_8_dot_3  [12*4+1];  /* UTF-8 ready */
static char utf8_16_dot_3 [20*4+1]; /* UTF-8 ready */
static struct moduleinfostruct mdbdata;

static void startpausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->InPause)
	{
		starttime=starttime+dos_clock()-pausetime;
	}

	if (pausefadedirect)
	{
		if (pausefadedirect<0)
		{
			cpifaceSession->InPause = 1;
		}
		pausefadestart=2*dos_clock()-DOS_CLK_TCK-pausefadestart;
	} else
		pausefadestart=dos_clock();

	if (cpifaceSession->InPause)
	{
		ayPause (cpifaceSession->InPause = 0);
		pausefadedirect=1;
	} else
		pausefadedirect=-1;
}

static void dopausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int16_t i;
	if (pausefadedirect>0)
	{
		i=((int32_t)dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i<0)
			i=0;
		if (i>=64)
		{
			i=64;
			pausefadedirect=0;
		}
	} else {
		i=64-((int32_t)dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i>=64)
			i=64;
		if (i<=0)
		{
			i=0;
			pausefadedirect=0;
			pausetime=dos_clock();
			ayPause (cpifaceSession->InPause = 1);
			cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, 64);
			return;
		}
	}
	pausefaderelspeed=i;
	cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, i);
}

static void ayCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	ayClosePlayer (cpifaceSession);
}

static int ayLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	if (pausefadedirect)
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
		utf8_8_dot_3,
		utf8_16_dot_3,
		globinfo.track,
		globinfo.numtracks,
		cpifaceSession->InPause,
		cpifaceSession->InPause?((pausetime-starttime)/DOS_CLK_TCK):((dos_clock()-starttime)/DOS_CLK_TCK),
		&mdbdata
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
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Jump to previous track");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump to previous track");
			cpiKeyHelp('>', "Jump to next track");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Jump to next track");
			return 0;
		case 'p': case 'P':
			startpausefade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			if (cpifaceSession->InPause)
			{
				starttime=starttime+dos_clock()-pausetime;
			} else {
				pausetime=dos_clock();
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
				starttime=dos_clock();
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT: /* curses.h can't do these */
			csg=globinfo.track+1;
			if (csg<=globinfo.numtracks)
			{
				ayStartSong (cpifaceSession, csg);
				starttime=dos_clock();
			}
			break;

		default:
			return 0;
	}
	return 1;

}

static int ayOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
{
	const char *filename;

	if (!file)
		return -1;

	mdbdata = *info;
	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s...\n", filename);
	utf8_XdotY_name ( 8, 3, utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, utf8_16_dot_3, filename);

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

	starttime=dos_clock();
	cpifaceSession->InPause = 0;

	pausefadedirect=0;

	return errOk;
}

struct cpifaceplayerstruct ayPlayer = {"[Aylet plugin]", ayOpenFile, ayCloseFile};
struct linkinfostruct dllextinfo = {.name = "playay", .desc = "OpenCP aylet Player (c) 2005-'22 Russell Marks, Ian Collier & Stian Skjelstad", .ver = DLLVERSION, .size = 0};
