/* OpenCP Module Player
 * copyright (c) 2019-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * HVLPlay interface routines
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "types.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "hvlpchan.h"
#include "hvlpdots.h"
#include "hvlpinst.h"
#include "hvlplay.h"
#include "hvlptrak.h"
#include "hvltype.h"
#include "player.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static void hvlDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int     row,     rows;
	int   order,   orders;
	int subsong, subsongs;
	int tempo;
	int speedmult;

	hvlGetStats (&row, &rows, &order, &orders, &subsong, &subsongs, &tempo, &speedmult);

	cpifaceSession->drawHelperAPI->GStringsTracked
	(
		cpifaceSession,
		subsong,    /* song X */
		subsongs,   /* song Y */
		row,        /* row X */
		rows-1,     /* row Y */
		order,      /* order X */
		orders-1,   /* order Y */
		tempo,      /* speed - do not ask */
		125*speedmult*4/tempo,/* tempo - do not ask*/
		-1,         /* gvol */
		0,          /* gvol slide direction */
		0,          /* chan X */
		0           /* chan Y */
	);
#warning we are missing the current tune title
//cpifaceSession->conFunc->WriteString (buf[2], 22, 0x0F, current_hvl_tune?current_hvl_tune->ht_Name:"", 44);
}

static int hvlProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('<', "Previous sub-song");
			cpifaceSession->KeyHelp ('>', "Next sub-song");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Restart song");
			return 0;
		case 'p': case 'P':
			cpifaceSession->TogglePauseFade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			cpifaceSession->TogglePause (cpifaceSession);
			break;
		case KEY_CTRL_HOME:
			hvlRestartSong();
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		case '<':
			hvlPrevSubSong();
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		case '>':
			hvlNextSubSong();
			cpifaceSession->ResetSongTimer (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}

static int hvlIsLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	hvlSetLoop (LoopMod);
	hvlIdle (cpifaceSession);
	return (!LoopMod) && hvlLooped();
}

static void hvlCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	hvlClosePlayer (cpifaceSession);
}

static int hvlOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	const char *filename;
	uint8_t *filebuf;
	uint64_t filelen;
	int retval;

	if (!file)
	{
		return errFileOpen;
	}

	filelen = file->filesize (file);

	cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	cpifaceSession->cpiDebug (cpifaceSession, "[HVL] loading %s (%"PRIu64" bytes)...\n", filename, filelen);

	if (filelen < 14)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL] file too small\n");
		return errFormStruc;
	}
	if (filelen > (1024*1024))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL] file too big\n");
		return errFormStruc;
	}

	filebuf = malloc (filelen);
	if (!filebuf)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL] malloc(%lu) failed\n", (unsigned long)filelen);
		return errAllocMem;
	}
	if (file->read (file, filebuf, filelen) != filelen)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[HVL] error reading file");
		free (filebuf);
		return errFileRead;
	}

	retval = hvlOpenPlayer (filebuf, filelen, file, cpifaceSession);
	free (filebuf);
	if (retval)
	{
		return retval;
	}

	cpifaceSession->IsEnd = hvlIsLooped;
	cpifaceSession->ProcessKey = hvlProcessKey;
	cpifaceSession->DrawGStrings = hvlDrawGStrings;

	cpifaceSession->InPause = 0;
	cpifaceSession->PhysicalChannelCount = ht->ht_Channels;
	cpifaceSession->LogicalChannelCount = ht->ht_Channels;
	cpifaceSession->SetMuteChannel = hvlMute;
	cpifaceSession->GetPChanSample = hvlGetChanSample;
	cpifaceSession->UseDots(hvlGetDots);
	hvlInstSetup (cpifaceSession);
	hvlChanSetup (cpifaceSession);
	hvlTrkSetup (cpifaceSession);

	return errOk;
}

static int hvlPluginInit (struct PluginInitAPI_t *API)
{
	return hvl_type_init (API);
}

static void hvlPluginClose (struct PluginCloseAPI_t *API)
{
	hvl_type_done (API);
}

OCP_INTERNAL const struct cpifaceplayerstruct hvlPlayer = {"[HivelyTracker plugin]", hvlOpenFile, hvlCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playhvl", .desc = "OpenCP HVL Player (c) 2019-'25 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = hvlPluginInit, .PluginClose = hvlPluginClose};
