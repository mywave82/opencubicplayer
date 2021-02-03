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
#include <linux/cdrom.h>
#include "types.h"
#include "cdaudio.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/devisamp.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

static unsigned long cdpTrackStarts[CDROM_LEADOUT+1];
static unsigned char cdpFirstTrack;
static unsigned char cdpPlayMode; /* 1 = disk, 0 = track */
static unsigned char cdpTrackNum; /* current track, calculated in GStrings */
static int cdpViewSectors; /* ??? view-option */
static unsigned long basesec; /* current start... usually a cdpTrack[n] value */
static unsigned long length; /* and the length of it */
static signed long newpos; /* skip to */
static unsigned char setnewpos; /* and the fact we should skip */
static char vdev[8];

static int16_t speed;
static char finespeed=8;
static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;
static int cdrom_fd;

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
		cdRestart(cdrom_fd);
		pausefadedirect=1;
	} else
		pausefadedirect=-1;
}

static void normalize(void)
{
	mcpNormalize(0);
	speed=set.speed;
	cdSetSpeed(speed);
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
			cdPause(cdrom_fd);
			plChanChanged=1;
			cdSetSpeed(speed);
			return;
		}
	}
	pausefaderelspeed=i;
	cdSetSpeed(speed*i/64);
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
	int i;
	char timestr[9];

	struct cdStat stat;

	cdGetStatus(cdrom_fd, &stat);


	memset(buf[0]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
	memset(buf[1]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
	memset(buf[2]+80, 0, (plScrWidth-80)*sizeof(uint16_t));

	writestring(buf[0], 0, 0x09, "  mode: ..........  ", 20);
	writestring(buf[0], 8, 0x0F, "cd-audio", 10);
	for (i=1; i<=cdpTrackNum; i++)
		if (stat.position<cdpTrackStarts[i])
			break;

	writestring(buf[0], 20, 0x09, "playmode: .....  status: .......", plScrWidth-20);
	writestring(buf[0], 30, 0x0F, cdpPlayMode?"disk ":"track", 5);
	writestring(buf[0], 45, 0x0F, (stat.error)?"  error":(stat.paused)?" paused":"playing", 7);
	writestring(buf[0], 52, 0x0F, "                 speed: ...%", 28);
	_writenum(buf[0], 76, 0x0F, stat.speed*100/256, 10, 3);

	writestring(buf[1], 0, 0x09, "drive: ....... start:   :..:..  pos:   :..:..  length:   :..:..  size: ...... kb", plScrWidth);
	writestring(buf[1], 7, 0x0F, vdev, 7); /* VERY TODO.. can we fit more data on this line??? */
	if (cdpViewSectors)
	{
		writenum(buf[1], 22, 0x0F, cdpTrackStarts[0], 10, 8, 0);
		writenum(buf[1], 37, 0x0F, stat.position-cdpTrackStarts[0], 10, 8, 0);
		writenum(buf[1], 55, 0x0F, cdpTrackStarts[cdpTrackNum+1]-cdpTrackStarts[0], 10, 8, 0);
	} else {
		writestring(buf[1], 22, 0x0F, gettimestr(cdpTrackStarts[0]+150, timestr), 8);
		writestring(buf[1], 37, 0x0F, gettimestr(stat.position-cdpTrackStarts[0], timestr), 8);
		writestring(buf[1], 55, 0x0F, gettimestr(cdpTrackStarts[cdpTrackNum+1]-cdpTrackStarts[0], timestr), 8);
	}
	_writenum(buf[1], 71, 0x0F, (cdpTrackStarts[cdpTrackNum+1]-cdpTrackStarts[0])*147/64, 10, 6);

	writestring(buf[2], 0, 0x09, "track: ..      start:   :..:..  pos:   :..:..  length:   :..:..  size: ...... kb", plScrWidth);
	_writenum(buf[2], 7, 0x0F, i-1+cdpFirstTrack, 10, 2);
	if (cdpViewSectors)
	{
		writenum(buf[2], 22, 0x0F, cdpTrackStarts[i-1]+150, 10, 8, 0);
		writenum(buf[2], 37, 0x0F, stat.position-cdpTrackStarts[i-1], 10, 8, 0);
		writenum(buf[2], 55, 0x0F, cdpTrackStarts[i]-cdpTrackStarts[i-1], 10, 8, 0);
	} else {
		writestring(buf[2], 22, 0x0F, gettimestr(cdpTrackStarts[i-1]+150, timestr), 8);
		writestring(buf[2], 37, 0x0F, gettimestr(stat.position-cdpTrackStarts[i-1], timestr), 8);
		writestring(buf[2], 55, 0x0F, gettimestr(cdpTrackStarts[i]-cdpTrackStarts[i-1], timestr), 8);
	}
	_writenum(buf[2], 71, 0x0F, (cdpTrackStarts[i]-cdpTrackStarts[i-1])*147/64, 10, 6);
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
			cpiKeyHelp('<', "Jump track back");
			cpiKeyHelp('>', "Jump track forward");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump track back");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Jump track forward");
			cpiKeyHelp(KEY_F(9), "Pitch speed down");
			cpiKeyHelp(KEY_F(11), "Pitch speed down");
			cpiKeyHelp(KEY_F(10), "Pitch speed up");
			cpiKeyHelp(KEY_F(12), "Pitch speed up");
			if (plrProcessKey)
				plrProcessKey(key);
			return 0;
		case 'p': case 'P':
			startpausefade();
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			plPause=!plPause;
			if (plPause)
				cdPause(cdrom_fd);
			else
				cdRestart(cdrom_fd);

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
				newpos=0;
				setnewpos=1;
				break;
			}
			for (i=1; i<=cdpTrackNum; i++)
				if (newpos<(signed)(cdpTrackStarts[i]-basesec))
					break;
			newpos=cdpTrackStarts[i-1]-basesec;
			setnewpos=1;
			break;
		case KEY_CTRL_UP:
		/* case 0x8D00: //ctrl-up */
			newpos-=60*75;
			setnewpos=1;
			break;
		case KEY_CTRL_DOWN:
		/*case 0x9100: //ctrl-down */
			newpos+=60*75;
			setnewpos=1;
			break;
		case KEY_CTRL_LEFT:
		case '<':
			if (!cdpPlayMode)
				break;
			for (i=2; i<=cdpTrackNum; i++)
				if (newpos<(signed)(cdpTrackStarts[i]-basesec))
					break;
			newpos=cdpTrackStarts[i-2]-basesec;
			setnewpos=1;
			break;
		case KEY_CTRL_RIGHT:
		case '>':
			if (!cdpPlayMode)
				break;
			for (i=1; i<=cdpTrackNum; i++)
				if (newpos<(signed)(cdpTrackStarts[i]-basesec))
					break;
			newpos=cdpTrackStarts[i]-basesec;
			setnewpos=1;
			break;
/* TODO-keys
		case 0x7700: //ctrl-home
			newpos=0;
			setnewpos=1;
			break;*/
		case KEY_F(9):
		case KEY_F(11):
			if ((speed-=finespeed)<16)
				speed=16;
			cdSetSpeed(speed);
			break;
		case KEY_F(10):
		case KEY_F(12):
			if ((speed+=finespeed)>2048)
				speed=2048;
			cdSetSpeed(speed);
			break;
		default:
			if (smpProcessKey)
			{
				int ret=smpProcessKey(key);
				if (ret==2)
					cpiResetScreen();
				if (ret)
					return 1;
			}
			if (plrProcessKey)
			{
				int ret=plrProcessKey(key);
				if (ret==2)
					cpiResetScreen();
				if (ret)
					return 1;
			}
			return 0;
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

	cdGetStatus(cdrom_fd, &stat);

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
		if (newpos>=(signed)length)
		{
			if (fsLoopMods)
				newpos=0;
			else
				return 1;
		}
		cdPause(cdrom_fd);
		cdRestartAt(cdrom_fd, basesec+newpos /*, length-newpos*/);
		if (plPause)
			cdPause(cdrom_fd);
		setnewpos=0;
	} else
		newpos=stat.position-basesec;

	return 0;
}

static void cdaCloseFile(void)
{
	cdStop(cdrom_fd);
}

static int cdaOpenFile (struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	char *name;
	unsigned char tnum;
	char buffer[64];

	buffer[0] = 0;

	file->seek_set (file, 0);
	file->read (file, buffer, sizeof (buffer));
	if (memcmp (buffer, "fd=", 3))
	{
		return -1;
	}
	cdrom_fd = atoi (buffer + 3);

	dirdbGetName_internalstr (file->dirdb_ref, &name);

	if (!strcmp(name, "DISK.CDA"))
		tnum=0xFF;
	else
		if (!memcmp(name, "TRACK", 5)&&isdigit(name[5])&&isdigit(name[6])&&(name[7]=='.'))
			tnum=(name[5]-'0')*10+(name[6]-'0');
		else
			return -1;

	if (!cdIsCDDrive(cdrom_fd))
		return -1;

	cdpTrackNum=cdGetTracks(cdrom_fd, cdpTrackStarts, &cdpFirstTrack, CDROM_LEADOUT);

	if (tnum!=0xFF)
	{
		if ((tnum<cdpFirstTrack)||(tnum>(cdpFirstTrack+cdpTrackNum)))
			return -1;
		cdpPlayMode=0;
		basesec=cdpTrackStarts[tnum-cdpFirstTrack];
		length=cdpTrackStarts[tnum-cdpFirstTrack+1]-basesec;
	} else {
		if (!cdpTrackNum)
			return -1;
		cdpPlayMode=1;
		basesec=cdpTrackStarts[0];
		length=cdpTrackStarts[cdpTrackNum+1]-basesec;
	}

	newpos=0;
	setnewpos=1;
	plPause=0;

	plIsEnd=cdaLooped;
	plProcessKey=cdaProcessKey;
	plDrawGStrings=cdaDrawGStrings;

	/*  cdLockTray(cdpDrive, 1); */

	strncpy(vdev, info->comment, 8);
	vdev[7]=0;

	if (cdPlay(cdrom_fd, basesec, length, file))
		return -1;

	normalize();
	pausefadedirect=0;

	return errOk;
}

struct cpifaceplayerstruct cdaPlayer = {cdaOpenFile, cdaCloseFile};
char *dllinfo = "";
struct linkinfostruct dllextinfo = {.name = "playcda", .desc = "OpenCP CDA Player (c) 1995-09 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
