/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface link info screen
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
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "stuff/poutput.h"
#include "filesel/pfilesel.h"
#include "cpiface.h"
#include "boot/plinkman.h"

static const int plWinFirstLine = 5;
static int plWinHeight;

static int plHelpHeight;
static int plHelpScroll;
static int mode;


static void plDisplayHelp (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int y;
	struct linkinfostruct l;
	uint32_t size;

	plHelpHeight=lnkCountLinks()*(mode?2:1);
	if ((plHelpScroll+plWinHeight)>plHelpHeight)
		plHelpScroll=plHelpHeight-plWinHeight;
	if (plHelpScroll<0)
		plHelpScroll=0;
	displaystr(plWinFirstLine, 0, 0x09, "  Link View", 15);
	displaystr(plWinFirstLine, 15, 0x08, "press tab to toggle copyright                               ", 65);

	for (y=0; y<plWinHeight; y++)
	{
		if (lnkGetLinkInfo(&l, &size, (y+plHelpScroll)/(mode?2:1)))
		{
			int dl=strlen(l.desc);
			int i;
			const char *d2;

			for (i=0; i<dl; i++)
				if (!strncasecmp(l.desc+i, "(c)", 3))
					break;
			d2=l.desc+i;
			if (i>/*58*/110)
				i=/*58*/110;
			if (!((y+plHelpScroll)&1)||!mode)
			{
				uint16_t buf[/*80*/132];
				writestring(buf, 0, 0, "", /*80*/132);

				writestring(buf, 2, 0x0A, l.name, 8);
				if (size)
				{
					writenum(buf, 12, 0x07, (size+1023)>>10, 10, 6, 1);
					writestring(buf, 18, 0x07, "k", 1);
				} else
					writestring(buf, 12, 0x07, "builtin", 7);
				writestring(buf, 22, 0x0F, l.desc, i);

				displaystrattr(y+plWinFirstLine+1, 0, buf, 132 /* 80 */);
			} else {
				char vbuf[32];

				snprintf (vbuf, sizeof (vbuf), "  version %d.%s%d.%d",
					l.ver>>16,
					((signed char)(l.ver>>8)>=0) ? "" : "-",
					((signed char)(l.ver>>8)>=0) ? ((signed char)(l.ver>>8)) : ((signed char)(l.ver>>8)/10),
					(unsigned char)l.ver);

				displaystr (y+plWinFirstLine+1, 0, 0x08, vbuf, 24);
				displaystr_utf8 (y+plWinFirstLine+1, 24, 0x08, d2, plScrWidth - 24);
			}
		} else {
			displayvoid (y+plWinFirstLine+1, 0, plScrWidth);
		}
	}
}

static int plHelpKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp(KEY_UP, "Scroll up");
			cpiKeyHelp(KEY_DOWN, "Scroll down");
			cpiKeyHelp(KEY_PPAGE, "Scroll up");
			cpiKeyHelp(KEY_NPAGE, "Scroll down");
			cpiKeyHelp(KEY_HOME, "Scroll to to the first line");
			cpiKeyHelp(KEY_END, "Scroll to to the last line");
			cpiKeyHelp(KEY_TAB, "Toggle copyright on/off");
			cpiKeyHelp(KEY_CTRL_PGUP, "Scroll a page up");
			cpiKeyHelp(KEY_CTRL_PGDN, "Scroll a page down");
			return 0;
		case KEY_TAB:
			if (mode)
				plHelpScroll/=2;
			else
				plHelpScroll*=2;
			mode=!mode;
			break;
		case KEY_UP:
		case KEY_PPAGE:
			plHelpScroll--;
			break;
		case KEY_DOWN:
		case KEY_NPAGE:
			plHelpScroll++;
			break;
		case KEY_CTRL_PGUP:
		/* case 0x8400: //ctrl-pgup */
			plHelpScroll-=plWinHeight;
			break;
		case KEY_CTRL_PGDN:
		/* case 0x7600: //ctrl-pgdn */
			plHelpScroll+=plWinHeight;
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plHelpScroll=0;
			break;
		/*case 0x4F00: //end*/
		case KEY_END:
			plHelpScroll=plHelpHeight;
			break;
		default:
			return 0;
	}
	if ((plHelpScroll+plWinHeight)>plHelpHeight)
		plHelpScroll=plHelpHeight-plWinHeight;
	if (plHelpScroll<0)
		plHelpScroll=0;
	return 1;
}

static void hlpDraw (struct cpifaceSessionAPI_t *cpifaceSession)
{
	plWinHeight=plScrHeight-6;
	cpiDrawGStrings (cpifaceSession);
	plDisplayHelp (cpifaceSession);
}

static void hlpSetMode()
{
	cpiSetTextMode(fsScrType);
	plWinHeight=plScrHeight-6;
}

static int hlpIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('\'', "View loaded dll/plugins");
			return 0;
		case '\'':
			cpiSetMode("links");
			break;
		default:
			return 0;
	}
	return 1;
}

static int hlpEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ignore)
{
	return 1;
}

static struct cpimoderegstruct cpiModeLinks = {"links", hlpSetMode, hlpDraw, hlpIProcessKey, plHelpKey, hlpEvent CPIMODEREGSTRUCT_TAIL};

OCP_INTERNAL void cpiLinksInit (void)
{
	cpiRegisterDefMode(&cpiModeLinks);
}

OCP_INTERNAL void cpiLinksDone (void)
{
	cpiUnregisterDefMode(&cpiModeLinks);
}
