/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CPIFace song message mode
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
 *  -fd981119   Felix Domke <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT'
 *  -doj20020901 Dirk Jagdmann <doj@cubic.org>
 *    -now uses ALT+F9
 *  -ss20040825 Stian Skjelstad <stian@nixia.no>
 *    -now uses |, since ALT-F9 can't be mapped when using curses
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "stuff/poutput.h"
#include "cpiface.h"

#include <curses.h>

static short plWinFirstLine;
static short plWinHeight;

static char *const *plSongMessage;

static short plMsgScroll;
static short plMsgHeight;

static void plDisplayMessage(void)
{
	int y;

	if ((plMsgScroll+plWinHeight)>plMsgHeight)
		plMsgScroll=plMsgHeight-plWinHeight;
	if (plMsgScroll<0)
		plMsgScroll=0;
	displaystr(plWinFirstLine-1, 0, 0x09, "   and that's what the composer really wants to tell you:", 80);
	for (y=0; y<plWinHeight; y++)
		if ((y+plMsgScroll)<plMsgHeight)
			displaystr(y+plWinFirstLine, 0, 0x07, plSongMessage[y+plMsgScroll], 80);
		else
			displayvoid(y+plWinFirstLine, 0, 80);
}

static int plMsgKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp(KEY_PPAGE, "Scroll up");
			cpiKeyHelp(KEY_NPAGE, "Scroll down");
			cpiKeyHelp(KEY_HOME, "Scroll to to the first line");
			cpiKeyHelp(KEY_END, "Scroll to to the last line");
			cpiKeyHelp(KEY_CTRL_PGUP, "Scroll a page up");
			cpiKeyHelp(KEY_CTRL_PGDN, "Scroll a page down");
			return 0;
		/*case 0x4900: //pgup*/
		case KEY_PPAGE:
			plMsgScroll--;
			break;
		/*case 0x5100: //pgdn*/
		case KEY_NPAGE:
			plMsgScroll++;
			break;
		case KEY_CTRL_PGUP:
		/* case 0x8400: //ctrl-pgup */
			plMsgScroll-=plWinHeight;
			break;
		case KEY_CTRL_PGDN:
		/* case 0x7600: //ctrl-pgdn */
			plMsgScroll+=plWinHeight;
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plMsgScroll=0;
			break;
		/*case 0x4F00: //end*/
		case KEY_END:
			plMsgScroll=plMsgHeight;
			break;
		default:
			return 0;
	}
	if ((plMsgScroll+plWinHeight)>plMsgHeight)
		plMsgScroll=plMsgHeight-plWinHeight;
	if (plMsgScroll<0)
		plMsgScroll=0;
	return 1;
}

static void msgDraw(void)
{
	cpiDrawGStrings();
	plDisplayMessage();
}

static void msgSetMode(void)
{
	cpiSetTextMode(0);
	plWinFirstLine=6;
	plWinHeight=19;
}

static int msgIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('|', "View file messages");
			return 0;
		case '|':
/* TODO-keys	case 0x7000: // ALT+F9 */
			cpiSetMode("msg");
			return 1;
	}
	return 0;
}

static int msgEvent(int _ignore)
{
	return 1;
}

static struct cpimoderegstruct plMessageMode = {"msg", msgSetMode, msgDraw, msgIProcessKey, plMsgKey, msgEvent CPIMODEREGSTRUCT_TAIL};

void plUseMessage(char **msg)
{
	plSongMessage=msg;
	for (plMsgHeight=0; plSongMessage[plMsgHeight]; plMsgHeight++);
		plMsgScroll=0;
	cpiRegisterMode(&plMessageMode);
}
