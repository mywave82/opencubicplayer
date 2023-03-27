/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface text mode channel display
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

static void (*ChanDisplay)(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiChanWidth width, int i, int compoMode);
static int plChanFirstLine;
static int plChanHeight;
static int plChanWidth;
static char plChannelType;
static int plChanStartCol;

static void ChanDraw (struct cpifaceSessionAPI_t *cpifaceSession, int ignore)
{
	uint16_t buf[CONSOLE_MAX_X];
	int i,y,x;
	int h =(plChannelType==1) ? ((cpifaceSession->LogicalChannelCount + 1) / 2) : cpifaceSession->LogicalChannelCount;
	int sh=(plChannelType==1) ?  (cpifaceSession->SelectedChannel          / 2) : cpifaceSession->SelectedChannel;
	int first;
	memset(buf, 0, sizeof(buf));
	if (h>plChanHeight)
		if (sh<(plChanHeight/2))
			first=0;
		else
			if (sh>=(h-plChanHeight/2))
				first=h-plChanHeight;
			else
				first=sh-(plChanHeight-1)/2;
	else
		first=0;

	for (y=0; y<plChanHeight; y++)
	{
		char *sign=" ";
		if (!y&&first)
			sign="\x18";
		if (((y+1)==plChanHeight)&&((y+first+1)!=h))
			sign="\x19";
		if (plChannelType==1)
		{
			for (x=0; x<2; x++)
			{
				i=2*first+y*2+x;
				if (cpifaceSession->PanType && (y&1))
					i^=1;
				if (i < cpifaceSession->LogicalChannelCount)
				{
					if (plChanWidth<132)
					{
						writestring(buf, x*40, cpifaceSession->MuteChannel[i]?0x08:0x07, " ##:", 4);
						writestring(buf, x*40, 0x0F, (i==cpifaceSession->SelectedChannel) ? ">" : sign, 1);
						writenum(buf, x*40+1, cpifaceSession->MuteChannel[i]?0x08:0x07, i+1, 10, 2, 1);
						ChanDisplay (cpifaceSession, buf+x*40+4, cpiChanWidth_36, i, plCompoMode);
					} else {
						writestring(buf, x*66, cpifaceSession->MuteChannel[i]?0x08:0x07, " ##:", 4);
						writestring(buf, x*66, 0x0F, (i==cpifaceSession->SelectedChannel) ? ">" : sign, 1);
						writenum(buf, x*66+1, cpifaceSession->MuteChannel[i]?0x08:0x07, i+1, 10, 2, 1);
						ChanDisplay (cpifaceSession, buf+x*66+4, cpiChanWidth_62, i, plCompoMode);
					}
				} else
					if (plChanWidth<132)
						writestring(buf, x*40, 0, "", 40);
					else
						writestring(buf, x*66, 0, "", 66);
			}
		} else {
			int i=y+first;
			if ((y + first) == cpifaceSession->SelectedChannel)
			{
				sign=">";
			}
			if (plChannelType==2)
			{
				writestring(buf, 0, cpifaceSession->MuteChannel[i]?0x08:0x07, " ##:", 4);
				writestring(buf, 0, 0x0F, sign, 1);
				writenum(buf, 1, cpifaceSession->MuteChannel[i]?0x08:0x07, i+1, 10, 2, 1);
				ChanDisplay (cpifaceSession, buf+4, (plChanWidth<132)?cpiChanWidth_76:cpiChanWidth_128, y+first, plCompoMode);
			} else {
				writestring(buf, 0, cpifaceSession->MuteChannel[i]?0x08:0x07, "     ##:", 8);
				writestring(buf, 4, 0x0F, sign, 1);
				writenum(buf, 5, cpifaceSession->MuteChannel[i]?0x08:0x07, i+1, 10, 2, 1);
				ChanDisplay (cpifaceSession, buf+8, cpiChanWidth_44, y+first, plCompoMode);
			}
		}
		displaystrattr(plChanFirstLine+y, plChanStartCol, buf, plChanWidth);
	}
}

static void ChanSetWin(struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	plChanFirstLine=ypos;
	plChanStartCol=xpos;
	plChanHeight=hgt;
	plChanWidth=wid;
}

static int ChanGetWin(struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	if ((plChannelType==3)&&(plScrWidth<132))
		plChannelType=0;
	if (!cpifaceSession->LogicalChannelCount)
		return 0;

	switch (plChannelType)
	{
		case 0:
			return 0;
		case 1:
			q->hgtmax=(cpifaceSession->LogicalChannelCount + 1)>>1;
			q->xmode=3;
			break;
		case 2:
			q->hgtmax = cpifaceSession->LogicalChannelCount;
			q->xmode=1;
			break;
		case 3:
			q->hgtmax = cpifaceSession->LogicalChannelCount;
			q->xmode=2;
			break;
	}
	q->size=1;
	q->top=1;
	q->killprio=128;
	q->viewprio=160;
	q->hgtmin=2;
	if (q->hgtmin>q->hgtmax)
		q->hgtmin=q->hgtmax;
	return 1;
}

static int ChanIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('c', "Enable channel viewer");
			cpiKeyHelp('C', "Enable channel viewer");
			break;
		case 'c': case 'C':
			if (!plChannelType)
				plChannelType=(plChannelType+1)%4;
			cpiTextSetMode (cpifaceSession, "chan");
			return 1;
		case 'x': case 'X':
			plChannelType=3;
			break;
		case KEY_ALT_X:
			plChannelType=2;
			break;
	}
	return 0;
}

static int ChanAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('c', "Change channel view mode");
			cpiKeyHelp('C', "Change channel view mode");
			return 0;
		case 'c': case 'C':
			plChannelType=(plChannelType+1)%4;
			cpiTextRecalc (cpifaceSession);
			break;
		default:
			return 0;
	}
	return 1;
}

static int ChanEvent(struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	return 1;
}

static struct cpitextmoderegstruct cpiTModeChan = {"chan", ChanGetWin, ChanSetWin, ChanDraw, ChanIProcessKey, ChanAProcessKey, ChanEvent CPITEXTMODEREGSTRUCT_TAIL};

void __attribute__ ((visibility ("internal"))) cpiChanInit (void)
{
	plChannelType = cfGetProfileInt2(cfScreenSec, "screen", "channeltype", 3, 10)&3;
}


void plUseChannels (struct cpifaceSessionAPI_t *cpifaceSession, void (*Display)(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiChanWidth width, int i, int compoMode))
{
	ChanDisplay=Display;
	if (!cpifaceSession->LogicalChannelCount)
		return;
	cpiTextRegisterMode(cpifaceSession, &cpiTModeChan);
}

