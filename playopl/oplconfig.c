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
#include <string.h>
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

static void oplConfigDraw (int EditPos, const struct DevInterfaceAPI_t *API)
{
	int mlWidth, mlHeight, mlTop, mlLeft;

	mlHeight = 17;
	mlWidth = 60;
	mlTop = (API->console->TextHeight - mlHeight) / 2;
	mlLeft = (API->console->TextWidth - mlWidth) / 2;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda%18C\xc4 AdPlug configuration %18C\xc4\xbf", mlWidth);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Navigate with arrows and hit %.15o<ESC>%.7o to save and exit.%*C %.9o\xb3", mlWidth - 55);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth-2);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Selected emulator: %*.*o1. ken  %0.9o%*C \xb3", EditPos==0?8:0, EditPos==0?7:7, mlWidth - 30);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o                    %*.*o2. nuked%0.9o%*C \xb3", EditPos==1?8:0, EditPos==1?7:7, mlWidth - 30);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o                    %*.*o2. satoh%0.9o%*C \xb3", EditPos==2?8:0, EditPos==2?7:7, mlWidth - 30);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o                    %*.*o3. woody%0.9o%*C \xb3", EditPos==3?8:0, EditPos==3?7:7, mlWidth - 30);

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
	}

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

static void oplConfigRun (void **token, const struct DevInterfaceAPI_t *API)
{
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
	} else /* nuked */
	{
		esel = 1;
	}

	while (1)
	{
		API->fsDraw();
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
					esel = key - '1'; break;
				case KEY_DOWN:
					if (esel < 3)
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
	else                API->configAPI->SetProfileString ("adplug", "emulator", "woody");
	API->configAPI->StoreConfig ();
}

static struct ocpfile_t *oplconfig;
static void oplConfigRun (void **token, const struct DevInterfaceAPI_t *API);

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
