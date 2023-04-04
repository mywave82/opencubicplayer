/* OpenCP Module Player
 * copyright (c) 2022-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Timidity karaoke viewer
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "cpikaraoke.h"
#include "cpiface/cpiface.h"
#include "stuff/poutput.h"

static unsigned int KaraokeType = 1;
static unsigned int KaraokeColumns = 1;
static unsigned int KaraokeLeft;   /* Last received xpos */
static unsigned int KaraokeWidth;  /* Last received width */
static unsigned int KaraokeTop;    /* Last received ypos */
static unsigned int KaraokeHeight; /* Last received height */
static unsigned int KaraokeTargetLine;
static unsigned int KaraokeTargetSyllable;
static struct lyric_t *KaraokeLyric;

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

int karaoke_new_line (struct lyric_t *lyric)
{
	if ((!lyric->lines) || lyric->line[lyric->lines-1].syllables)
	{
		/* allocate a new line if lyric is empty or the last line is not empty */
		void *temp = realloc (lyric->line, sizeof (lyric->line[0]) * (lyric->lines + 1));
		if (!temp)
		{
			fprintf (stderr, "karaoke_new_line: realloc() failed\n");
			return -1;
		}
		lyric->line = temp;
		bzero (&lyric->line[lyric->lines], sizeof (lyric->line[0]));
		lyric->lines++;
		return 0;
	}
	if (!lyric->line[lyric->lines-1].syllables)
	{ /* two new-lines in a row is a paragraph */
		lyric->line[lyric->lines-1].is_paragraph = 1;
	}
	return 0;
}

int karaoke_new_paragraph (struct lyric_t *lyric)
{
	if (karaoke_new_line (lyric))
	{
		return -1;
	}
	lyric->line[lyric->lines-1].is_paragraph = 1;
	return 0;
}

int karaoke_new_syllable (struct cpifaceSessionAPI_t *cpifaceSession, struct lyric_t *lyric, uint32_t timecode, const char *src, int length)
{
	unsigned int measuredwith = cpifaceSession->console->Driver->MeasureStr_utf8 (src, length);
	void *temp;
	struct line_t *theline;
	if (!lyric->lines)
	{
		if (karaoke_new_line (lyric))
		{
			return -1;
		}
	}
	theline = &lyric->line[lyric->lines-1];
	temp = realloc (theline->syllable, sizeof (theline->syllable[0]) * (theline->syllables + 1));
	if (!temp)
	{
		fprintf (stderr, "karaoke_new_syllable: realloc() failed\n");
		return -1;
	}
	theline->syllable = temp;
	theline->syllable[theline->syllables] = malloc (sizeof (struct syllable_t) + length + 1);
	if (!theline->syllable[theline->syllables])
	{
		fprintf (stderr, "karaoke_new_syllable: malloc() failed\n");
		return -1;
	}
	theline->syllable[theline->syllables]->timecode = timecode;
	theline->syllable[theline->syllables]->measuredwidth = measuredwith;
	memcpy (theline->syllable[theline->syllables]->text, src, length);
	theline->syllable[theline->syllables]->text[length] = 0;
	theline->syllables++;
	theline->measuredwidth += measuredwith;
	return 0;
}

void karaoke_clear (struct lyric_t *lyric)
{
	unsigned int i;
	for (i=0; i < lyric->lines; i++)
	{
		unsigned int j;
		for (j=0; j < lyric->line[i].syllables; j++)
		{
			free (lyric->line[i].syllable[j]);
		}
		free (lyric->line[i].syllable);
	}
	free (lyric->line);
	lyric->lines = 0;
	lyric->line = 0;
}

static unsigned int CalculateColumns (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int TargetWidth)
{
	unsigned int maxwidth = 1;
	unsigned int i;

	for (i=0; i < KaraokeLyric->lines; i++)
	{
		maxwidth = MAX (maxwidth, KaraokeLyric->line[i].measuredwidth);
	}

	if (TargetWidth < (maxwidth * 2 + 2))
	{
		return 1;
	}

	return (TargetWidth + 2) / (maxwidth + 2);
}

static int KaraokeGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	if (!KaraokeLyric)
	{
		return 0;
	}

	if ((KaraokeType == 3) && (cpifaceSession->console->TextWidth < 132))
	{
		KaraokeType = 0;
	}

	switch (KaraokeType)
	{
		case 0: /* not visible */
			return 0;
		case 1: /* if right-columns, then we do the wide left - otherwice we are full width, not priorized for height... multi-columned mode */
			q->hgtmin = 3;
			KaraokeColumns = 1; // This is calculated in KaraokeSetWin
			q->xmode=1;
			break;
		case 2: /* we are always full width priority! */
			KaraokeColumns = 1; // This is calculated in KaraokeSetWin
			q->hgtmin = 3;
			q->xmode = 3;
			break;
		case 3: /* right column, only possible for wide screens, width is fixed at 52 */
			KaraokeColumns = 1;
			q->hgtmin = 5;
			q->xmode=2;
			break;
	}
	q->hgtmax = 1 + (KaraokeLyric->lines + KaraokeColumns - 1) / KaraokeColumns;
	q->hgtmax = MAX (q->hgtmin, q->hgtmax);
	q->size=1;
	q->top=1;
	q->killprio=96;
	q->viewprio=144;
	return 1;
}

static void KaraokeSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int xpos, int wid, int ypos, int hgt)
{
	KaraokeTop = ypos;
	KaraokeHeight = hgt;
	KaraokeWidth = wid;
	KaraokeLeft = xpos;

	if (KaraokeType != 3)
	{
		KaraokeColumns = CalculateColumns (cpifaceSession, KaraokeWidth);
	} else {
		KaraokeColumns = 1;
	}
}

static void KaraokeDrawLine (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int Top, unsigned int Left, unsigned int Width, unsigned int LineNo)
{
	unsigned int y;
	unsigned int i;
	struct line_t *line;


	if (LineNo >= KaraokeLyric->lines)
	{
		cpifaceSession->console->Driver->DisplayVoid (Top, Left, Width);
		return;
	}

	for (y = 0, i = 0, line = KaraokeLyric->line + LineNo;
	    (y < Width) && (i < line->syllables);
	    i++)
	{
		cpifaceSession->console->DisplayPrintf (
			Top,
			Left + y,
			((LineNo == KaraokeTargetLine) && (i == KaraokeTargetSyllable)) ? 0x09 :
			 (LineNo == KaraokeTargetLine) ? 0x0f : 0x07,
			MIN(Width - y, line->syllable[i]->measuredwidth),
			"%S",
			line->syllable[i]->text
		);
		y += line->syllable[i]->measuredwidth;
	}
	if (y < Width)
	{
		cpifaceSession->console->Driver->DisplayVoid (Top, Left + y, Width - y);
	}
}

static void KaraokeDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	unsigned int LyricFullHeight = (KaraokeHeight - 1) * KaraokeColumns;
	unsigned int LyricScroll = 0;
	unsigned int LyricMaxScroll = 0;
	unsigned int u;

	if (KaraokeLyric->lines > LyricFullHeight)
	{
		LyricMaxScroll = LyricFullHeight - KaraokeLyric->lines;
		if (KaraokeTargetLine >= ((KaraokeHeight - 1) / 2))
		{
			LyricScroll = MIN (KaraokeTargetLine - (KaraokeHeight - 1) / 2, LyricMaxScroll);
		}
	}

	cpifaceSession->console->DisplayPrintf (KaraokeTop, KaraokeLeft, focus ? 0x09 : 0x01, KaraokeWidth, " Karaoke Lyrics (k to toggle) - Line %u", KaraokeTargetLine + 1);

	if (KaraokeColumns == 1)
	{
		for (u = 0; u < (KaraokeHeight - 1); u++)
		{
			KaraokeDrawLine (cpifaceSession, KaraokeTop + 1 + u, KaraokeLeft, KaraokeWidth, LyricScroll + u);
		}
	} else {
		unsigned KW = (KaraokeWidth - (KaraokeColumns - 1) * 2) / KaraokeColumns;
		for (u = 0; u < (KaraokeHeight - 1); u++)
		{
			unsigned v;
			for (v = 0; v < KaraokeColumns; v++)
			{
				KaraokeDrawLine (cpifaceSession, KaraokeTop + 1 + u, KaraokeLeft + (KW + 2 ) * v, KW, LyricScroll + v * KaraokeHeight + u);
				if (v != (KaraokeColumns - 1))
				{
					cpifaceSession->console->DisplayPrintf (
						KaraokeTop + 1 + u,
						KaraokeLeft + (KW + 2) * (v + 1) - 2,
						0x07,
						2,
						"| "
					);
				} else {
					cpifaceSession->console->Driver->DisplayVoid (
						KaraokeTop + 1 + u,
						KaraokeLeft + (KW + 2) * (v + 1) - 2,
						KaraokeWidth - ((KW + 2) * (v + 1) - 2)
					);
				}
			}
		}
	}
}

static int KaraokeIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp('k', "Enable karaoke viewer");
			cpifaceSession->KeyHelp('K', "Enable karaoke viewer");
			break;
		case 'k': case 'K':
			if (!KaraokeType)
				KaraokeType = (KaraokeType + 1) % 4;
			cpifaceSession->cpiTextSetMode (cpifaceSession, "karaoke");
			return 1;
		case 'x': case 'X':
			KaraokeType = 3;
			break;
		case KEY_ALT_X:
			KaraokeType = 1;
			break;
	}
	return 0;
}

static int KaraokeAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
/* In the future, we might add support for scrolling on user-demands if they hit space (same style navigation as global tracker viewer) */
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp('k', "Toggle karaoke viewer types");
			cpifaceSession->KeyHelp('K', "Toggle karaoke viewer types");
#if 0
			cpifaceSession->KeyHelp(' ', "Enable lyric manual scroller");
			cpifaceSession->KeyHelp(KEY_PPAGE, "Scroll up in lyric viewer");
			cpifaceSession->KeyHelp(KEY_NPAGE, "Scroll down in lyric viewer");
			cpifaceSession->KeyHelp(KEY_HOME, "Scroll to to the first line in lyric viewer");
			cpifaceSession->KeyHelp(KEY_END, "Scroll to to the last line in lyric viewer");
			cpifaceSession->KeyHelp(KEY_CTRL_PGUP, "Scroll up a page in the lyric viewer");
			cpifaceSession->KeyHelp(KEY_CTRL_PGDN, "Scroll down a page in the lyric viewer");
#endif
			return 0;
		case 'k': case 'K':
			KaraokeType = (KaraokeType + 1) % 4;
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;
#if 0
		case ' ':
		case KEY_PPAGE:
		case KEY_NPAGE:
		case KEY_CTRL_PGUP:
		case KEY_CTRL_PGDN:
		case KEY_HOME:
		case KEY_END:
#endif
		default:
		return 0;
	}
	return 1;
}

static int KaraokeEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	return 1;
}

static struct cpitextmoderegstruct cpiTKaraoke = {"karaoke", KaraokeGetWin, KaraokeSetWin, KaraokeDraw, KaraokeIProcessKey, KaraokeAProcessKey, KaraokeEvent CPITEXTMODEREGSTRUCT_TAIL};

void cpiKaraokeInit (struct cpifaceSessionAPI_t *cpifaceSession, struct lyric_t *lyric)
{
	KaraokeTargetSyllable = 0;
	KaraokeTargetLine = 0;
	KaraokeLyric = lyric;
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiTKaraoke);
}

void cpiKaraokeDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (KaraokeLyric)
	{
		cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiTKaraoke);
		KaraokeLyric = 0;
	}
}

void __attribute__ ((visibility ("internal"))) cpiKaraokeSetTimeCode (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t timecode)
{
	unsigned int u;
	if (!KaraokeLyric)
	{
		return;
	}

	KaraokeTargetLine = 0;
	KaraokeTargetSyllable = 0x7fffffff;

	for (u = 0; u < KaraokeLyric->lines; u++)
	{
		unsigned int v;
		for (v = 0; v < KaraokeLyric->line[u].syllables; v++)
		{
			if (timecode >= KaraokeLyric->line[u].syllable[v]->timecode)
			{
				KaraokeTargetLine = u;
				KaraokeTargetSyllable = v;
			}
			if (timecode == KaraokeLyric->line[u].syllable[v]->timecode)
			{
				return;
			}
		}
	}
}
