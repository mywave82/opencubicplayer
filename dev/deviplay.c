/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "dev/plrasm.h"
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

const struct plrDevAPI_t *plrDevAPI; /* handle from the selected driver */
static const struct plrDriver_t *plrDriver; /* current selected driver */
static const struct plrDriverAPI_t plrDriverAPI = /* API provided from OCP to the driver */
{
	&ringbufferAPI,
	plrGetRealMasterVolume,
	plrGetMasterSample,
	plrConvertBufferFromStereo16BitSigned
};

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

static int deviplayPreInit (const struct configAPI_t *configAPI)
{
	const char *str, *next;
	/* this is ran before plugins are initialized */

	plrDriverListNone = -1;

	str = configAPI->GetProfileString2 (configAPI->SoundSec, "sound", "playerdevices", "devpNone");
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

static int deviplayLateInit (struct PluginInitAPI_t *API)
{
	const char *def;
	int i;


	setup_devp = API->dev_file_create (
		API->dmSetup->basedir,
		"devp.dev",
		"Select audio playback driver",
		"",
		0, /* token */
		0, /* Init */
		setup_devp_run,
		0, /* Close */
		0  /* Destructor */
	);
	API->filesystem_setup_register_file (setup_devp);

	fprintf (stderr, "playbackdevices:\n");

	/* Do we have a specific device specified on the command-line ? */
	def=API->configAPI->GetProfileString("commandline_s", "p", "");
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
						plrDevAPI = plrDriverList[i].driver->Open (plrDriverList[i].driver, &plrDriverAPI);
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
			plrDevAPI = plrDriverList[i].driver->Open (plrDriverList[i].driver, &plrDriverAPI);
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

static void deviplayPreClose (struct PluginCloseAPI_t *API)
{
	int i;

	if (setup_devp)
	{
		API->filesystem_setup_unregister_file (setup_devp);
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

static void setup_devp_draw_driver (const struct DevInterfaceAPI_t *API, const char *title, int dsel)
{
	unsigned int mlHeight;
	unsigned int mlTop;
	unsigned int mlLeft;
	unsigned int mlWidth;

	unsigned int i, skip, half, dot, fit;

#if (CONSOLE_MIN_Y < 10)
# error setup_devp_draw_driver() requires CONSOLE_MIN_Y >= 10
#endif

	/* SETUP the framesize */
	if (plrDriverListEntries < 3)
	{
		mlHeight = 10;
	} else {
		mlHeight = plrDriverListEntries + 7;
		if (mlHeight > (API->console->TextHeight - 2))
		{
			mlHeight = API->console->TextHeight - 2;
		}
	}
	fit = mlHeight - 7;
	mlTop = (API->console->TextHeight - mlHeight) / 2;

	mlWidth = 70;
	mlLeft = (API->console->TextWidth - mlWidth) / 2;

	half = fit / 2;
	if (plrDriverListEntries <= fit)
	{ /* all entries can fit */
		skip = 0;
		dot = 0;
	} else if (dsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 3;
	} else if (dsel >= (plrDriverListEntries - half))
	{ /* we are at the bottom part */
		skip = plrDriverListEntries - fit;
		dot = fit + 2;
	} else {
		skip = dsel - half;
		dot = skip * (fit) / (plrDriverListEntries - (fit)) + 3;
	}

	API->console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, title, dot, 2, mlHeight - 4);
	mlWidth -= 2;
	mlHeight -= 2;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Available audio drivers, and their priority in the autodetection:");

	mlTop++; // 2: horizontal bar

	for (i = 2; i < (mlHeight-3); i++)
	{
		int index = i - 2 + skip;
		int color;
		const char *msg;

		if (index >= plrDriverListEntries)
		{
			mlTop++;
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

		API->console->DisplayPrintf (mlTop++, mlLeft,
			(((dsel == index)?7:0) << 4) |
			 ((dsel == index)?0:7),
			mlWidth, "%-.3d %.*o%.8s: %s %.*o%.18s",
			index + 1,
			(dsel == index)?0:3,
			plrDriverList[index].name,
			dots(plrDriverList[index].driver?plrDriverList[index].driver->description:""),
			color,
			msg
		);
	}

	mlTop++; // Horizontal bar
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x0f, mlWidth,
		" <\x18>%0.7o/%0.15o<\x19>%0.7o: Navigate  "
		"%0.15o<+>%0.7o/%0.15o<->%0.7o: Change priority  "
		"%0.15o<ESC>%0.7o close dialog"
	);

	if ((dsel >= plrDriverListEntries) || (plrDriver && (plrDriverList[dsel].driver == plrDriver)))
	{
		mlTop++;
	} else if ((!plrDriverList[dsel].driver) && (!plrDriverList[dsel].disabled))
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x0f, mlWidth,
			" <d>%0.7o: disable driver  "
			"%0.15o<DEL>%0.7o: delete entry"
		);
	} else if ((!plrDriverList[dsel].driver) && (plrDriverList[dsel].disabled))
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x0f, mlWidth,
			" <e>%0.7o: enable driver  "
			"%0.15o<DEL>%0.7o: delete entry"
		);
	} else if (plrDriverList[dsel].disabled)
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x0f, mlWidth,
			" <e>%0.7o: enable driver"
		);
	} else {
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x0f, mlWidth,
			" <ENTER>%0.7o: activate driver  "
			"%0.15o<d>%0.7o: disable driver"
		);
	}
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
	API->configAPI->SetProfileString (API->configAPI->SoundSec, "playerdevices", tmp);
	free (tmp);
}

static void setup_devp_run_driver (void **token, const struct DevInterfaceAPI_t *API)
{
	int dsel = 0;
	while (1)
	{
		API->fsDraw();
		setup_devp_draw_driver (API, "Playback plugins", dsel);
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
							plrDevAPI = plrDriverList[dsel].driver->Open (plrDriverList[dsel].driver, &plrDriverAPI);
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

static void DrawDelayBar (const struct DevInterfaceAPI_t *API, const int left, const int lineno, const int width, int value, const int active)
{
 /* 500ms  150 [......#] 1000
   [  6 ][  6 ]1       1[ 5 ] */
	if (value > 1000)
	{
		value = 1000;
	}
	if (value < 150)
	{
		value = 150;
	}

	int tw = width - 6 - 6 - 1 - 1 - 5;
	int pw = tw * (value - 150) / (1000 - 150 + 1) /* max-level - min-level */;
	API->console->DisplayPrintf (lineno, left, active ? 0x0f : 0x08, width, "%-4dms%.*o  150 [%*C.%.*o#%.*o%*C.] 1000",
		value,
		active ? 0x07 : 0x08,
		pw,
		active ? 0x0f : 0x08,
		active ? 0x07 : 0x08,
		tw - pw - 1);
}

static void setup_devp_draw_main (const struct DevInterfaceAPI_t *API, const char *title, int dsel, const char *plrDriverName, const int plrbufsize)
{
/************************ Playback plugins ****************************  1
 *                                                                    *  2  opt
 *       Setup of audio playback driver. Exit with <ESC> key          *  3
 *                                                                    *  4  opt
 **********************************************************************  5
 *                                                                    *  6  opt
 * 1. Modify detection priority order, and/or temporary force driver. *  7
 *    Current active driver: devpALSA                                 *  8
 *                                                                    *  9
 * 2. Audio buffer length (in miliseconds)                            *  10
 '    200ms    150 [...........#.........................] 1000       *  11
 *                                                                    *  12 opt
 **********************************************************************  13 */
#if (CONSOLE_MIN_Y < 10)
# error setup_devp_draw_driver() requires CONSOLE_MIN_Y >= 9
#endif

	int big = API->console->TextHeight >= 9;
	unsigned int mlHeight = big ? 13 : 9;
	unsigned int mlWidth = 70;
	unsigned int mlTop = (API->console->TextHeight - mlHeight) / 2;
	unsigned int mlLeft = (API->console->TextWidth - mlWidth) / 2;

	API->console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, title, -1, big ? 4 : 2, 0 /* no second bar */);
	mlWidth -= 2;
	mlHeight -= 2;

	mlTop += big;

	API->console->DisplayPrintf (mlTop++, mlLeft + 7, 0x07, mlWidth - 7, "Setup of audio playback driver. Exit with %.15o<ESC>%.7o key.");

	mlTop += big;

	mlTop += 1; // horizontal bar

	mlTop += big;

	API->console->DisplayPrintf (mlTop++, mlLeft + 1, 0x07, mlWidth-2, "%*.*o1. %.*oModify detection priority order, and/or temporary force driver.", (dsel == 0) ? 7 : 0, (dsel == 0) ? 1 : 7, (dsel == 0) ? 1 : 3);
	API->console->DisplayPrintf (mlTop++, mlLeft + 4, 0x02, mlWidth-4, "Current active driver: %.*o%s", plrDriverName ? 1 : 2, plrDriverName ? plrDriverName : "none");

	mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft + 1, 0x07, mlWidth-2, "%*.*o2. %.*oAudio buffer length (in miliseconds).", (dsel == 1) ? 7 : 0, (dsel == 1) ? 1 : 7, (dsel == 1) ? 1 : 3);
	DrawDelayBar (API, mlLeft + 4, mlTop++, mlWidth - 5, plrbufsize, dsel == 1);
}


static void setup_devp_run (void **token, const struct DevInterfaceAPI_t *API)
{
	int repeat = 1;
	uint32_t lastpress = 0;
	int dsel = 0;
	int plrbufsize = API->configAPI->GetProfileInt2 (API->configAPI->SoundSec, "sound", "plrbufsize", 200, 10);
	while (1)
	{
		API->fsDraw();
		setup_devp_draw_main (API, "Playback plugins", dsel, plrDriver ? plrDriver->name : 0, plrbufsize);
		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();
			if ((key != KEY_LEFT) && (key != KEY_RIGHT) && (key != '+') && (key != '-'))
			{
				lastpress = 0;
				repeat = 1;
			} else {
				uint32_t newpress = clock_ms();
				if ((newpress-lastpress) > 250) /* 250 ms */
				{
					repeat = 1;
				} else {
					if (repeat < 20)
					{
						repeat += 1;
					}
				}
				lastpress = newpress;
			}

			switch (key)
			{
				case KEY_DOWN:
					if (dsel < 1)
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
					dsel = 1;
					break;
				case KEY_EXIT:
				case KEY_ESC:
					API->configAPI->SetProfileInt (API->configAPI->SoundSec, "plrbufsize", plrbufsize, 10);
					API->configAPI->StoreConfig();
					return;
				case _KEY_ENTER:
					if (dsel == 0)
					{
						setup_devp_run_driver (token, API);
					}
					break;
				case KEY_LEFT:
				case '-':
					if (dsel == 1)
					{
						plrbufsize-=repeat;
						if (plrbufsize < 150)
						{
							plrbufsize = 150;
						}
					}
					break;
				case KEY_RIGHT:
				case '+':
					if (dsel == 1)
					{
						plrbufsize += repeat;
						if (plrbufsize > 1000)
						{
							plrbufsize = 1000;
						}
					}
					break;
				default:
					break;
			}
		}
		API->console->FrameLock();
	}
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo =
{
	.name = "plrbase",
	.desc = "OpenCP Player Devices System (c) 1994-'26 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad",
	.ver = DLLVERSION,
	.PreInit = deviplayPreInit,
	.LateInit = deviplayLateInit,
	.PreClose = deviplayPreClose,
	.LateClose = deviplayLateClose,
	.sortindex = 30
};
