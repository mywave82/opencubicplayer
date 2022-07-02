/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "cpiface-private.h"

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

static void drawpeakpower (struct cpifaceSessionAPI_t *cpifaceSession, int y, int x)
{
	unsigned short strbuf[40];
	int l,r;

	writestring(strbuf, 0, plPause?COLMUTE:COLTEXT, " ["
	                                                "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa"
	                                                " -- "
	                                                "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa"
							"] ", 40);
	cpifaceSession->GetRealMasterVolume (&l, &r);
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

static void drawbigpeakpower (struct cpifaceSessionAPI_t *cpifaceSession, int y, int x)
{
	unsigned short strbuf[80];
	int l,r;

	writestring(strbuf, 0, plPause?COLMUTE:COLTEXT,
			"   ["
			"\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa"
			" -=\xf0\xf0=- "
			"\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa"
			"]   ", 80);
	cpifaceSession->GetRealMasterVolume (&l, &r);
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

static void MVolDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	if (plMVolType==2)
	{
		displayvoid (plMVolFirstLine, plMVolFirstCol /* 80 */, 8);
		displayvoid (plMVolFirstLine, plMVolFirstCol + 48 /* 128 */, 4);
		if (plMVolHeight==2)
		{
			displayvoid (plMVolFirstLine+1, plMVolFirstCol /* 80 */, 8);
			displayvoid (plMVolFirstLine+1, plMVolFirstCol + 48 /* 128 */, 4);
		}
		drawpeakpower (cpifaceSession, plMVolFirstLine, plMVolFirstCol + 8 /* 88 */);
	} else {
		int l=(plMVolWidth>=132)?(plMVolWidth/2)-40 :20;
		displaystr(plMVolFirstLine, plMVolFirstCol /* 0 */, plPause?COLMUTE:(focus?COLTITLEH:COLTEXT), "  peak power level:", l);
		displayvoid (plMVolFirstLine, plMVolWidth-l + plMVolFirstCol /* 0 */ , l);
		if (plMVolHeight==2)
		{
			displayvoid (plMVolFirstLine+1, plMVolFirstCol /* 0 */, l);
			displayvoid (plMVolFirstLine+1, plMVolWidth-l + plMVolFirstCol /* 0 */, l);
		}
		if (plMVolWidth>=132)
			drawbigpeakpower (cpifaceSession, plMVolFirstLine, l);
		else
			drawpeakpower (cpifaceSession, plMVolFirstLine, l);
	}
}

static void MVolSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	plMVolFirstCol=xpos;
	plMVolFirstLine=ypos;
	plMVolHeight=hgt;
	plMVolWidth=wid;
}

static int MVolGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
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

static int MVolIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('v', "Enable volume viewer");
			cpiKeyHelp('V', "Enable volume viewer");
			break;
		case 'v': case 'V':
			if (!plMVolType)
				plMVolType=1;
			cpiTextSetMode (cpifaceSession, "mvol");
			return 1;
		case 'x': case 'X':
			plMVolType = cpifaceSession->LogicalChannelCount ? 2 : 1;
			break;
		case KEY_ALT_X:
			plMVolType=1;
			break;
	}
	return 0;
}

static int MVolAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('v', "Change volume viewer mode");
			cpiKeyHelp('V', "Change volume viewer mode");
			return 0;
		case 'v': case 'V':
			plMVolType=(plMVolType+1)%3;
			cpiTextRecalc (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}

static int MVolCan (struct cpifaceSessionAPI_t *cpifaceSession)
{
	return !!cpifaceSession->GetRealMasterVolume;
}

static int MVolEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInit:
			return MVolCan (cpifaceSession);
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
