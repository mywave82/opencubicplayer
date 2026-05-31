/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Display WavPack TAG text info
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
#include "wavpackplay.h"

static int wavpackInfoActive;

static int wavpackInfoFirstColumn;
static int wavpackInfoFirstLine;
static int wavpackInfoHeight;
static int wavpackInfoWidth;
static int wavpackInfoDesiredHeight;
static int wavpackInfoScroll;
static int wavpackInfoWidestTitle;

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

static void Update_wavpackinfoLastHeightNeed(void)
{
	int needed = 1;
	int i;
	wavpackInfoWidestTitle = 0;
	for (i=0; i < wavpack_comments_count; i++)
	{
		int len = strlen (wavpack_comments[i].title);
		if (len > wavpackInfoWidestTitle)
		{
			wavpackInfoWidestTitle = len;
		}
		needed += wavpack_comments[i].value_count;
	}
	wavpackInfoDesiredHeight = needed;
}

static void wavpackInfoSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	wavpackInfoFirstColumn=xpos;
	wavpackInfoFirstLine=ypos;
	wavpackInfoHeight=hgt;
	wavpackInfoWidth=wid;
}

static int wavpackInfoGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
#if 0
	if (wavpack_comments_count <= 0)
	{
		return 0;
	}
#endif
	if ((wavpackInfoActive==3) && (cpifaceSession->console->TextWidth < 132))
	{
		wavpackInfoActive=0;
	}

	Update_wavpackinfoLastHeightNeed();

	switch (wavpackInfoActive)
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
	q->hgtmax = (wavpackInfoDesiredHeight < 2) ? 3 : wavpackInfoDesiredHeight;
	q->killprio=64;
	q->viewprio=110;
	if (q->hgtmin>q->hgtmax)
	{
		q->hgtmin=q->hgtmax;
	}

	return 1;
}

static void wavpackInfoDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	int line = 0;

	while (wavpackInfoScroll && ((wavpackInfoScroll + wavpackInfoHeight) > wavpackInfoDesiredHeight))
	{
		wavpackInfoScroll--;
	}

	cpifaceSession->console->Driver->DisplayStr(wavpackInfoFirstLine + (line++), wavpackInfoFirstColumn, focus?COLTITLE1H:COLTITLE1, "wavpack tag view - page up/dn to scroll", wavpackInfoWidth);

	line -= wavpackInfoScroll;

	if (!wavpack_comments_count)
	{
		if (wavpackInfoHeight > 2)
		{
			cpifaceSession->console->Driver->DisplayVoid (wavpackInfoFirstLine + line, wavpackInfoFirstColumn, wavpackInfoWidth);
			line++;
		}

		cpifaceSession->console->Driver->DisplayStr (wavpackInfoFirstLine + line, wavpackInfoFirstColumn, 0x07, "     No information to display", wavpackInfoWidth);
		line++;
	} else {
		int i, j;

		for (i=0; i < wavpack_comments_count; i++)
		{
			for (j=0; j < wavpack_comments[i].value_count; j++)
			{
				if ((line >= 0) && (line < wavpackInfoHeight))
				{
					if (j == 0)
					{
						cpifaceSession->console->Driver->DisplayStr  (wavpackInfoFirstLine + line, wavpackInfoFirstColumn,                                      0x07, wavpack_comments[i].title,                          strlen (wavpack_comments[i].title));
						cpifaceSession->console->Driver->DisplayStr  (wavpackInfoFirstLine + line, wavpackInfoFirstColumn + strlen (wavpack_comments[i].title), 0x07,                       ":", wavpackInfoWidestTitle - strlen (wavpack_comments[i].title) + 2);
					} else {
						cpifaceSession->console->Driver->DisplayVoid (wavpackInfoFirstLine + line, wavpackInfoFirstColumn, wavpackInfoWidestTitle + 2);
					}
					cpifaceSession->console->Driver->DisplayStr_utf8 (wavpackInfoFirstLine + line, wavpackInfoFirstColumn + wavpackInfoWidestTitle + 2, 0x09, wavpack_comments[i].value[j], wavpackInfoWidth - wavpackInfoWidestTitle - 2);
				}
				line++;
			}
		}
	}

	while (line < wavpackInfoHeight)
	{
		cpifaceSession->console->Driver->DisplayVoid (wavpackInfoFirstLine + line, wavpackInfoFirstColumn, wavpackInfoWidth);
		line++;
	}
}

static int wavpackInfoIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i', "Enable wavpack info viewer");
			cpifaceSession->KeyHelp ('I', "Enable wavpack info viewer");
			break;
		case 'i': case 'I':
			if (!wavpackInfoActive)
			{
				wavpackInfoActive=1;
			}
			cpifaceSession->cpiTextSetMode (cpifaceSession, "wvinfo");
			return 1;
		case 'x': case 'X':
			wavpackInfoActive=3;
			break;
		case KEY_ALT_X:
			wavpackInfoActive=2;
			break;
	}
	return 0;
}

static int wavpackInfoAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case 'i': case 'I':
			wavpackInfoActive=(wavpackInfoActive+1)%4;
			if ((wavpackInfoActive==3) && (cpifaceSession->console->TextWidth < 132))
			{
				wavpackInfoActive=0;
			}
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;

		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i',       "Disable wavpack info viewer");
			cpifaceSession->KeyHelp ('I',       "Disable wavpack info viewer");
			cpifaceSession->KeyHelp (KEY_PPAGE, "Scroll wavpack info viewer up");
			cpifaceSession->KeyHelp (KEY_NPAGE, "Scroll wavpack info viewer down");
			cpifaceSession->KeyHelp (KEY_HOME,  "Scroll wavpack info viewer to the top");
			cpifaceSession->KeyHelp (KEY_END,   "Scroll wavpack info viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (wavpackInfoScroll)
			{
				wavpackInfoScroll--;
			}
			break;
		case KEY_NPAGE:
			wavpackInfoScroll++;
			break;
		case KEY_HOME:
			wavpackInfoScroll=0;
		case KEY_END:
			wavpackInfoScroll=wavpackInfoDesiredHeight - wavpackInfoHeight;
			break;
		default:
			return 0;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiwavpackInfo;

static int wavpackInfoEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			wavpackInfoActive=2;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiwavpackInfo = {"wvinfo", wavpackInfoGetWin, wavpackInfoSetWin, wavpackInfoDraw, wavpackInfoIProcessKey, wavpackInfoAProcessKey, wavpackInfoEvent CPITEXTMODEREGSTRUCT_TAIL};

OCP_INTERNAL void wavpackInfoInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiwavpackInfo);
}

OCP_INTERNAL void wavpackInfoDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiwavpackInfo);
}
