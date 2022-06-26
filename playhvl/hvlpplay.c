/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <errno.h>
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
#include "player.h"
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

static void startpausefade (void)
{
	if (plPause)
	{
		starttime = starttime + dos_clock () - pausetime;
	}

	if (pausefadedirect)
	{
		if (pausefadedirect < 0)
		{
			plPause = 1;
		}
		pausefadestart = 2 * dos_clock () - DOS_CLK_TCK - pausefadestart;
	} else {
		pausefadestart = dos_clock ();
	}

	if (plPause)
	{
		hvlPause ( plPause = 0 );
		pausefadedirect = 1;
	} else
		pausefadedirect = -1;
}

static void dopausefade (void)
{
	int16_t i;
	if (pausefadedirect>0)
	{
		i=(dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i<0)
			i=1;
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
			hvlPause(plPause=1);
			mcpSetMasterPauseFadeParameters (64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetMasterPauseFadeParameters (i);
}

static void hvlDrawGStrings (void)
{
	int     row,     rows;
	int   order,   orders;
	int subsong, subsongs;
	int tempo;
	int speedmult;

	mcpDrawGStrings ();

	hvlGetStats (&row, &rows, &order, &orders, &subsong, &subsongs, &tempo, &speedmult);

	mcpDrawGStringsTracked
	(
		utf8_8_dot_3,
		utf8_16_dot_3,
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
		0,          /* chan Y */
		-1,         /* amplification */
		0,          /* filter */
		plPause,
		plPause?((pausetime-starttime)/DOS_CLK_TCK):((dos_clock()-starttime)/DOS_CLK_TCK),
		&mdbdata
	);
#warning we are missing the current tune title
//writestring(buf[2], 22, 0x0F, current_hvl_tune?current_hvl_tune->ht_Name:"", 44);
}

static int hvlProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Previous sub-song");
			cpiKeyHelp('>', "Next sub-song");
			cpiKeyHelp(KEY_CTRL_HOME, "Restart song");
			mcpSetProcessKey (key);
			return 0;
		case 'p': case 'P':
			startpausefade();
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			if (plPause)
			{
				starttime=starttime+dos_clock()-pausetime;
			} else {
				pausetime=dos_clock();
			}
			plPause=!plPause;
			hvlPause(plPause);
			break;
		case KEY_CTRL_HOME:
			hvlRestartSong();
			break;
		case '<':
			hvlPrevSubSong();
			break;
		case '>':
			hvlNextSubSong();
			break;
		default:
			return mcpSetProcessKey (key);
	}
	return 1;
}

static int hvlIsLooped(void)
{
	if (pausefadedirect)
	{
		dopausefade();
	}
	hvlSetLoop(fsLoopMods);
	hvlIdle();
	return !fsLoopMods&&hvlLooped();
}

static void hvlCloseFile(void)
{
	hvlClosePlayer();
}

static int hvlOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader, struct cpifaceSessionAPI_t *cpiSessionAPI) /* no loader needed/used by this plugin */
{
	const char *filename;
	uint8_t *filebuf;
	uint64_t filelen;

	if (!file)
	{
		return errFileOpen;
	}

	filelen = file->filesize (file);

	mdbdata = *info;

	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s (%"PRIu64" bytes)...\n", filename, filelen);
	utf8_XdotY_name ( 8, 3, utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, utf8_16_dot_3, filename);

	if (filelen < 14)
	{
		fprintf (stderr, "hvlOpenFile: file too small\n");
		return errGen;
	}
	if (filelen > (1024*1024))
	{
		fprintf (stderr, "hvlOpenFile: file too big\n");
		return errGen;
	}

	filebuf = malloc (filelen);
	if (!filebuf)
	{
		fprintf (stderr, "hvlOpenFile: malloc(%ld) failed\n", (long)filelen);
		return errAllocMem;
	}
	if (file->read (file, filebuf, filelen) != filelen)
	{
		fprintf (stderr, "hvlOpenFile: error reading file: %s\n", strerror(errno));
		free (filebuf);
		return errFileRead;
	}

	hvlOpenPlayer (filebuf, filelen, file, cpiSessionAPI);
	free (filebuf);
	if (!current_hvl_tune)
	{
		return errGen;
	}

	plIsEnd=hvlIsLooped;
	plProcessKey=hvlProcessKey;
	plDrawGStrings=hvlDrawGStrings;

	starttime=dos_clock();
	plPause=0;
	pausefadedirect=0;
	cpiSessionAPI->PhysicalChannelCount = ht->ht_Channels;
	cpiSessionAPI->LogicalChannelCount = ht->ht_Channels;
	plIdle=hvlIdle;
	plSetMute=hvlMute;
	plGetPChanSample=hvlGetChanSample;
	plUseDots(hvlGetDots);
	hvlInstSetup ();
	hvlChanSetup ();
	hvlTrkSetup ();

	return errOk;
}

struct cpifaceplayerstruct hvlPlayer = {"[HivelyTracker plugin]", hvlOpenFile, hvlCloseFile};
struct linkinfostruct dllextinfo = {.name = "playhvl", .desc = "OpenCP HVL Player (c) 2019-'22 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
