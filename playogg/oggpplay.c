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
#include "oggtype.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static ogg_int64_t ogglen;
static uint32_t oggrate;

static time_t starttime;      /* when did the song start, if paused, this is slided if unpaused */
static time_t pausetime;      /* when did the pause start (fully paused) */
static time_t pausefadestart; /* when did the pause fade start, used to make the slide */
static int8_t pausefadedirection; /* 0 = no slide, +1 = sliding from pause to normal, -1 = sliding from normal to pause */

static void togglepausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (pausefadedirection)
	{ /* we are already in a pause-fade, reset the fade-start point */
		pausefadestart = clock_ms() - 1000 + (clock_ms() - pausefadestart);
		pausefadedirection *= -1; /* inverse the direction */
	} else if (cpifaceSession->InPause)
	{ /* we are in full pause already */
		pausefadestart = clock_ms();
		starttime = starttime + pausefadestart - pausetime; /* we are unpausing, so push starttime the amount we have been paused */
		oggPause (cpifaceSession->InPause = 0);
		pausefadedirection = 1;
	} else { /* we were not in pause, start the pause fade */
		pausefadestart = clock_ms();
		pausefadedirection = -1;
	}
}

static void dopausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int16_t i;
	if (pausefadedirection > 0)
	{ /* unpause fade */
		i = ((int_fast32_t)(clock_ms() - pausefadestart)) * 64 / 1000;
		if (i < 1)
		{
			i = 1;
		}
		if (i >= 64)
		{
			i = 64;
			pausefadedirection = 0; /* we reached the end of the slide */
		}
	} else { /* pause fade */
		i = 64 - ((int_fast32_t)(clock_ms() - pausefadestart)) * 64 / 1000;
		if (i >= 64)
		{
			i = 64;
		}
		if (i <= 0)
		{ /* we reached the end of the slide, finish the pause command */
			pausefadedirection = 0;
			pausetime = clock_ms();
			oggPause (cpifaceSession->InPause = 1);
			return;
		}
	}
	cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, i);
}

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
		inf.bitrate / 1000,
		cpifaceSession->InPause ? ((pausetime - starttime) / 1000) : ((clock_ms() - starttime) / 1000)
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
			togglepausefade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			/* cancel any pause-fade that might be in progress */
			pausefadedirection = 0;
			cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, 64);

			if (cpifaceSession->InPause)
			{
				starttime = starttime + clock_ms() - pausetime; /* we are unpausing, so push starttime for the amount we have been paused */
			} else {
				pausetime = clock_ms();
			}
			cpifaceSession->InPause =! cpifaceSession->InPause;
			oggPause (cpifaceSession->InPause);
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
	if (pausefadedirection)
	{
		dopausefade (cpifaceSession);
	}
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

static int oggOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *oggf, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
{
	const char *filename;
	struct ogginfo inf;

	if (!oggf)
		return -1;

	dirdbGetName_internalstr (oggf->dirdb_ref, &filename);
	fprintf(stderr, "preloading %s...\n", filename);

	cpifaceSession->IsEnd = oggIsLooped;
	cpifaceSession->ProcessKey = oggProcessKey;
	cpifaceSession->DrawGStrings = oggDrawGStrings;

	if (!oggOpenPlayer(oggf, cpifaceSession))
		return -1;

	starttime = clock_ms();
	cpifaceSession->InPause = 0;
	pausefadedirection = 0;

	oggGetInfo (cpifaceSession, &inf);
	ogglen=inf.len;
	oggrate=inf.rate;

	OggInfoInit (cpifaceSession);
	OggPicInit (cpifaceSession);

	return errOk;
}

static int oggInit (void)
{
	return ogg_type_init ();
}

static void oggClose (void)
{
	ogg_type_done ();
}

struct cpifaceplayerstruct oggPlayer = {"[OGG Vorbis plugin]", oggOpenFile, oggCloseFile};
struct linkinfostruct dllextinfo = {.name = "playogg", .desc = "OpenCP Ogg Vorbis Player (c) 1994-'22 Stian Skjelstad, Niklas Beisert & Tammo Hinrichs", .ver = DLLVERSION, .size = 0, .Init = oggInit, .Close = oggClose};
