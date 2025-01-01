/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "wavtype.h"

static unsigned long wavelen;
static unsigned long waverate;

static void wavDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct waveinfo inf;

	wpGetInfo (cpifaceSession, &inf);

	cpifaceSession->drawHelperAPI->GStringsFixedLengthStream
	(
		cpifaceSession,
		inf.pos,
		inf.len,
		1, /* KB */
		inf.opt25,
		inf.opt50,
		(inf.rate << (3 + (!!inf.stereo) + (!!inf.bit16))) / 1000
	);
}

static int wavProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			wpSetPos (cpifaceSession, wpGetPos (cpifaceSession) - waverate);
			break;
		case KEY_CTRL_DOWN:
			wpSetPos (cpifaceSession, wpGetPos (cpifaceSession) + waverate);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			{
				uint32_t pos = wpGetPos (cpifaceSession);
				uint32_t newpos = pos -(wavelen>>5);
				if (newpos > pos)
				{
					newpos = 0;
				}
				wpSetPos (cpifaceSession, newpos);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				uint32_t pos = wpGetPos (cpifaceSession);
				uint32_t newpos = pos + (wavelen>>5);
				if ((newpos < pos) || (newpos > wavelen)) /* catch both wrap around (not likely), and overshots */
				{
					newpos = wavelen - 4;
				}
				wpSetPos (cpifaceSession, newpos);
			}
			break;
		case KEY_CTRL_HOME:
			wpSetPos (cpifaceSession, 0);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}


static int wavLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	wpSetLoop (LoopMod);
	wpIdle (cpifaceSession);
	return (!LoopMod) && wpLooped();
}

static void wavCloseFile(struct cpifaceSessionAPI_t *cpifaceSession)
{
	wpClosePlayer(cpifaceSession);
}

static int wavOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *wavf)
{
	const char *filename;
	struct waveinfo inf;
	int retval;

	if (!wavf)
		return errFormStruc;

	cpifaceSession->dirdb->GetName_internalstr (wavf->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[WAVE] preloading %s...\n", filename);

	cpifaceSession->IsEnd = wavLooped;
	cpifaceSession->ProcessKey = wavProcessKey;
	cpifaceSession->DrawGStrings = wavDrawGStrings;

	if ((retval = wpOpenPlayer(wavf, cpifaceSession)))
	{
		return retval;
	}

	cpifaceSession->InPause = 0;

	wpGetInfo(cpifaceSession, &inf);
	wavelen=inf.len;
	waverate=inf.rate;

	return errOk;
}

static int wavPluginInit (struct PluginInitAPI_t *API)
{
	return wav_type_init (API);
}

static void wavPluginClose (struct PluginCloseAPI_t *API)
{
	wav_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct wavPlayer = {"[WAVE plugin]", wavOpenFile, wavCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playwav", .desc = "OpenCP Wave Player (c) 1994-'25 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit=wavPluginInit, .PluginClose=wavPluginClose};
