/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Display Flac TAG text info
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
#include "flacplay.h"

static int FlacInfoActive;

static int FlacInfoFirstColumn;
static int FlacInfoFirstLine;
static int FlacInfoHeight;
static int FlacInfoWidth;
static int FlacInfoDesiredHeight;
static int FlacInfoScroll;
static int FlacInfoWidestTitle;

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

static void Update_FlacinfoLastHeightNeed(void)
{
	int needed = 1;
	int i;
	FlacInfoWidestTitle = 0;
	for (i=0; i < flac_comments_count; i++)
	{
		int len = strlen (flac_comments[i]->title);
		if (len > FlacInfoWidestTitle)
		{
			FlacInfoWidestTitle = len;
		}
		needed += flac_comments[i]->value_count;
	}
	FlacInfoDesiredHeight = needed;
}

static void FlacInfoSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	FlacInfoFirstColumn=xpos;
	FlacInfoFirstLine=ypos;
	FlacInfoHeight=hgt;
	FlacInfoWidth=wid;
}

static int FlacInfoGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
#if 0
	if (flac_comments_count <= 0)
	{
		return 0;
	}
#endif
	if ((FlacInfoActive==3) && (cpifaceSession->console->TextWidth < 132))
	{
		FlacInfoActive=0;
	}

	flacMetaDataLock();

	Update_FlacinfoLastHeightNeed();

	flacMetaDataUnlock();

	switch (FlacInfoActive)
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
	q->hgtmax = (FlacInfoDesiredHeight < 2) ? 3 : FlacInfoDesiredHeight;
	q->killprio=64;
	q->viewprio=110;
	if (q->hgtmin>q->hgtmax)
	{
		q->hgtmin=q->hgtmax;
	}

	return 1;
}

static void FlacInfoDraw(struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	int line = 0;

	flacMetaDataLock();

	while (FlacInfoScroll && ((FlacInfoScroll + FlacInfoHeight) > FlacInfoDesiredHeight))
	{
		FlacInfoScroll--;
	}

	cpifaceSession->console->Driver->DisplayStr(FlacInfoFirstLine + (line++), FlacInfoFirstColumn, focus?COLTITLE1H:COLTITLE1, "Flac tag view - page up/dn to scroll", FlacInfoWidth);

	line -= FlacInfoScroll;

	if (!flac_comments_count)
	{
		if (FlacInfoHeight > 2)
		{
			cpifaceSession->console->Driver->DisplayVoid (FlacInfoFirstLine + line, FlacInfoFirstColumn, FlacInfoWidth);
			line++;
		}

		cpifaceSession->console->Driver->DisplayStr (FlacInfoFirstLine + line, FlacInfoFirstColumn, 0x07, "     No information to display", FlacInfoWidth);
		line++;
	} else {
		int i, j;

		for (i=0; i < flac_comments_count; i++)
		{
			for (j=0; j < flac_comments[i]->value_count; j++)
			{
				if ((line >= 0) && (line < FlacInfoHeight))
				{
					if (j == 0)
					{
						cpifaceSession->console->Driver->DisplayStr  (FlacInfoFirstLine + line, FlacInfoFirstColumn,                                   0x07, flac_comments[i]->title,                      strlen (flac_comments[i]->title));
						cpifaceSession->console->Driver->DisplayStr  (FlacInfoFirstLine + line, FlacInfoFirstColumn + strlen (flac_comments[i]->title), 0x07,                    ":", FlacInfoWidestTitle - strlen (flac_comments[i]->title) + 2);
					} else {
						cpifaceSession->console->Driver->DisplayVoid (FlacInfoFirstLine + line, FlacInfoFirstColumn, FlacInfoWidestTitle + 2);
					}
					cpifaceSession->console->Driver->DisplayStr_utf8 (FlacInfoFirstLine + line, FlacInfoFirstColumn + FlacInfoWidestTitle + 2, 0x09, flac_comments[i]->value[j], FlacInfoWidth - FlacInfoWidestTitle - 2);
				}
				line++;
			}
		}
	}

	while (line < FlacInfoHeight)
	{
		cpifaceSession->console->Driver->DisplayVoid (FlacInfoFirstLine + line, FlacInfoFirstColumn, FlacInfoWidth);
		line++;
	}

	flacMetaDataUnlock();
}

static int FlacInfoIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i', "Enable Flac info viewer");
			cpifaceSession->KeyHelp ('I', "Enable Flac info viewer");
			break;
		case 'i': case 'I':
			if (!FlacInfoActive)
			{
				FlacInfoActive=1;
			}
			cpifaceSession->cpiTextSetMode(cpifaceSession, "flacinfo");
			return 1;
		case 'x': case 'X':
			FlacInfoActive=3;
			break;
		case KEY_ALT_X:
			FlacInfoActive=2;
			break;
	}
	return 0;
}

static int FlacInfoAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case 'i': case 'I':
			FlacInfoActive=(FlacInfoActive+1)%4;
			if ((FlacInfoActive==3)&& (cpifaceSession->console->TextWidth < 132))
			{
				FlacInfoActive=0;
			}
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;

		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i',       "Disable Flac info viewer");
			cpifaceSession->KeyHelp ('I',       "Disable Flac info viewer");
			cpifaceSession->KeyHelp (KEY_PPAGE, "Scroll Flac info viewer up");
			cpifaceSession->KeyHelp (KEY_NPAGE, "Scroll Flac info viewer down");
			cpifaceSession->KeyHelp (KEY_HOME,  "Scroll Flac info viewer to the top");
			cpifaceSession->KeyHelp (KEY_END,   "Scroll Flac info viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (FlacInfoScroll)
			{
				FlacInfoScroll--;
			}
			break;
		case KEY_NPAGE:
			FlacInfoScroll++;
			break;
		case KEY_HOME:
			FlacInfoScroll=0;
		case KEY_END:
			FlacInfoScroll=FlacInfoDesiredHeight - FlacInfoHeight;
			break;
		default:
			return 0;
	}
	return 1;
}


static int FlacInfoEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			FlacInfoActive=2;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}


static struct cpitextmoderegstruct cpiFlacInfo = {"flacinfo", FlacInfoGetWin, FlacInfoSetWin, FlacInfoDraw, FlacInfoIProcessKey, FlacInfoAProcessKey, FlacInfoEvent CPITEXTMODEREGSTRUCT_TAIL};

void __attribute__ ((visibility ("internal"))) FlacInfoInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiFlacInfo);
}

void __attribute__ ((visibility ("internal"))) FlacInfoDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiFlacInfo);
}
