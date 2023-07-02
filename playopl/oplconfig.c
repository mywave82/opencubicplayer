/* OpenCP Module Player
 * copyright (c) 2023 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * AdPlug config (setup:) editor
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

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_GRP_H
# include <grp.h>
#endif

#ifdef HAVE_PWD_H
# include <pwd.h>
#endif

#include <stdlib.h>

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/filesystem-setup.h"
#include "filesel/pfilesel.h"
#include "playopl/oplconfig.h"
#include "stuff/err.h"
#include "stuff/poutput.h"

#define MAX(a,b) ((a)>=(b)?(a):(b))

static void oplConfigDraw (int EditPos, const struct DevInterfaceAPI_t *API)
{
	int mlWidth, mlHeight, mlTop, mlLeft;

	mlHeight = 19;
	mlWidth = 60;
	mlTop = (API->console->TextHeight - mlHeight) / 2;
	mlLeft = (API->console->TextWidth - mlWidth) / 2;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda%18C\xc4 AdPlug configuration %18C\xc4\xbf");

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Navigate with arrows and hit %.15o<ESC>%.7o to save and exit.%*C %.9o\xb3", mlWidth - 55);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth-2);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Selected emulator:%0.9o%*C \xb3", mlWidth - 21);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o   %*.*o1. Ken  %0.9o%*C \xb3", EditPos==0?8:0, EditPos==0?7:7, mlWidth - 13);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o   %*.*o2. Nuked%0.9o%*C \xb3", EditPos==1?8:0, EditPos==1?7:7, mlWidth - 13);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o   %*.*o3. Satoh%0.9o%*C \xb3", EditPos==2?8:0, EditPos==2?7:7, mlWidth - 13);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o   %*.*o4. Woody%0.9o%*C \xb3", EditPos==3?8:0, EditPos==3?7:7, mlWidth - 13);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o   %*.*o5. RetroWave OPL3 [Express]  %.15o<ENTER>%.7o to configure%0.9o%*C \xb3", EditPos==4?8:0, EditPos==3?7:7, mlWidth - 54);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth-2);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	switch (EditPos)
	{
		case 0:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Ken is a dual OPL2 emulator based on Ken Silverman       %.9o\xb3");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o OPL2 emulator.                                           %.9o\xb3"); break;
		case 1:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Nuked is a bit-perfect OPL3 emulator made by Nuke.YKT    %.9o\xb3");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o based on die shots.                                      %.9o\xb3"); break;
		case 2:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Satoh is a dual OPL2 emulator based on code by Tatsuyuki %.9o\xb3");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Satoh by the MAME Team.                                  %.9o\xb3"); break;
		case 3:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Woody is an OPL3 emulator by the DOSBox Team. It is a    %.9o\xb3");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o further development of Ken Silverman OPL2 emulator.      %.9o\xb3"); break;
		case 4:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o RetroWave OPL3 [Express] are external USB devices with   %.9o\xb3");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o real OPL3 hardware by SudoMaker.                         %.9o\xb3"); break;
	}

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

#if 0
************************************************************
*                                                          *
* Select RetroWave device by using arrow keys, hit <ESC>   *
* when satisfied. Press <t> to test any given device.      *
*                                                          *
************************************************************
* auto (default)                                           *
* custom: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx  *
* /dev/ttyACM0 ! (detected as a RetroWave OPL3 xxxxxx)     *
*                                                          *
************************************************************

* Permission error, try ro run                             *
* sudo adduser %s %s,                                      *
* logout and login again for change to take effect         *
************************************************************

* Automatic use the first possible device.                 *
*                                                          *
*                                                          *

* Detected as a RetroWave OPL3 xxxxx device.               *
* Press <t> to test.                                       *
*                                                          *
************************************************************

* This is a possible RetroWave OPL [Express] device,       *
* unable to verify. Press <t> to test.                     *
*                                                          *
************************************************************
#endif




#define RETRODEVICE_MAXLEN 64
struct oplRetroDeviceEntry_t
{
	char device[RETRODEVICE_MAXLEN];
	int verified; // only Linux can do this at the moment 1 = RetroWave OPL3, 2 = RetroWave OPL3 Express
	int usererror;
	int grouperror;
	char groupname[64];
};
static struct oplRetroDeviceEntry_t *oplRetroDeviceEntry;
static int                           oplRetroDeviceEntries;

static char oplRetroCurrentDevice[RETRODEVICE_MAXLEN];
static char oplRetroCustomDevice[RETRODEVICE_MAXLEN];

#ifndef _WIN32
static uid_t uid;
static uid_t euid;
static gid_t gid;
static gid_t egid;
static char username[64];
static gid_t gids[512];
static int gids_count = 0;

static void oplRetroDevicesDestroy (void)
{
				free (oplRetroDeviceEntry);
				oplRetroDeviceEntry = 0;
				oplRetroDeviceEntries = 0;
}

static void oplRetroRefreshPrepare (void)
{
	struct passwd *pw;

	oplRetroDevicesDestroy();

	uid = getuid();
	euid = getuid();
	gid = getgid();
	egid = getegid();
	gids_count = getgroups (512, gids);
	if (gids_count < 0)
	{
		fprintf (stderr, "oplRetroRefreshPrepare(): getgroups() failed, buffer probably too small\n");
		gids_count = 0;
	}
	pw = getpwuid (uid);
	if (pw && pw->pw_name)
	{
		snprintf (username, sizeof (username), "%s", pw->pw_name);
	} else {
		snprintf (username, sizeof (username), "%ld", (long)uid);
	}
}
#endif

static void oplRetroDraw (const struct DevInterfaceAPI_t *API, int esel, int *state)
{
	int mlWidth, mlHeight, mlTop, mlLeft;
	int i;
	int skip;
	int dot;
	int half;
	int contentheight;
	int d;

	mlHeight = 19;
	mlWidth = 60;
	mlTop = (API->console->TextHeight - mlHeight) / 2;
	mlLeft = (API->console->TextWidth - mlWidth) / 2;

#define HEIGHT (mlHeight - 11)

	contentheight = oplRetroDeviceEntries + 2;
	half = HEIGHT / 2;

	if (contentheight <= HEIGHT)
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (esel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (esel >= (contentheight - half))
	{ /* we are at the bottom part */
		skip = contentheight - HEIGHT;
		dot = HEIGHT - 1;
	} else {
		skip = esel - half;
		dot = skip * HEIGHT / (contentheight - HEIGHT);
	}

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda%12C\xc4 AdPlug => RetroWave configuration %11C\xc4\xbf");

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Select RetroWave device by using arrow keys, hit %.15o<ESC>   %.9o\xb3");
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o when satisfied. Press <t> to test any given device.      %.9o\xb3");

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth-2);

	for (i = skip - 2, d = 0; (i < oplRetroDeviceEntries) && (d < HEIGHT); i++, d++)
	{
		if (i == -2)
		{
		      API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 %*.2oauto%0.9o%*C %c", (esel==0)?8:0, mlWidth - 7, (dot == d) ? '\xdd' : '\xb3');
		} else if (i == -1)
		{
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 %.7oCustom: %*.7o%.*S%0.9o %c", (esel==1)?8:0, mlWidth - 12, oplRetroCustomDevice, (dot == d) ? '\xdd' : '\xb3');
		} else {
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.*o%c%*.7o%.s%0.9o%*C %c",
				oplRetroDeviceEntry[i].verified ?  10 : (oplRetroDeviceEntry[i].grouperror == 1) ?  12 :   0,
				oplRetroDeviceEntry[i].verified ? '+' : (oplRetroDeviceEntry[i].grouperror == 1) ? '!' : ' ',
				(esel==(i+2))?8:0,
				oplRetroDeviceEntry[i].device, MAX(60 - 3 - strlen(oplRetroDeviceEntry[i].device), 0),
				(dot == d) ? '\xdd' : '\xb3');
		}
	}

	while (d < HEIGHT)
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
		d++;
	}

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth-2);

	if (esel == 0)
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Automatic use the first possible device.                 %.9o\xb3");
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
	} else if (esel == 1)
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Press %.15o<ENTER>%.7o to edit the custom device string.          %.9o\xb3");
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
	} else if (oplRetroDeviceEntry[esel-2].grouperror == 1)
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Permission error, try ro run                             %.9o\xb3");
		API->console->DisplayPrintf (mlTop,   mlLeft, 0x09, mlWidth - 1, "\xb3%.15o sudo adduser %S %S", username, oplRetroDeviceEntry[esel-2].groupname);
		API->console->DisplayPrintf (mlTop++, mlLeft + mlWidth - 1, 0x09, 1, "\xb3");
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o logout and login again for change to take effect         %.9o\xb3");
	} else if (oplRetroDeviceEntry[esel-2].verified)
	{
		if (oplRetroDeviceEntry[esel-2].verified == 1)
		{
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Detected as a RetroWave OPL3 Express device.             %.9o\xb3");
		} else {
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Detected as a RetroWave OPL3 device.                     %.9o\xb3");
		}
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Press %.15o<t>%.7o to test.                                       %.9o\xb3");
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
	} else {
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o This is a possible RetroWave OPL3 [Express] device,      %.9o\xb3");
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o unable to verify. Press %.15o<t>%.7o to test.                     %.9o\xb3");
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
	}
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);

	if (*state == 2) // edit custom
	{
		if (API->console->EditStringUTF8z (mlTop - 12, mlLeft + 10, mlWidth - 12, sizeof (oplRetroCustomDevice), oplRetroCustomDevice) <= 0)
		{
			*state = 1; // normal
		}
	}
}

static struct oplRetroDeviceEntry_t *oplRetroRefreshChar (const char *devname)
{
	struct oplRetroDeviceEntry_t *e;
	struct stat st;

	/* allocate one more entry */
	e = realloc (oplRetroDeviceEntry, sizeof (oplRetroDeviceEntry[0]) * (oplRetroDeviceEntries + 1));
	if (!e)
	{
		return 0;
	}
	oplRetroDeviceEntry = e;
	e = oplRetroDeviceEntry + (oplRetroDeviceEntries++);

	/* fill it up */
	memset (e, 0, sizeof (*e));
	snprintf (e->device, sizeof (e->device), "/dev/%s", devname);
	if (stat (e->device, &st))
	{ /* abort */
		oplRetroDeviceEntries--;
		return 0;
	}
	if (st.st_mode & S_IROTH) goto uid_ok; // both uid and gid can be skipped
	if (!(st.st_mode & S_IRGRP))
	{
		e->grouperror = 2;
	} else {
		int i;
		struct group *gr;
		if (st.st_gid == gid) goto gid_ok;
		if (st.st_gid == egid) goto gid_ok;
		for (i=0; i < gids_count; i++)
		{
			if (st.st_gid == gids[i]) goto gid_ok;
		}
		e->grouperror = 1;
		gr = getgrgid (st.st_gid);
		if (gr && gr->gr_name)
		{
			snprintf (e->groupname, sizeof (e->groupname), "%s", gr->gr_name);
		} else {
			snprintf (e->groupname, sizeof (e->groupname), "%ld", (long)st.st_gid);
		}
	}
gid_ok:
	if (!(st.st_mode & S_IRUSR))
	{
		e->usererror = 2;
	} else {
		if (st.st_uid == uid) goto uid_ok;
		if (st.st_uid == euid) goto uid_ok;
		e->usererror = 1;
	}
uid_ok:
	return e;
}

#ifdef __linux
static void oplRetroRefreshLinux (const char *devname)
{
	char name[RETRODEVICE_MAXLEN + 59];
	int fd;
	int res;
	char buffer[128];
	struct oplRetroDeviceEntry_t *e = oplRetroRefreshChar (devname);
	if (!e)
	{
		return;
	}
	snprintf (name, sizeof (name), "/sys/class/tty/%s/device/firmware_node/physical_node1/product", devname);
	fd = open (name, O_RDONLY);
	if (fd < 0)
	{
		return;
	}
	res = read (fd, buffer, sizeof (buffer) - 1);
	close (fd);
	if (res <= 0)
	{
		return;
	}
	buffer[res] = 0;
	if (!strcmp (buffer, "RetroWave OPL3 Express\n"))
	{
		e->verified = 2;
	} else if (!strcmp (buffer, "RetroWave OPL3\n"))
	{
		e->verified = 1;
	}
	return;
}
#endif

static int cmpoplRetroDeviceEntry (const void *p1, const void *p2)
{
	struct oplRetroDeviceEntry_t *e1 = (struct oplRetroDeviceEntry_t *)p1;
	struct oplRetroDeviceEntry_t *e2 = (struct oplRetroDeviceEntry_t *)p2;
	return strcmp (e1->device, e2->device);
}

static void oplRetroRefresh (void)
{
#ifdef _WIN32
	#error implement me
#else
	DIR *d = opendir ("/dev/");

	oplRetroRefreshPrepare ();

	if (d)
	{
		struct dirent *de;
		while ((de = readdir(d)))
		{
#ifdef _DIRENT_HAVE_D_TYPE
			if ((de->d_type != DT_CHR) && (de->d_type != DT_LNK))
			{
				continue;
			}
#endif
			if ((strlen (de->d_name) + 5 + 1 ) > RETRODEVICE_MAXLEN)
			{
				continue;
			}

			if (!strncmp (de->d_name, "ttyACM", 6)) // Linux
			{
#ifdef __linux
				oplRetroRefreshLinux (de->d_name);
#else
				oplRetroRefreshChar (de->d_name);
#endif
				continue;
			}
			if (!strncmp (de->d_name, "cuaU", 4)) // DragonFly, FreeBSD, OpenBSD
			{
				oplRetroRefreshChar (de->d_name);
				continue;
			}
			if (!strncmp (de->d_name, "dtyU", 4)) // NetBSD
			{
				oplRetroRefreshChar (de->d_name);
				continue;
			}
			if (!strncmp (de->d_name, "cu.usbmodem", 11)) // MacOS / OSX
			{
#warning MacOS probably has some special checks we can do
				oplRetroRefreshChar (de->d_name);
				continue;
			}
		}
		closedir (d);
	}
#endif

	qsort (oplRetroDeviceEntry, oplRetroDeviceEntries, sizeof (oplRetroDeviceEntry[0]), cmpoplRetroDeviceEntry);
}

static int oplRetroConfigRun (const struct DevInterfaceAPI_t *API)
{
	static int inActive = 0;
	static int esel = 0;
	if (!inActive)
	{
		snprintf (oplRetroCurrentDevice, sizeof (oplRetroCurrentDevice), "%s", API->configAPI->GetProfileString ("adplug", "retrowave", "auto"));
		strcpy (oplRetroCustomDevice, oplRetroCurrentDevice);
#warning Call refresh once every second?
		oplRetroRefresh ();

		if (!strcasecmp (oplRetroCurrentDevice, "auto"))
		{
			esel = 0;
		} else {
			int i;
			esel = 1;
			for (i=0; i < oplRetroDeviceEntries; i++)
			{
				if (!strcmp (oplRetroCurrentDevice, oplRetroDeviceEntry[i].device))
				{
					esel = i + 2;
				}
			}
		}
		inActive = 1;
	}

	oplRetroDraw (API, esel, &inActive);

	while (API->console->KeyboardHit() && inActive == 1) // = 0 exit, 1 = normal, 2 = edit custom
	{
		int key = API->console->KeyboardGetChar();

		switch (key)
		{
			case _KEY_ENTER:
				if (esel == 1) // custom
				{
					inActive = 2;
				}
				break;
			case KEY_UP:
				if (esel)
				{
					esel--;
				}
				break;
			case KEY_DOWN:
				if ((esel + 1) < (oplRetroDeviceEntries + 2))
				{
					esel++;
				}
				break;
			case KEY_ESC:
				if (esel == 0)
				{
					API->configAPI->SetProfileString ("adplug", "retrowave", "auto");
				} else if (esel == 1)
				{
					API->configAPI->SetProfileString ("adplug", "retrowave", oplRetroCustomDevice);
				} else {
					API->configAPI->SetProfileString ("adplug", "retrowave", oplRetroDeviceEntry[esel - 2].device);
				}
				inActive = 0;
				/* free dynamic data */
				oplRetroDevicesDestroy();
				return 0;
		}
	}

	return 1;
}

static void oplConfigRun (void **token, const struct DevInterfaceAPI_t *API)
{
	int inRetroConfig = 0;
	int esel = 0;
	const char *str;

	str = API->configAPI->GetProfileString ("adplug", "emulator", "nuked");

	if (!strcasecmp (str, "ken"))
	{
		esel = 0;
	} else if (!strcasecmp (str, "satoh"))
	{
		esel = 2;
	} else if (!strcasecmp (str, "woody"))
	{
		esel = 3;
	} else if (!strcasecmp (str, "retrowave"))
	{
		esel = 4;
	} else /* nuked */
	{
		esel = 1;
	}

	while (1)
	{
		API->fsDraw();
		if (inRetroConfig)
		{
			inRetroConfig = oplRetroConfigRun (API);
			continue;
		}
		oplConfigDraw (esel, API);
		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();

			switch (key)
			{
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
					esel = key - '1'; break;
				case KEY_DOWN:
					if (esel < 4)
					{
						esel++;
					}
					break;
				case KEY_UP:
					if (esel)
					{
						esel--;
					}
					break;
				case _KEY_ENTER:
					if (esel == 4)
					{
						inRetroConfig = 1;
					}
					break;
				case KEY_EXIT:
				case KEY_ESC:
					goto superexit;
					break;
			}
		}
		API->console->FrameLock ();
	}

superexit:

	     if (esel == 0) API->configAPI->SetProfileString ("adplug", "emulator", "ken");
	else if (esel == 1) API->configAPI->SetProfileString ("adplug", "emulator", "nuked");
	else if (esel == 2) API->configAPI->SetProfileString ("adplug", "emulator", "satoh");
	else if (esel == 4) API->configAPI->SetProfileString ("adplug", "emulator", "retrowave");
	else                API->configAPI->SetProfileString ("adplug", "emulator", "woody");
	API->configAPI->StoreConfig ();
}

static struct ocpfile_t *oplconfig;
static void oplConfigRun (void **token, const struct DevInterfaceAPI_t *API);

static char *opl_config_retrowave_device_auto (void)
{
  int i;
  char *retval = 0;
	oplRetroRefresh();
  for (i=0; i < oplRetroDeviceEntries; i++)
	{
		if (oplRetroDeviceEntry[i].verified)
		{
			retval = strdup (oplRetroDeviceEntry[i].device);
			goto out;
		}
	}
	if (oplRetroDeviceEntries)
	{
		retval = strdup (oplRetroDeviceEntry[0].device);
	}
out:
	oplRetroDevicesDestroy();
	return retval;
}

OCP_INTERNAL char *opl_config_retrowave_device (const struct configAPI_t *configAPI)
{
	const char *temp = configAPI->GetProfileString ("adplug", "retrowave", "auto");
	if (!strcmp (temp, "auto"))
	{
		return opl_config_retrowave_device_auto ();
	}
	return strdup (temp);
}

OCP_INTERNAL int opl_config_init (struct PluginInitAPI_t *API)
{
	oplconfig = API->dev_file_create (
		API->dmSetup->basedir,
		"adplugconfig.dev",
		"AdPlug Configuration (playopl)",
		"",
		0, /* token */
		0, /* Init */
		oplConfigRun,
		0, /* Close */
		0  /* Destructor */
	);

	API->filesystem_setup_register_file (oplconfig);

	return errOk;
}

OCP_INTERNAL void opl_config_done (struct PluginCloseAPI_t *API)
{
	if (oplconfig)
	{
		API->filesystem_setup_unregister_file (oplconfig);
		oplconfig->unref (oplconfig);
		oplconfig = 0;
	}
}
