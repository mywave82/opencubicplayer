/* OpenCP Module Player
 * copyright (c) 2005-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OPLPlay interface routines
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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
extern "C"
{
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/pfilesel.h"
#include "filesel/mdb.h"
#include "playopl/oplconfig.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
}
#include "playopl/oplplay.h"
#include "playopl/opltype.h"

static oplTuneInfo globinfo;

static void oplDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	oplpGetGlobInfo(globinfo);

	cpifaceSession->drawHelperAPI->GStringsSongXofY
	(
		cpifaceSession,
		globinfo.currentSong,
		globinfo.songs
	);
}

static int oplProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	struct oplTuneInfo ti;

	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Restart Song");
			cpifaceSession->KeyHelp ('<', "Previous Song");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Previous Song");
			cpifaceSession->KeyHelp ('>', "Next song");
			cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Next song");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			return 0;
		case 'p': case 'P':
			cpifaceSession->TogglePauseFade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			cpifaceSession->TogglePause (cpifaceSession);
			break;
		case KEY_CTRL_HOME:
			oplpGetGlobInfo (ti);
			oplSetSong (cpifaceSession, ti.currentSong);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		case '<':
		case KEY_CTRL_LEFT: /* curses.h can't do these */
			oplpGetGlobInfo (ti);
			oplSetSong (cpifaceSession, ti.currentSong - 1);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		case '>':
		case KEY_CTRL_RIGHT: /* curses.h can't do these */
			oplpGetGlobInfo (ti);
			oplSetSong (cpifaceSession, ti.currentSong + 1);
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
#if 0
		case KEY_CTRL_UP:
			oplSetPos(oplGetPos()-oplrate);
			break;
		case KEY_CTRL_DOWN:
			oplSetPos(oplGetPos()+oplrate);
			break;*/
		case '<':
		case KEY_CTRL_LEFT:
			{
				int skip=opllen>>5;
				if (skip<128*1024)
					skip=128*1024;
				oplSetPos(oplGetPos()-skip);
			}
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			{
				int skip=opllen>>5;
				if (skip<128*1024)
					skip=128*1024;
				oplSetPos(oplGetPos()+skip);
			}
			break;
		case 0x7700: //ctrl-home TODO keys
			oplSetPos(0);
			break;
#endif
		default:
			return 0;
	}
	return 1;
}


static int oplLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	oplSetLoop (LoopMod);
	oplIdle (cpifaceSession);
	return (!LoopMod) && oplIsLooped();
}

static void oplCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	oplClosePlayer (cpifaceSession);
}

static int oplOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	const char *filename;
	size_t buffersize = 16*1024;
	uint8_t *buffer = (uint8_t *)malloc (buffersize);
	size_t bufferfill = 0;
	int retval;

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	{
		int res;
		while (!file->eof(file))
		{
			if (buffersize == bufferfill)
			{
				if (buffersize >= 16*1024*1024)
				{
					cpifaceSession->cpiDebug (cpifaceSession, "[Adplug OPL] %s is bigger than 16 Mb - further loading blocked\n", filename);
					free (buffer);
					return -1;
				}
				buffersize += 16*1024;
				buffer = (uint8_t *)realloc (buffer, buffersize);
			}
			res = file->read (file, buffer + bufferfill, buffersize - bufferfill);
			if (res<=0)
				break;
			bufferfill += res;
		}
	}
	cpifaceSession->cpiDebug (cpifaceSession, "[Adplug OPL] loading %s\n", filename);

	cpifaceSession->IsEnd = oplLooped;
	cpifaceSession->ProcessKey = oplProcessKey;
	cpifaceSession->DrawGStrings = oplDrawGStrings;

	if ((retval = oplOpenPlayer(filename, buffer, bufferfill, file, cpifaceSession)))
	{
		return retval;
	}
	buffer=0;

	cpifaceSession->InPause = 0;

	OPLChanInit (cpifaceSession);
	cpifaceSession->LogicalChannelCount = 18;
	cpifaceSession->SetMuteChannel = oplMute;

	oplpGetGlobInfo(globinfo);

	return errOk;
}

static int oplPluginInit (struct PluginInitAPI_t *API)
{
	opl_config_init (API);
	return opl_type_init (API);
}

static void oplPluginClose (struct PluginCloseAPI_t *API)
{
	opl_config_done (API);
	opl_type_done (API);
}

extern "C"
{
	const cpifaceplayerstruct oplPlayer = {"[AdPlug OPL plugin]", oplOpenFile, oplCloseFile};
	DLLEXTINFO_PLAYBACK_PREFIX_CPP struct linkinfostruct dllextinfo =
	{ /* c++ historically does not support named initializers, and size needs to be writable... */
		/* .name = */ "playopl",
		/* .desc = */ "OpenCP Adplug (OPL) Detection and Player (c) 2005-'23 Stian Skjelstad",
		/* .ver  = */ DLLVERSION,
		/* .sortindex = */ 95,
		/* .PreInit = */ 0,
		/* .Init = */ 0,
		/* .PluginInit = */ oplPluginInit,
		/* .LateInit = */ 0,
		/* .PreClose = */ 0,
		/* .PluginClose = */ oplPluginClose,
		/* .Close = */ 0,
		/* .LateClose = */ 0,
	};
}
