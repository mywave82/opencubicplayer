/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static ogg_int64_t ogglen;
static uint32_t oggrate;
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
		oggPause (cpifaceSession->InPause = 0);
		pausefadedirect=1;
	} else
		pausefadedirect=-1;
}

static void dopausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int16_t i;
	if (pausefadedirect>0)
	{
		i=(dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i<0)
			i=0;
		if (i>=64)
		{
			i=64;
			pausefadedirect=0;
		}
	} else {
		i=64-(dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i>=64)
			i=64;
		if (i<=0)
		{
			i=0;
			pausefadedirect=0;
			pausetime=dos_clock();
			oggPause (cpifaceSession->InPause = 1);
			mcpSetMasterPauseFadeParameters (64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetMasterPauseFadeParameters (i);
}

static void oggDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct ogginfo inf;

	mcpDrawGStrings (cpifaceSession);

	oggGetInfo (&inf);

	mcpDrawGStringsFixedLengthStream
	(
		cpifaceSession,
		utf8_8_dot_3,
		utf8_16_dot_3,
		inf.pos,
		inf.len,
		1, /* KB */
		inf.opt25,
		inf.opt50,
		inf.bitrate / 1000,
		cpifaceSession->InPause,
		cpifaceSession->InPause ? ((pausetime-starttime)/DOS_CLK_TCK) : ((dos_clock()-starttime)/DOS_CLK_TCK),
		&mdbdata
	);
}

static int oggProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Jump back (big)");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump back (big)");
			cpiKeyHelp('>', "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_UP, "Jump back (small)");
			cpiKeyHelp(KEY_CTRL_DOWN, "Jump forward (small)");
			cpiKeyHelp(KEY_CTRL_HOME, "Jump to start of track");
			mcpSetProcessKey (key);
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
			cpifaceSession->InPause =! cpifaceSession->InPause;
			oggPause (cpifaceSession->InPause);
			break;
		case KEY_CTRL_UP:
			oggSetPos(oggGetPos()-oggrate);
			break;
		case KEY_CTRL_DOWN:
			oggSetPos(oggGetPos()+oggrate);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			{
				ogg_int64_t pos = oggGetPos();
				ogg_int64_t newpos = pos -(ogglen>>5);
				if (newpos > pos)
				{
					newpos = 0;
				}
				oggSetPos(newpos);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				ogg_int64_t pos = oggGetPos();
				ogg_int64_t newpos = pos + (ogglen>>5);
				if ((newpos < pos) || (newpos > ogglen)) /* catch both wrap around (not likely), and overshots */
				{
					newpos = ogglen - 4;
				}
				oggSetPos(newpos);
			}
			break;
		case KEY_CTRL_HOME:
			oggSetPos(0);
			break;
		default:
			return mcpSetProcessKey (key);
	}
	return 1;
}

static int oggIsLooped (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (pausefadedirect)
	{
		dopausefade (cpifaceSession);
	}
	oggSetLoop(fsLoopMods);
	oggIdle();
	return !fsLoopMods&&oggLooped();
}


static void oggCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	oggClosePlayer();

	OggInfoDone (cpifaceSession);
	OggPicDone (cpifaceSession);
}

static int oggOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *oggf, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
{
	const char *filename;
	struct ogginfo inf;

	if (!oggf)
		return -1;

	mdbdata = *info;
	dirdbGetName_internalstr (oggf->dirdb_ref, &filename);
	fprintf(stderr, "preloading %s...\n", filename);
	utf8_XdotY_name ( 8, 3, utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, utf8_16_dot_3, filename);

	plIsEnd=oggIsLooped;
	plProcessKey=oggProcessKey;
	plDrawGStrings=oggDrawGStrings;

	if (!oggOpenPlayer(oggf, cpifaceSession))
		return -1;

	starttime=dos_clock();
	cpifaceSession->InPause = 0;
	pausefadedirect=0;

	oggGetInfo(&inf);
	ogglen=inf.len;
	oggrate=inf.rate;

	OggInfoInit (cpifaceSession);
	OggPicInit (cpifaceSession);

	return errOk;
}


struct cpifaceplayerstruct oggPlayer = {"[OGG Vorbis plugin]", oggOpenFile, oggCloseFile};
struct linkinfostruct dllextinfo = {.name = "playogg", .desc = "OpenCP Ogg Vorbis Player (c) 1994-'22 Stian Skjelstad, Niklas Beisert & Tammo Hinrichs", .ver = DLLVERSION, .size = 0};
