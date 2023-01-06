/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Interface routines for Audio CD player
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
 *  -ss040907   Stian Skjelstad <stian@nixia.no>
 *    -removed cdReadDir
 *    -Linux related changes
 */

#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "cdaudio.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "filesel/cdrom.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "cdatype.h"

static struct ioctl_cdrom_readtoc_request_t TOC;
static unsigned char cdpPlayMode; /* 1 = disk, 0 = track */
static uint8_t cdpTrackNum; /* used in track-mode */

static int cdpViewSectors; /* view-option */
static signed long newpos; /* skip to */
static unsigned char setnewpos; /* and the fact we should skip */

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
		cdPause (cpifaceSession->InPause = 0);
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
			cdPause (cpifaceSession->InPause = 1);
			return;
		}
	}
	cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, i);
}
static char *gettimestr(unsigned long s, char *time)
{
	unsigned char csec, sec, min;
	csec=(s%75)*4/3;
	time[8]=0;
	time[7]='0'+csec%10;
	time[6]='0'+csec/10;
	sec=(s/75)%60;
	time[5]=':';
	time[4]='0'+sec%10;
	time[3]='0'+sec/10;
	min=s/(75*60);
	time[2]=':';
	time[1]='0'+min%10;
	time[0]='0'+min/10;
	return time;
}

static void cdaDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int trackno;
	char timestr[9];

	struct cdStat stat;

	cdGetStatus (&stat);

	for (trackno=1; trackno<=TOC.lasttrack; trackno++)
	{
		if (stat.position<TOC.track[trackno].lba_addr)
		{
			break;
		}
	}
	trackno--;

	if (cpifaceSession->console->TextWidth < 128)
	{
		cpifaceSession->console->Driver->DisplayStr (2,  0, 0x09, " mode: ", 7);
		cpifaceSession->console->Driver->DisplayStr (2,  7, 0x0f, cdpPlayMode?"disk   ":"track  ", 7);
		cpifaceSession->console->Driver->DisplayStr (2, 14, 0x09, " start: ", 8);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)TOC.track[TOC.starttrack].lba_addr);
		} else {
			gettimestr(TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (2, 22, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (2, 30, 0x09, "  pos: ", 7);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(stat.position - TOC.track[TOC.starttrack].lba_addr));
		} else {
			gettimestr(stat.position - TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (2, 37, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (2, 45, 0x09, "  length: ", 10);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr));
		} else {
			gettimestr(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (2, 55, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (2, 63, 0x09, "  size: ", 8);
		snprintf (timestr, sizeof (timestr), "%6" PRId32, (uint32_t)((TOC.track[TOC.lasttrack+1].lba_addr-TOC.track[TOC.starttrack].lba_addr)*147/64));
		cpifaceSession->console->Driver->DisplayStr (2, 71, 0x0f, timestr, 6);
		cpifaceSession->console->Driver->DisplayStr (2, 77, 0x09, " kb", cpifaceSession->console->TextWidth - 77);

		cpifaceSession->console->Driver->DisplayStr (3,  0, 0x09, "track: ", 7);
		snprintf (timestr, sizeof (timestr), "%2d", trackno);
		cpifaceSession->console->Driver->DisplayStr (3,  7, 0x0f, timestr, 2);
		cpifaceSession->console->Driver->DisplayStr (3,  9, 0x09, "      start: ", 13);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[trackno].lba_addr));
		} else {
			gettimestr(TOC.track[trackno].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (3, 22, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (3, 30, 0x09, "  pos: ", 7);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(stat.position - TOC.track[trackno].lba_addr));
		} else {
			gettimestr(stat.position - TOC.track[trackno].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (3, 37, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (3, 45, 0x09, "  length: ", 10);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr));
		} else {
			gettimestr(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (3, 55, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (3, 63, 0x09, "  size: ", 8);
		snprintf (timestr, sizeof (timestr), "%6" PRId32, (uint32_t)((TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr)*147/64));
		cpifaceSession->console->Driver->DisplayStr (3, 71, 0x0f, timestr, 6);
		cpifaceSession->console->Driver->DisplayStr (3, 77, 0x09, " kb", cpifaceSession->console->TextWidth - 77);
	} else {
		cpifaceSession->console->Driver->DisplayStr (2,  0, 0x09, "      mode: ", 12);
		cpifaceSession->console->Driver->DisplayStr (2, 12, 0x0f, cdpPlayMode?"disk   ":"track  ", 7);
		cpifaceSession->console->Driver->DisplayStr (2, 19, 0x09, "    start: ", 11);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)TOC.track[TOC.starttrack].lba_addr);
		} else {
			gettimestr(TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (2, 30, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (2, 38, 0x09, "     pos: ", 10);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(stat.position - TOC.track[TOC.starttrack].lba_addr));
		} else {
			gettimestr(stat.position - TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (2, 48, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (2, 56, 0x09, "     length: ", 13);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr));
		} else {
			gettimestr(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (2, 69, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (2, 77, 0x09, "     size: ", 11);
		snprintf (timestr, sizeof (timestr), "%6" PRId32, (uint32_t)((TOC.track[TOC.lasttrack+1].lba_addr-TOC.track[TOC.starttrack].lba_addr)*147/64));
		cpifaceSession->console->Driver->DisplayStr (2, 88, 0x0f, timestr, 6);
		cpifaceSession->console->Driver->DisplayStr (2, 94, 0x09, " kb", cpifaceSession->console->TextWidth - 94);

		cpifaceSession->console->Driver->DisplayStr (3,  0, 0x09, "     track: ", 12);
		snprintf (timestr, sizeof (timestr), "%2d", trackno);
		cpifaceSession->console->Driver->DisplayStr (3, 12, 0x0f, timestr, 2);
		cpifaceSession->console->Driver->DisplayStr (3, 14, 0x09, "         start: ", 16);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[trackno].lba_addr));
		} else {
			gettimestr(TOC.track[trackno].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (3, 30, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (3, 38, 0x09, "     pos: ", 10);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(stat.position - TOC.track[trackno].lba_addr));
		} else {
			gettimestr(stat.position - TOC.track[trackno].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (3, 48, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (3, 56, 0x09, "     length: ", 13);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr));
		} else {
			gettimestr(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr, timestr);
		}
		cpifaceSession->console->Driver->DisplayStr (3, 69, 0x0f, timestr, 8);
		cpifaceSession->console->Driver->DisplayStr (3, 77, 0x09, "     size: ", 11);
		snprintf (timestr, sizeof (timestr), "%6" PRId32, (uint32_t)((TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr)*147/64));
		cpifaceSession->console->Driver->DisplayStr (3, 88, 0x0f, timestr, 6);
		cpifaceSession->console->Driver->DisplayStr (3, 94, 0x09, " kb", cpifaceSession->console->TextWidth - 94);
	}
}

static int cdaProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	int i;
	struct cdStat stat;

	cdGetStatus(&stat);
	newpos = stat.position;

	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('t', "Toggle sector view mode");
			cpifaceSession->KeyHelp (KEY_DOWN, "Jump back (small)");
			cpifaceSession->KeyHelp (KEY_UP, "Jump forward (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_DOWN, "Jump back (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_UP, "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_LEFT, "Jump back");
			cpifaceSession->KeyHelp (KEY_RIGHT, "Jump forward");
			cpifaceSession->KeyHelp (KEY_HOME, "Jump to start of track");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Jump to start of disc");
			cpifaceSession->KeyHelp ('<', "Jump track back");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Jump track back");
			if (cdpPlayMode)
			{
				cpifaceSession->KeyHelp ('>', "Jump track forward");
				cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Jump track forward");
			}
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
				cdPause (cpifaceSession->InPause = 0);
			} else {
				cdPause (cpifaceSession->InPause = 1);
			}

			break;
		case 't':
			cdpViewSectors=!cdpViewSectors;
			break;
/*
		case 0x2000:
			cdpPlayMode=1;
			setnewpos=0;
			basesec=cdpTrackStarts[0];
			length=cdpTrackStarts[cdpTrackNum]-basesec;
			//    strcpy(name, "DISC");
			break;
*/
		case KEY_DOWN:
			newpos-=75;
			setnewpos=1;
			break;
		case KEY_UP:
			newpos+=75;
			setnewpos=1;
			break;
		case KEY_LEFT:
			newpos-=75*10;
			setnewpos=1;
			break;
		case KEY_RIGHT:
			newpos+=75*10;
			setnewpos=1;
			break;
		case KEY_HOME:
			if (!cdpPlayMode)
			{
				newpos = TOC.track[cdpTrackNum].lba_addr;
				setnewpos=1;
			} else {
				for (i=TOC.starttrack; i<=TOC.lasttrack; i++)
				{
					if (newpos < TOC.track[i].lba_addr)
					{
						break;
					}
				}
				i-=1;
				if (i <= TOC.starttrack)
				{
					i = TOC.starttrack;
				}
				newpos = TOC.track[i].lba_addr;
				setnewpos = 1;
			}
			break;
		case KEY_CTRL_UP:
			newpos-=60*75;
			setnewpos=1;
			break;
		case KEY_CTRL_DOWN:
			newpos+=60*75;
			setnewpos=1;
			break;
		case KEY_CTRL_LEFT:
		case '<':
			if (!cdpPlayMode)
			{
				newpos = TOC.track[cdpTrackNum].lba_addr;
				setnewpos = 1;
			} else {
				for (i=TOC.starttrack; i<=TOC.lasttrack; i++)
				{
					if (newpos < TOC.track[i].lba_addr)
					{
						break;
					}
				}
				i-=2;
				if (i <= TOC.starttrack)
				{
					i = TOC.starttrack;
				}
				newpos = TOC.track[i].lba_addr;
				setnewpos = 1;
			}
			break;
		case KEY_CTRL_RIGHT:
		case '>':
			if (cdpPlayMode)
			{
				for (i=TOC.starttrack; i<=TOC.lasttrack; i++)
				{
					if (newpos < TOC.track[i].lba_addr)
					{
						break;
					}
				}
				if (i > TOC.lasttrack)
				{
					break;
				}
				newpos = TOC.track[i].lba_addr;
				setnewpos = 1;
			}
			break;

		case KEY_CTRL_HOME:
			if (!cdpPlayMode)
			{
				newpos = TOC.track[cdpTrackNum].lba_addr;
				setnewpos = 1;
			} else {
				newpos = 0;
				setnewpos = 1;
			}
			break;

		default:
			return 0;
	}
	return 1;
}

static int cdaLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	struct cdStat stat;

	if (pausefadedirection)
	{
		dopausefade (cpifaceSession);
	}

	cdSetLoop (LoopMod);

	cdIdle (cpifaceSession);

	cdGetStatus(&stat);

	if (stat.looped)
		return 1;

	if (setnewpos)
	{
		if (newpos < 0)
		{
			newpos = 0;
		}
		cdJump (cpifaceSession, newpos);
		setnewpos = 0;
	} else {
		newpos = stat.position;
	}

	return 0;
}

static void cdaCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cdClose (cpifaceSession);
}

static int cdaOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	const char *name = 0;
	int32_t start = -1;
	int32_t stop = -1;
	int retval;

	if (file->ioctl (file, IOCTL_CDROM_READTOC, &TOC))
	{
		return errFormSig;
	}

	name = file->filename_override (file);
	if (!name)
	{
		cpifaceSession->dirdb->GetName_internalstr (file->dirdb_ref, &name);
	}

	if (!strcmp (name, "DISC.CDA"))
	{
		int i;
		for (i=TOC.starttrack; i <= TOC.lasttrack; i++)
		{
			if (!TOC.track[i].is_data)
			{
				if (start < 0)
				{
					cdpTrackNum = i;
					start = TOC.track[i].lba_addr;
				}
				stop = TOC.track[i+1].lba_addr;
			}
		}
		cdpPlayMode=1;
	} else if ((!strncmp (name, "TRACK", 5)) && (strlen(name) >=7))
	{
		cdpTrackNum = (name[5] - '0') * 10 + (name[6] - '0');
		if ((cdpTrackNum < 1) || (cdpTrackNum > 99))
		{

			return errFormSig;
		}
		if (TOC.track[cdpTrackNum].is_data)
		{
			return errFormStruc;
		}
		start = TOC.track[cdpTrackNum].lba_addr;
		stop = TOC.track[cdpTrackNum+1].lba_addr;
		cdpPlayMode=0;
	} else {
		return errFormStruc;
	}

	newpos = start;
	setnewpos=0;
	cpifaceSession->InPause = 0;

	cpifaceSession->IsEnd = cdaLooped;
	cpifaceSession->ProcessKey = cdaProcessKey;
	cpifaceSession->DrawGStrings = cdaDrawGStrings;

	if ((retval = cdOpen(start, stop - start, file, cpifaceSession)))
	{
		return retval;
	}

	pausefadedirection = 0;

	return errOk;
}

static int cdaPluginInit (struct PluginInitAPI_t *API)
{
	return cda_type_init (API);
}

static void cdaPluginClose (struct PluginCloseAPI_t *API)
{
	cda_type_done (API);
}

const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) cdaPlayer = {"[CDROM Audio plugin]", cdaOpenFile, cdaCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playcda", .desc = "OpenCP CDA Player (c) 1995-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .PluginInit = cdaPluginInit, .PluginClose = cdaPluginClose};
