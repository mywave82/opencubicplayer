/* OpenCP Module Player
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Display opus TAG text info
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
#include "opusplay.h"

static int opusInfoActive;

static int opusInfoFirstColumn;
static int opusInfoFirstLine;
static int opusInfoHeight;
static int opusInfoWidth;
static int opusInfoDesiredHeight;
static int opusInfoScroll;
static int opusInfoWidestTitle;

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

static void Update_opusinfoLastHeightNeed(void)
{
	int needed = 1;
	int i;
	opusInfoWidestTitle = 0;
	for (i=0; i < opus_comments_count; i++)
	{
		int len = strlen (opus_comments[i]->title);
		if (len > opusInfoWidestTitle)
		{
			opusInfoWidestTitle = len;
		}
		needed += opus_comments[i]->value_count;
	}
	opusInfoDesiredHeight = needed;
}

static void opusInfoSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	opusInfoFirstColumn=xpos;
	opusInfoFirstLine=ypos;
	opusInfoHeight=hgt;
	opusInfoWidth=wid;
}

static int opusInfoGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
#if 0
	if (opus_comments_count <= 0)
	{
		return 0;
	}
#endif
	if ((opusInfoActive==3) && (cpifaceSession->console->TextWidth < 132))
	{
		opusInfoActive=0;
	}

	Update_opusinfoLastHeightNeed();

	switch (opusInfoActive)
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
	q->hgtmax = (opusInfoDesiredHeight < 2) ? 3 : opusInfoDesiredHeight;
	q->killprio=64;
	q->viewprio=110;
	if (q->hgtmin>q->hgtmax)
	{
		q->hgtmin=q->hgtmax;
	}

	return 1;
}

static void opusInfoDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	int line = 0;

	while (opusInfoScroll && ((opusInfoScroll + opusInfoHeight) > opusInfoDesiredHeight))
	{
		opusInfoScroll--;
	}

	cpifaceSession->console->Driver->DisplayStr(opusInfoFirstLine + (line++), opusInfoFirstColumn, focus?COLTITLE1H:COLTITLE1, "opus tag view - page up/dn to scroll", opusInfoWidth);

	line -= opusInfoScroll;

	if (!opus_comments_count)
	{
		if (opusInfoHeight > 2)
		{
			cpifaceSession->console->Driver->DisplayVoid (opusInfoFirstLine + line, opusInfoFirstColumn, opusInfoWidth);
			line++;
		}

		cpifaceSession->console->Driver->DisplayStr (opusInfoFirstLine + line, opusInfoFirstColumn, 0x07, "     No information to display", opusInfoWidth);
		line++;
	} else {
		int i, j;

		for (i=0; i < opus_comments_count; i++)
		{
			for (j=0; j < opus_comments[i]->value_count; j++)
			{
				if ((line >= 0) && (line < opusInfoHeight))
				{
					if (j == 0)
					{
						cpifaceSession->console->Driver->DisplayStr  (opusInfoFirstLine + line, opusInfoFirstColumn,                                   0x07, opus_comments[i]->title,                      strlen (opus_comments[i]->title));
						cpifaceSession->console->Driver->DisplayStr  (opusInfoFirstLine + line, opusInfoFirstColumn + strlen (opus_comments[i]->title), 0x07,                    ":", opusInfoWidestTitle - strlen (opus_comments[i]->title) + 2);
					} else {
						cpifaceSession->console->Driver->DisplayVoid (opusInfoFirstLine + line, opusInfoFirstColumn, opusInfoWidestTitle + 2);
					}
					cpifaceSession->console->Driver->DisplayStr_utf8 (opusInfoFirstLine + line, opusInfoFirstColumn + opusInfoWidestTitle + 2, 0x09, opus_comments[i]->value[j], opusInfoWidth - opusInfoWidestTitle - 2);
				}
				line++;
			}
		}
	}

	while (line < opusInfoHeight)
	{
		cpifaceSession->console->Driver->DisplayVoid (opusInfoFirstLine + line, opusInfoFirstColumn, opusInfoWidth);
		line++;
	}
}

static int opusInfoIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i', "Enable opus info viewer");
			cpifaceSession->KeyHelp ('I', "Enable opus info viewer");
			break;
		case 'i': case 'I':
			if (!opusInfoActive)
			{
				opusInfoActive=1;
			}
			cpifaceSession->cpiTextSetMode (cpifaceSession, "opusinfo");
			return 1;
		case 'x': case 'X':
			opusInfoActive=3;
			break;
		case KEY_ALT_X:
			opusInfoActive=2;
			break;
	}
	return 0;
}

static int opusInfoAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case 'i': case 'I':
			opusInfoActive=(opusInfoActive+1)%4;
			if ((opusInfoActive==3) && (cpifaceSession->console->TextWidth < 132))
			{
				opusInfoActive=0;
			}
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;

		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i',       "Disable opus info viewer");
			cpifaceSession->KeyHelp ('I',       "Disable opus info viewer");
			cpifaceSession->KeyHelp (KEY_PPAGE, "Scroll opus info viewer up");
			cpifaceSession->KeyHelp (KEY_NPAGE, "Scroll opus info viewer down");
			cpifaceSession->KeyHelp (KEY_HOME,  "Scroll opus info viewer to the top");
			cpifaceSession->KeyHelp (KEY_END,   "Scroll opus info viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (opusInfoScroll)
			{
				opusInfoScroll--;
			}
			break;
		case KEY_NPAGE:
			opusInfoScroll++;
			break;
		case KEY_HOME:
			opusInfoScroll=0;
		case KEY_END:
			opusInfoScroll=opusInfoDesiredHeight - opusInfoHeight;
			break;
		default:
			return 0;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiopusInfo;

static int opusInfoEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			opusInfoActive=2;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiopusInfo = {"opusinfo", opusInfoGetWin, opusInfoSetWin, opusInfoDraw, opusInfoIProcessKey, opusInfoAProcessKey, opusInfoEvent CPITEXTMODEREGSTRUCT_TAIL};

OCP_INTERNAL void opusInfoInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiopusInfo);
}

OCP_INTERNAL void opusInfoDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiopusInfo);
}
