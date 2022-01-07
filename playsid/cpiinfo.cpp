/* OpenCP Module Player
 * copyright (c) 2020-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Display information about the current SID/C64 emulation setup.
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

extern "C"
{
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "stuff/latin1.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"
}
#include "sidplay.h"

static int SidInfoActive;

static int SidInfoFirstLine;
static int SidInfoHeight;
static int SidInfoWidth;
static int SidInfoDesiredHeight;
static int SidInfoScroll;

static char SidUTF8Buffer[CONSOLE_MAX_X*2+1];

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

#if 0

HEADER
+------------------------------------------------------+ 0
|  SIDPLAYFP - Music Player and C64 SID Chip Emulator  | 1
|        Libsidplayfp V2.1.0 + OCP patches             | 2
+------------------------------------------------------+ 3
| Title        : Ocean Loader v2                       | 4  Always present (info count grabs it)
| Author       : Martin Galway                         | 5  Optional
| Released     : 1986 Ocean                            | 6  Optional
| Comment      : Foo                                   | 7  Optional
| Comment      : Bar                                   | 8  Optional
| Comment      : Test                                  | 9  Optional
+------------------------------------------------------+ 10
| Filename     : test.sid                              | TODO TODO TODO TODO
| File format  : PlaySID one-file format (PSID)        | 12
| Condition    : No errors                             | 13
| Song Speed   : PAL (50Hz)                            | 14
+------------------------------------------------------+ 15
| Kernal ROM   : None - Some tunes may not play!       | 16
| BASIC ROM    : None - Basic tunes will not play!     | 17
| Chargen ROM  : None                                  | 18
+------------------------------------------------------+ 19
| CPU speed    : 985248.611111 Hz                      | 20
| VIC-II Model : MOS6569 PAL-B                         | 21
| CIA x2 Model : MOS6526                               | 22
| SID[1] Model : MOS8580 $d400                         | 23
| SID[2] Model : MOS8580 $d420                         | 24
| SID[3] Model : MOS8580 $d440                         | 25
+------------------------------------------------------+ 26

#endif

static void SidInfoSetWin(int _ignore, int wid, int ypos, int hgt)
{
	SidInfoFirstLine=ypos;
	SidInfoHeight=hgt;
	SidInfoWidth=wid;
}

static int SidInfoGetWin(struct cpitextmodequerystruct *q)
{
	if (!SidInfoActive)
		return 0;
	q->hgtmin=3;
	q->hgtmax = SidInfoDesiredHeight = 17 + sidNumberOfChips() + sidNumberOfComments() + sidNumberOfInfos();
	q->xmode=1;
	q->size=2;
	q->top=0;
	q->killprio=64;
	q->viewprio=160;
	return 1;
}

static void SidInfoDraw(int focus)
{
	char LineBuffer[CONSOLE_MAX_X+1];
	int i;
	int line = 0;
	const char *temp;
	char StringBuffer[64];
	const char *tmp;

	while (SidInfoScroll && ((SidInfoScroll + SidInfoHeight) > SidInfoDesiredHeight))
	{
		SidInfoScroll--;
	}

	LineBuffer[0] = ' ';
	for (i=2; i < SidInfoWidth-2; i++)
	{
		LineBuffer[i] = '\xc4';
	}
	LineBuffer[SidInfoWidth-1] = ' ';
	LineBuffer[SidInfoWidth] = 0;

	displaystr(SidInfoFirstLine + (line++), 0, focus?COLTITLE1H:COLTITLE1, "libsidplayfp info view - page up/dn to scroll", SidInfoWidth);

	LineBuffer[1] = '\xda';
	LineBuffer[SidInfoWidth-2] = '\xbf';

	line -= SidInfoScroll;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, LineBuffer, SidInfoWidth);
	}
	line++;

	LineBuffer[1] = '\xc3';
	LineBuffer[SidInfoWidth-2] = '\xb4';

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line,  0, 0x07, " \xb3  ", 4);
		displaystr(SidInfoFirstLine + line,  4, 0x03, "OpenCubicPlayer ", 16);
		displaystr(SidInfoFirstLine + line, 20, 0x07,                 "+ ", 2);
		displaystr(SidInfoFirstLine + line, 22, 0x0a,                   "LIB", 3);
		displaystr(SidInfoFirstLine + line, 25, 0x0c,                      "SID", 3);
		displaystr(SidInfoFirstLine + line, 28, 0x09,                         "PLAY ", 5);
		displaystr(SidInfoFirstLine + line, 33, 0x07,                              "- Music Player and C64 SID Chip Emulator", SidInfoWidth - 33 - 2);
		displaystr(SidInfoFirstLine + line, SidInfoWidth-2, 0x07, "\xb3", 1);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line,  0, 0x07, " \xb3        Libsidplayfp V2.1.0 + OCP patches", SidInfoWidth-2);
		displaystr(SidInfoFirstLine + line, SidInfoWidth-2, 0x07, "\xb3", 1);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, LineBuffer, SidInfoWidth);
	}

	for (i=0; i < sidNumberOfInfos(); i++)
	{
		if ((line >= 1) && (line < SidInfoHeight))
		{
			displaystr(SidInfoFirstLine + line, 0, 0x07, " \xb3 ", 3);
			switch (i)
			{
				case 0:  displaystr(SidInfoFirstLine + line, 3, 0x0b, "Title        : ", 15); break;
				case 1:  displaystr(SidInfoFirstLine + line, 3, 0x0b, "Author       : ", 15); break;
				case 2:  displaystr(SidInfoFirstLine + line, 3, 0x0b, "Released     : ", 15); break;
				default: displaystr(SidInfoFirstLine + line, 3, 0x0b, "(info)       : ", 15); break;
			}
			tmp = sidInfoString(i);
			latin1_f_to_utf8_z (tmp, strlen (tmp), SidUTF8Buffer, sizeof (SidUTF8Buffer));
			displaystr_utf8(SidInfoFirstLine + line, 18, 0x05, SidUTF8Buffer, SidInfoWidth - 18 - 2);
			displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
		}
		line++;
	}

	for (i=0; i < sidNumberOfComments(); i++)
	{
		if ((line >= 1) && (line < SidInfoHeight))
		{
			displaystr              (SidInfoFirstLine + line,  0, 0x07, " \xb3 ", 3);
			displaystr              (SidInfoFirstLine + line,  3, 0x0b, "Comment      : ", 15);
			tmp = sidCommentString(i);
			latin1_f_to_utf8_z (tmp, strlen (tmp), SidUTF8Buffer, sizeof (SidUTF8Buffer));
			displaystr_utf8(SidInfoFirstLine + line, 18, 0x05, SidUTF8Buffer, SidInfoWidth - 18 - 2);
			displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
		}
		line++;
	}

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, LineBuffer, SidInfoWidth);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line,  0, 0x07, " \xb3 ", 3);
		displaystr(SidInfoFirstLine + line,  3, 0x0a, "File format  : ", 15);
		displaystr(SidInfoFirstLine + line, 18, 0x0f, sidFormatString(), SidInfoWidth - 18 - 2);
		displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line,  0, 0x07, " \xb3 ", 3);
		displaystr(SidInfoFirstLine + line,  3, 0x0a, "Condition    : ", 15);
		displaystr(SidInfoFirstLine + line, 18, 0x0f, sidTuneStatusString(), SidInfoWidth - 18 - 2);
		displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line,  0, 0x07, " \xb3 ", 3);
		displaystr(SidInfoFirstLine + line,  3, 0x0a, "Song Speed   : ", 15);
		displaystr(SidInfoFirstLine + line, 18, 0x0f, sidTuneInfoClockSpeedString(), SidInfoWidth - 18 - 2);
		displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, LineBuffer, SidInfoWidth);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, " \xb3 ", 3);
		displaystr(SidInfoFirstLine + line, 3, 0x05, "Kernal ROM   : ", 15);
		temp = sidROMDescKernal();
		if (temp[0])
		{
			displaystr(SidInfoFirstLine + line, 18, 0x07, temp, SidInfoWidth - 18 - 2);
		} else {
			displaystr(SidInfoFirstLine + line, 18, 0x04, "None - Some tunes may not play!", SidInfoWidth - 18 - 2);
		}
		displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, " \xb3 ", 3);
		displaystr(SidInfoFirstLine + line, 3, 0x05, "BASIC ROM    : ", 15);
		temp = sidROMDescBasic();
		if (temp[0])
		{
			displaystr(SidInfoFirstLine + line, 18, 0x07, temp, SidInfoWidth - 18 - 2);
		} else {
			displaystr(SidInfoFirstLine + line, 18, 0x04, "None - Basic tunes will not play!", SidInfoWidth - 18 - 2);
		}
		displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, " \xb3 ", 3);
		displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
		displaystr(SidInfoFirstLine + line, 3, 0x05, "Chargen ROM  : ", 15);
		temp = sidROMDescChargen();
		if (temp[0])
		{
			displaystr(SidInfoFirstLine + line, 18, 0x07, temp, SidInfoWidth - 18 - 2);
		} else {
			displaystr(SidInfoFirstLine + line, 18, 0x04, "None", SidInfoWidth - 18 - 2);
		}
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, LineBuffer, SidInfoWidth);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, " \xb3 ", 3);
		displaystr(SidInfoFirstLine + line, 3, 0x03, "CPU speed    : ", 15);
		snprintf (StringBuffer, sizeof (StringBuffer), "%.3f MHz", sidGetCPUSpeed()/1000000.0f);
		displaystr(SidInfoFirstLine + line, 18, 0x06, StringBuffer, SidInfoWidth - 18 - 2);
		displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, " \xb3 ", 3);
		displaystr(SidInfoFirstLine + line, 3, 0x03, "VIC-II Model : ", 15);
		displaystr(SidInfoFirstLine + line, 18, 0x06, sidGetVICIIModelString(), SidInfoWidth - 18 - 2);
		displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
	}
	line++;

	if ((line >= 1) && (line < SidInfoHeight))
	{
		displaystr(SidInfoFirstLine + line, 0, 0x07, " \xb3 ", 3);
		displaystr(SidInfoFirstLine + line, 3, 0x03, "CIA x2 Model : ", 15);
		displaystr(SidInfoFirstLine + line, 18, 0x06, sidGetCIAModelString(), SidInfoWidth - 18 - 2);
		displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
	}
	line++;

	for (i=0; i<sidNumberOfChips(); i++)
	{
		if ((line >= 1) && (line < SidInfoHeight))
		{
			displaystr(SidInfoFirstLine + line, 0, 0x07, " \xb3 ", 3);
			snprintf (StringBuffer, sizeof (StringBuffer), "SID[%d] Model : ", i+1);
			displaystr(SidInfoFirstLine + line, 3, 0x03, StringBuffer, 15);
			snprintf (StringBuffer, sizeof (StringBuffer), "%s $%04x", sidChipModel(i), sidChipAddr(i));
			displaystr(SidInfoFirstLine + line, 18, 0x06, StringBuffer, SidInfoWidth - 18 - 2);
			displaystr(SidInfoFirstLine + line, SidInfoWidth - 2, 0x07, "\xb3", 1);
		}
		line++;
	}

	if ((line >= 1) && (line < SidInfoHeight))
	{
		LineBuffer[1] = '\xc0';
		LineBuffer[SidInfoWidth-2] = '\xd9';
		displaystr(SidInfoFirstLine + line, 0, 0x07, LineBuffer, SidInfoWidth);
	}
	line++;
}

static int SidInfoIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('t', "Enable SID info viewer");
			cpiKeyHelp('T', "Enable SID info viewer");
			break;
		case 't': case 'T':
			SidInfoActive=1;
			cpiTextSetMode("sidinfo");
			return 1;
		case 'x': case 'X':
			SidInfoActive=1;
			break;
		case KEY_ALT_X:
			SidInfoActive=0;
			break;
	}
	return 0;
}

static int SidInfoAProcessKey(uint16_t key)
{
	switch (key)
	{
		case 't': case 'T':
			SidInfoActive=!SidInfoActive;
			cpiTextRecalc();
			break;

		case KEY_ALT_K:
			cpiKeyHelp('t',       "Disable SID info viewer");
			cpiKeyHelp('T',       "Disable SID info viewer");
			cpiKeyHelp(KEY_PPAGE, "Scroll SID info viewer up");
			cpiKeyHelp(KEY_NPAGE, "Scroll SID info viewer down");
			cpiKeyHelp(KEY_HOME,  "Scroll SID info viewer to the top");
			cpiKeyHelp(KEY_END,   "Scroll SID info viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (SidInfoScroll)
			{
				SidInfoScroll--;
			}
			break;
		case KEY_NPAGE:
			SidInfoScroll++;
			break;
		case KEY_HOME:
			SidInfoScroll=0;
		case KEY_END:
			SidInfoScroll=SidInfoDesiredHeight - SidInfoHeight;
			break;
		default:
			return 0;
	}
	return 1;
}


static int SidInfoEvent(int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			SidInfoActive=1;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}


static struct cpitextmoderegstruct cpiSidInfo = {"sidinfo", SidInfoGetWin, SidInfoSetWin, SidInfoDraw, SidInfoIProcessKey, SidInfoAProcessKey, SidInfoEvent CPITEXTMODEREGSTRUCT_TAIL};

void __attribute__ ((visibility ("internal"))) SidInfoInit (void)
{
	//cpiTextRegisterDefMode(&cpiSidInfo);
	cpiTextRegisterMode(&cpiSidInfo);
}

void __attribute__ ((visibility ("internal"))) SidInfoDone (void)
{
	//cpiTextUnregisterDefMode(&cpiSidInfo);
}
