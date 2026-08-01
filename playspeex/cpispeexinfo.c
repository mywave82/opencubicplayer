/* OpenCP Module Player
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Display speex TAG text info
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
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "stuff/poutput.h"
#include "speexplay.h"

static int speexInfoActive;

static int speexInfoFirstColumn;
static int speexInfoFirstLine;
static int speexInfoHeight;
static int speexInfoWidth;
static int speexInfoDesiredHeight;
static int speexInfoScroll;
static int speexInfoWidestTitle;

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

static void Update_speexinfoLastHeightNeed(void)
{
	int needed = 1;
	int i;
	speexInfoWidestTitle = 0;
	for (i=0; i < speex_comments_count; i++)
	{
		int len = strlen (speex_comments[i]->title);
		if (len > speexInfoWidestTitle)
		{
			speexInfoWidestTitle = len;
		}
		needed += speex_comments[i]->value_count;
	}
	speexInfoDesiredHeight = needed;
}

static void speexInfoSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	speexInfoFirstColumn=xpos;
	speexInfoFirstLine=ypos;
	speexInfoHeight=hgt;
	speexInfoWidth=wid;
}

static int speexInfoGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
#if 0
	if (speex_comments_count <= 0)
	{
		return 0;
	}
#endif
	if ((speexInfoActive==3) && (cpifaceSession->console->TextWidth < 132))
	{
		speexInfoActive=0;
	}

	Update_speexinfoLastHeightNeed();

	switch (speexInfoActive)
	{
		case 0:
			return 0;
		case 1:
			q->xmode=3;
			break;
		case 2:
			q->xmode=1;
			break;
		case 3:
			q->xmode=2;
			break;
	}

	q->size=1;
	q->top=1;

	q->hgtmin = 3;
	q->hgtmax = (speexInfoDesiredHeight < 2) ? 3 : speexInfoDesiredHeight;
	q->killprio=64;
	q->viewprio=110;
	if (q->hgtmin>q->hgtmax)
	{
		q->hgtmin=q->hgtmax;
	}

	return 1;
}

static void speexInfoDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	int line = 0;

	while (speexInfoScroll && ((speexInfoScroll + speexInfoHeight) > speexInfoDesiredHeight))
	{
		speexInfoScroll--;
	}

	cpifaceSession->console->Driver->DisplayStr(speexInfoFirstLine + (line++), speexInfoFirstColumn, focus?COLTITLE1H:COLTITLE1, "speex tag view - page up/dn to scroll", speexInfoWidth);

	line -= speexInfoScroll;

	if (!speex_comments_count)
	{
		if (speexInfoHeight > 2)
		{
			cpifaceSession->console->Driver->DisplayVoid (speexInfoFirstLine + line, speexInfoFirstColumn, speexInfoWidth);
			line++;
		}

		cpifaceSession->console->Driver->DisplayStr (speexInfoFirstLine + line, speexInfoFirstColumn, 0x07, "     No information to display", speexInfoWidth);
		line++;
	} else {
		int i, j;

		for (i=0; i < speex_comments_count; i++)
		{
			for (j=0; j < speex_comments[i]->value_count; j++)
			{
				if ((line >= 0) && (line < speexInfoHeight))
				{
					if (j == 0)
					{
						cpifaceSession->console->Driver->DisplayStr  (speexInfoFirstLine + line, speexInfoFirstColumn,                                   0x07, speex_comments[i]->title,                      strlen (speex_comments[i]->title));
						cpifaceSession->console->Driver->DisplayStr  (speexInfoFirstLine + line, speexInfoFirstColumn + strlen (speex_comments[i]->title), 0x07,                    ":", speexInfoWidestTitle - strlen (speex_comments[i]->title) + 2);
					} else {
						cpifaceSession->console->Driver->DisplayVoid (speexInfoFirstLine + line, speexInfoFirstColumn, speexInfoWidestTitle + 2);
					}
					cpifaceSession->console->Driver->DisplayStr_utf8 (speexInfoFirstLine + line, speexInfoFirstColumn + speexInfoWidestTitle + 2, 0x09, speex_comments[i]->value[j], speexInfoWidth - speexInfoWidestTitle - 2);
				}
				line++;
			}
		}
	}

	while (line < speexInfoHeight)
	{
		cpifaceSession->console->Driver->DisplayVoid (speexInfoFirstLine + line, speexInfoFirstColumn, speexInfoWidth);
		line++;
	}
}

static int speexInfoIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i', "Enable speex info viewer");
			cpifaceSession->KeyHelp ('I', "Enable speex info viewer");
			break;
		case 'i': case 'I':
			if (!speexInfoActive)
			{
				speexInfoActive=1;
			}
			cpifaceSession->cpiTextSetMode (cpifaceSession, "spxinfo");
			return 1;
		case 'x': case 'X':
			speexInfoActive=3;
			break;
		case KEY_ALT_X:
			speexInfoActive=2;
			break;
	}
	return 0;
}

static int speexInfoAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case 'i': case 'I':
			speexInfoActive=(speexInfoActive+1)%4;
			if ((speexInfoActive==3) && (cpifaceSession->console->TextWidth < 132))
			{
				speexInfoActive=0;
			}
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;

		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i',       "Disable speex info viewer");
			cpifaceSession->KeyHelp ('I',       "Disable speex info viewer");
			cpifaceSession->KeyHelp (KEY_PPAGE, "Scroll speex info viewer up");
			cpifaceSession->KeyHelp (KEY_NPAGE, "Scroll speex info viewer down");
			cpifaceSession->KeyHelp (KEY_HOME,  "Scroll speex info viewer to the top");
			cpifaceSession->KeyHelp (KEY_END,   "Scroll speex info viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (speexInfoScroll)
			{
				speexInfoScroll--;
			}
			break;
		case KEY_NPAGE:
			speexInfoScroll++;
			break;
		case KEY_HOME:
			speexInfoScroll=0;
		case KEY_END:
			speexInfoScroll=speexInfoDesiredHeight - speexInfoHeight;
			break;
		default:
			return 0;
	}
	return 1;
}

static struct cpitextmoderegstruct cpispeexInfo;

static int speexInfoEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			speexInfoActive=2;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}

static struct cpitextmoderegstruct cpispeexInfo = {"spxinfo", speexInfoGetWin, speexInfoSetWin, speexInfoDraw, speexInfoIProcessKey, speexInfoAProcessKey, speexInfoEvent CPITEXTMODEREGSTRUCT_TAIL};

OCP_INTERNAL void speexInfoInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpispeexInfo);
}

OCP_INTERNAL void speexInfoDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpispeexInfo);
}
