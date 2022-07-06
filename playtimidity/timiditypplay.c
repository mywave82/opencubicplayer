/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Timidity front-end interface
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "cpitimiditysetup.h"
#include "timidityplay.h"
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

/* options */
static time_t starttime;
static time_t pausetime;
static char utf8_8_dot_3  [12*4+1];  /* UTF-8 ready */
static char utf8_16_dot_3 [20*4+1]; /* UTF-8 ready */
static struct moduleinfostruct mdbdata;

static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;

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
		timidityPause (cpifaceSession->InPause = 0);
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
			timidityPause (cpifaceSession->InPause = 1);
			mcpSetMasterPauseFadeParameters (cpifaceSession, 64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetMasterPauseFadeParameters (cpifaceSession, i);
}

static int timidityLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	if (pausefadedirect)
	{
		dopausefade (cpifaceSession);
	}
	timiditySetLoop (LoopMod);
	timidityIdle ();
	return (!LoopMod) && timidityIsLooped();
}

static void timidityDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct mglobinfo gi;

	timidityGetGlobInfo(&gi);

	mcpDrawGStringsFixedLengthStream
	(
		cpifaceSession,
		utf8_8_dot_3,
		utf8_16_dot_3,
		gi.curtick,
		gi.ticknum,
		0, /* events */
		"",//gi.opt25,
		"",//gi.opt50,
		-1,
		cpifaceSession->InPause,
		cpifaceSession->InPause ? ((pausetime-starttime)/DOS_CLK_TCK) : ((dos_clock()-starttime)/DOS_CLK_TCK),
		&mdbdata
	);
}

static int timidityProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			timidityPause (cpifaceSession->InPause);
			break;
		case KEY_CTRL_UP:
			timiditySetRelPos(-1); /* seconds */
			break;
		case KEY_CTRL_DOWN:
			timiditySetRelPos(1);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			timiditySetRelPos(-10);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			timiditySetRelPos(10);
			break;
		case KEY_CTRL_HOME:
			timidityRestart ();
			break;
		default:
			return 0;
	}
	return 1;

}

static int timidityOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
{
	const char *filename;
	int err;

	if (!file)
		return errGen;

	mdbdata = *info;
	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s...\n", filename);
	utf8_XdotY_name ( 8, 3, utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, utf8_16_dot_3, filename);

	cpifaceSession->IsEnd = timidityLooped;
	cpifaceSession->ProcessKey = timidityProcessKey;
	cpifaceSession->DrawGStrings = timidityDrawGStrings;
	plUseDots(timidityGetDots);

	cpifaceSession->LogicalChannelCount = 16;
	timidityChanSetup(/*&mid*/);

	{
		const char *path;
		size_t buffersize = 64*1024;
		uint8_t *buffer = (uint8_t *)malloc (buffersize);
		size_t bufferfill = 0;

		int res;

		dirdbGetName_internalstr (file->dirdb_ref, &path);

		while (!file->eof (file))
		{
			if (buffersize == bufferfill)
			{
				if (buffersize >= 64*1024*1024)
				{
					fprintf (stderr, "timidityOpenFile: %s is bigger than 64 Mb - further loading blocked\n", path);
					free (buffer);
					return -1;
				}
				buffersize += 64*1024;
				buffer = (uint8_t *)realloc (buffer, buffersize);
			}
			res = file->read (file, buffer + bufferfill, buffersize - bufferfill);
			if (res<=0)
				break;
			bufferfill += res;
		}

		err = timidityOpenPlayer(path, buffer, bufferfill, file, cpifaceSession); /* buffer will be owned by the player */
		if (err)
		{
			free (buffer);
			return err;
		}
	}

	starttime=dos_clock();
	cpifaceSession->InPause = 0;
	pausefadedirect=0;

	cpiTimiditySetupInit (cpifaceSession);

	return errOk;
}

static void timidityCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	timidityClosePlayer();
	cpiTimiditySetupDone (cpifaceSession);
}

struct cpifaceplayerstruct timidityPlayer = {"[TiMidity++ MIDI plugin]", timidityOpenFile, timidityCloseFile};
struct linkinfostruct dllextinfo = {.name = "playtimidity", .desc = "OpenCP TiMidity++ Player (c) 2016-'22 TiMidity++ team & Stian Skjelstad", .ver = DLLVERSION, .size = 0};
