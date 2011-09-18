/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CP hypertext help viewer (Fileselector wrapper)
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
 *  -fg980924  Fabian Giesen <gfabian@jdcs.su.nw.schule.de>
 *    -first release
 */

#include "config.h"
#include "types.h"
#include "stuff/poutput.h"
#include "help/cphelper.h"
#include "stuff/framelock.h"
#include "cphlpfs.h"

#include <curses.h>

int fsmode;

/* ripped from fileselector */


/* the wrapper */

static int plHelpKey(unsigned short key)
{
	switch(key)
	{
		case 'h': case 'H': case '?': case '!': case KEY_F(1): case KEY_ESC:
			fsmode=0;
			break;
		default:
			return brHelpKey(key);
	}
	return 1;
}

unsigned char fsHelp2(void)
{
	unsigned short sbuf[CONSOLE_MAX_X];
	helppage *cont;

	plSetTextMode(0);

	fillstr(sbuf, 0, 0x30, 0, CONSOLE_MAX_X);
	writestring(sbuf, 2, 0x30, "opencp help", 11);
	writestring(sbuf, plScrWidth-29, 0x30, "(c) '94-'10 Stian Skjelstad", 27);
	displaystrattr(0, 0, sbuf, plScrWidth);

	cont=brDecodeRef("Contents");

	if (!cont)
		displaystr(1, 0, 0x04, "shit!", 5);

	brSetPage(cont);

	brSetWinStart(2);
	brSetWinHeight(plScrHeight-2);

	fsmode=1;

	while (fsmode)
	{
		unsigned short key;
		brDisplayHelp();

		while (!ekbhit())
			framelock();
		key=egetch();

		plHelpKey(key);
		framelock();
	};

	return 1;
}
