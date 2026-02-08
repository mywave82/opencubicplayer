/* OpenCP Module Player
 * copyright (c) 2023-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
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

#include <stdarg.h>
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

#ifdef _WIN32
# include <windows.h>
# include <winreg.h>
#endif

#include "types.h"
extern "C"
{
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/filesystem-setup.h"
#include "filesel/pfilesel.h"
#include "playopl/oplconfig.h"
#include "playopl/oplplay.h"
#include "stuff/err.h"
#include "stuff/piperun.h"
#include "stuff/poutput.h"
}

#include "oplRetroWave.h"

#define MAX(a,b) ((a)>=(b)?(a):(b))
#define MIN(a,b) ((a)<=(b)?(a):(b))

enum VerifiedAs
{
	NotVerified = 0,
	VerifiedAs_RetroWaveOPL3 = 1,
	VIDPID_Possible_RetroWaveOPL3 = 2,
	VIDPID_Possible_PotatoPiSTM32 = 3,
	VIDPID_Possible_PotatoPiPIC24 = 4,
};

#define RETROWAVE_OPL3_EXPRESS_VID 0x04D8 /* Microchip Technology, Inc. */
#define RETROWAVE_OPL3_EXPRESS_PID 0x000A /* CDC RS-232 Emulation Demo */

#define POTATOPI_STM32_VID         0x0483 /* STMicroelectronics */
#define POTATOPI_STM32_PID         0x5740 /* Virtual COM Port */

#define POTATOPI_PIC24_VID         0x04d8 /* Microchip Technology, Inc. */
#define POTATOPI_PIC24_PID         0xe966


static void oplConfigDraw (int EditPos, const struct DevInterfaceAPI_t *API)
{
	int mlWidth, mlHeight, mlTop, mlLeft;

#if (CONSOLE_MIN_Y < 19)
# error oplConfigDraw() requires CONSOLE_MIN_Y >= 19
#endif

	mlHeight = 19;
	mlWidth = 60;
	mlTop = (API->console->TextHeight - mlHeight) / 2;
	mlLeft = (API->console->TextWidth - mlWidth) / 2;

	API->console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "AdPlug configuration", 0, 4, 13);
	mlHeight -= 2;
	mlWidth  -= 2;

	mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Navigate with arrows and hit %.15o<ESC>%.7o to save and exit.");

	mlTop++;
	mlTop++; // 4: horizontal bar
	mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Selected emulator:");
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "   %*.*o1. Ken  %0.9o%", EditPos==0?8:0, EditPos==0?7:7);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "   %*.*o2. Nuked%0.9o%", EditPos==1?8:0, EditPos==1?7:7);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "   %*.*o3. Satoh%0.9o%", EditPos==2?8:0, EditPos==2?7:7);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "   %*.*o4. Woody%0.9o%", EditPos==3?8:0, EditPos==3?7:7);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "   %*.*o5. RetroWave OPL3 [Express]  %.15o<ENTER>%.7o to configure", EditPos==4?8:0, EditPos==3?7:7);

	mlTop++;
	mlTop++; // 13: horizontal bar
	mlTop++;

	switch (EditPos)
	{
		case 0:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Ken is a dual OPL2 emulator based on Ken Silverman       ");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " OPL2 emulator.                                           "); break;
		case 1:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Nuked is a bit-perfect OPL3 emulator made by Nuke.YKT    ");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " based on die shots.                                      "); break;
		case 2:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Satoh is a dual OPL2 emulator based on code by Tatsuyuki ");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Satoh by the MAME Team.                                  "); break;
		case 3:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Woody is an OPL3 emulator by the DOSBox Team. It is a    ");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " further development of Ken Silverman OPL2 emulator.      "); break;
		case 4:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " RetroWave OPL3 [Express] are external USB devices with   ");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " real OPL3 hardware by SudoMaker.                         "); break;
	}

	mlTop++;
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

#define TESTLINES 10
static class oplRetroWave *oplRetroTest;
static char oplRetroTestLineBuffers[TESTLINES][59];
static int oplRetroTestNextLine;

static void oplRetroTestDebugAppend (const char *line)
{
	if (oplRetroTestNextLine >= TESTLINES)
	{
		return;
	}

	snprintf (oplRetroTestLineBuffers[oplRetroTestNextLine++], sizeof (oplRetroTestLineBuffers[0]), "%s", line);
}

// We assume each call is a single line
static void oplRetroTestDebug (struct cpifaceSessionAPI_t *cpifaceSession, const char *fmt, ...)
{
	char *line;
	char temp[128];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf (temp, sizeof (temp), fmt, ap);
	va_end (ap);

	if (!temp[0])
	{
		return;
	}
	while (strlen(temp) && (temp[strlen(temp)-1] == '\n' || temp[strlen(temp)-1] == '\r')) temp[strlen(temp)-1] = 0;

	for (line = temp; strlen (line); line += MIN (strlen (line), 58))
	{
		 oplRetroTestDebugAppend (line);
	}
}

static void oplRetroTestStart (const struct DevInterfaceAPI_t *API, const char *device)
{
	memset (oplRetroTestLineBuffers, 0, sizeof (oplRetroTestLineBuffers));
	oplRetroTestNextLine = 0;

	oplRetroTest = new oplRetroWave (oplRetroTestDebug, 0, device, 10000);

	oplRetroTest->write (0x20, 0x23);
	oplRetroTest->write (0x23, 0x20);
	oplRetroTest->write (0x40, 0x2f);
	oplRetroTest->write (0x43, 0x00);
	oplRetroTest->write (0x60, 0x11);
	oplRetroTest->write (0x63, 0x11);
	oplRetroTest->write (0x80, 0x21);
	oplRetroTest->write (0x83, 0x21);
	oplRetroTest->write (0xa0, 0x44);
	oplRetroTest->write (0xc0, 0xff);
	oplRetroTest->write (0xb0, 0x32);
	oplRetroTest->write (0xb3, 0x31);
}

static void oplRetroTestDraw (const struct DevInterfaceAPI_t *API)
{
	int mlWidth, mlHeight, mlTop, mlLeft, i;

#if (CONSOLE_MIN_Y < 19)
# error oplRetroTestDraw() requires CONSOLE_MIN_Y >= 19
#endif

	mlHeight = 19;
	mlWidth = 60;
	mlTop = (API->console->TextHeight - mlHeight) / 2;
	mlLeft = (API->console->TextWidth - mlWidth) / 2;

	API->console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "AdPlug => RetroWave configuration => Test", 0, 7, 0);
	mlHeight -= 2;
	mlWidth  -= 2;

	mlTop++;
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Attempting to make a test sound on the RetroWave");
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " OPL3 [Express] device.");
	mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Stop test by pressing %.15o<t>%.7o, %.15o<ENTER>%.7o or %.15o<ESC>%.7o.");
	mlTop++;
	mlTop++; // 7: horizontal line

	for (i = 0; i < TESTLINES; i++)
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "%.S", oplRetroTestLineBuffers[i]);
	}
}

static int oplRetroTestRun (const struct DevInterfaceAPI_t *API)
{
	oplRetroTestDraw (API);

	while (API->console->KeyboardHit())
	{
		int key = API->console->KeyboardGetChar();

		switch (key)
		{
			case 't':
			case 'T':
			case _KEY_ENTER:
			case KEY_ESC:
				oplRetroTest->write(0xb0, 0x02);
				oplRetroTest->write(0xb3, 0x03);
				usleep(100000); /* 100 ms */
				oplRetroTest->init();
				delete (oplRetroTest);
				oplRetroTest = 0;
				return 0;
		}
	}

	return 1;

}

#define RETRODEVICE_MAXLEN 64
struct oplRetroDeviceEntry_t
{
	char device[RETRODEVICE_MAXLEN];
	enum VerifiedAs verified; // only MacOS, Linux and Windows can do this at the moment
#ifdef _WIN32
	char refname[RETRODEVICE_MAXLEN];
#else
	int usererror;
	int grouperror;
	char groupname[64];
#endif
};
static struct oplRetroDeviceEntry_t *oplRetroDeviceEntry;
static int                           oplRetroDeviceEntries;

static char oplRetroCurrentDevice[RETRODEVICE_MAXLEN];
static char oplRetroCustomDevice[RETRODEVICE_MAXLEN];

static void oplRetroDevicesDestroy (void)
{
	free (oplRetroDeviceEntry);
	oplRetroDeviceEntry = 0;
	oplRetroDeviceEntries = 0;
}

#ifndef _WIN32
static uid_t uid;
static uid_t euid;
static gid_t gid;
static gid_t egid;
static char username[64];
static gid_t gids[512];
static int gids_count = 0;

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

	if (*state == 3) // do test
	{
		if (oplRetroTestRun (API) <= 0)
		{
			*state = 1; // normal
		} else {
			return;
		}
	}

#define HEIGHT 8
	mlHeight = 11 + HEIGHT;
	mlWidth = 60;
	mlTop = (API->console->TextHeight - mlHeight) / 2;
	mlLeft = (API->console->TextWidth - mlWidth) / 2;

#if (CONSOLE_MIN_Y < 19)
# error oplRetroDraw() requires CONSOLE_MIN_Y >= 19
#endif

	contentheight = oplRetroDeviceEntries + 2;
	half = HEIGHT / 2;

	if (contentheight <= HEIGHT)
	{ /* all entries can fit */
		skip = 0;
		dot = 0;
	} else if (esel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 6;
	} else if (esel >= (contentheight - half))
	{ /* we are at the bottom part */
		skip = contentheight - HEIGHT;
		dot = HEIGHT - 1 + 6;
	} else {
		skip = esel - half;
		dot = skip * HEIGHT / (contentheight - HEIGHT) + 6;
	}

	API->console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "AdPlug => RetroWave configuration", dot, 5, 5 + HEIGHT);
	mlWidth -= 2;
	mlHeight -= 2;

	mlTop++;
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Select RetroWave device by using arrow keys, hit %.15o<ESC>");
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " when satisfied. Press %.15o<t>%0.7o to test any given device.");
	mlTop++;
	mlTop++; // 5: horizontal line

	for (i = skip - 2, d = 0; (i < oplRetroDeviceEntries) && (d < HEIGHT); i++, d++)
	{
		if (i == -2)
		{
		      API->console->DisplayPrintf (mlTop++, mlLeft, 0x02, mlWidth, " %*.2oauto%0.9o", (esel==0)?8:0);
		} else if (i == -1)
		{
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Custom: %*.7o%.*S%0.9o ", (esel==1)?8:0, mlWidth - 10, oplRetroCustomDevice);
		} else {
#ifdef _WIN32
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "%.*o%c%*.7o%s %s%0.9o ",
				oplRetroDeviceEntry[i].verified ?  10 :   0,
				oplRetroDeviceEntry[i].verified ? '+' : ' ',
				(esel==(i+2))?8:0,
				oplRetroDeviceEntry[i].device,
				oplRetroDeviceEntry[i].refname);
#else
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "%.*o%c%*.7o%s%0.9o ",
				(oplRetroDeviceEntry[i].grouperror == 1) ? 12  : oplRetroDeviceEntry[i].verified ?  10 :   0,
				(oplRetroDeviceEntry[i].grouperror == 1) ? '!' : oplRetroDeviceEntry[i].verified ? '+' : ' ',
				(esel==(i+2))?8:0,
				oplRetroDeviceEntry[i].device);
#endif
		}
	}

	while (d < HEIGHT)
	{
		mlTop++;
		d++;
	}

	mlTop++; // 5 + HEIGHT: horizontal line

	if (esel == 0)
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Automatic use the first possible device.");
		mlTop++;
		mlTop++;
	} else if (esel == 1)
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Press %.15o<ENTER>%.7o to edit the custom device string.");
		mlTop++;
		mlTop++;
#ifndef _WIN32
	} else if (oplRetroDeviceEntry[esel-2].grouperror == 1)
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Permission error, try ro run");
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x0f, mlWidth, " sudo adduser %S %S", username, oplRetroDeviceEntry[esel-2].groupname);
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " logout and login again for change to take effect.");
#endif
	} else if (oplRetroDeviceEntry[esel-2].verified)
	{
		switch (oplRetroDeviceEntry[esel-2].verified)
		{
			case VerifiedAs_RetroWaveOPL3:
				API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Detected as a RetroWave OPL3 Express device."); break;
			case VIDPID_Possible_RetroWaveOPL3:
				API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Detected as a possible RetroWave OPL3 Express device."); break;
			default:
			case VIDPID_Possible_PotatoPiSTM32:
			case VIDPID_Possible_PotatoPiPIC24:
				API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Detected as a possible PotatoPi + RetroWave OPL3 device."); break;
		}
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Press %.15o<t>%.7o to test.");
		mlTop++;
	} else {
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " This could potentialy a be RetroWave OPL3 [Express]");
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " device, unable to verify. Press %.15o<t>%.7o to test.");
		mlTop++;
	}

	if (*state == 2) // edit custom
	{
		if (API->console->EditStringUTF8z (mlTop - 11, mlLeft + 9, mlWidth - 10, sizeof (oplRetroCustomDevice), oplRetroCustomDevice) <= 0)
		{
			*state = 1; // normal
		}
	}
}

#ifndef _WIN32
static struct oplRetroDeviceEntry_t *oplRetroRefreshChar (const char *devname)
{
	struct oplRetroDeviceEntry_t *e;
	struct stat st;

	/* allocate one more entry */
	e = (struct oplRetroDeviceEntry_t *) realloc (oplRetroDeviceEntry, sizeof (oplRetroDeviceEntry[0]) * (oplRetroDeviceEntries + 1));
	if (!e)
	{
		return 0;
	}
	oplRetroDeviceEntry = e;
	e = oplRetroDeviceEntry + (oplRetroDeviceEntries++);

	/* fill it up */
	memset (e, 0, sizeof (*e));
#ifdef __HAIKU__
	snprintf (e->device, sizeof (e->device), "/dev/ports/%s", devname);
#else
	snprintf (e->device, sizeof (e->device), "/dev/%s", devname);
#endif
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
#endif

#ifdef __linux
static void oplRetroRefreshLinux (const char *devname)
{
	char filename[RETRODEVICE_MAXLEN + 59];
	int fd;
	int res;
	char buffer[128];
	struct oplRetroDeviceEntry_t *e = oplRetroRefreshChar (devname);
	uint_fast16_t VID, PID;
	if (!e)
	{
		return;
	}
	snprintf (filename, sizeof (filename), "/sys/class/tty/%s/device/firmware_node/physical_node1/product", devname);
	fd = open (filename, O_RDONLY);
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
		e->verified = VerifiedAs_RetroWaveOPL3;
		return;
	}

	snprintf (filename, sizeof (filename), "/sys/class/tty/%s/device/firmware_node/physical_node1/idVendor", devname);
	fd = open (filename, O_RDONLY);
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
	VID = strtoul(buffer, 0, 16);

	snprintf (filename, sizeof (filename), "/sys/class/tty/%s/device/firmware_node/physical_node1/idProduct", devname);
	fd = open (filename, O_RDONLY);
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
	PID = strtoul(buffer, 0, 16);

	if ((VID == RETROWAVE_OPL3_EXPRESS_VID) && (PID == RETROWAVE_OPL3_EXPRESS_PID))
	{
		e->verified = VIDPID_Possible_RetroWaveOPL3;
	} else if ((VID == POTATOPI_STM32_VID) && (PID == POTATOPI_STM32_PID))
	{
		e->verified = VIDPID_Possible_PotatoPiSTM32;
	} else if ((VID == POTATOPI_PIC24_VID) && (PID == POTATOPI_PIC24_PID))
	{
		e->verified = VIDPID_Possible_PotatoPiPIC24;
	}

	return;
}
#endif

#if defined(__APPLE__)
static void oplRetroRefreshMacOS (const struct PipeProcessAPI_t *PipeProcess, const char *devname)
{
	struct oplRetroDeviceEntry_t *e = oplRetroRefreshChar (devname);
	if (!e)
	{
		return;
	}

	char buffer1[8192];
	size_t buffer1fill = 0;
	char buffer2[64];

	const char *cmdline[] = {"ioreg", "-l", NULL};
	void *ioreg_pipe = PipeProcess->Create (cmdline);

	int res1, res2;
	enum VerifiedAs NextVerified = NotVerified;
	int VID = 0, PID = 0;

	do
	{
		if (buffer1fill >= sizeof (buffer1))
		{
			buffer1fill = 0;
		}
		res1 = PipeProcess->ReadStdOut (ioreg_pipe, buffer1 + buffer1fill, sizeof (buffer1) - buffer1fill);
		res2 = PipeProcess->ReadStdErr (ioreg_pipe, buffer2, sizeof (buffer2));

		if (res1 > 0)
		{
			buffer1fill += res1;
			while (buffer1fill)
			{
				const char *eol = (const char *)memchr (buffer1, '\n', buffer1fill);
				if (!eol)
				{
					break;
				}
				if (memmem (buffer1, eol - buffer1, "<class IOUSBHostDevice, ", 24))
				{
					NextVerified = NotVerified; // fresh device */
					VID = 0;
					PID = 0;
				} else if (memmem (buffer1, eol - buffer1, "\"USB Product Name\" = \"RetroWave OPL3 Express\"", 45))
				{
					NextVerified = VerifiedAs_RetroWaveOPL3;
				} else if (memmem (buffer1, eol - buffer1, "\"idProduct\" =", 13))
				{
					const char *pos = strchr (buffer1, '=');
					if (pos && !PID)
					{
						PID = atoi (pos + 2);
					}
				} else if (memmem (buffer1, eol - buffer1, "\"idVendor\" =", 12))
				{
					const char *pos = strchr (buffer1, '=');
					if (pos && !VID)
					{
						VID = atoi (pos + 2);
					}
				} else if (memmem (buffer1, eol - buffer1, devname, strlen (devname)))
				{
					if (NextVerified == NotVerified)
					{
						if ((VID == RETROWAVE_OPL3_EXPRESS_VID) && (PID == RETROWAVE_OPL3_EXPRESS_PID))
						{
							NextVerified = VIDPID_Possible_RetroWaveOPL3;
						} else if ((VID == POTATOPI_STM32_VID) && (PID == POTATOPI_STM32_PID))
						{
							NextVerified = VIDPID_Possible_PotatoPiSTM32;
						} else if ((VID == POTATOPI_PIC24_VID) && (PID == POTATOPI_PIC24_PID))
						{
							NextVerified = VIDPID_Possible_PotatoPiPIC24;
						}
					}
					e->verified = NextVerified;
				}
				memmove (buffer1, eol + 1, buffer1fill - (eol - buffer1) - 1);
				buffer1fill -= (eol - buffer1) + 1;
			}
		}
	} while ((res1 >= 0) && (res2 >= 0));

	ocpPipeProcess_destroy (ioreg_pipe);
}
#endif

static int cmpoplRetroDeviceEntry (const void *p1, const void *p2)
{
	struct oplRetroDeviceEntry_t *e1 = (struct oplRetroDeviceEntry_t *)p1;
	struct oplRetroDeviceEntry_t *e2 = (struct oplRetroDeviceEntry_t *)p2;
	return strcmp (e1->device, e2->device);
}

#ifdef _WIN32

static void oplRetroRefreshVerifyAs (const char *device, const enum VerifiedAs VA)
{
	for (int i = 0; i < oplRetroDeviceEntries; i++)
	{
#ifdef _WIN32
		if (!strcasecmp (oplRetroDeviceEntry[i].device, device))
#else
		if (!strcasecmp (oplRetroDeviceEntry[i].device, device))
#endif
		{
			if ((!oplRetroDeviceEntry[i].verified) || (oplRetroDeviceEntry[i].verified > VA))
			{
				oplRetroDeviceEntry[i].verified = VA;
			}
			return;
		}
	}
}

static struct oplRetroDeviceEntry_t *oplRetroRefreshPort (const char *devname, const char *refname)
{
	struct oplRetroDeviceEntry_t *e;

	/* allocate one more entry */
	e = (struct oplRetroDeviceEntry_t *) realloc (oplRetroDeviceEntry, sizeof (oplRetroDeviceEntry[0]) * (oplRetroDeviceEntries + 1));
	if (!e)
	{
		return 0;
	}
	oplRetroDeviceEntry = e;
	e = oplRetroDeviceEntry + (oplRetroDeviceEntries++);

	/* fill it up */
	memset (e, 0, sizeof (*e));
	snprintf (e->device, sizeof (e->device), "%s", devname);
	snprintf (e->refname, sizeof (e->refname), "%s", refname);

	return e;
}

static void oplRetroRefreshCOMs (void)
{
	/* GetCommPorts() is only available from Windows 10 at some patch level and later, so we query the registry instead */
	DWORD nValues, nMaxValueNameLen, nMaxValueLen;
	HKEY hKey = NULL;
	LPBYTE szDeviceName = NULL;
	LPBYTE szFriendlyName = NULL;
	DWORD dwType = 0;
	DWORD nValueNameLen = 0;
	DWORD nValueLen = 0;
	LSTATUS lResult;

	lResult = RegOpenKeyEx (HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey);
	if (lResult != ERROR_SUCCESS)
	{
		if (lResult != ERROR_FILE_NOT_FOUND)
		{
			char *lpMsgBuf = NULL;
			if (FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM     |
				FORMAT_MESSAGE_IGNORE_INSERTS,             /* dwFlags */
				NULL,                                      /* lpSource */
				GetLastError(),                            /* dwMessageId */
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
				(LPSTR) &lpMsgBuf,                         /* lpBuffer */
				0,                                         /* nSize */
				NULL                                       /* Arguments */
			))
			{
				fprintf (stderr, "[RetroWave] RegOpenKeyEx(HKEY_LOCAL_MACHINE, \"HARDWARE\\DEVICEMAP\\SERIALCOMM\"): %s\n", lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
		}
		return;
	}

	if (lResult != ERROR_SUCCESS)
	{
		fprintf(stderr, "Failed to open key HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\SERIALCOMM\n");
		return;
	}

	lResult = RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &nValues, &nMaxValueNameLen, &nMaxValueLen, NULL, NULL);
	if (lResult != ERROR_SUCCESS)
	{
		fprintf (stderr, "Failed to perform RegQueryInfoKey()\n");
		RegCloseKey(hKey);
		return;
	}

	szDeviceName = (LPBYTE)malloc(nMaxValueNameLen + 1);
	if (!szDeviceName)
	{
		fprintf (stderr, "malloc() failed #1\n");
		RegCloseKey(hKey);
		return;
	}

	szFriendlyName = (LPBYTE)malloc(nMaxValueLen + 1);
	if (!szFriendlyName)
	{
		free(szDeviceName);
		fprintf (stderr, "malloc() failed #2\n");
		RegCloseKey(hKey);
		return;
	}

	for (DWORD dwIndex = 0; dwIndex < nValues; ++dwIndex)
	{
		dwType = 0;
		nValueNameLen = nMaxValueNameLen + 1;
		nValueLen = nMaxValueLen + 1;

		lResult = RegEnumValue
		(
			hKey,
			dwIndex,
			(LPSTR)szDeviceName,
			&nValueNameLen,
			NULL,
			&dwType,
			szFriendlyName,
			&nValueLen
		);

		if ((lResult != ERROR_SUCCESS) || (dwType != REG_SZ))
		{
			fprintf (stderr, "can't process entry with index %lu\n", (unsigned long)dwIndex);
			continue;
		}
		oplRetroRefreshPort ((const char *)szFriendlyName, (const char *)szDeviceName);
	}
	free(szDeviceName);
	free(szFriendlyName);
	RegCloseKey(hKey);
}

static void oplRetroRefreshVID_PID_sub (const char *basepath, const char *instance, const enum VerifiedAs VA)
{
	char path[96];
	/* GetCommPorts() is only available from Windows 10 at some patch level and later, so we query the registry instead */
	DWORD nValues, nMaxValueNameLen, nMaxValueLen;
	HKEY hKey = NULL;
	LPBYTE szName = NULL;
	LPBYTE szValue = NULL;
	DWORD dwType = 0;
	DWORD nValueNameLen = 0;
	DWORD nValueLen = 0;
	LSTATUS lResult;

	sprintf (path, "%s\\%s\\Device Parameters", basepath, instance);
	lResult = RegOpenKeyEx (HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey);
	if (lResult != ERROR_SUCCESS)
	{
		if (lResult != ERROR_FILE_NOT_FOUND)
		{
			char *lpMsgBuf = NULL;
			if (FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM     |
				FORMAT_MESSAGE_IGNORE_INSERTS,             /* dwFlags */
				NULL,                                      /* lpSource */
				GetLastError(),                            /* dwMessageId */
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
				(LPSTR) &lpMsgBuf,                         /* lpBuffer */
				0,                                         /* nSize */
				NULL                                       /* Arguments */
			))
			{
				fprintf (stderr, "[RetroWave] RegOpenKeyEx(HKEY_LOCAL_MACHINE, \"%s\"): %s\n", path, lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
		}
		return;
	}

	lResult = RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &nValues, &nMaxValueNameLen, &nMaxValueLen, NULL, NULL);
	if (lResult != ERROR_SUCCESS)
	{
		fprintf (stderr, "Failed to perform RegQueryInfoKey()\n");
		RegCloseKey(hKey);
		return;
	}

	szName = (LPBYTE)malloc(nMaxValueNameLen + 1);
	if (!szName)
	{
		fprintf (stderr, "malloc() failed #1\n");
		RegCloseKey(hKey);
		return;
	}

	szValue = (LPBYTE)malloc(nMaxValueLen + 1);
	if (!szValue)
	{
		free(szName);
		fprintf (stderr, "malloc() failed #2\n");
		RegCloseKey(hKey);
		return;
	}

	for (DWORD dwIndex = 0; dwIndex < nValues; ++dwIndex)
	{
		dwType = 0;
		nValueNameLen = nMaxValueNameLen + 1;
		nValueLen = nMaxValueLen + 1;

		lResult = RegEnumValue
		(
			hKey,
			dwIndex,
			(LPSTR)szName,
			&nValueNameLen,
			NULL,
			&dwType,
			szValue,
			&nValueLen
		);

		if ((lResult != ERROR_SUCCESS) || (dwType != REG_SZ))
		{
			//fprintf (stderr, "can't process entry with index %lu\n", (unsigned long)dwIndex);
			continue;
		}
		if (!strcasecmp ((char *)szName, "PortName"))
		{
			oplRetroRefreshVerifyAs ((const char *)szValue, VA);
		}
	}
	free(szName);
	free(szValue);
	RegCloseKey(hKey);
}

static void oplRetroRefreshVID_PID (const uint16_t VID, const uint16_t PID, const enum VerifiedAs VA)
{
	char path[64];
	HKEY hKey = NULL;
	DWORD nSubKeys = 0;
	DWORD MaxSubKeyLen = 0;
	LPBYTE szSubKey = NULL;
	LSTATUS lResult;

	sprintf (path, "SYSTEM\\CurrentControlSet\\Enum\\USB\\VID_%04X&PID_%04X", VID, PID);
	lResult = RegOpenKeyEx (HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey);
	if (lResult != ERROR_SUCCESS)
	{
		if (lResult != ERROR_FILE_NOT_FOUND)
		{
			char *lpMsgBuf = NULL;
			if (FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM     |
				FORMAT_MESSAGE_IGNORE_INSERTS,             /* dwFlags */
				NULL,                                      /* lpSource */
				GetLastError(),                            /* dwMessageId */
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
				(LPSTR) &lpMsgBuf,                         /* lpBuffer */
				0,                                         /* nSize */
				NULL                                       /* Arguments */
			))
			{
				fprintf (stderr, "[RetroWave] RegOpenKeyEx(HKEY_LOCAL_MACHINE, \"%s\"): %s\n", path, lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
		}
		return;
	}

	lResult = RegQueryInfoKey(hKey, NULL, NULL, NULL, &nSubKeys, &MaxSubKeyLen, NULL, NULL, NULL, NULL, NULL, NULL);
	if (lResult != ERROR_SUCCESS)
	{
		fprintf (stderr, "Failed to perform RegQueryInfoKey()\n");
		RegCloseKey(hKey);
		return;
	}

	szSubKey = (LPBYTE)malloc(MaxSubKeyLen + 1);
	if (!szSubKey)
	{
		fprintf (stderr, "malloc() failed #1\n");
		RegCloseKey(hKey);
		return;
	}

	for (DWORD dwIndex = 0; dwIndex < nSubKeys; ++dwIndex)
	{
		DWORD nNameLen = MaxSubKeyLen + 1;

		lResult = RegEnumKeyExA
		(
			hKey,
			dwIndex,
			(LPSTR)szSubKey,
			&nNameLen,
			NULL,
			NULL,
			NULL,
			NULL
		);
		if (lResult != ERROR_SUCCESS)
		{
			fprintf (stderr, "can't process entry with index %lu\n", (unsigned long)dwIndex);
			continue;
		}
		oplRetroRefreshVID_PID_sub (path, (const char *)szSubKey, VA);
	}
	free (szSubKey);
	RegCloseKey(hKey);
}

static void oplRetroRefresh (const struct PipeProcessAPI_t *PipeProcess)
{
	oplRetroRefreshCOMs ();
	oplRetroRefreshVID_PID (RETROWAVE_OPL3_EXPRESS_VID, RETROWAVE_OPL3_EXPRESS_PID, VIDPID_Possible_RetroWaveOPL3);
	oplRetroRefreshVID_PID (        POTATOPI_STM32_VID,         POTATOPI_STM32_PID, VIDPID_Possible_PotatoPiSTM32);
	oplRetroRefreshVID_PID (        POTATOPI_PIC24_VID,         POTATOPI_PIC24_PID, VIDPID_Possible_PotatoPiPIC24);

	qsort (oplRetroDeviceEntry, oplRetroDeviceEntries, sizeof (oplRetroDeviceEntry[0]), cmpoplRetroDeviceEntry);
}

#elif defined(__HAIKU__)
static void oplRetroRefresh (const struct PipeProcessAPI_t *PipeProcess)
{
	DIR *d = opendir ("/dev/ports");

	oplRetroRefreshPrepare ();

	if (d)
	{
		struct dirent *de;
		while ((de = readdir(d)))
		{
			if ((strlen (de->d_name) + 11 + 1 ) > RETRODEVICE_MAXLEN)
			{
				continue;
			}

			if (!strncmp (de->d_name, "usb", 3)) // Haiku: /dev/ports/usb0
			{
				oplRetroRefreshChar (de->d_name);
				continue;
			}
		}
		closedir (d);
	}
	qsort (oplRetroDeviceEntry, oplRetroDeviceEntries, sizeof (oplRetroDeviceEntry[0]), cmpoplRetroDeviceEntry);
}
#else
static void oplRetroRefresh (const struct PipeProcessAPI_t *PipeProcess)
{
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

#if defined(__linux)
			if (!strncmp (de->d_name, "ttyACM", 6)) // Linux
			{
				oplRetroRefreshLinux (de->d_name);
				continue;
			}
#elif defined(__APPLE__)
			if (!strncmp (de->d_name, "cu.usbmodem", 11)) // MacOS / OSX
			{
				oplRetroRefreshMacOS (PipeProcess, de->d_name);
				continue;
			}
#else
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
#endif
		}
		closedir (d);
	}
	qsort (oplRetroDeviceEntry, oplRetroDeviceEntries, sizeof (oplRetroDeviceEntry[0]), cmpoplRetroDeviceEntry);
}
#endif

static char *opl_config_retrowave_device_auto (const struct PipeProcessAPI_t *PipeProcess);
static int oplRetroConfigRun (const struct DevInterfaceAPI_t *API)
{
	static int inActive = 0;
	static int esel = 0;
	if (!inActive)
	{
		snprintf (oplRetroCurrentDevice, sizeof (oplRetroCurrentDevice), "%s", API->configAPI->GetProfileString ("adplug", "retrowave", "auto"));
		strcpy (oplRetroCustomDevice, oplRetroCurrentDevice);
#warning Call refresh once every second?
		oplRetroRefresh (API->PipeProcess);

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

	if (inActive == 3) // do test
	{
		if (oplRetroTestRun (API) > 0)
		{
			return 1;
		} else  {
			inActive = 1; // normal
		}
	}

	oplRetroDraw (API, esel, &inActive);

	while (API->console->KeyboardHit() && inActive == 1) // = 0 exit, 1 = normal, 2 = edit custom
	{
		int key = API->console->KeyboardGetChar();

		switch (key)
		{
			case 't':
			case 'T':
				if ( (esel == 0) ||
				     ((esel == 1) && !strcasecmp (oplRetroCustomDevice, "auto")) )
				{
					char *temp = opl_config_retrowave_device_auto (API->PipeProcess); /* warning, this will refresh the device list, but we have selected the very first node, so should be safe */
					oplRetroRefresh (API->PipeProcess);
					oplRetroTestStart (API, temp ? temp : "NULL");
					free (temp);
				} else if (esel == 1)
				{
					oplRetroTestStart (API, oplRetroCustomDevice);
				} else {
					oplRetroTestStart (API, oplRetroDeviceEntry[esel - 2].device);
				}
				inActive = 3;
				break;
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

static char *opl_config_retrowave_device_auto (const struct PipeProcessAPI_t *PipeProcess)
{
	int i;
	char *retval = 0;
	oplRetroRefresh (PipeProcess);
	/* First try pure hits */
	for (i=0; i < oplRetroDeviceEntries; i++)
	{
		if (oplRetroDeviceEntry[i].verified == VerifiedAs_RetroWaveOPL3)
		{
			retval = strdup (oplRetroDeviceEntry[i].device);
			goto out;
		}
	}
	/* Secondly try possible hits */
	for (i=0; i < oplRetroDeviceEntries; i++)
	{
		if (oplRetroDeviceEntry[i].verified != NotVerified)
		{
			retval = strdup (oplRetroDeviceEntry[i].device);
			goto out;
		}
	}
	/* Third, take the first usb serial device if any */
	if (oplRetroDeviceEntries)
	{
		retval = strdup (oplRetroDeviceEntry[0].device);
	}
out:
	oplRetroDevicesDestroy();
	return retval;
}

extern "C"
{

OCP_INTERNAL char *opl_config_retrowave_device (const struct PipeProcessAPI_t *PipeProcess, const struct configAPI_t *configAPI)
{
	const char *temp = configAPI->GetProfileString ("adplug", "retrowave", "auto");
	if (!strcmp (temp, "auto"))
	{
		return opl_config_retrowave_device_auto (PipeProcess);
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

}
