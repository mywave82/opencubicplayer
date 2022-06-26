/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * WAVPlay interface routines
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -ryg981219 Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -made max amplification 793% (as in module players)
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
#include "wave.h"

static unsigned long wavelen;
static unsigned long waverate;
static time_t starttime;
static time_t pausetime;
static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;
static char utf8_8_dot_3  [12*4+1];  /* UTF-8 ready */
static char utf8_16_dot_3 [20*4+1]; /* UTF-8 ready */
static struct moduleinfostruct mdbdata;

static void startpausefade(void)
{
	if (plPause)
		starttime=starttime+dos_clock()-pausetime;

	if (pausefadedirect)
	{
		if (pausefadedirect<0)
			plPause=1;
		pausefadestart=2*dos_clock()-DOS_CLK_TCK-pausefadestart;
	} else
		pausefadestart=dos_clock();

	if (plPause)
	{
		wpPause(plPause=0);
		pausefadedirect=1;
	} else
		pausefadedirect=-1;
}

static void dopausefade(void)
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
			wpPause(plPause=1);
			mcpSetMasterPauseFadeParameters (64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetMasterPauseFadeParameters (i);
}

static void wavDrawGStrings (void)
{
	struct waveinfo inf;

	mcpDrawGStrings ();

	wpGetInfo (&inf);

	mcpDrawGStringsFixedLengthStream
	(
		utf8_8_dot_3,
		utf8_16_dot_3,
		inf.pos,
		inf.len,
		1, /* KB */
		inf.opt25,
		inf.opt50,
		(inf.rate << (3 + (!!inf.stereo) + (!!inf.bit16))) / 1000,
		plPause,
		plPause?((pausetime-starttime)/DOS_CLK_TCK):((dos_clock()-starttime)/DOS_CLK_TCK),
		&mdbdata
	);
}

static int wavProcessKey(unsigned short key)
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
			startpausefade();
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			if (plPause)
				starttime=starttime+dos_clock()-pausetime;
			else
				pausetime=dos_clock();
			plPause=!plPause;
			wpPause(plPause);
			break;
		case KEY_CTRL_UP:
			wpSetPos(wpGetPos()-waverate);
			break;
		case KEY_CTRL_DOWN:
			wpSetPos(wpGetPos()+waverate);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			{
				uint32_t pos = wpGetPos();
				uint32_t newpos = pos -(wavelen>>5);
				if (newpos > pos)
				{
					newpos = 0;
				}
				wpSetPos(newpos);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				uint32_t pos = wpGetPos();
				uint32_t newpos = pos + (wavelen>>5);
				if ((newpos < pos) || (newpos > wavelen)) /* catch both wrap around (not likely), and overshots */
				{
					newpos = wavelen - 4;
				}
				wpSetPos(newpos);
			}
			break;
		case KEY_CTRL_HOME:
			wpSetPos(0);
			break;
		default:
			return mcpSetProcessKey (key);
	}
	return 1;
}


static int wavLooped(void)
{
	if (pausefadedirect)
		dopausefade();
	wpSetLoop(fsLoopMods);
	wpIdle();
	return !fsLoopMods&&wpLooped();
}

static void wavCloseFile()
{
	wpClosePlayer();
}

static int wavOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *wavf, const char *ldlink, const char *loader, struct cpifaceSessionAPI_t *cpiSessionAPI) /* no loader needed/used by this plugin */
{
	const char *filename;
	struct waveinfo inf;

	if (!wavf)
		return -1;

	mdbdata = *info;
	dirdbGetName_internalstr (wavf->dirdb_ref, &filename);
	fprintf(stderr, "preloading %s...\n", filename);
	utf8_XdotY_name ( 8, 3, utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, utf8_16_dot_3, filename);

	plIsEnd=wavLooped;
	plProcessKey=wavProcessKey;
	plDrawGStrings=wavDrawGStrings;

	if (!wpOpenPlayer(wavf, cpiSessionAPI))
	{
#ifdef INITCLOSE_DEBUG
		fprintf(stderr, "wpOpenPlayer FAILED\n");
#endif
		return -1;
	}

	starttime=dos_clock();
	plPause=0;
	pausefadedirect=0;

	wpGetInfo(&inf);
	wavelen=inf.len;
	waverate=inf.rate;

	return errOk;
}

struct cpifaceplayerstruct wavPlayer = {"[WAVE plugin]", wavOpenFile, wavCloseFile};
struct linkinfostruct dllextinfo = {.name = "playwav", .desc = "OpenCP Wave Player (c) 1994-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
