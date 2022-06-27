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
 *    -added calling plInst::Done (Memory Leak)
 *  -fd981215   Felix Domke <tmbinc@gmx.net>
 *    -plInst::Done should be only called when provided, not
 *     if it's 0. (this caused crash with the gmi-player.)
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "boot/psetting.h"
#include "stuff/poutput.h"
#include "cpiface.h"
#include "cpiface-private.h"

static void displayshortins(int sel)
{
	int y,x,i;
	uint16_t buf[40];
	int cols = cpifaceSessionAPI.InstWidth/40; /* 2 */
	int left = cpifaceSessionAPI.InstWidth%40;

	displaystr (cpifaceSessionAPI.InstFirstLine - 1, 0, sel?0x09:0x01, "   instruments (short):", 23);
	if (sel)
		displaystr (cpifaceSessionAPI.InstFirstLine - 1, 23, 0x08, " press i to toggle mode", cpifaceSessionAPI.InstWidth - 23);
	else
		displaystr (cpifaceSessionAPI.InstFirstLine - 1, 23, 0x08, " press i to select mode", cpifaceSessionAPI.InstWidth - 23);
	for (y=0; y < cpifaceSessionAPI.InstHeight; y++)
	{
		if (y >= cpifaceSessionAPI.InstLength)
		{
			displayvoid (y + cpifaceSessionAPI.InstFirstLine, cpifaceSessionAPI.InstStartCol, cpifaceSessionAPI.InstWidth);
			continue;
		}

		for (x=0; x<cols; x++)
		{
			i = y + cpifaceSessionAPI.InstScroll + x * cpifaceSessionAPI.InstLength;
			if (i>=cpifaceSessionAPI.Inst.height)
			{
				displayvoid ( y + cpifaceSessionAPI.InstFirstLine, x*40, 40);
				continue;
			}
			cpifaceSessionAPI.Inst.Display (&cpifaceSessionAPI.Public, buf, 40, i, cpifaceSessionAPI.InstMode);
			displaystrattr (y + cpifaceSessionAPI.InstFirstLine, x*40 + cpifaceSessionAPI.InstStartCol, buf, 40);
		}
		displayvoid (y + cpifaceSessionAPI.InstFirstLine, 40*cols, left);
	}
}

static void displayxshortins(int sel)
{
	int y,x,i;
	uint16_t buf[33];
	int cols = cpifaceSessionAPI.InstWidth / 33; /* 4 */
	int left = cpifaceSessionAPI.InstWidth % 33;

	displaystr (cpifaceSessionAPI.InstFirstLine - 1, 0, sel?0x09:0x01, "   instruments (short):", 23);
	if (sel)
		displaystr (cpifaceSessionAPI.InstFirstLine - 1, 23, 0x08, " press i to toggle mode", cpifaceSessionAPI.InstWidth - 23);
	else
		displaystr (cpifaceSessionAPI.InstFirstLine - 1, 23, 0x08, " press i to select mode", cpifaceSessionAPI.InstWidth - 23);
	for (y=0; y < cpifaceSessionAPI.InstHeight; y++)
	{
		if (y >= cpifaceSessionAPI.InstLength)
		{
			displayvoid (y + cpifaceSessionAPI.InstFirstLine, cpifaceSessionAPI.InstStartCol, cpifaceSessionAPI.InstWidth);
			continue;
		}
		for (x=0; x<cols; x++)
		{
			i = y + cpifaceSessionAPI.InstScroll + x * cpifaceSessionAPI.InstLength;
			if (i >= cpifaceSessionAPI.Inst.height)
			{
				displayvoid (y + cpifaceSessionAPI.InstFirstLine, x*33, 33);
				continue;
			}
			cpifaceSessionAPI.Inst.Display (&cpifaceSessionAPI.Public, buf, 33, i, cpifaceSessionAPI.InstMode);
			displaystrattr (y + cpifaceSessionAPI.InstFirstLine, x * 33 + cpifaceSessionAPI.InstStartCol, buf, 33);
		}
		displayvoid (y + cpifaceSessionAPI.InstFirstLine, 33*cols, left);
	}
}

static void displaysideins(int sel)
{
	int y;
	uint16_t buf[52];
	int left = cpifaceSessionAPI.InstWidth - 52;

	displaystr (cpifaceSessionAPI.InstFirstLine - 1, cpifaceSessionAPI.InstStartCol, sel?0x09:0x01, "       instruments (side): ", 27);
	if (sel)
		displaystr (cpifaceSessionAPI.InstFirstLine - 1, cpifaceSessionAPI.InstStartCol + 28, 0x08, " press i to toggle mode", 52-27);
	else
		displaystr (cpifaceSessionAPI.InstFirstLine - 1, cpifaceSessionAPI.InstStartCol + 28, 0x08, " press i to select mode", 52-27);
	for (y=0; y < cpifaceSessionAPI.InstHeight; y++)
	{
		if (y >= cpifaceSessionAPI.Inst.height)
		{
			displayvoid (y + cpifaceSessionAPI.InstFirstLine, cpifaceSessionAPI.InstStartCol /* 80 */, cpifaceSessionAPI.InstWidth /* 52 */);
			continue;
		}
		cpifaceSessionAPI.Inst.Display (&cpifaceSessionAPI.Public, buf, cpifaceSessionAPI.InstWidth /* 52 */,  y + cpifaceSessionAPI.InstScroll, cpifaceSessionAPI.InstMode);
		displaystrattr (y + cpifaceSessionAPI.InstFirstLine, cpifaceSessionAPI.InstStartCol /* 80 */, buf, cpifaceSessionAPI.InstWidth /* 52 */);
		displayvoid (y + cpifaceSessionAPI.InstFirstLine, 52, left);
	}
}

static void displaylongins(int sel)
{
	int y;
	uint16_t buf[80];
	int left = cpifaceSessionAPI.InstWidth - 80;

	displaystr (cpifaceSessionAPI.InstFirstLine - 2, 0, sel?0x09:0x01, "   instruments (long): ", 23);
	if (sel)
		displaystr (cpifaceSessionAPI.InstFirstLine - 2, 23, 0x08, " press i to toggle mode", 57);
	else
		displaystr (cpifaceSessionAPI.InstFirstLine - 2, 23, 0x08, " press i to select mode", 57);
	displaystr (cpifaceSessionAPI.InstFirstLine - 1, 0, 0x07, cpifaceSessionAPI.Inst.title80, 80);
	for (y=0; y < cpifaceSessionAPI.InstHeight; y++)
	{
		if (y >= cpifaceSessionAPI.Inst.bigheight)
		{
			displayvoid (y + cpifaceSessionAPI.InstFirstLine, cpifaceSessionAPI.InstStartCol /* 0 */, 80);
			continue;
		}
		cpifaceSessionAPI.Inst.Display (&cpifaceSessionAPI.Public, buf, 80, y + cpifaceSessionAPI.InstScroll, cpifaceSessionAPI.InstMode);

		displaystrattr (y + cpifaceSessionAPI.InstFirstLine, cpifaceSessionAPI.InstStartCol /* 0 */, buf, 80);
		displayvoid (y + cpifaceSessionAPI.InstFirstLine, 80, left);
	}
}

static void displayxlongins(int sel)
{
	int y;
	uint16_t buf[132];
	int left = cpifaceSessionAPI.InstWidth - 132;

	displaystr (cpifaceSessionAPI.InstFirstLine - 2, 0, sel?0x09:0x01, "   instruments (long): ", 23);
	if (sel)
		displaystr (cpifaceSessionAPI.InstFirstLine - 2, 23, 0x08, " press i to toggle mode", 109);
	else
		displaystr (cpifaceSessionAPI.InstFirstLine - 2, 23, 0x08, " press i to select mode", 109);
	displaystr (cpifaceSessionAPI.InstFirstLine - 1, 0, 0x07, cpifaceSessionAPI.Inst.title132, 132);
	for (y=0; y < cpifaceSessionAPI.InstHeight; y++)
	{
		if (y >= cpifaceSessionAPI.Inst.bigheight)
		{
			displayvoid (y + cpifaceSessionAPI.InstFirstLine, cpifaceSessionAPI.InstStartCol /* 0 */, 132);
			continue;
		}
		cpifaceSessionAPI.Inst.Display (&cpifaceSessionAPI.Public, buf, 132, y + cpifaceSessionAPI.InstScroll, cpifaceSessionAPI.InstMode);
		displaystrattr (y + cpifaceSessionAPI.InstFirstLine, cpifaceSessionAPI.InstStartCol /* 0 */, buf, 132);
		displayvoid (y + cpifaceSessionAPI.InstFirstLine, 132, left);
	}
}

static void plDisplayInstruments(int sel)
{
	if (!cpifaceSessionAPI.InstType)
		return;

	if ((cpifaceSessionAPI.InstScroll + cpifaceSessionAPI.InstHeight) > cpifaceSessionAPI.InstLength)
	{
		cpifaceSessionAPI.InstScroll = cpifaceSessionAPI.InstLength - cpifaceSessionAPI.InstHeight;
	}
	if (cpifaceSessionAPI.InstScroll < 0)
	{
		cpifaceSessionAPI.InstScroll = 0;
	}

	cpifaceSessionAPI.Inst.Mark (&cpifaceSessionAPI.Public);

	switch (cpifaceSessionAPI.InstType)
	{
		case 1:
			if (cpifaceSessionAPI.InstWidth >= 132)
				displayxshortins(sel);
			else
				displayshortins(sel);
			break;
		case 2:
			if (cpifaceSessionAPI.InstWidth >= 132)
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
	int titlehgt = (cpifaceSessionAPI.InstType == 2) ? 2 : 1;
	cpifaceSessionAPI.InstFirstLine = ypos + titlehgt;
	cpifaceSessionAPI.InstHeight = hgt - titlehgt;
	cpifaceSessionAPI.InstWidth = wid;
	cpifaceSessionAPI.InstStartCol = xpos;

	if (cpifaceSessionAPI.InstType == 1)
	{
		if (cpifaceSessionAPI.InstWidth >= 132)
		{
			int cols = plScrWidth/33;
			cpifaceSessionAPI.InstLength = (cpifaceSessionAPI.Inst.height + cols - 1) / cols;
		} else {
			int cols = plScrWidth/40;
			cpifaceSessionAPI.InstLength = (cpifaceSessionAPI.Inst.height + cols - 1) / cols;
		}
	} else if (cpifaceSessionAPI.InstType == 2)
	{
		cpifaceSessionAPI.InstLength = cpifaceSessionAPI.Inst.bigheight;
	} else {
		cpifaceSessionAPI.InstLength = cpifaceSessionAPI.Inst.height;
	}
}

static int InstGetWin(struct cpitextmodequerystruct *q)
{
	if ((cpifaceSessionAPI.InstType == 3) && (plScrWidth<132))
		cpifaceSessionAPI.InstType = 0;

	switch (cpifaceSessionAPI.InstType)
	{
		case 0:
			return 0;
		case 1:
			q->hgtmin=2;
			if (cpifaceSessionAPI.InstWidth >= 132)
			{
				int cols = plScrWidth/33;
				int lines = (cpifaceSessionAPI.Inst.height + cols - 1) / cols;
				q->hgtmax=1+lines;
			} else {
				int cols = plScrWidth/40;
				int lines = (cpifaceSessionAPI.Inst.height + cols - 1) / cols;
				q->hgtmax=1+lines;
			}
			q->xmode=1;
			break;
		case 2:
			q->hgtmin = 3;
			q->hgtmax = 2 + cpifaceSessionAPI.Inst.bigheight;
			q->xmode = 3;
			break;
		case 3:
			q->hgtmin = 2;
			q->hgtmax = 1 + cpifaceSessionAPI.Inst.height;
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
			if (!cpifaceSessionAPI.InstType)
				cpifaceSessionAPI.InstType = (cpifaceSessionAPI.InstType + 1) % 4;
			cpiTextSetMode("inst");
			return 1;
		case 'x': case 'X':
			cpifaceSessionAPI.InstType = 3;
			break;
		case KEY_ALT_X:
			cpifaceSessionAPI.InstType = 1;
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
			cpifaceSessionAPI.InstType = (cpifaceSessionAPI.InstType + 1) % 4;
			cpiTextRecalc();
			break;
		/*case 0x4900: //pgup*/
		case KEY_PPAGE:
			cpifaceSessionAPI.InstScroll--;
			break;
		/*case 0x5100: //pgdn*/
		case KEY_NPAGE:
			cpifaceSessionAPI.InstScroll++;
			break;
		case KEY_CTRL_PGUP:
		/* case 0x8400: //ctrl-pgup */
			cpifaceSessionAPI.InstScroll -= cpifaceSessionAPI.InstHeight;
			break;
		case KEY_CTRL_PGDN:
		/* case 0x7600: //ctrl-pgdn */
			cpifaceSessionAPI.InstScroll += cpifaceSessionAPI.InstHeight;
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			cpifaceSessionAPI.InstScroll = 0;
			break;
		/*case 0x4F00: //end*/
		case KEY_END:
			cpifaceSessionAPI.InstScroll = cpifaceSessionAPI.InstLength;
			break;
		case KEY_ALT_I:
			cpifaceSessionAPI.Inst.Clear (&cpifaceSessionAPI.Public);
			break;
		case KEY_TAB: /* tab */
		case KEY_SHIFT_TAB: /* 0x0f00 */
/* TODO-keys    case 0xA500:  alt-tab */
			cpifaceSessionAPI.InstMode = !cpifaceSessionAPI.InstMode;
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
			cpifaceSessionAPI.InstType = cfGetProfileInt2(cfScreenSec, "screen", "insttype", 3, 10) & 3;
			return 0;
		case cpievDone: case cpievDoneAll:
			if(cpifaceSessionAPI.Inst.Done) cpifaceSessionAPI.Inst.Done (&cpifaceSessionAPI.Public);
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
	cpifaceSessionAPI.InstScroll=0;
	cpifaceSessionAPI.Inst = *x;
	cpiTextRegisterMode(&cpiTModeInst);
}
