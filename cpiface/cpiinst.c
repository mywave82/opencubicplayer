/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface text mode instrument display
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
 *  -kb981201   Tammo Hinrichs <opencp@gmx.net>
 *    -added calling plInsDisplay::Done (Memory Leak)
 *  -fd981215   Felix Domke <tmbinc@gmx.net>
 *    -plInsDisplay::Done should be only called when provided, not
 *     if it's 0. (this caused crash with the gmi-player.)
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "boot/psetting.h"
#include "stuff/poutput.h"
#include "cpiface.h"

static struct insdisplaystruct plInsDisplay;

static int plInstScroll;
static int plInstFirstLine;
static int plInstStartCol;
static int plInstLength;
static int plInstHeight;
static int plInstWidth;
static char plInstType;
static char plInstMode;

static void displayshortins(int sel)
{
	int y,x,i;
	uint16_t buf[40];
	int cols=plInstWidth/40; /* 2 */
	int left=cols%40;

	displaystr(plInstFirstLine-1, 0, sel?0x09:0x01, "   instruments (short):", 23);
	if (sel)
		displaystr(plInstFirstLine-1, 23, 0x08, " press i to toggle mode", plInstWidth-23);
	else
		displaystr(plInstFirstLine-1, 23, 0x08, " press i to select mode", plInstWidth-23);
	for (y=0; y<plInstHeight; y++)
	{
		if (y>=plInstLength)
		{
			displayvoid(y+plInstFirstLine, plInstStartCol, plInstWidth);
			continue;
		}

		for (x=0; x<cols; x++)
		{
			i=y+plInstScroll+x*plInstLength;
			if (i>=plInsDisplay.height)
			{
				displayvoid(y+plInstFirstLine, x*40, 40);
				continue;
			}
			plInsDisplay.Display(buf, 40, i, plInstMode);
			displaystrattr(y+plInstFirstLine, x*40 + plInstStartCol, buf, 40);
		}
		displayvoid(y+plInstFirstLine, 40*cols, left);
	}
}

static void displayxshortins(int sel)
{
	int y,x,i;
	uint16_t buf[33];
	int cols=plInstWidth/33; /* 4 */
	int left=cols%33;

	displaystr(plInstFirstLine-1, 0, sel?0x09:0x01, "   instruments (short):", 23);
	if (sel)
		displaystr(plInstFirstLine-1, 23, 0x08, " press i to toggle mode", plInstWidth-23);
	else
		displaystr(plInstFirstLine-1, 23, 0x08, " press i to select mode", plInstWidth-23);
	for (y=0; y<plInstHeight; y++)
	{
		if (y>=plInstLength)
		{
			displayvoid(y+plInstFirstLine, plInstStartCol/* 0 */, plInstWidth);
			continue;
		}
		for (x=0; x<cols; x++)
		{
			i=y+plInstScroll+x*plInstLength;
			if (i>=plInsDisplay.height)
			{
				displayvoid(y+plInstFirstLine, x*33, 33);
				continue;
			}
			plInsDisplay.Display(buf, 33, i, plInstMode);
			displaystrattr(y+plInstFirstLine, x*33 + plInstStartCol, buf, 33);
		}
		displayvoid(y+plInstFirstLine, 33*cols, left);
	}
}

static void displaysideins(int sel)
{
	int y;
	uint16_t buf[52];
	int left=plInstWidth-52;

	displaystr(plInstFirstLine-1, plInstStartCol, sel?0x09:0x01, "       instruments (side): ", 27);
	if (sel)
		displaystr(plInstFirstLine-1, plInstStartCol + 28, 0x08, " press i to toggle mode", 52-27);
	else
		displaystr(plInstFirstLine-1, plInstStartCol + 28, 0x08, " press i to select mode", 52-27);
	for (y=0; y<plInstHeight; y++)
	{
		if (y>=plInsDisplay.height)
		{
			displayvoid(y+plInstFirstLine, plInstStartCol /* 80 */, plInstWidth /* 52 */);
			continue;
		}
		plInsDisplay.Display(buf, plInstWidth /* 52 */, y+plInstScroll, plInstMode);
		displaystrattr(y+plInstFirstLine, plInstStartCol /* 80 */, buf, plInstWidth /* 52 */);
		displayvoid(y+plInstFirstLine, 52, left);
	}
}

static void displaylongins(int sel)
{
	int y;
	uint16_t buf[80];
	int left=plInstWidth-80;

	displaystr(plInstFirstLine-2, 0, sel?0x09:0x01, "   instruments (long): ", 23);
	if (sel)
		displaystr(plInstFirstLine-2, 23, 0x08, " press i to toggle mode", 57);
	else
		displaystr(plInstFirstLine-2, 23, 0x08, " press i to select mode", 57);
	displaystr(plInstFirstLine-1, 0, 0x07, plInsDisplay.title80, 80);
	for (y=0; y<plInstHeight; y++)
	{
		if (y>=plInsDisplay.bigheight)
		{
			displayvoid(y+plInstFirstLine, plInstStartCol/* 0 */, 80);
			continue;
		}
		plInsDisplay.Display(buf, 80, y+plInstScroll, plInstMode);

		displaystrattr(y+plInstFirstLine, plInstStartCol /* 0 */, buf, 80);
		displayvoid(y+plInstFirstLine, 80, left);
	}
}

static void displayxlongins(int sel)
{
	int y;
	uint16_t buf[132];
	int left=plInstWidth-132;

	displaystr(plInstFirstLine-2, 0, sel?0x09:0x01, "   instruments (long): ", 23);
	if (sel)
		displaystr(plInstFirstLine-2, 23, 0x08, " press i to toggle mode", 109);
	else
		displaystr(plInstFirstLine-2, 23, 0x08, " press i to select mode", 109);
	displaystr(plInstFirstLine-1, 0, 0x07, plInsDisplay.title132, 132);
	for (y=0; y<plInstHeight; y++)
	{
		if (y>=plInsDisplay.bigheight)
		{
			displayvoid(y+plInstFirstLine, plInstStartCol/* 0 */, 132);
			continue;
		}
		plInsDisplay.Display(buf, 132, y+plInstScroll, plInstMode);
		displaystrattr(y+plInstFirstLine, plInstStartCol /* 0 */, buf, 132);
		displayvoid(y+plInstFirstLine, 132, left);
	}
}

static void plDisplayInstruments(int sel)
{
	if (!plInstType)
		return;

	if ((plInstScroll+plInstHeight)>plInstLength)
		plInstScroll=plInstLength-plInstHeight;
	if (plInstScroll<0)
		plInstScroll=0;

	plInsDisplay.Mark();

	switch (plInstType)
	{
		case 1:
			if (plInstWidth>=132)
				displayxshortins(sel);
			else
				displayshortins(sel);
			break;
		case 2:
			if (plInstWidth>=132)
				displayxlongins(sel);
			else
				displaylongins(sel);
			break;
		case 3:
			displaysideins(sel);
		break;
	}
}

static void InstSetWin(int xpos, int wid, int ypos, int hgt)
{
	int titlehgt=(plInstType==2)?2:1;
	plInstFirstLine=ypos+titlehgt;
	plInstHeight=hgt-titlehgt;
	plInstWidth=wid;
	plInstStartCol=xpos;

	if (plInstType==1)
	{
		if (plInstWidth>=132)
		{
			int cols = plScrWidth/33;
			plInstLength = (plInsDisplay.height + cols - 1) / cols;
		} else {
			int cols = plScrWidth/40;
			plInstLength = (plInsDisplay.height + cols - 1) / cols;
		}
	} else if (plInstType==2)
	{
		plInstLength=plInsDisplay.bigheight;
	} else {
		plInstLength=plInsDisplay.height;
	}
}

static int InstGetWin(struct cpitextmodequerystruct *q)
{
	if ((plInstType==3)&&(plScrWidth<132))
		plInstType=0;

	switch (plInstType)
	{
		case 0:
			return 0;
		case 1:
			q->hgtmin=2;
			if (plInstWidth>=132)
			{
				int cols = plScrWidth/33;
				int lines = (plInsDisplay.height + cols - 1) / cols;
				q->hgtmax=1+lines;
			} else {
				int cols = plScrWidth/40;
				int lines = (plInsDisplay.height + cols - 1) / cols;
				q->hgtmax=1+lines;
			}
			q->xmode=1;
			break;
		case 2:
			q->hgtmin=3;
			q->hgtmax=2+plInsDisplay.bigheight;
			q->xmode=3;
			break;
		case 3:
			q->hgtmin=2;
			q->hgtmax=1+plInsDisplay.height;
			q->xmode=2;
			break;
	}
	q->size=1;
	q->top=1;
	q->killprio=96;
	q->viewprio=144;
	if (q->hgtmin>q->hgtmax)
		q->hgtmin=q->hgtmax;
	return 1;
}

static void InstDraw(int focus)
{
	plDisplayInstruments(focus);
}

static int InstIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('i', "Enable instrument viewer");
			cpiKeyHelp('I', "Enable instrument viewer");
			break;
		case 'i': case 'I':
			if (!plInstType)
				plInstType=(plInstType+1)%4;
			cpiTextSetMode("inst");
			return 1;
		case 'x': case 'X':
			plInstType=3;
			break;
		case KEY_ALT_X:
			plInstType=1;
			break;
	}
	return 0;
}

static int InstAProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('i', "Toggle instrument viewer types");
			cpiKeyHelp('I', "Toggle instrument viewer types");
			cpiKeyHelp(KEY_PPAGE, "Scroll up in instrument viewer");
			cpiKeyHelp(KEY_NPAGE, "Scroll down in instrument viewer");
			cpiKeyHelp(KEY_HOME, "Scroll to to the first line in instrument viewer");
			cpiKeyHelp(KEY_END, "Scroll to to the last line in instrument viewer");
			cpiKeyHelp(KEY_TAB, "Toggle instrument viewer mode");
			cpiKeyHelp(KEY_ALT_I, "Clear instrument used bits");
			cpiKeyHelp(KEY_SHIFT_TAB, "Toggle instrument viewer mode");
			cpiKeyHelp(KEY_CTRL_PGUP, "Scroll up a page in the instrument viewer");
			cpiKeyHelp(KEY_CTRL_PGDN, "Scroll down a page in the instrument viewer");
			return 0;
		case 'i': case 'I':
			plInstType=(plInstType+1)%4;
			cpiTextRecalc();
			break;
		/*case 0x4900: //pgup*/
		case KEY_PPAGE:
			plInstScroll--;
			break;
		/*case 0x5100: //pgdn*/
		case KEY_NPAGE:
			plInstScroll++;
			break;
		case KEY_CTRL_PGUP:
		/* case 0x8400: //ctrl-pgup */
			plInstScroll-=plInstHeight;
			break;
		case KEY_CTRL_PGDN:
		/* case 0x7600: //ctrl-pgdn */
			plInstScroll+=plInstHeight;
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plInstScroll=0;
			break;
		/*case 0x4F00: //end*/
		case KEY_END:
			plInstScroll=plInstLength;
			break;
		case KEY_ALT_I:
			plInsDisplay.Clear();
			break;
		case KEY_TAB: /* tab */
		case KEY_SHIFT_TAB: /* 0x0f00 */
/* TODO-keys    case 0xA500:  alt-tab */
			plInstMode=!plInstMode;
			break;
		default:
		return 0;
	}
	return 1;
}

static int InstEvent(int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			plInstType=cfGetProfileInt2(cfScreenSec, "screen", "insttype", 3, 10)&3;
			return 0;
		case cpievDone: case cpievDoneAll:
			if(plInsDisplay.Done) plInsDisplay.Done();
				return 0;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiTModeInst = {"inst", InstGetWin, InstSetWin, InstDraw, InstIProcessKey, InstAProcessKey, InstEvent CPITEXTMODEREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	cpiTextRegisterDefMode(&cpiTModeInst);
}

static void __attribute__((destructor))done(void)
{
	cpiTextUnregisterDefMode(&cpiTModeInst);
}

void plUseInstruments(struct insdisplaystruct *x)
{
	plInstScroll=0;
	/*plInsDisplay=x;*/
	memcpy(&plInsDisplay, x, sizeof(*x));
	cpiTextRegisterMode(&cpiTModeInst);
}
