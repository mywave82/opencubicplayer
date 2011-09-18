/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CPIFace main volume bar
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
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "boot/psetting.h"
#include "stuff/poutput.h"
#include "cpiface.h"

#define COLTEXT 0x07
#define COLTITLEH 0x09
#define COLMUTE 0x08
const uint16_t STRLS[] = {0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
const uint16_t STRRS[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe};
const uint16_t STRLL[] = {0x0ffe, 0x0ffe, 0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
const uint16_t STRRL[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe, 0x0ffe, 0x0ffe};

static int plMVolFirstLine;
static int plMVolFirstCol;
static int plMVolHeight;
static int plMVolWidth;
static int plMVolType;

static void logvolbar(int *l, int *r)
{
	if (*l>32)
		*l=32+((*l-32)>>1);
	if (*l>48)
		*l=48+((*l-48)>>1);
	if (*l>56)
		*l=56+((*l-56)>>1);
	if (*l>64)
		*l=64;
	if (*r>32)
		*r=32+((*r-32)>>1);
	if (*r>48)
		*r=48+((*r-48)>>1);
	if (*r>56)
		*r=56+((*r-56)>>1);
	if (*r>64)
		*r=64;
}

static void drawpeakpower(int y, int x)
{
	unsigned short strbuf[40];
	int l,r;

	writestring(strbuf, 0, plPause?COLMUTE:COLTEXT, " ["
	                                                "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa"
	                                                " -- "
	                                                "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa"
							"] ", 40);
	plGetRealMasterVolume(&l, &r);
	logvolbar(&l, &r);
	l=(l+2)>>2;
	r=(r+2)>>2;
	if (plPause)
	{
		writestring(strbuf, 18-l, COLMUTE, "----------------", l);
		writestring(strbuf, 22, COLMUTE, "----------------", r);
	} else {
		writestringattr(strbuf, 18-l, STRLS+16-l, l);
		writestringattr(strbuf, 22, STRRS, r);
	}
	displaystrattr(y, x, strbuf, 40);
	if (plMVolHeight==2)
		displaystrattr(y+1, x, strbuf, 40);
}

static void drawbigpeakpower(int y, int x)
{
	unsigned short strbuf[80];
	int l,r;

	writestring(strbuf, 0, plPause?COLMUTE:COLTEXT,
			"   ["
			"\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa"
			" -=\xf0\xf0=- "
			"\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa"
			"]   ", 80);
	plGetRealMasterVolume(&l, &r);
	logvolbar(&l, &r);
	l=(l+1)>>1;
	r=(r+1)>>1;
	if (plPause)
	{
		writestring(strbuf, 36-l, COLMUTE, "--------------------------------", l);
		writestring(strbuf, 44, COLMUTE, "--------------------------------", r);
	} else {
		writestringattr(strbuf, 36-l, STRLL+32-l, l);
		writestringattr(strbuf, 44, STRRL, r);
	}
	displaystrattr(y, x, strbuf, 80);
	if (plMVolHeight==2)
		displaystrattr(y+1, x, strbuf, 80);
}

static void MVolDraw(int sel)
{
	if (plMVolType==2)
	{
		displaystr(plMVolFirstLine, plMVolFirstCol /* 80 */, COLTEXT, "", 8);
		displaystr(plMVolFirstLine, plMVolFirstCol + 48 /* 128 */, COLTEXT, "", 4);
		if (plMVolHeight==2)
		{
			displaystr(plMVolFirstLine+1, plMVolFirstCol /* 80 */, COLTEXT, "", 8);
			displaystr(plMVolFirstLine+1, plMVolFirstCol + 48 /* 128 */, COLTEXT, "", 4);
		}
		drawpeakpower(plMVolFirstLine, plMVolFirstCol + 8 /* 88 */);
	} else {
		int l=(plMVolWidth>=132)?(plMVolWidth/2)-40 :20;
		displaystr(plMVolFirstLine, plMVolFirstCol /* 0 */, plPause?COLMUTE:(sel?COLTITLEH:COLTEXT), "  peak power level:", l);
		displaystr(plMVolFirstLine, plMVolWidth-l + plMVolFirstCol /* 0 */ , COLTEXT, "", l);
		if (plMVolHeight==2)
		{
			displaystr(plMVolFirstLine+1, plMVolFirstCol /* 0 */, COLTEXT, "", l);
			displaystr(plMVolFirstLine+1, plMVolWidth-l + plMVolFirstCol /* 0 */, COLTEXT, "", l);
		}
		if (plMVolWidth>=132)
			drawbigpeakpower(plMVolFirstLine, l);
		else
			drawpeakpower(plMVolFirstLine, l);
	}
}

static void MVolSetWin(int xpos, int wid, int ypos, int hgt)
{
	plMVolFirstCol=xpos;
	plMVolFirstLine=ypos;
	plMVolHeight=hgt;
	plMVolWidth=wid;
}

static int MVolGetWin(struct cpitextmodequerystruct *q)
{
	int pplheight;

	if ((plMVolType==2)&&(plScrWidth<132))
		plMVolType=0;
	pplheight=(plScrHeight>30)?2:1;

	switch (plMVolType)
	{
		case 0:
			return 0;
		case 1:
			q->xmode=3;
			break;
		case 2:
			q->xmode=2;
			break;
	}
	q->size=0;
	q->top=1;
	q->killprio=128;
	q->viewprio=176;
	q->hgtmax=pplheight;
	q->hgtmin=pplheight;
	return 1;
}

static int MVolIProcessKey(unsigned short key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('v', "Enable volume viewer");
			cpiKeyHelp('V', "Enable volume viewer");
			return 0;
		case 'v': case 'V':
			if (!plMVolType)
				plMVolType=1;
			cpiTextSetMode("mvol");
			break;
		case 'x': case 'X':
			plMVolType=plNLChan?2:1;
			return 0;
		case KEY_ALT_X:
			plMVolType=1;
			return 0;
		default:
			return 0;
	}
	return 1;
}

static int MVolAProcessKey(unsigned short key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('v', "Change volume viewer mode");
			cpiKeyHelp('V', "Change volume viewer mode");
			return 0;
		case 'v': case 'V':
			plMVolType=(plMVolType+1)%3;
			cpiTextRecalc();
			break;
		default:
			return 0;
	}
	return 1;
}

static int MVolCan(void)
{
	return !!plGetRealMasterVolume;
}

static int MVolEvent(int ev)
{
	switch (ev)
	{
		case cpievInit:
			return MVolCan();
		case cpievInitAll:
			plMVolType=cfGetProfileInt2(cfScreenSec, "screen", "mvoltype", 2, 10)%3;
			return 1;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiTModeMVol = {"mvol", MVolGetWin, MVolSetWin, MVolDraw, MVolIProcessKey, MVolAProcessKey, MVolEvent CPITEXTMODEREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	cpiTextRegisterDefMode(&cpiTModeMVol);
}

static void __attribute__((destructor))done(void)
{
	cpiTextUnregisterDefMode(&cpiTModeMVol);
}
