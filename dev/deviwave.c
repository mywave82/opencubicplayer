/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Wavetable devices system
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -changed INI reading of driver symbols to _dllinfo lookup
 */

#include "config.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "deviwave.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/filesystem-setup.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "mcp.h"
#include "player.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"

struct mcpDriverListEntry_t
{
	char name[32];
	const struct mcpDriver_t *driver; /* can be NULL if driver is not found */
	int detected;
	int probed;
	int disabled;
};

static struct mcpDriverListEntry_t *mcpDriverList;
static int                          mcpDriverListEntries;
static int                          mcpDriverListNone;

const struct mcpDriver_t *mcpDriver;
const struct mcpDevAPI_t *mcpDevAPI;

static struct preprocregstruct mcpPreprocess;

static int deviwaveDriverListInsert (int insertat, const char *name, int length)
{
	int i;
	int disabled = 0;

	if (name[0] == '-')
	{
		disabled = 1;
		name++;
		length--;
	}

	if (!length)
	{
		return errOk;
	}

	assert ((insertat >= 0) && (insertat <= mcpDriverListEntries));
	/* soft assert for name/length, since it comes from userspace */
	for (i = 0; i < mcpDriverListEntries; i++)
	{
		if ((strlen (mcpDriverList[i].name) == length) &&
		    !strncasecmp (mcpDriverList[i].name, name, length))
		{
			fprintf (stderr, "deviwaveDriverListInsert: duplicate entry found\n");
			return errOk;
		}
	}

	/* grow the mcpDriverList */
	{
		struct mcpDriverListEntry_t *temp;
		temp = realloc (mcpDriverList, sizeof (mcpDriverList[0]) * (mcpDriverListEntries + 1));
		if (!temp)
		{
			fprintf (stderr, "deviwaveDriverListInsert: realloc() failed\n");
			return errAllocMem;
		}
		mcpDriverList = temp;
	}
	memmove (mcpDriverList + insertat + 1, mcpDriverList + insertat, sizeof (mcpDriverList[0]) * (mcpDriverListEntries - insertat));
	mcpDriverListEntries++;
	snprintf (mcpDriverList[insertat].name, sizeof (mcpDriverList[insertat].name),
		  "%.*s", length, name);
	mcpDriverList[insertat].driver = 0;
	mcpDriverList[insertat].detected = 0;
	mcpDriverList[insertat].probed = 0;
	mcpDriverList[insertat].disabled = disabled;

	if ((length == 8) && !strncasecmp (name, "devwNone", 8))
	{
		mcpDriverListNone = insertat;
	} else if (mcpDriverListNone <= insertat)
	{
		mcpDriverListNone = 0;
	}

	return errOk;
}

static int deviwavePreInit (void)
{
	const char *str, *next;
	/* this is ran before plugins are initialized */

	mcpDriverListNone = -1;

	str = cfGetProfileString2 (cfSoundSec, "sound", "wavetabledevices", "devwNone");
	if (!strlen(str))
	{
		return errOk;
	}

	for (; *str; str = next)
	{
		int length, err;

		next = strpbrk (str, " \t\r\n");
		if (next)
		{
			length = next - str;
			next++;
		} else {
			length = strlen (str);
			next = str + length;
		}

		if ((err = deviwaveDriverListInsert (mcpDriverListEntries, str, length)))
		{
			return err;
		}
	}

	return errOk;
}

void mcpRegisterDriver (const struct mcpDriver_t *driver)
{
	int i;

	/* Locate the slot in the list from ocp.ini */
	for (i=0; i < mcpDriverListEntries; i++)
	{
		if (!strcmp (mcpDriverList[i].name, driver->name))
		{
			break;
		}
	}

	/* new driver that is not listed in ocp.ini? allocate a slot just before devwNone */
	if (i == mcpDriverListEntries)
	{
		i = mcpDriverListNone >= 0 ? mcpDriverListNone : mcpDriverListEntries;
		if (deviwaveDriverListInsert (mcpDriverListNone >= 0 ? mcpDriverListNone : mcpDriverListEntries, driver->name, strlen (driver->name)))
		{
			/* failure */
			return;
		}
	}

	if (mcpDriverList[i].driver)
	{
		fprintf (stderr, "mcpRegisterDriver: warning, driver %s already registered\n", driver->name);
		return;
	}

	mcpDriverList[i].driver = driver;
}

static const char *dots (const char *src)
{
	static char buf[34];
	int l = strlen (src);
	if (l > 32)
	{
		l = 32;
	}
	snprintf (buf, sizeof (buf), "%.*s%.*s", l, src, 32 - l, "................................");
	return buf;
}

static struct ocpfile_t *setup_devw;
static void setup_devw_run (void **token, const struct DevInterfaceAPI_t *API);

static int deviwaveLateInit (void)
{
	const char *def;
	int i, playrate;

	plRegisterPreprocess (&mcpPreprocess);

	setup_devw = dev_file_create (
		dmSetup->basedir,
		"devw.dev",
		"Select audio playback driver",
		"",
		0, /* token */
		0, /* Init */
		setup_devw_run,
		0, /* Close */
		0  /* Destructor */
	);
	filesystem_setup_register_file (setup_devw);

	playrate=cfGetProfileInt("commandline_s", "r", cfGetProfileInt2(cfSoundSec, "sound", "mixrate", 44100, 10), 10);
	if (playrate<66)
	{
		if (playrate%11)
			playrate*=1000;
		else
			playrate=playrate*11025/11;
	}
	mcpMixMaxRate=playrate;
	mcpMixProcRate=cfGetProfileInt2(cfSoundSec, "sound", "mixprocrate", 1536000, 10);

	fprintf (stderr, "wavetabledevices:\n");

	/* Do we have a specific device specified on the command-line ? */
	def=cfGetProfileString("commandline_s", "w", "");
	if (strlen(def))
	{
		for (i=0; i < mcpDriverListEntries; i++)
		{
			if (!strcasecmp (def, mcpDriverList[i].name))
			{
				if (mcpDriverList[i].driver)
				{
					mcpDriverList[i].detected = mcpDriverList[i].driver->Detect (mcpDriverList[i].driver);
					mcpDriverList[i].probed = 1;
					if (mcpDriverList[i].detected)
					{
						mcpDevAPI = mcpDriverList[i].driver->Open (mcpDriverList[i].driver);
						if (mcpDevAPI)
						{
							fprintf (stderr, " %-8s: %s (selected due to -sw commandline)\n", mcpDriverList[i].name, dots(""));
							mcpDriver = mcpDriverList[i].driver;
							return errOk;
						}
					}
				}
				break;
			}
		}
		fprintf (stderr, "Unable to find/initialize driver specificed with -sp\n");
	}

	/* Do the regular auto-detection */
	for (i=0; i < mcpDriverListEntries; i++)
	{
		if (!mcpDriverList[i].driver)
		{
			fprintf (stderr, " %-8s: %s (driver not found)\n", mcpDriverList[i].name, dots(""));
			continue;
		}
		if (mcpDriverList[i].probed)
		{
			fprintf (stderr, " %-8s: %s (already probed)\n", mcpDriverList[i].name, dots(mcpDriverList[i].driver->description));
			continue;
		}

		mcpDriverList[i].detected = mcpDriverList[i].driver->Detect (mcpDriverList[i].driver);
		mcpDriverList[i].probed = 1;
		if (mcpDriverList[i].detected)
		{
			mcpDevAPI = mcpDriverList[i].driver->Open (mcpDriverList[i].driver);
			if (mcpDevAPI)
			{
				fprintf (stderr, " %-8s: %s (detected)\n", mcpDriverList[i].name, dots(mcpDriverList[i].driver->description));
				mcpDriver = mcpDriverList[i].driver;
				for (i++ ;i < mcpDriverListEntries; i++)
				{
					if (mcpDriverList[i].driver)
					{
						fprintf (stderr, " %-8s: %s (skipped)\n", mcpDriverList[i].name, dots(mcpDriverList[i].driver->description));
					} else {
						fprintf (stderr, " %-8s: %s (driver not found)\n", mcpDriverList[i].name, dots(""));
					}
				}
				return errOk;
			}
			fprintf (stderr, " %-8s: %s (not detected)\n", mcpDriverList[i].name, dots(mcpDriverList[i].driver->description));
		}
	}

	/* no driver enabled yet, soft error only */
	return errOk;
}

static void deviwavePreClose (void)
{
	int i;

	if (setup_devw)
	{
		filesystem_setup_unregister_file (setup_devw);
		setup_devw->unref (setup_devw);
		setup_devw = 0;
	}

	if (!mcpDriver)
	{
		return;
	}
	for (i = 0; i < mcpDriverListEntries; i++)
	{
		if (mcpDriverList[i].driver == mcpDriver)
		{
			mcpDriverList[i].driver->Close (mcpDriverList[i].driver);
			mcpDriver = 0;
			mcpDevAPI = 0;
			return;
		}
	}
}

void mcpUnregisterDriver (const struct mcpDriver_t *driver)
{
	int i;
	for (i=0; i < mcpDriverListEntries; i++)
	{
		if (mcpDriverList[i].driver == driver)
		{
			/* shutdown driver if active */
			if (driver == mcpDriver)
			{
				mcpDriverList[i].driver->Close (driver);
				mcpDriver = 0;
				mcpDevAPI = 0;
			}
			mcpDriverList[i].driver = 0;
			return;
		}
	}
	fprintf (stderr, "mcpUnregisterDriver: warning, driver %s not registered\n", driver->name);
}

static void deviwaveLateClose (void)
{
	int i;
	for (i = 0; i < mcpDriverListEntries; i++)
	{
		if (mcpDriverList[i].driver)
		{
			fprintf (stderr, "deviwaveLateClose: warning, driver %s still registered\n", mcpDriverList[i].driver->name);
		}
	}

	free (mcpDriverList);
	mcpDriverList = 0;
	mcpDriverListEntries = 0;
	mcpDriverListNone = -1;
}

static void setup_devw_draw (const char *title, int dsel)
{
	unsigned int mlHeight;
	unsigned int mlTop;
	unsigned int mlLeft;
	unsigned int mlWidth;

	unsigned int i, skip, half, dot, fit;

	/* SETUP the framesize */
	if (mcpDriverListEntries < 3)
	{
		mlHeight = 10;
	} else {
		mlHeight = mcpDriverListEntries + 7;
		if (mlHeight > (plScrHeight - 2))
		{
			mlHeight = plScrHeight - 2;
		}
	}
	fit = mlHeight - 7;
	mlTop = (plScrHeight - mlHeight) / 2;

	mlWidth = 70;
	mlLeft = (plScrWidth - mlWidth) / 2;

	half = fit / 2;
	if (mcpDriverListEntries <= fit)
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (dsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (dsel >= (mcpDriverListEntries - half))
	{ /* we are at the bottom part */
		skip = mcpDriverListEntries - fit;
		dot = fit - 1;
	} else {
		skip = dsel - half;
		dot = skip * (fit) / (mcpDriverListEntries - (fit));
	}

	{
		int CacheLen = strlen (title);
		int Skip = (mlWidth - CacheLen - 2) / 2;
		display_nprintf (mlTop, mlLeft, 0x09, mlWidth, "\xda%*C\xc4 %s %*C\xc4\xbf", Skip - 1, title, mlWidth - Skip - 3 - CacheLen); /* +----- title ------+ */
	}

	display_nprintf (mlTop + 1, mlLeft, 0x09, mlWidth, "\xb3%0.7o Available audio drivers, and their priority in the autodection%*C %0.9o\xb3", mlWidth - 65);

	display_nprintf (mlTop + 2,  mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2); /* |            | */

	for (i = 3; i < (mlHeight-3); i++)
	{
		int index = i - 3 + skip;
		int color;
		const char *msg;

		if (index >= mcpDriverListEntries)
		{
			display_nprintf (mlTop + i, mlLeft, 0x09, mlWidth, "\xb3%.*C \xb3", mlWidth - 2);
			continue;
		}

		if (!mcpDriverList[index].driver)
		{
			color = 12;
			msg = "(driver not found)";
		} else if (mcpDriverList[index].driver == mcpDriver)
		{
			color = 10;
			msg="(active)";
		} else if (mcpDriverList[index].disabled)
		{
			color = 1;
			msg = "(disabled)";
		} else if (mcpDriverList[index].probed && !mcpDriverList[index].detected)
		{
			color = 1;
			msg = "(detection failed)";
		} else {
			color = 7;
			msg = "";
		}

		display_nprintf (mlTop + i, mlLeft, 0x09, mlWidth, "\xb3%*.*o%-.3d %.*o%.8s: %s %.*o%.18s   %0.9o%c",
			(dsel == index)?7:0,
			(dsel == index)?0:7,
			index + 1,
			(dsel == index)?0:3,
			mcpDriverList[index].name,
			dots(mcpDriverList[index].driver?mcpDriverList[index].driver->description:""),
			color,
			msg,
			((i-3) == dot) ? '\xdd':'\xb3'
		);
	}

	display_nprintf (mlTop + mlHeight - 4, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2); /* +---------------+ */
	display_nprintf (mlTop + mlHeight - 3, mlLeft, 0x09, mlWidth,
		"\xb3%0.7o "
		"%0.15o<\x18>%0.7o/%0.15o<\x19>%0.7o: Navigate  "
		"%0.15o<+>%0.7o/%0.15o<->%0.7o: Change priority  "
		"<ESC>%0.7o close dialog  "
		"%0.9o  \xb3"
	);

	if ((dsel >= mcpDriverListEntries) || (mcpDriver && (mcpDriverList[dsel].driver == mcpDriver)))
	{
		display_nprintf (mlTop + mlHeight - 2,  mlLeft, 0x09, mlWidth, "\xc3%*C \xb4", mlWidth - 2); /* |            | */
	} else if ((!mcpDriverList[dsel].driver) && (!mcpDriverList[dsel].disabled))
	{
		display_nprintf (mlTop + mlHeight - 2, mlLeft, 0x09, mlWidth,
			"\xb3%0.7o "
			"%0.15o<d>%0.7o: disable driver  "
			"%0.15o<DEL>%0.7o: delete entry  "
			"%0.9o                         \xb3"
		);
	} else if ((!mcpDriverList[dsel].driver) && (mcpDriverList[dsel].disabled))
	{
		display_nprintf (mlTop + mlHeight - 2, mlLeft, 0x09, mlWidth,
			"\xb3%0.7o "
			"%0.15o<e>%0.7o: enable driver  "
			"%0.15o<DEL>%0.7o: delete entry  "
			"%0.9o                          \xb3"
		);
	} else if (mcpDriverList[dsel].disabled)
	{
		display_nprintf (mlTop + mlHeight - 2, mlLeft, 0x09, mlWidth,
			"\xb3%0.7o "
			"%0.15o<e>%0.7o: enable driver"
			"                       "
			"%0.9o                          \xb3"
		);
	} else {
		display_nprintf (mlTop + mlHeight - 2, mlLeft, 0x09, mlWidth,
			"\xb3%0.7o "
			"%0.15o<ENTER>%0.7o: activate driver  "
			"%0.15o<d>%0.7o: disable driver  "
			"%0.9o                       \xb3"
		);
	}

	display_nprintf (mlTop + mlHeight - 1,  mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2); /* +---------------+ */
}

static void devw_save_devices (const struct DevInterfaceAPI_t *API)
{
	int len = 1;
	int i;
	char *tmp;
	for (i=0; i < mcpDriverListEntries; i++)
	{
		len += (i?1:0) + (mcpDriverList[i].disabled?1:0) + strlen (mcpDriverList[i].name);
	}
	tmp = calloc (1, len);
	if (!tmp)
	{
		fprintf (stderr, "devw_save_devices: calloc() failed\n");
		return;
	}
	for (i=0; i < mcpDriverListEntries; i++)
	{
		if (i) strcat (tmp, " ");
		if (mcpDriverList[i].disabled) strcat (tmp, "-");
		strcat (tmp, mcpDriverList[i].name);
	}
	cfSetProfileString (cfSoundSec, "wavetabledevices", tmp);
	free (tmp);
}

static void setup_devw_run (void **token, const struct DevInterfaceAPI_t *API)
{
	int dsel = 0;
	while (1)
	{
		API->fsDraw();
		setup_devw_draw("Wavetable plugins", dsel);
		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_DOWN:
					if (dsel + 1 < mcpDriverListEntries)
					{
						dsel++;
					}
					break;
				case KEY_UP:
					if (dsel > 0)
					{
						dsel--;
					}
					break;
				case KEY_HOME:
					dsel = 0;
					break;
				case KEY_END:
					dsel = mcpDriverListEntries ? mcpDriverListEntries - 1 : 0;
					break;
				case KEY_EXIT:
				case KEY_ESC:
					devw_save_devices (API);
					API->configAPI->StoreConfig();
					return;
				case _KEY_ENTER:
					if ((dsel < mcpDriverListEntries) &&
					    mcpDriverList[dsel].driver &&
					    (mcpDriverList[dsel].driver != mcpDriver) &&
					    (!mcpDriverList[dsel].disabled) &&
					    (!(mcpDriverList[dsel].probed && !mcpDriverList[dsel].detected)))
					{
						API->console->Driver->consoleRestore();
						if (mcpDriver)
						{
							mcpDriver->Close (mcpDriver);
							mcpDriver=0;
						}
						if (!mcpDriverList[dsel].probed)
						{
							mcpDriverList[dsel].detected = mcpDriverList[dsel].driver->Detect (mcpDriverList[dsel].driver);
							mcpDriverList[dsel].probed = 1;
						}
						if (mcpDriverList[dsel].detected)
						{
							mcpDevAPI = mcpDriverList[dsel].driver->Open (mcpDriverList[dsel].driver);
							if (mcpDevAPI)
							{
								mcpDriver = mcpDriverList[dsel].driver;
							}
						}
						API->console->Driver->consoleSave();
					}
					break;
				case '+':
					if (dsel)
					{
						struct mcpDriverListEntry_t temp;
						temp                  = mcpDriverList[dsel-1];
						mcpDriverList[dsel-1] = mcpDriverList[dsel];
						mcpDriverList[dsel]   = temp;
						dsel--;
					}
					break;
				case '-':
					if ((mcpDriverListEntries >= 2) &&
					    (dsel < (mcpDriverListEntries-1)))
					{
						struct mcpDriverListEntry_t temp;
						temp                  = mcpDriverList[dsel+1];
						mcpDriverList[dsel+1] = mcpDriverList[dsel];
						mcpDriverList[dsel]   = temp;
						dsel++;
					}
					break;
				case 'd':
				case 'D':
					if ((dsel < mcpDriverListEntries) &&
					    (!(mcpDriverList[dsel].driver && (mcpDriverList[dsel].driver == mcpDriver))) &&
					    (!mcpDriverList[dsel].disabled) &&
					    (!(mcpDriverList[dsel].probed && !mcpDriverList[dsel].detected)))
					{
						mcpDriverList[dsel].disabled = 1;
					}
					break;
				case 'e':
				case 'E':
					if ((dsel < mcpDriverListEntries) &&
					    mcpDriverList[dsel].disabled)
					{
						mcpDriverList[dsel].disabled = 0;
					}
					break;
				case KEY_DELETE:
					if ((dsel < mcpDriverListEntries) &&
					    !mcpDriverList[dsel].driver)
					{
						memmove (mcpDriverList + dsel, mcpDriverList + dsel + 1, sizeof (mcpDriverList[0]) * (mcpDriverListEntries - dsel - 1));
						mcpDriverListEntries--;
					}
					if (dsel >= mcpDriverListEntries)
					{
						dsel = mcpDriverListEntries ? mcpDriverListEntries - 1 : 0;
					}
					break;
				default:
					break;
			}
		}
		API->console->FrameLock();
	}
}

static void mcpPrep(struct moduleinfostruct *info, struct ocpfilehandle_t **bp)
{
	// mcpResetDevice();
/*
	if (info->gen.moduleflags&MDB_BIGMODULE)           TODO
		mcpSetDevice(cfGetProfileString2(cfSoundSec, "sound", "bigmodules", ""), 0);
*/
}

static struct preprocregstruct mcpPreprocess = {mcpPrep PREPROCREGSTRUCT_TAIL};
DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "mcpbase", .desc = "OpenCP Wavetable Devices System (c) 1994-'23 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .PreInit = deviwavePreInit, .LateInit = deviwaveLateInit, .PreClose = deviwavePreClose, .LateClose = deviwaveLateClose, .sortindex = 30};
