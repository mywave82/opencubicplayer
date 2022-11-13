/* OpenCP Module Player
 * copyright (c) 2007-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
                flacPause (cpifaceSession->InPause = 0);
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
			flacPause (cpifaceSession->InPause = 1);
			return;
		}
	}
	cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, i);
}

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
		inf.bitrate / 1000,
		cpifaceSession->InPause?((pausetime - starttime) / 1000):((clock_ms() - starttime) / 1000)
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
			cpifaceSession->InPause = !cpifaceSession->InPause;
			flacPause(cpifaceSession->InPause);
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
			break;
		default:
			return 0;
	}
	return 1;
}


static int flacLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	if (pausefadedirection)
	{
		dopausefade (cpifaceSession);
	}
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
	const char *filename;
	struct flacinfo inf;

	if (!flacf)
		return -1;

	cpifaceSession->dirdb->GetName_internalstr (flacf->dirdb_ref, &filename);
	fprintf(stderr, "preloading %s...\n", filename);

	cpifaceSession->IsEnd = flacLooped;
	cpifaceSession->ProcessKey = flacProcessKey;
	cpifaceSession->DrawGStrings = flacDrawGStrings;

	if (!flacOpenPlayer(flacf, cpifaceSession))
		return -1;

	starttime = clock_ms();
	cpifaceSession->InPause = 0;

	pausefadedirection = 0;

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

const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) flacPlayer = {"[FLAC plugin]", flacOpenFile, flacCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playflac", .desc = "OpenCP FLAC Player (c) 2007-'22 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = flacPluginInit, .PluginClose = flacPluginClose};
