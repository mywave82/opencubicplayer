/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static uint32_t mpeglen;
static uint32_t mpegrate;
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
		mpegPause (cpifaceSession->InPause = 0);
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
			mpegPause (cpifaceSession->InPause = 1);
			mcpSetMasterPauseFadeParameters (64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetMasterPauseFadeParameters (i);
}

static void mpegDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct mpeginfo inf;

	mpegGetInfo (&inf);

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
		inf.rate / 1000,
		cpifaceSession->InPause,
		cpifaceSession->InPause ? ((pausetime-starttime)/DOS_CLK_TCK) : ((dos_clock()-starttime)/DOS_CLK_TCK),
		&mdbdata
	);
}

static int mpegProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			mpegPause (cpifaceSession->InPause = !cpifaceSession->InPause);
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
			break;
		default:
			return 0;
	}
	return 1;
}


static int mpegLooped (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (pausefadedirect)
	{
		dopausefade (cpifaceSession);
	}
	mpegSetLoop(fsLoopMods);
	mpegIdle();
	return !fsLoopMods&&mpegIsLooped();
}


static void mpegCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	ID3InfoDone (cpifaceSession);
	mpegClosePlayer ();
}

static int mpegOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *mpegfile, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
{
	const char *filename;
	struct mpeginfo inf;

	if (!mpegfile)
		return -1;

	mdbdata = *info;

	dirdbGetName_internalstr (mpegfile->dirdb_ref, &filename);
	fprintf(stderr, "preloading %s...\n", filename);
	utf8_XdotY_name ( 8, 3, utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, utf8_16_dot_3, filename);

	plIsEnd=mpegLooped;
	cpifaceSession->ProcessKey = mpegProcessKey;
	cpifaceSession->DrawGStrings = mpegDrawGStrings;

	if (!mpegOpenPlayer(mpegfile, cpifaceSession))
		return errFileRead;

	starttime=dos_clock();
	cpifaceSession->InPause = 0;
	pausefadedirect=0;

	mpegGetInfo(&inf);
	mpeglen=inf.len;
	mpegrate=inf.rate;

	ID3InfoInit (cpifaceSession);

	return errOk;
}

struct cpifaceplayerstruct mpegPlayer = {"[MPEG, libmad plugin]", mpegOpenFile, mpegCloseFile};
struct linkinfostruct dllextinfo = {.name = "playmp2", .desc = "OpenCP Audio MPEG Player (c) 1994-'22 Stian Skjelstad, Niklas Beisert & Tammo Hinrichs", .ver = DLLVERSION, .size = 0};
