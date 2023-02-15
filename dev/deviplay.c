/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Player devices system
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
#include "dev/deviplay.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/filesystem-setup.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"

struct plrDriverListEntry_t
{
	char name[32];
	const struct plrDriver_t *driver; /* can be NULL if driver is not found */
	int detected;
	int probed;
	int disabled;
};

static struct plrDriverListEntry_t *plrDriverList;
static int                          plrDriverListEntries;
static int                          plrDriverListNone;

const struct plrDriver_t *plrDriver;
const struct plrDevAPI_t *plrDevAPI;

static struct preprocregstruct plrPreprocess;

static int deviplayDriverListInsert (int insertat, const char *name, int length)
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

	assert ((insertat >= 0) && (insertat <= plrDriverListEntries));
	/* soft assert for name/length, since it comes from userspace */
	for (i = 0; i < plrDriverListEntries; i++)
	{
		if ((strlen (plrDriverList[i].name) == length) &&
		    !strncasecmp (plrDriverList[i].name, name, length))
		{
			fprintf (stderr, "deviplayDriverListInsert: duplicate entry found\n");
			return errOk;
		}
	}

	/* grow the plrDriverList */
	{
		struct plrDriverListEntry_t *temp;
		temp = realloc (plrDriverList, sizeof (plrDriverList[0]) * (plrDriverListEntries + 1));
		if (!temp)
		{
			fprintf (stderr, "deviplayDriverListInsert: realloc() failed\n");
			return errAllocMem;
		}
		plrDriverList = temp;
	}
	memmove (plrDriverList + insertat + 1, plrDriverList + insertat, sizeof (plrDriverList[0]) * (plrDriverListEntries - insertat));
	plrDriverListEntries++;
	snprintf (plrDriverList[insertat].name, sizeof (plrDriverList[insertat].name),
		  "%.*s", length, name);
	plrDriverList[insertat].driver = 0;
	plrDriverList[insertat].detected = 0;
	plrDriverList[insertat].probed = 0;
	plrDriverList[insertat].disabled = disabled;

	if ((length == 8) && !strncasecmp (name, "devpNone", 8))
	{
		plrDriverListNone = insertat;
	} else if (plrDriverListNone <= insertat)
	{
		plrDriverListNone = 0;
	}

	return errOk;
}

static int deviplayPreInit (void)
{
	const char *str, *next;
	/* this is ran before plugins are initialized */

	plrDriverListNone = -1;

	str = cfGetProfileString2 (cfSoundSec, "sound", "playerdevices", "devpNone");
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

		if ((err = deviplayDriverListInsert (plrDriverListEntries, str, length)))
		{
			return err;
		}
	}

	return errOk;
}

void plrRegisterDriver (const struct plrDriver_t *driver)
{
	int i;

	/* Locate the slot in the list from ocp.ini */
	for (i=0; i < plrDriverListEntries; i++)
	{
		if (!strcmp (plrDriverList[i].name, driver->name))
		{
			break;
		}
	}

	/* new driver that is not listed in ocp.ini? allocate a slot just before devpNone */
	if (i == plrDriverListEntries)
	{
		i = plrDriverListNone >= 0 ? plrDriverListNone : plrDriverListEntries;
		if (deviplayDriverListInsert (plrDriverListNone >= 0 ? plrDriverListNone : plrDriverListEntries, driver->name, strlen (driver->name)))
		{
			/* failure */
			return;
		}
	}

	if (plrDriverList[i].driver)
	{
		fprintf (stderr, "plrRegisterDriver: warning, driver %s already registered\n", driver->name);
		return;
	}

	plrDriverList[i].driver = driver;
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

static struct ocpfile_t *setup_devp;
static void setup_devp_run (void **token, const struct DevInterfaceAPI_t *API);

static int deviplayLateInit (void)
{
	const char *def;
	int i;

	plRegisterPreprocess (&plrPreprocess);

	setup_devp = dev_file_create (
		dmSetup->basedir,
		"devp.dev",
		"Select audio playback driver",
		"",
		0, /* token */
		0, /* Init */
		setup_devp_run,
		0, /* Close */
		0  /* Destructor */
	);
	filesystem_setup_register_file (setup_devp);


	fprintf (stderr, "playbackdevices:\n");

	/* Do we have a specific device specified on the command-line ? */
	def=cfGetProfileString("commandline_s", "p", "");
	if (strlen(def))
	{
		for (i=0; i < plrDriverListEntries; i++)
		{
			if (!strcasecmp (def, plrDriverList[i].name))
			{
				if (plrDriverList[i].driver)
				{
					plrDriverList[i].detected = plrDriverList[i].driver->Detect (plrDriverList[i].driver);
					plrDriverList[i].probed = 1;
					if (plrDriverList[i].detected)
					{
						plrDevAPI = plrDriverList[i].driver->Open (plrDriverList[i].driver, &ringbufferAPI);
						if (plrDevAPI)
						{
							fprintf (stderr, " %-8s: %s (selected due to -sp commandline)\n", plrDriverList[i].name, dots(""));
							plrDriver = plrDriverList[i].driver;
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
	for (i=0; i < plrDriverListEntries; i++)
	{
		if (!plrDriverList[i].driver)
		{
			fprintf (stderr, " %-8s: %s (driver not found)\n", plrDriverList[i].name, dots(""));
			continue;
		}
		if (plrDriverList[i].probed)
		{
			fprintf (stderr, " %-8s: %s (already probed)\n", plrDriverList[i].name, dots(plrDriverList[i].driver->description));
			continue;
		}

		plrDriverList[i].detected = plrDriverList[i].driver->Detect (plrDriverList[i].driver);
		plrDriverList[i].probed = 1;
		if (plrDriverList[i].detected)
		{
			plrDevAPI = plrDriverList[i].driver->Open (plrDriverList[i].driver, &ringbufferAPI);
			if (plrDevAPI)
			{
				fprintf (stderr, " %-8s: %s (detected)\n", plrDriverList[i].name, dots(plrDriverList[i].driver->description));
				plrDriver = plrDriverList[i].driver;
				for (i++ ;i < plrDriverListEntries; i++)
				{
					if (plrDriverList[i].driver)
					{
						fprintf (stderr, " %-8s: %s (skipped)\n", plrDriverList[i].name, dots(plrDriverList[i].driver->description));
					} else {
						fprintf (stderr, " %-8s: %s (driver not found)\n", plrDriverList[i].name, dots(""));
					}
				}
				return errOk;
			}
			fprintf (stderr, " %-8s: %s (not detected)\n", plrDriverList[i].name, dots(plrDriverList[i].driver->description));
		}
	}

	/* no driver enabled yet, soft error only */
	return errOk;
}

static void deviplayPreClose (void)
{
	int i;

	if (setup_devp)
	{
		filesystem_setup_unregister_file (setup_devp);
		setup_devp->unref (setup_devp);
		setup_devp = 0;
	}

	if (!plrDriver)
	{
		return;
	}
	for (i = 0; i < plrDriverListEntries; i++)
	{
		if (plrDriverList[i].driver == plrDriver)
		{
			plrDriverList[i].driver->Close (plrDriverList[i].driver);
			plrDriver = 0;
			plrDevAPI = 0;
			return;
		}
	}
}

void plrUnregisterDriver (const struct plrDriver_t *driver)
{
	int i;
	for (i=0; i < plrDriverListEntries; i++)
	{
		if (plrDriverList[i].driver == driver)
		{
			/* shutdown driver if active */
			if (driver == plrDriver)
			{
				plrDriverList[i].driver->Close (driver);
				plrDriver = 0;
				plrDevAPI = 0;
			}
			plrDriverList[i].driver = 0;
			return;
		}
	}
	fprintf (stderr, "plrUnregisterDriver: warning, driver %s not registered\n", driver->name);
}

static void deviplayLateClose (void)
{
	int i;
	for (i = 0; i < plrDriverListEntries; i++)
	{
		if (plrDriverList[i].driver)
		{
			fprintf (stderr, "deviplayLateClose: warning, driver %s still registered\n", plrDriverList[i].driver->name);
		}
	}

	free (plrDriverList);
	plrDriverList = 0;
	plrDriverListEntries = 0;
	plrDriverListNone = -1;
}

static void setup_devp_draw (const char *title, int dsel)
{
	unsigned int mlHeight;
	unsigned int mlTop;
	unsigned int mlLeft;
	unsigned int mlWidth;

	unsigned int i, skip, half, dot, fit;

	/* SETUP the framesize */
	if (plrDriverListEntries < 3)
	{
		mlHeight = 10;
	} else {
		mlHeight = plrDriverListEntries + 7;
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
	if (plrDriverListEntries <= fit)
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (dsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (dsel >= (plrDriverListEntries - half))
	{ /* we are at the bottom part */
		skip = plrDriverListEntries - fit;
		dot = fit - 1;
	} else {
		skip = dsel - half;
		dot = skip * (fit) / (plrDriverListEntries - (fit));
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

		if (index >= plrDriverListEntries)
		{
			display_nprintf (mlTop + i, mlLeft, 0x09, mlWidth, "\xb3%.*C \xb3", mlWidth - 2);
			continue;
		}

		if (!plrDriverList[index].driver)
		{
			color = 12;
			msg = "(driver not found)";
		} else if (plrDriverList[index].driver == plrDriver)
		{
			color = 10;
			msg="(active)";
		} else if (plrDriverList[index].disabled)
		{
			color = 1;
			msg = "(disabled)";
		} else if (plrDriverList[index].probed && !plrDriverList[index].detected)
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
			plrDriverList[index].name,
			dots(plrDriverList[index].driver?plrDriverList[index].driver->description:""),
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

	if ((dsel >= plrDriverListEntries) || (plrDriver && (plrDriverList[dsel].driver == plrDriver)))
	{
		display_nprintf (mlTop + mlHeight - 2,  mlLeft, 0x09, mlWidth, "\xc3%*C \xb4", mlWidth - 2); /* |            | */
	} else if ((!plrDriverList[dsel].driver) && (!plrDriverList[dsel].disabled))
	{
		display_nprintf (mlTop + mlHeight - 2, mlLeft, 0x09, mlWidth,
			"\xb3%0.7o "
			"%0.15o<d>%0.7o: disable driver  "
			"%0.15o<DEL>%0.7o: delete entry  "
			"%0.9o                         \xb3"
		);
	} else if ((!plrDriverList[dsel].driver) && (plrDriverList[dsel].disabled))
	{
		display_nprintf (mlTop + mlHeight - 2, mlLeft, 0x09, mlWidth,
			"\xb3%0.7o "
			"%0.15o<e>%0.7o: enable driver  "
			"%0.15o<DEL>%0.7o: delete entry  "
			"%0.9o                          \xb3"
		);
	} else if (plrDriverList[dsel].disabled)
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

static void devp_save_devices (const struct DevInterfaceAPI_t *API)
{
	int len = 1;
	int i;
	char *tmp;
	for (i=0; i < plrDriverListEntries; i++)
	{
		len += (i?1:0) + (plrDriverList[i].disabled?1:0) + strlen (plrDriverList[i].name);
	}
	tmp = calloc (1, len);
	if (!tmp)
	{
		fprintf (stderr, "devp_save_devices: calloc() failed\n");
		return;
	}
	for (i=0; i < plrDriverListEntries; i++)
	{
		if (i) strcat (tmp, " ");
		if (plrDriverList[i].disabled) strcat (tmp, "-");
		strcat (tmp, plrDriverList[i].name);
	}
	cfSetProfileString (cfSoundSec, "playerdevices", tmp);
	free (tmp);
}

static void setup_devp_run (void **token, const struct DevInterfaceAPI_t *API)
{
	int dsel = 0;
	while (1)
	{
		API->fsDraw();
		setup_devp_draw("Playback plugins", dsel);
		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_DOWN:
					if (dsel + 1 < plrDriverListEntries)
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
					dsel = plrDriverListEntries ? plrDriverListEntries - 1 : 0;
					break;
				case KEY_EXIT:
				case KEY_ESC:
					devp_save_devices (API);
					API->configAPI->StoreConfig();
					return;
				case _KEY_ENTER:
					if ((dsel < plrDriverListEntries) &&
					    plrDriverList[dsel].driver &&
					    (plrDriverList[dsel].driver != plrDriver) &&
					    (!plrDriverList[dsel].disabled) &&
					    (!(plrDriverList[dsel].probed && !plrDriverList[dsel].detected)))
					{
						API->console->Driver->consoleRestore();
						if (plrDriver)
						{
							plrDriver->Close (plrDriver);
							plrDriver=0;
						}
						if (!plrDriverList[dsel].probed)
						{
							plrDriverList[dsel].detected = plrDriverList[dsel].driver->Detect (plrDriverList[dsel].driver);
							plrDriverList[dsel].probed = 1;
						}
						if (plrDriverList[dsel].detected)
						{
							plrDevAPI = plrDriverList[dsel].driver->Open (plrDriverList[dsel].driver, &ringbufferAPI);
							if (plrDevAPI)
							{
								plrDriver = plrDriverList[dsel].driver;
							}
						}
						API->console->Driver->consoleSave();
					}
					break;
				case '+':
					if (dsel)
					{
						struct plrDriverListEntry_t temp;
						temp                  = plrDriverList[dsel-1];
						plrDriverList[dsel-1] = plrDriverList[dsel];
						plrDriverList[dsel]   = temp;
						dsel--;
					}
					break;
				case '-':
					if ((plrDriverListEntries >= 2) &&
					    (dsel < (plrDriverListEntries-1)))
					{
						struct plrDriverListEntry_t temp;
						temp                  = plrDriverList[dsel+1];
						plrDriverList[dsel+1] = plrDriverList[dsel];
						plrDriverList[dsel]   = temp;
						dsel++;
					}
					break;
				case 'd':
				case 'D':
					if ((dsel < plrDriverListEntries) &&
					    (!(plrDriverList[dsel].driver && (plrDriverList[dsel].driver == plrDriver))) &&
					    (!plrDriverList[dsel].disabled) &&
					    (!(plrDriverList[dsel].probed && !plrDriverList[dsel].detected)))
					{
						plrDriverList[dsel].disabled = 1;
					}
					break;
				case 'e':
				case 'E':
					if ((dsel < plrDriverListEntries) &&
					    plrDriverList[dsel].disabled)
					{
						plrDriverList[dsel].disabled = 0;
					}
					break;
				case KEY_DELETE:
					if ((dsel < plrDriverListEntries) &&
					    !plrDriverList[dsel].driver)
					{
						memmove (plrDriverList + dsel, plrDriverList + dsel + 1, sizeof (plrDriverList[0]) * (plrDriverListEntries - dsel - 1));
						plrDriverListEntries--;
					}
					if (dsel >= plrDriverListEntries)
					{
						dsel = plrDriverListEntries ? plrDriverListEntries - 1 : 0;
					}
					break;
				default:
					break;
			}
		}
		API->console->FrameLock();
	}
}

static void plrPrep(struct moduleinfostruct *m, struct ocpfilehandle_t **bp)
{
	// plrResetDevice ();
}

static struct preprocregstruct plrPreprocess = {plrPrep PREPROCREGSTRUCT_TAIL};
DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "plrbase", .desc = "OpenCP Player Devices System (c) 1994-'23 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .PreInit = deviplayPreInit, .LateInit = deviplayLateInit, .PreClose = deviplayPreClose, .LateClose = deviplayLateClose, .sortindex = 30};
