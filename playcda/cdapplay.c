/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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

static void startpausefade(void)
{
	if (pausefadedirect)
	{
		if (pausefadedirect<0)
			plPause=1;
		pausefadestart=2*dos_clock()-DOS_CLK_TCK-pausefadestart;
	} else
		pausefadestart=dos_clock();

	if (plPause)
	{
		plChanChanged=1;
		plPause=0;
		cdUnpause ();
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
			plPause=1;
			cdPause();
			plChanChanged=1;
			mcpSetFadePars(64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetFadePars(i);
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

static void cdaDrawGStrings(uint16_t (*buf)[CONSOLE_MAX_X])
{
	int trackno;
	char timestr[9];

	struct cdStat stat;

	mcpDrawGStrings(buf);

	cdGetStatus(&stat);

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
#if 0
		memset(buf[0]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
#endif
		memset(buf[1]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[2]+80, 0, (plScrWidth-80)*sizeof(uint16_t));

#if 0
		writestring(buf[0], 0, 0x09, " vol: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 15);
		writestring(buf[0], 15, 0x09, " srnd: \xfa  pan: l\xfa\xfa\xfam\xfa\xfa\xfar  bal: l\xfa\xfa\xfam\xfa\xfa\xfar ", 41);
		writestring(buf[0], 56, 0x09, " spd: ---% \x1D ptch: ---% ", 24);
		writestring(buf[0], 6, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+4)>>3);
		writestring(buf[0], 22, 0x0F, srnd?"x":"o", 1);
		if (((pan+70)>>4)==4)
			writestring(buf[0], 34, 0x0F, "m", 1);
		else {
			writestring(buf[0], 30+((pan+70)>>4), 0x0F, "r", 1);
			writestring(buf[0], 38-((pan+70)>>4), 0x0F, "l", 1);
		}
		writestring(buf[0], 46+((bal+70)>>4), 0x0F, "I", 1);
		_writenum(buf[0], 62, 0x0F, speed*100/256, 10, 3);
		_writenum(buf[0], 75, 0x0F, speed*100/256, 10, 3);
#endif

		writestring(buf[1], 0, 0x09, " mode: ....... start:   :..:..  pos:   :..:..  length:   :..:..  size: ...... kb", plScrWidth);
		writestring(buf[1], 7, 0x0F, cdpPlayMode?"disk   ":"track  ", 7);
		if (cdpViewSectors)
		{
			writenum(buf[1], 22, 0x0F, TOC.track[TOC.starttrack].lba_addr, 10, 8, 0);
			writenum(buf[1], 37, 0x0F, stat.position - TOC.track[TOC.starttrack].lba_addr, 10, 8, 0);
			writenum(buf[1], 55, 0x0F, TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr, 10, 8, 0);
		} else {
			writestring(buf[1], 22, 0x0F, gettimestr(TOC.track[TOC.starttrack].lba_addr, timestr), 8);
			writestring(buf[1], 37, 0x0F, gettimestr(stat.position - TOC.track[TOC.starttrack].lba_addr, timestr), 8);
			writestring(buf[1], 55, 0x0F, gettimestr(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr, timestr), 8);
		}
		_writenum(buf[1], 71, 0x0F, (TOC.track[TOC.lasttrack+1].lba_addr-TOC.track[TOC.starttrack].lba_addr)*147/64, 10, 6);

		writestring(buf[2], 0, 0x09, "track: ..      start:   :..:..  pos:   :..:..  length:   :..:..  size: ...... kb", plScrWidth);
		_writenum(buf[2], 7, 0x0F, trackno, 10, 2);
		if (cdpViewSectors)
		{
			writenum(buf[2], 22, 0x0F, TOC.track[trackno].lba_addr, 10, 8, 0);
			writenum(buf[2], 37, 0x0F, stat.position - TOC.track[trackno].lba_addr, 10, 8, 0);
			writenum(buf[2], 55, 0x0F, TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr, 10, 8, 0);
		} else {
			writestring(buf[2], 22, 0x0F, gettimestr(TOC.track[trackno].lba_addr, timestr), 8);
			writestring(buf[2], 37, 0x0F, gettimestr(stat.position - TOC.track[trackno].lba_addr, timestr), 8);
			writestring(buf[2], 55, 0x0F, gettimestr(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr, timestr), 8);
		}
		_writenum(buf[2], 71, 0x0F, (TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr)*147/64, 10, 6);
	} else {
#if 0
		memset(buf[0]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
#endif
		memset(buf[1]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[2]+128, 0, (plScrWidth-128)*sizeof(uint16_t));

#if 0
		writestring(buf[0], 0, 0x09, "    volume: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa  ", 30);
		writestring(buf[0], 30, 0x09, " surround: \xfa   panning: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar   balance: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar  ", 72);
		writestring(buf[0], 102, 0x09,  " speed: ---% \x1D pitch: ---%    ", 30);
		writestring(buf[0], 12, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+2)>>2);
		writestring(buf[0], 41, 0x0F, srnd?"x":"o", 1);
		if (((pan+68)>>3)==8)
			writestring(buf[0], 62, 0x0F, "m", 1);
		else {
			writestring(buf[0], 54+((pan+68)>>3), 0x0F, "r", 1);
			writestring(buf[0], 70-((pan+68)>>3), 0x0F, "l", 1);
		}
		writestring(buf[0], 83+((bal+68)>>3), 0x0F, "I", 1);
		_writenum(buf[0], 110, 0x0F, speed*100/256, 10, 3);
		_writenum(buf[0], 124, 0x0F, speed*100/256, 10, 3);
#endif

		writestring(buf[1],  0, 0x09, "      mode: .......    start:   :..:..     pos:   :..:..     length:   :..:..     size: ...... kb", plScrWidth);
		writestring(buf[1], 12, 0x0F, cdpPlayMode?"disk   ":"track  ", 7);
		if (cdpViewSectors)
		{
			writenum(buf[1], 30, 0x0F, TOC.track[TOC.starttrack].lba_addr, 10, 8, 0);
			writenum(buf[1], 48, 0x0F, stat.position - TOC.track[TOC.starttrack].lba_addr, 10, 8, 0);
			writenum(buf[1], 69, 0x0F, TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr, 10, 8, 0);
		} else {
			writestring(buf[1], 30, 0x0F, gettimestr(TOC.track[TOC.starttrack].lba_addr, timestr), 8);
			writestring(buf[1], 48, 0x0F, gettimestr(stat.position - TOC.track[TOC.starttrack].lba_addr, timestr), 8);
			writestring(buf[1], 69, 0x0F, gettimestr(TOC.track[TOC.lasttrack + 1].lba_addr - TOC.track[TOC.starttrack].lba_addr, timestr), 8);
		}
		_writenum(buf[1], 88, 0x0F, (TOC.track[TOC.lasttrack+1].lba_addr-TOC.track[TOC.starttrack].lba_addr)*147/64, 10, 6);

		writestring(buf[2],  0, 0x09, "     track: ..         start:   :..:..     pos:   :..:..     length:   :..:..     size: ...... kb", plScrWidth);
		_writenum(buf[2], 12, 0x0F, trackno, 10, 2);
		if (cdpViewSectors)
		{
			writenum(buf[2], 30, 0x0F, TOC.track[trackno].lba_addr, 10, 8, 0);
			writenum(buf[2], 48, 0x0F, stat.position - TOC.track[trackno].lba_addr, 10, 8, 0);
			writenum(buf[2], 69, 0x0F, TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr, 10, 8, 0);
		} else {
			writestring(buf[2], 30, 0x0F, gettimestr(TOC.track[trackno].lba_addr, timestr), 8);
			writestring(buf[2], 48, 0x0F, gettimestr(stat.position - TOC.track[trackno].lba_addr, timestr), 8);
			writestring(buf[2], 69, 0x0F, gettimestr(TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr, timestr), 8);
		}
		_writenum(buf[2], 88, 0x0F, (TOC.track[trackno+1].lba_addr - TOC.track[trackno].lba_addr)*147/64, 10, 6);
	}
}

static int cdaProcessKey(uint16_t key)
{
	int i;
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
			cpiKeyHelp('>', "Jump track forward");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump track back");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Jump track forward");
			mcpSetProcessKey (key);
			return 0;
		case 'p': case 'P':
			startpausefade();
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			plPause=!plPause;
			if (plPause)
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
			//    strcpy(name, "DISK");
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
				for (i=TOC.lasttrack; i>=TOC.starttrack; i--)
				{
					if (newpos < TOC.track[i].lba_addr)
					{
						break;
					}
				}
				if (!i)
				{
					i = 1;
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
			if (cdpPlayMode)
			{
				newpos = TOC.track[cdpTrackNum].lba_addr;
				setnewpos = 1;
			} else {
				for (i=TOC.lasttrack; i>=TOC.starttrack; i--)
				{
					if (newpos < TOC.track[i].lba_addr)
					{
						break;
					}
				}
				if (i <= 2)
				{
					i = 1;
				} else {
					i--;
				}
				newpos = TOC.track[i].lba_addr;
				setnewpos = 1;
			}
			break;
		case KEY_CTRL_RIGHT:
		case '>':
			if (cdpPlayMode)
			{
				for (i=TOC.lasttrack; i>=TOC.starttrack; i--)
				{
					if (newpos < TOC.track[i].lba_addr)
					{
						break;
					}
				}
				if (i <= TOC.lasttrack)
				{
					break;
				}
				i++;
				newpos = TOC.track[i].lba_addr;
				setnewpos = 1;
			}
			break;

		case KEY_CTRL_HOME:
			newpos = 0;
			setnewpos = 1;
			break;

		default:
			return mcpSetProcessKey (key);
	}
	return 1;
}

static int cdaLooped(void)
{
	struct cdStat stat;

	if (pausefadedirect)
		dopausefade();

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

static void cdaCloseFile(void)
{
	cdClose();
}

static int cdaOpenFile (struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
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

	if (!strcmp (name, "DISK.CDA"))
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
	plPause=0;

	plIsEnd=cdaLooped;
	plProcessKey=cdaProcessKey;
	plDrawGStrings=cdaDrawGStrings;
	plGetMasterSample=plrGetMasterSample;
	plGetRealMasterVolume=plrGetRealMasterVolume;

	if (cdOpen(start, stop - start, file))
		return -1;

	pausefadedirect=0;

	return errOk;
}

struct cpifaceplayerstruct cdaPlayer = {cdaOpenFile, cdaCloseFile};
char *dllinfo = "";
struct linkinfostruct dllextinfo = {.name = "playcda", .desc = "OpenCP CDA Player (c) 1995-09 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
