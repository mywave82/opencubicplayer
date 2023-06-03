/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Display information about the current GME setup.
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
#include <gme/gme.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "playgme/gmeplay.h"
#include "stuff/poutput.h"

static int gmeInfoActive;

static int gmeInfoFirstLine;
static int gmeInfoHeight;
static int gmeInfoWidth;
static int gmeInfoDesiredHeight;
static int gmeInfoScroll;

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

#if 0

HEADER
+------------------------------------------------------+ 0
|  Game Music Emulator, compiled against version 0.6.4 | 1
+------------------------------------------------------+ 3
| System      :                                        | 4 Optional
| Game        :                                        | 5 Optional
| Song        :                                        | 6 Optional
| Author      :                                        | 7 Optional
| Copyright   :                                        | 8 Optional
| Comment     :                                        | 9 Optional
| Dumper      :                                        | 10 Optional
| Length      : 0:12.123                               | 11 Optional
| Intro       : 0.12.123                               | 12 Optional
| Loop        : 0.12.123                               | 13 Optional
| Play-Length : 0.12.123                               | 14 Optional
+------------------------------------------------------+ 15
| Filename    : test.gym                               | 16
| File format : GYM sdfsdfsdafsdf                      | 17
+------------------------------------------------------+ 18

#endif

static void gmeInfoSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int _ignore, int wid, int ypos, int hgt)
{
	gmeInfoFirstLine=ypos;
	gmeInfoHeight=hgt;
	gmeInfoWidth=wid;
}

static int gmeInfoGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	if (!gmeInfoActive)
		return 0;
	q->hgtmin=6;
	q->hgtmax = gmeInfoDesiredHeight = 20;
	q->xmode=1;
	q->size=2;
	q->top=0;
	q->killprio=64;
	q->viewprio=160;
	return 1;
}

static void gmeInfoDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	int line = 0;
	struct gmeinfo gmeInfo;

	gmeGetInfo (&gmeInfo);

	while (gmeInfoScroll && ((gmeInfoScroll + gmeInfoHeight) > gmeInfoDesiredHeight))
	{
		gmeInfoScroll--;
	}

	cpifaceSession->console->Driver->DisplayStr(gmeInfoFirstLine + (line++), 0, focus?COLTITLE1H:COLTITLE1, "libgme info view - page up/dn to scroll", gmeInfoWidth);

	line -= gmeInfoScroll;

	if ((line >= 1) && (line < gmeInfoHeight))
	{
		cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xda%*C\xc4\xbf", gmeInfoWidth - 2);
	}
	line++;

	if ((line >= 1) && (line < gmeInfoHeight))
	{
                char version[12];
		snprintf (version, sizeof (version), "%u.%u.%u", (GME_VERSION >> 16) & 255, (GME_VERSION >> 8) & 255, GME_VERSION & 255);
		cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.3o Game Music Emulator, compiled against version %s%*C %.7o\xb3", version, gmeInfoWidth - 49 - strlen (version));
	}
	line++;

	if ((line >= 1) && (line < gmeInfoHeight))
	{
		cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xc3%*C\xc4\xb4", gmeInfoWidth - 2);
	}
  line++;

	if (gmeInfo.system && gmeInfo.system[0])
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o System      : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, gmeInfo.system);
		}
		line++;
	}

	if (gmeInfo.game && gmeInfo.game[0])
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Game        : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, gmeInfo.game);
		}
		line++;
	}

	if (gmeInfo.song && gmeInfo.song[0])
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Song        : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, gmeInfo.song);
		}
		line++;
	}

	if (gmeInfo.author && gmeInfo.author[0])
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Author      : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, gmeInfo.author);
		}
		line++;
	}

	if (gmeInfo.copyright && gmeInfo.copyright[0])
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Copyright   : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, gmeInfo.copyright);
		}
		line++;
	}

	if (gmeInfo.comment && gmeInfo.comment[0])
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Comment     : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, gmeInfo.comment);
		}
		line++;
	}

	if (gmeInfo.dumper && gmeInfo.dumper[0])
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Dumper      : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, gmeInfo.dumper);
		}
		line++;
	}

	if (gmeInfo.length >= 0)
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Length      : %.5o%.01u.%02u:%03u%*C %.7o\xb3", gmeInfo.length / 60000, (gmeInfo.length % 60000) / 1000, gmeInfo.length % 1000, gmeInfoWidth - 25);
		}
		line++;
	}

	if (gmeInfo.introlength >= 0)
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Intro Length: %.5o%.01u.%02u:%03u%*C %.7o\xb3", gmeInfo.introlength / 60000, (gmeInfo.introlength % 60000) / 1000, gmeInfo.introlength % 1000, gmeInfoWidth - 25);
		}
		line++;
	}

	if (gmeInfo.looplength >= 0)
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Loop Length : %.5o%.01u.%02u:%03u%*C %.7o\xb3", gmeInfo.looplength / 60000, (gmeInfo.looplength % 60000) / 1000, gmeInfo.looplength % 1000, gmeInfoWidth - 25);
		}
		line++;
	}

	if (gmeInfo.playlength >= 0)
	{
		if ((line >= 1) && (line < gmeInfoHeight))
		{
			cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Play Length : %.5o%.01u.%02u:%03u%*C %.7o\xb3", gmeInfo.playlength / 60000, (gmeInfo.playlength % 60000) / 1000, gmeInfo.playlength % 1000, gmeInfoWidth - 25);
		}
		line++;
	}

	if ((line >= 1) && (line < gmeInfoHeight))
	{
		cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xc3%*C\xc4\xb4", gmeInfoWidth - 2);
	}
  line++;

	if ((line >= 1) && (line < gmeInfoHeight))
	{
		cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o Filename    : %.5o%*S%.7o\xb3", gmeInfoWidth - 17, gme_filename);
	}
	line++;

	if ((line >= 1) && (line < gmeInfoHeight))
	{
		if (gme_mt.integer.i == MODULETYPE("AY2"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "AY - Z80 + AY-3-8910");
		} else if (gme_mt.integer.i == MODULETYPE("GBS"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "GBS - GameBoy Sound System");
		} else if (gme_mt.integer.i == MODULETYPE("GYM"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "GYM - Genesis YM2612");
		} else if (gme_mt.integer.i == MODULETYPE("HES"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "HES - Hudson Entertainment Sound");
		} else if (gme_mt.integer.i == MODULETYPE("KSS"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "Konami Sound System?");
		} else if (gme_mt.integer.i == MODULETYPE("NSF"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "NSF - Nintendo Sound Format");
		} else if (gme_mt.integer.i == MODULETYPE("NSFe"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "NSF - Nintendo Sound Format extended");
		} else if (gme_mt.integer.i == MODULETYPE("SAP"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "SAP - Slight Atari Player");
		} else if (gme_mt.integer.i == MODULETYPE("SPC"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "SPC - Super Nintento / Famicom SPC-700 co-processor");
		} else if (gme_mt.integer.i == MODULETYPE("VGM"))
		{
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "VGM - Video Game Music");
		} else {
		  cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xb3%.11o File format : %.5o%*s%.7o\xb3", gmeInfoWidth - 17, "Unknown");
		}
	}
	line++;

	if ((line >= 1) && (line < gmeInfoHeight))
	{
		cpifaceSession->console->DisplayPrintf(gmeInfoFirstLine + line, 0, 0x07, gmeInfoWidth, "\xc0%*C\xc4\xd9", gmeInfoWidth - 2);

	}
	line++;

	while (line < gmeInfoHeight)
	{
		cpifaceSession->console->Driver->DisplayVoid(gmeInfoFirstLine + line, 0, gmeInfoWidth);
		line++;
	}
}

static int gmeInfoIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('t', "Enable libGME info viewer");
			cpifaceSession->KeyHelp ('T', "Enable libGME info viewer");
			break;
		case 't': case 'T':
			gmeInfoActive=1;
			cpifaceSession->cpiTextSetMode (cpifaceSession, "gmeinfo");
			return 1;
		case 'x': case 'X':
			gmeInfoActive=1;
			break;
		case KEY_ALT_X:
			gmeInfoActive=0;
			break;
	}
	return 0;
}

static int gmeInfoAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case 't': case 'T':
			gmeInfoActive=!gmeInfoActive;
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;

		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('t',       "Disable libGME info viewer");
			cpifaceSession->KeyHelp ('T',       "Disable libGME info viewer");
			cpifaceSession->KeyHelp (KEY_PPAGE, "Scroll libGME info viewer up");
			cpifaceSession->KeyHelp (KEY_NPAGE, "Scroll libGME info viewer down");
			cpifaceSession->KeyHelp (KEY_HOME,  "Scroll libGME info viewer to the top");
			cpifaceSession->KeyHelp (KEY_END,   "Scroll libGME info viewer to the bottom");
			return 0;

		case KEY_PPAGE:
			if (gmeInfoScroll)
			{
				gmeInfoScroll--;
			}
			break;
		case KEY_NPAGE:
			gmeInfoScroll++;
			break;
		case KEY_HOME:
			gmeInfoScroll=0;
		case KEY_END:
			gmeInfoScroll=gmeInfoDesiredHeight - gmeInfoHeight;
			break;
		default:
			return 0;
	}
	return 1;
}


static int gmeInfoEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			gmeInfoActive=1;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}


static struct cpitextmoderegstruct cpiGmeInfo = {"gmeinfo", gmeInfoGetWin, gmeInfoSetWin, gmeInfoDraw, gmeInfoIProcessKey, gmeInfoAProcessKey, gmeInfoEvent CPITEXTMODEREGSTRUCT_TAIL};

OCP_INTERNAL void gmeInfoInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiGmeInfo);
}

OCP_INTERNAL void gmeInfoDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiGmeInfo);
}
