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

static struct ioctl_cdrom_readtoc_request_t TOC;
static unsigned char cdpPlayMode; /* 1 = disk, 0 = track */
static uint8_t cdpTrackNum; /* used in track-mode */

static int cdpViewSectors; /* view-option */
static signed long newpos; /* skip to */
static unsigned char setnewpos; /* and the fact we should skip */

static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;

static void startpausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
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
		cpifaceSession->InPause = 0;
		cdUnpause ();
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
			cpifaceSession->InPause = 1;
			cdPause();
			mcpSetMasterPauseFadeParameters (64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetMasterPauseFadeParameters (i);
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

	if (plScrWidth<128)
	{
		//writestring(buf[1], 0, 0x09, " mode: ....... start:   :..:..  pos:   :..:..  length:   :..:..  size: ...... kb", plScrWidth);
		displaystr (2,  0, 0x09, " mode: ", 7);
		displaystr (2,  7, 0x0f, cdpPlayMode?"disk   ":"track  ", 7);
		displaystr (2, 14, 0x09, " start: ", 8);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)TOC.track[TOC.starttrack].lba_addr);
		} else {
			gettimestr(TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		displaystr (2, 22, 0x0f, timestr, 8);
		displaystr (2, 30, 0x09, "  pos: ", 7);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(stat.position - TOC.track[TOC.starttrack].lba_addr));
		} else {
			gettimestr(stat.position - TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		displaystr (2, 37, 0x0f, timestr, 8);
		displaystr (2, 45, 0x09, "  length: ", 10);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr));
		} else {
			gettimestr(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		displaystr (2, 55, 0x0f, timestr, 8);
		displaystr (2, 63, 0x09, "  size: ", 8);
		snprintf (timestr, sizeof (timestr), "%6" PRId32, (uint32_t)((TOC.track[TOC.lasttrack+1].lba_addr-TOC.track[TOC.starttrack].lba_addr)*147/64));
		displaystr (2, 71, 0x0f, timestr, 6);
		displaystr (2, 77, 0x09, " kb", plScrWidth - 77);

		//writestring(buf[2], 0, 0x09, "track: ..      start:   :..:..  pos:   :..:..  length:   :..:..  size: ...... kb", plScrWidth);
		displaystr (3,  0, 0x09, "track: ", 7);
		snprintf (timestr, sizeof (timestr), "%2d", trackno);
		displaystr (3,  7, 0x0f, timestr, 2);
		displaystr (3,  9, 0x09, "      start: ", 13);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[trackno].lba_addr));
		} else {
			gettimestr(TOC.track[trackno].lba_addr, timestr);
		}
		displaystr (3, 22, 0x0f, timestr, 8);
		displaystr (3, 30, 0x09, "  pos: ", 7);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(stat.position - TOC.track[trackno].lba_addr));
		} else {
			gettimestr(stat.position - TOC.track[trackno].lba_addr, timestr);
		}
		displaystr (3, 37, 0x0f, timestr, 8);
		displaystr (3, 45, 0x09, "  length: ", 10);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr));
		} else {
			gettimestr(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr, timestr);
		}
		displaystr (3, 55, 0x0f, timestr, 8);
		displaystr (3, 63, 0x09, "  size: ", 8);
		snprintf (timestr, sizeof (timestr), "%6" PRId32, (uint32_t)((TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr)*147/64));
		displaystr (3, 71, 0x0f, timestr, 6);
		displaystr (3, 77, 0x09, " kb", plScrWidth - 77);
	} else {
		//writestring(buf[1],  0, 0x09, "      mode: .......    start:   :..:..     pos:   :..:..     length:   :..:..     size: ...... kb", plScrWidth);
		displaystr (2,  0, 0x09, "      mode: ", 12);
		displaystr (2, 12, 0x0f, cdpPlayMode?"disk   ":"track  ", 7);
		displaystr (2, 19, 0x09, "    start: ", 11);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)TOC.track[TOC.starttrack].lba_addr);
		} else {
			gettimestr(TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		displaystr (2, 30, 0x0f, timestr, 8);
		displaystr (2, 38, 0x09, "     pos: ", 10);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(stat.position - TOC.track[TOC.starttrack].lba_addr));
		} else {
			gettimestr(stat.position - TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		displaystr (2, 48, 0x0f, timestr, 8);
		displaystr (2, 56, 0x09, "     length: ", 13);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr));
		} else {
			gettimestr(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr, timestr);
		}
		displaystr (2, 69, 0x0f, timestr, 8);
		displaystr (2, 77, 0x09, "     size: ", 11);
		snprintf (timestr, sizeof (timestr), "%6" PRId32, (uint32_t)((TOC.track[TOC.lasttrack+1].lba_addr-TOC.track[TOC.starttrack].lba_addr)*147/64));
		displaystr (2, 88, 0x0f, timestr, 6);
		displaystr (2, 94, 0x09, " kb", plScrWidth - 94);

		//writestring(buf[2],  0, 0x09, "     track: ..         start:   :..:..     pos:   :..:..     length:   :..:..     size: ...... kb", plScrWidth);
		displaystr (3,  0, 0x09, "     track: ", 12);
		snprintf (timestr, sizeof (timestr), "%2d", trackno);
		displaystr (3, 12, 0x0f, timestr, 2);
		displaystr (3, 14, 0x09, "         start: ", 16);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[trackno].lba_addr));
		} else {
			gettimestr(TOC.track[trackno].lba_addr, timestr);
		}
		displaystr (3, 30, 0x0f, timestr, 8);
		displaystr (3, 38, 0x09, "     pos: ", 10);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(stat.position - TOC.track[trackno].lba_addr));
		} else {
			gettimestr(stat.position - TOC.track[trackno].lba_addr, timestr);
		}
		displaystr (3, 48, 0x0f, timestr, 8);
		displaystr (3, 56, 0x09, "     length: ", 13);
		if (cdpViewSectors)
		{
			snprintf (timestr, sizeof (timestr), "%8" PRId32, (uint32_t)(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr));
		} else {
			gettimestr(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr, timestr);
		}
		displaystr (3, 69, 0x0f, timestr, 8);
		displaystr (3, 77, 0x09, "     size: ", 11);
		snprintf (timestr, sizeof (timestr), "%6" PRId32, (uint32_t)((TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr)*147/64));
		displaystr (3, 88, 0x0f, timestr, 6);
		displaystr (3, 94, 0x09, " kb", plScrWidth - 94);
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
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('t', "Toggle sector view mode");
			cpiKeyHelp(KEY_DOWN, "Jump back (small)");
			cpiKeyHelp(KEY_UP, "Jump forward (small)");
			cpiKeyHelp(KEY_CTRL_DOWN, "Jump back (big)");
			cpiKeyHelp(KEY_CTRL_UP, "Jump forward (big)");
			cpiKeyHelp(KEY_LEFT, "Jump back");
			cpiKeyHelp(KEY_RIGHT, "Jump forward");
			cpiKeyHelp(KEY_HOME, "Jump to start of track");
			cpiKeyHelp(KEY_CTRL_HOME, "Jump to start of disc");
			cpiKeyHelp('<', "Jump track back");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump track back");
			if (cdpPlayMode)
			{
				cpiKeyHelp('>', "Jump track forward");
				cpiKeyHelp(KEY_CTRL_RIGHT, "Jump track forward");
			}
			return 0;
		case 'p': case 'P':
			startpausefade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			cpifaceSession->InPause = !cpifaceSession->InPause;
			if (cpifaceSession->InPause)
				cdPause();
			else
				cdUnpause();

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

static int cdaLooped (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct cdStat stat;

	if (pausefadedirect)
	{
		dopausefade (cpifaceSession);
	}

	cdSetLoop(fsLoopMods);

	cdIdle();

	cdGetStatus(&stat);

	/*
	if (status->error&STATUS_ERROR)
		return 1;
	*/

	if (stat.looped)
		return 1;

	if (setnewpos)
	{
		if (newpos<0)
			newpos=0;
#if 0
		if (newpos>=(signed)length)
		{
			if (fsLoopMods)
				newpos=0;
			else
				return 1;
		}
#endif
		cdJump(newpos /*, length-newpos*/);
		setnewpos=0;
	} else
		newpos=stat.position;

	return 0;
}

static void cdaCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cdClose();
}

static int cdaOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
{
	const char *name = 0;
	int32_t start = -1;
	int32_t stop = -1;

	if (file->ioctl (file, IOCTL_CDROM_READTOC, &TOC))
	{
		return -1;
	}

	name = file->filename_override (file);
	if (!name)
	{
		dirdbGetName_internalstr (file->dirdb_ref, &name);
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

			return -1;
		}
		if (TOC.track[cdpTrackNum].is_data)
		{
			return -1;
		}
		start = TOC.track[cdpTrackNum].lba_addr;
		stop = TOC.track[cdpTrackNum+1].lba_addr;
		cdpPlayMode=0;
	} else {
		return -1;
	}

	newpos=start;
	setnewpos=0;
	cpifaceSession->InPause = 0;

	plIsEnd=cdaLooped;
	cpifaceSession->ProcessKey = cdaProcessKey;
	cpifaceSession->DrawGStrings = cdaDrawGStrings;

	if (cdOpen(start, stop - start, file, cpifaceSession))
		return -1;

	pausefadedirect=0;

	return errOk;
}

struct cpifaceplayerstruct cdaPlayer = {"[CDROM Audio plugin]", cdaOpenFile, cdaCloseFile};
char *dllinfo = "";
struct linkinfostruct dllextinfo = {.name = "playcda", .desc = "OpenCP CDA Player (c) 1995-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
