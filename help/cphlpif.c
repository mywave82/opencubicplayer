/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CP hypertext help viewer (CPIFACE wrapper)
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

 * revision history: (please note changes here)
 *  -fg980924  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -first release
 */

#include "config.h"
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "stuff/poutput.h"
#include "stuff/framelock.h"
#include "stuff/err.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "help/cphelper.h"

static char beforehelp[9] = {0};

static int plHelpInit(void)
{
	*beforehelp=0;
	return 1;
}

static void hlpDraw(void)
{
	cpiDrawGStrings();
	brDisplayHelp();
	framelock();
}

static void hlpSetMode(void)
{
	cpiSetTextMode(0);
	brSetWinStart(6);
	brSetWinHeight(/*19*/plScrHeight-6);
}

static int hlpOpen(void)
{
	return 1;
}

static int hlpIProcessKey(unsigned short key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('h', "Enable help browser");
			cpiKeyHelp('H', "Enable help browser");
			cpiKeyHelp('?', "Enable help browser");
			cpiKeyHelp('!', "Enable help browser");
			cpiKeyHelp(KEY_F(1), "Enable help browser");
			return 0;
		/* case 0x6800: // alt-f1 TODO keys */
		case 'h': case 'H': case '?': case '!': case KEY_F(1):
			cpiGetMode(beforehelp);
			cpiSetMode("coolhelp");
			break;
		default:
			return 0;
	}
	return 1;
}

static int plHelpKey(unsigned short key)
{
	switch(key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('h', "Exit help browser");
			cpiKeyHelp('H', "Exit help browser");
			cpiKeyHelp('?', "Exit help browser");
			cpiKeyHelp('!', "Exit help browser");
			cpiKeyHelp(KEY_F(1), "Exit help browser");
			cpiKeyHelp(KEY_ESC, "Exit help browser");
			return brHelpKey(key);
		/* case 0x6800: // alt-f1 TODO keys */
		case 'h': case 'H': case '?': case '!': case 27:
		case KEY_F(1):
			cpiSetMode(beforehelp);
			break;
		default:
			return brHelpKey(key);
	}
	return 1;
}

static int hlpEvent(int ev)
{
	switch (ev)
	{
		case cpievOpen:
			return hlpOpen();
		case cpievInitAll:
			return plHelpInit();
	}
	return 1;
}

static struct cpimoderegstruct hlpHelpBrowser = {"coolhelp", hlpSetMode, hlpDraw, hlpIProcessKey, plHelpKey, hlpEvent CPIMODEREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	cpiRegisterDefMode(&hlpHelpBrowser);
}

static void __attribute__((destructor))done(void)
{
	cpiUnregisterDefMode(&hlpHelpBrowser);
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {"cphlpif", "OpenCP help browser CPIFACE wrapper (c) 1998-09 Fabian Giesen", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
