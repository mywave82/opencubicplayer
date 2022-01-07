/* OpenCP Module Player
 * copyright (c) 2020-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Display Ogg TAG text info
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
#include "oggplay.h"

static int OggInfoActive;

static int OggInfoFirstColumn;
static int OggInfoFirstLine;
static int OggInfoHeight;
static int OggInfoWidth;
static int OggInfoDesiredHeight;
static int OggInfoScroll;
static int OggInfoWidestTitle;

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

static void Update_OgginfoLastHeightNeed(void)
{
	int needed = 1;
	int i;
	OggInfoWidestTitle = 0;
	for (i=0; i < ogg_comments_count; i++)
	{
		int len = strlen (ogg_comments[i]->title);
		if (len > OggInfoWidestTitle)
		{
			OggInfoWidestTitle = len;
		}
		needed += ogg_comments[i]->value_count;
	}
	OggInfoDesiredHeight = needed;
}

static void OggInfoSetWin(int xpos, int wid, int ypos, int hgt)
{
	OggInfoFirstColumn=xpos;
	OggInfoFirstLine=ypos;
	OggInfoHeight=hgt;
	OggInfoWidth=wid;
}

static int OggInfoGetWin(struct cpitextmodequerystruct *q)
{
#if 0
	if (ogg_comments_count <= 0)
	{
		return 0;
	}
#endif
	if ((OggInfoActive==3)&&(plScrWidth<132))
	{
		OggInfoActive=0;
	}

	Update_OgginfoLastHeightNeed();

	switch (OggInfoActive)
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
	q->hgtmax = (OggInfoDesiredHeight < 2) ? 3 : OggInfoDesiredHeight;
	q->killprio=64;
	q->viewprio=110;
	if (q->hgtmin>q->hgtmax)
	{
		q->hgtmin=q->hgtmax;
	}

	return 1;
}

static void OggInfoDraw(int focus)
{
	int line = 0;

	while (OggInfoScroll && ((OggInfoScroll + OggInfoHeight) > OggInfoDesiredHeight))
	{
		OggInfoScroll--;
	}

	displaystr(OggInfoFirstLine + (line++), OggInfoFirstColumn, focus?COLTITLE1H:COLTITLE1, "Ogg tag view - page up/dn to scroll", OggInfoWidth);

	line -= OggInfoScroll;

	if (!ogg_comments_count)
	{
		if (OggInfoHeight > 2)
		{
			displayvoid (OggInfoFirstLine + line, OggInfoFirstColumn, OggInfoWidth);
			line++;
		}

		displaystr (OggInfoFirstLine + line, OggInfoFirstColumn, 0x07, "     No information to display", OggInfoWidth);
		line++;
	} else {
		int i, j;

		for (i=0; i < ogg_comments_count; i++)
		{
			for (j=0; j < ogg_comments[i]->value_count; j++)
			{
				if ((line >= 0) && (line < OggInfoHeight))
				{
					if (j == 0)
					{
						displaystr  (OggInfoFirstLine + line, OggInfoFirstColumn,                                   0x07, ogg_comments[i]->title,                      strlen (ogg_comments[i]->title));
						displaystr  (OggInfoFirstLine + line, OggInfoFirstColumn + strlen (ogg_comments[i]->title), 0x07,                    ":", OggInfoWidestTitle - strlen (ogg_comments[i]->title) + 2);
					} else {
						displayvoid (OggInfoFirstLine + line, OggInfoFirstColumn, OggInfoWidestTitle + 2);
					}
					displaystr_utf8 (OggInfoFirstLine + line, OggInfoFirstColumn + OggInfoWidestTitle + 2, 0x09, ogg_comments[i]->value[j], OggInfoWidth - OggInfoWidestTitle - 2);
				}
				line++;
			}
		}
	}

	while (line < OggInfoHeight)
	{
		displayvoid (OggInfoFirstLine + line, OggInfoFirstColumn, OggInfoWidth);
		line++;
	}
}

static int OggInfoIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('i', "Enable Ogg info viewer");
			cpiKeyHelp('I', "Enable Ogg info viewer");
			break;
		case 'i': case 'I':
			if (!OggInfoActive)
			{
				OggInfoActive=1;
			}
			cpiTextSetMode("ogginfo");
			return 1;
		case 'x': case 'X':
			OggInfoActive=3;
			break;
		case KEY_ALT_X:
			OggInfoActive=2;
			break;
	}
	return 0;
}

static int OggInfoAProcessKey(uint16_t key)
{
	switch (key)
	{
		case 'i': case 'I':
			OggInfoActive=(OggInfoActive+1)%4;
			if ((OggInfoActive==3)&&(plScrWidth<132))
			{
				OggInfoActive=0;
			}
			cpiTextRecalc();
			break;

		case KEY_ALT_K:
			cpiKeyHelp('i',       "Disable Ogg info viewer");
			cpiKeyHelp('I',       "Disable Ogg info viewer");
			cpiKeyHelp(KEY_PPAGE, "Scroll Ogg info viewer up");
			cpiKeyHelp(KEY_NPAGE, "Scroll Ogg info viewer down");
			cpiKeyHelp(KEY_HOME,  "Scroll Ogg info viewer to the top");
			cpiKeyHelp(KEY_END,   "Scroll Ogg info viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (OggInfoScroll)
			{
				OggInfoScroll--;
			}
			break;
		case KEY_NPAGE:
			OggInfoScroll++;
			break;
		case KEY_HOME:
			OggInfoScroll=0;
		case KEY_END:
			OggInfoScroll=OggInfoDesiredHeight - OggInfoHeight;
			break;
		default:
			return 0;
	}
	return 1;
}


static int OggInfoEvent(int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			OggInfoActive=2;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}


static struct cpitextmoderegstruct cpiOggInfo = {"ogginfo", OggInfoGetWin, OggInfoSetWin, OggInfoDraw, OggInfoIProcessKey, OggInfoAProcessKey, OggInfoEvent CPITEXTMODEREGSTRUCT_TAIL};

void __attribute__ ((visibility ("internal"))) OggInfoInit (void)
{
	//cpiTextRegisterDefMode(&cpiOggInfo);
	cpiTextRegisterMode(&cpiOggInfo);
}

void __attribute__ ((visibility ("internal"))) OggInfoDone (void)
{
	//cpiTextUnregisterDefMode(&cpiOggInfo);
	cpiTextUnregisterMode(&cpiOggInfo);
}
