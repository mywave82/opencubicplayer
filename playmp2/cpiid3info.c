/* OpenCP Module Player
 * copyright (c) 2020-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Display ID3 TAG text info
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
#include "id3.h"
#include "mpplay.h"

static int ID3InfoActive;

static int ID3InfoFirstColumn;
static int ID3InfoFirstLine;
static int ID3InfoHeight;
static int ID3InfoWidth;
static int ID3InfoDesiredHeight;
static int ID3InfoScroll;

static int ID3InfoBiggestHeight;
static int ID3InfoNeedRecalc;

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

#if 0

HEADER

 No ID3 information to display

or..

HEADER
Content Group: Piano Concert no5
Track Title:   Adagio II
Subtitle:      Spring
Lead Artist:   Mister Muscle
Band:          National gangsters
Conductor:     Mister R
Interpretor:   DJ cool
Album:         Spring collection
Composer:      Old man
Lyrics:        Young man
Track number:  4/18
Content Type:  Classic Music, House
Recorded:      2020-04-15
Released:      2020-04-16
Comment:       This was super fun

#endif

static void Update_ID3infoLastHeightNeed(struct ID3_t *ID3)
{
	int needed = 1;
	if (ID3->TIT1) needed++;
	if (ID3->TIT2) needed++;
	if (ID3->TIT3) needed++;
	if (ID3->TPE1) needed++;
	if (ID3->TPE2) needed++;
	if (ID3->TPE3) needed++;
	if (ID3->TPE4) needed++;
	if (ID3->TALB) needed++;
	if (ID3->TCOM) needed++;
	if (ID3->TEXT) needed++;
	if (ID3->TRCK) needed++;
	if (ID3->TCON) needed++;
	if (ID3->TDRC || ID3->TYER) needed++;
	if (ID3->TDRL) needed++;
	if (ID3->COMM) needed++;
	if (ID3InfoBiggestHeight < 2)
	{
		ID3InfoBiggestHeight = 2;
	}
	if (ID3InfoBiggestHeight < needed)
	{
		ID3InfoBiggestHeight = needed;
		ID3InfoNeedRecalc = 1;
	}
	ID3InfoDesiredHeight = needed;
}

static void ID3InfoSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	ID3InfoFirstColumn=xpos;
	ID3InfoFirstLine=ypos;
	ID3InfoHeight=hgt;
	ID3InfoWidth=wid;
}

static int ID3InfoGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	struct ID3_t *ID3;

	if (!ID3InfoActive)
		return 0;

	if ((ID3InfoActive==3) && (cpifaceSession->console->TextWidth < 132))
	{
		ID3InfoActive=0;
	}

	if (!ID3InfoActive)
	{
		return 0;
	}

	mpegGetID3(&ID3);
	Update_ID3infoLastHeightNeed(ID3);

	switch (ID3InfoActive)
	{
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

	q->hgtmin=3;
	q->hgtmax = ID3InfoBiggestHeight;
	q->killprio=64;
	q->viewprio=110;

	ID3InfoNeedRecalc = 0;
	return 1;
}

static void ID3InfoDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	int line = 0;
	char StringBuffer[64*3];
	struct ID3_t *ID3;

	mpegGetID3(&ID3);
	Update_ID3infoLastHeightNeed(ID3);

	while (ID3InfoScroll && ((ID3InfoScroll + ID3InfoHeight) > ID3InfoDesiredHeight))
	{
		ID3InfoScroll--;
	}

	cpifaceSession->console->Driver->DisplayStr(ID3InfoFirstLine + (line++), ID3InfoFirstColumn, focus?COLTITLE1H:COLTITLE1, "MPx ID3 tag view - page up/dn to scroll", ID3InfoWidth);

	line -= ID3InfoScroll;

	if (ID3InfoDesiredHeight < 2)
	{
		if (ID3InfoHeight > 2)
		{
			cpifaceSession->console->Driver->DisplayVoid (ID3InfoFirstLine + line, ID3InfoFirstColumn, ID3InfoWidth);
			line++;
		}

		cpifaceSession->console->Driver->DisplayStr (ID3InfoFirstLine + line, ID3InfoFirstColumn, 0x07, "     No ID3 information to display", ID3InfoWidth);
		line++;
	} else {
		if (ID3->TIT1)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Content Group: ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TIT1, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TIT2)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Track Title:   ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TIT2, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TIT3)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Subtitle:      ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TIT3, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TPE1)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Lead Artist:   ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TPE1, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TPE2)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Group:         ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TPE2, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TPE3)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Conductor:     ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TPE3, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TPE4)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Interpreted by:", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TPE4, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TALB)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Album:         ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TALB, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TCOM)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Composer:      ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TCOM, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TRCK)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Track Number:  ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TRCK, ID3InfoWidth-15);
			}
			line++;
		}

	#warning This needs better support!
		if (ID3->TCON)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Content Type:  ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TCON, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->TDRC || ID3->TYER)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn, 0x07, "Recorded:      ", 15);
				if (ID3->TDRC)
				{
					cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TDRC, ID3InfoWidth-15);
				} else {
					if (ID3->TDAT)
					{
						if (ID3->TIME)
						{
							snprintf (StringBuffer, sizeof (StringBuffer), "%s-%s-%s", ID3->TYER, ID3->TDAT, ID3->TIME);
						} else {
							snprintf (StringBuffer, sizeof (StringBuffer), "%s-%s", ID3->TYER, ID3->TDAT);
						}
						cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, StringBuffer, ID3InfoWidth-15);
					} else {
						cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TYER, ID3InfoWidth-15);
					}
				}
			}
			line++;
		}

		if (ID3->TDRL)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Released:      ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->TDRL, ID3InfoWidth-15);
			}
			line++;
		}

		if (ID3->COMM)
		{
			if ((line >= 0) && (line < ID3InfoHeight))
			{
				cpifaceSession->console->Driver->DisplayStr     (ID3InfoFirstLine + line, ID3InfoFirstColumn,      0x07, "Comment:       ", 15);
				cpifaceSession->console->Driver->DisplayStr_utf8(ID3InfoFirstLine + line, ID3InfoFirstColumn + 15, 0x09, (char *)ID3->COMM, ID3InfoWidth-15);
			}
			line++;
		}
	}

	while (line < ID3InfoHeight)
	{
		cpifaceSession->console->Driver->DisplayVoid (ID3InfoFirstLine + line, ID3InfoFirstColumn, ID3InfoWidth);
		line++;
	}
}

static int ID3InfoIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i', "Enable ID3 info viewer");
			cpifaceSession->KeyHelp ('I', "Enable ID3 info viewer");
			break;
		case 'i': case 'I':
			if (!ID3InfoActive)
			{
				ID3InfoActive=1;
			}
			cpifaceSession->cpiTextSetMode (cpifaceSession, "id3info");
			return 1;
		case 'x': case 'X':
			ID3InfoActive=1;
			break;
		case KEY_ALT_X:
			ID3InfoActive=0;
			break;
	}
	return 0;
}

static int ID3InfoAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case 'i': case 'I':
			ID3InfoActive=(ID3InfoActive+1)%4;
			if ((ID3InfoActive==3) && (cpifaceSession->console->TextWidth < 132))
			{
				ID3InfoActive=0;
			}
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;

		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i',       "Disable ID3 info viewer");
			cpifaceSession->KeyHelp ('I',       "Disable ID3 info viewer");
			cpifaceSession->KeyHelp (KEY_PPAGE, "Scroll ID3 info viewer up");
			cpifaceSession->KeyHelp (KEY_NPAGE, "Scroll ID3 info viewer down");
			cpifaceSession->KeyHelp (KEY_HOME,  "Scroll ID3 info viewer to the top");
			cpifaceSession->KeyHelp (KEY_END,   "Scroll ID3 info viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (ID3InfoScroll)
			{
				ID3InfoScroll--;
			}
			break;
		case KEY_NPAGE:
			ID3InfoScroll++;
			break;
		case KEY_HOME:
			ID3InfoScroll=0;
		case KEY_END:
			ID3InfoScroll=ID3InfoDesiredHeight - ID3InfoHeight;
			break;
		default:
			return 0;
	}
	return 1;
}


static int ID3InfoEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievKeepalive:
			if (ID3InfoNeedRecalc)
			{
				if (ID3InfoActive)
				{
					cpifaceSession->cpiTextRecalc (cpifaceSession);
				}
				ID3InfoNeedRecalc = 0;
			}
			break;
		case cpievInitAll:
			return 1;
		case cpievInit:
			ID3InfoActive=1;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}


static struct cpitextmoderegstruct cpiID3Info = {"id3info", ID3InfoGetWin, ID3InfoSetWin, ID3InfoDraw, ID3InfoIProcessKey, ID3InfoAProcessKey, ID3InfoEvent CPITEXTMODEREGSTRUCT_TAIL};

OCP_INTERNAL void ID3InfoInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiID3Info);
}

OCP_INTERNAL void ID3InfoDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiID3Info);
}
