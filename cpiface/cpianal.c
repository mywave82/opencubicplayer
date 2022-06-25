/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface text mode spectrum analyser
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
 *
 *  -doj980924  Dirk Jagdmann <doj@cubic.org>
 *    -changed code with fftanalyse to meet dependencies from changes in fft.cpp
 *  -fd981119   Felix Domke   <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT'
 *  -doj981220  Dirk Jagdmann <doj@cubic.org>
 *    -generation of title string changed from str() functions to sprintf()
 *  -fd9903030   Felix Domke   <tmbinc@gmx.net>
 *    -added "kb"-mode, blinks the keyboard-leds if you want :)
 *     (useless feature, i know, but it's FUN...)
 */

#include "config.h"
#include <stdio.h>
#include "types.h"
#include "boot/psetting.h"
#include "cpiface.h"
#include "dev/mcp.h"
#include "fft.h"
#include "stuff/poutput.h"

#define COLBACK 0x00
#define COLTITLE 0x01
#define COLTITLEH 0x09
#define COLSET0 0x090B0A
#define COLSET1 0x0C0E0A
#define COLSET2 0x070707
#define COLSET3 0x0A0A0A

static int analactive;
static unsigned long plAnalRate;
static int plAnalFirstLine;
static unsigned int plAnalHeight;
static unsigned int plAnalWidth;
static int plAnalCol;
static unsigned int plAnalScale;

static int plAnalChan;
static int plAnalFlip=0;

static int16_t plSampBuf[4096];
static uint16_t ana[1024];

static void plDrawFFT(char sel)
{
	char str[80]; /* contains the title string */
	char s2[20];
	char *s; /* pointer to temp string */
	unsigned int wid;
	unsigned int ofs;
	unsigned long col;
	unsigned int i;
	int bits;

	if ((plAnalChan==2)&&!plGetLChanSample)
		plAnalChan=0;
	if (((plAnalChan==0)||(plAnalChan==1)) && (!cpifaceSessionAPI.GetMasterSample))
		plAnalChan=2;
	if ((plAnalChan==2)&&!plGetLChanSample)
		plAnalChan=0;

	/* make the *s point to the right string */
	if (plAnalChan==2)
	{
		s=s2;
		snprintf(s2, sizeof(s2), "single channel: %3i", plSelCh+1);
	} else {
		if (plAnalChan)
			s="master channel, mono";
		else
			s="master channel, stereo";
	}

	/* 7 bits 64+8 => 72
	 * 8 bits 128+8 => 136
	 * 9 bits 256+8 => 304
	 * 10 bits 512+8 => 520
	 * 11 bits..........
	 */
	if (plAnalWidth<=72)
		bits=7;
	else if (plAnalWidth<=136)
		bits=8;
	else if (plAnalWidth<=264)
		bits=9;
	else if (plAnalWidth<=520)
		bits=10;
	else
		bits=11;

	/* print the title string */
	snprintf (str, sizeof (str), "  spectrum analyser, step: %3iHz, max: %5iHz, %s",
		  (int)(plAnalRate>>bits),
		  (int)(plAnalRate>>1),
		  s
	);

	displaystr(plAnalFirstLine-1, 0, sel?COLTITLEH:COLTITLE, str, plAnalWidth);

	wid=plAnalWidth-8;
	ofs=(plAnalWidth-wid)>>1;

	col=(plAnalCol==0)?COLSET0:(plAnalCol==1)?COLSET1:(plAnalCol==2)?COLSET2:COLSET3;

	for (i=0; i<plAnalHeight; i++)
	{
		displayvoid (i+plAnalFirstLine, 0, ofs);
		displayvoid (i+plAnalFirstLine, plAnalWidth-ofs, ofs);
	}

	if (!plAnalChan)
	{
		unsigned int wh2;
		unsigned int fl;

		cpifaceSessionAPI.GetMasterSample(plSampBuf, 1<<bits, plAnalRate, mcpGetSampleStereo);
		if (plAnalHeight&1)
			displayvoid (plAnalFirstLine+plAnalHeight-1, ofs, plAnalWidth-2*ofs);
		wh2=plAnalHeight>>1;
		fl=plAnalFirstLine+wh2-1;

		fftanalyseall(ana, plSampBuf, 2, bits);
		for (i=0; i<wid; i++)
			if ((plAnalFlip==2)||(plAnalFlip==3))
				idrawbar(i+ofs, fl, wh2, (((ana[i]*plAnalScale)>>11)*wh2)>>8, col);
			else
				drawbar(i+ofs, fl, wh2, (((ana[i]*plAnalScale)>>11)*wh2)>>8, col);


		fl+=wh2;
		fftanalyseall(ana, plSampBuf+1, 2, bits);
		for (i=0; i<wid; i++)
			if ((plAnalFlip==1)||(plAnalFlip==2))
				idrawbar(i+ofs, fl, wh2, (((ana[i]*plAnalScale)>>11)*wh2)>>8, col);
			else
				drawbar(i+ofs, fl, wh2, (((ana[i]*plAnalScale)>>11)*wh2)>>8, col);

	} else {
		if (plAnalChan!=2)
			cpifaceSessionAPI.GetMasterSample(plSampBuf, 1<<bits, plAnalRate, 0);
		else
			plGetLChanSample(plSelCh, plSampBuf, 1<<bits, plAnalRate, 0);
		fftanalyseall(ana, plSampBuf, 1, bits);
		for (i=0; i<wid; i++)
			if (plAnalFlip&1)
				idrawbar(i+ofs, plAnalFirstLine+plAnalHeight-1, plAnalHeight, (((ana[i]*plAnalScale)>>11)*plAnalHeight)>>8, col);
			else
				drawbar(i+ofs, plAnalFirstLine+plAnalHeight-1, plAnalHeight, (((ana[i]*plAnalScale)>>11)*plAnalHeight)>>8, col);
	}
}

static void AnalSetWin(int unused, int wid, int ypos, int hgt)
{
	plAnalFirstLine=ypos+1;
	plAnalHeight=hgt-1;
	plAnalWidth=wid;
}

static int AnalGetWin(struct cpitextmodequerystruct *q)
{
	if (!analactive)
		return 0;

	q->hgtmin=3;
	q->hgtmax=100;
	q->xmode=1;
	q->size=1;
	q->top=1;
	q->killprio=112;
	q->viewprio=128;
	return 1;
}

static void AnalDraw(int focus)
{
	plDrawFFT(focus);
}

static int AnalIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('a', "Enable analalyzer mode");
			cpiKeyHelp('A', "Enable analalyzer mode");
			break;
		case 'a': case 'A':
			analactive=1;
			cpiTextSetMode("anal");
			return 1; /* do swallow */
		case 'x': case 'X':
			analactive=1;
			break;
		case KEY_ALT_X:
			analactive=0;
			break;
	}
	return 0;
}

static int AnalAProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('A', "Change analyzer orientations");
			cpiKeyHelp('a', "Toggle analyzer off");
			cpiKeyHelp(KEY_PPAGE, "Change analyzer frequenzy space down");
			cpiKeyHelp(KEY_NPAGE, "Change analyzer frequenzy space up");
			cpiKeyHelp(KEY_CTRL_PGUP, "Adjust scale up");
			cpiKeyHelp(KEY_CTRL_PGDN, "Adjust scale down");
			cpiKeyHelp(KEY_HOME, "Reset analyzer settings");
			cpiKeyHelp(KEY_ALT_A, "Change analyzer channel mode");
			cpiKeyHelp(KEY_TAB, "Change the analyzer color");
			cpiKeyHelp(KEY_SHIFT_TAB, "Change the analyzer color (reverse)");
			return 0;
		case 'A':
			plAnalFlip=(plAnalFlip+1)&3;
			break;
		case 'a':
			analactive=!analactive;
			cpiTextRecalc();
			break;
		case KEY_CTRL_PGUP:
		/*case 0x7600: //ctrl-pgdn*/
			plAnalScale=(plAnalScale+1)*32/31;
			plAnalScale=(plAnalScale>=4096)?4096:(plAnalScale<256)?256:plAnalScale;
			break;
		case KEY_CTRL_PGDN:
		/*case 0x7600: //ctrl-pgdn*/
			plAnalScale=plAnalScale*31/32;
			plAnalScale=(plAnalScale>=4096)?4096:(plAnalScale<256)?256:plAnalScale;
			break;
		/* case 0x4900: //pgup*/
		case KEY_PPAGE:
			plAnalRate=plAnalRate*30/32;
			plAnalRate=(plAnalRate>=64000)?64000:(plAnalRate<1024)?1024:plAnalRate;
			break;
		/* case 0x5100: //pgdn*/
		case KEY_NPAGE:
			plAnalRate=plAnalRate*32/30;
			plAnalRate=(plAnalRate>=64000)?64000:(plAnalRate<1024)?1024:plAnalRate;
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plAnalRate=5512;
			plAnalScale=2048;
			plAnalChan=0;
			break;
		/*case 0x1E00:*/
		case KEY_ALT_A: /* ALT A, is escaped, so we have control A for now TODO-KEYS*/
			plAnalChan=(plAnalChan+1)%3;
			break;
		case KEY_TAB: /* tab */
		/*  case 0x0F00: // shift-tab*/
			plAnalCol=(plAnalCol+1)%4;
			break;
		/* case 0xA500 alt-tab really, but we can't do that key in X11*/
		case KEY_SHIFT_TAB:
			plAnalCol=(plAnalCol+3)%4;
			break;
		default:
			return 0;
	}
	return 1;
}

static int AnalInit(void)
{
	plAnalRate=5512;
	plAnalScale=2048;
	plAnalChan=0;
	analactive=cfGetProfileBool2(cfScreenSec, "screen", "analyser", 0, 0);
	return 1;
}

static void AnalClose(void)
{
}

static int AnalCan(void)
{
	if ((!cpifaceSessionAPI.GetMasterSample) && (!plGetLChanSample))
		return 0;
	return 1;
}

static void AnalSetMode(void)
{
}

static int AnalEvent(int ev)
{
	switch (ev)
	{
		case cpievInit:
			return AnalCan();
		case cpievInitAll:
			return AnalInit();
		case cpievDoneAll:
			AnalClose();
			return 1;
		case cpievSetMode:
			AnalSetMode();
			return 1;
	}
	return 1;
}


static struct cpitextmoderegstruct cpiTModeAnal = {"anal", AnalGetWin, AnalSetWin, AnalDraw, AnalIProcessKey, AnalAProcessKey, AnalEvent CPITEXTMODEREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	cpiTextRegisterDefMode(&cpiTModeAnal);
}

static void __attribute__((destructor))done(void)
{
	cpiTextUnregisterDefMode(&cpiTModeAnal);
}
