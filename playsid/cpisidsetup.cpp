/* OpenCP Module Player
 * copyright (c) 2021-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * SID audio setup/viewer
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

extern "C" {
#include "config.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "stuff/compat.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"
}
#include "cpisidsetup.h"
#include "sidplay.h"

#define COLTITLE1 0x01
#define COLTITLE1H 0x09

static int SidSetupActive;

static int SidSetupFirstLine;
static int SidSetupHeight;
static int SidSetupWidth;
//static int SidSetupHeight;

static int SidSetupEditPos = 0;

static int SidSetupUseresidfp = 0; // true for residfp, false for residfpII
static int SidSetupFilter = 0;
static int SidSetupFilterCurve6581 = 0;
static int SidSetupFilterRange6581 = 0;
static int SidSetupFilterCurve8580 = 0;
static int SidSetupCombinedWaveformsStrength = 0;

static void SidSetupSetWin (struct cpifaceSessionAPI_t *cpifaceSession, int _ignore, int wid, int ypos, int hgt)
{
	SidSetupFirstLine=ypos;
	SidSetupHeight=hgt;
	SidSetupWidth=wid;
}

static int SidSetupGetWin (struct cpifaceSessionAPI_t *cpifaceSession, struct cpitextmodequerystruct *q)
{
	if (!SidSetupActive)
	{
		return 0;
	}
	q->hgtmin = 6;
	q->hgtmax = 6;
	q->xmode = 1;
	q->size = 2;
	q->top = 0;
	q->killprio = 64;
	q->viewprio = 160;
	return 1;
}

static void SidSetupDrawItems (struct cpifaceSessionAPI_t *cpifaceSession, const int focus, const int lineno, const int skip, const char **list, const int listlength, const int selected, const int active, const int disabled)
{
	int xpos = 27 + skip;
	int i;

	if (disabled)
	{
		cpifaceSession->console->Driver->DisplayStr (SidSetupFirstLine + lineno, 27 + skip, 0x08, "  ----", SidSetupWidth - 27 - skip);
		return;
	}

	for (i=0; i < listlength; i++)
	{
		int l = strlen (list[i]);
		if (selected == i)
		{
			cpifaceSession->console->DisplayPrintf (SidSetupFirstLine + lineno, xpos, (focus&&active)?0x09:0x01, l + 2, "[%.*o%s%.*o]", (focus&&active)?0x0f:0x07, list[i], (focus&&active)?0x09:0x01);
		} else {
			cpifaceSession->console->DisplayPrintf (SidSetupFirstLine + lineno, xpos, 0x00, l + 2, " %.*o%s%.0o ", (focus&&active)?0x07:0x08, list[i]);
		}
		xpos += l + 2;
	}
	cpifaceSession->console->Driver->DisplayStr (SidSetupFirstLine + lineno, xpos, 0x00, " ", SidSetupWidth - xpos);
}

static void SidSetupDrawBar (struct cpifaceSessionAPI_t *cpifaceSession, const int focus, const int lineno, const int skip, int scale, const char *suffix, int minlevel, int maxlevel, int level, const int active, const int disabled)
{
	char prefix[11];
	char min[8];
	char max[7];

	unsigned int abslevel;
	unsigned int absmin;

	int pos, p1, p2;

	assert ((scale == 10) || (scale == 100));

	if (disabled)
	{
		cpifaceSession->console->Driver->DisplayStr (SidSetupFirstLine + lineno, 27 + skip, 0x08, "  ----", SidSetupWidth - 27 - skip);
		return;
	}

	if (scale == 100)
	{
		level    = saturate (level,    -99999, 99999);
		minlevel = saturate (minlevel, -99999, 99999);
		maxlevel = saturate (maxlevel,      0, 99999);
		abslevel = abs(level);
		absmin   = abs(minlevel);

		snprintf (prefix, sizeof (prefix), "%3d.%02d%s",
			level / scale,
			abslevel % scale,
			suffix);

		snprintf (min, sizeof (min), "%3d.%02d",
			minlevel / scale,
			absmin % scale);

		snprintf (max, sizeof (max), "%3d.%02d",
			maxlevel / scale,
			maxlevel % scale);
	} else {
		level    = saturate (level,    -9999, 9999);
		minlevel = saturate (minlevel, -9999, 9999);
		maxlevel = saturate (maxlevel,     0, 9999);
		abslevel = abs(level);
		absmin   = abs(minlevel);

		snprintf (prefix, sizeof (prefix), "%4d.%01d%s",
			level / scale,
			abslevel % scale,
			suffix);

		snprintf (min, sizeof (min), "%4d.%01d",
			minlevel / scale,
			absmin % scale);

		snprintf (max, sizeof (max), "%4d.%01d",
			maxlevel / scale,
			maxlevel % scale);
	}
#define _22 (SidSetupWidth - 27 - skip - 28)
	pos = (/*(maxlevel - minlevel / 46) - 1 +*/ (level - minlevel) * _22) / (maxlevel - minlevel);

	p1 = pos;
        p2 = _22 - pos;

	cpifaceSession->console->DisplayPrintf (SidSetupFirstLine + lineno, 27 + skip, (active)?0x07:0x08, SidSetupWidth - 27 - skip, "%10s%-7s [%*C.#%*C.] %-6s", prefix, min, p1, p2, max);
#undef _22
}


static void SidSetupDraw (struct cpifaceSessionAPI_t *cpifaceSession, int focus)
{
	const char *offon[] = {"off", "on"};
	const char *combinedwaveforms[] = {"Average", "Weak", "Strong" };
	int skip;

	if (SidSetupWidth >= 83)
	{
		skip = 2;
	} else if (SidSetupWidth >= 81)
	{
		skip = 1;
	} else {
		skip = 0;
	}

	cpifaceSession->console->Driver->DisplayStr (SidSetupFirstLine, 0, focus?COLTITLE1H:COLTITLE1, focus?" Sid Setup":" Sid Setup (press i to focus)", SidSetupWidth);

	cpifaceSession->console->Driver->DisplayStr (SidSetupFirstLine + 1, 0, (focus&&(SidSetupEditPos==0))?0x07:0x08, &"  Filter:"[2 - skip], 27 + skip);
	SidSetupDrawItems (cpifaceSession, focus, 1, skip, offon, 2, SidSetupFilter, SidSetupEditPos==0, 0);

	cpifaceSession->console->Driver->DisplayStr (SidSetupFirstLine + 2, 0, (focus&&(SidSetupEditPos==1))?0x07:0x08, &"  FilterCurve6581:"[2 - skip], 27 + skip);
	SidSetupDrawBar (cpifaceSession, focus, 2, skip, 100, "", 0, 100, SidSetupFilterCurve6581, SidSetupEditPos == 1, (SidSetupFilter == 0) || strcmp (sidChipModel(0), "MOS6581") /* || (SidSetupUseresidfp == 0)*/);

	cpifaceSession->console->Driver->DisplayStr (SidSetupFirstLine + 3, 0, (focus&&(SidSetupEditPos==2))?0x07:0x08, &"  FilterRange6581:"[2 - skip], 27 + skip);
	SidSetupDrawBar (cpifaceSession, focus, 3, skip, 100, "", 0, 100, SidSetupFilterRange6581, SidSetupEditPos == 2, (SidSetupFilter == 0) || strcmp (sidChipModel(0), "MOS6581") /* || (SidSetupUseresidfp == 0)*/);

	cpifaceSession->console->Driver->DisplayStr (SidSetupFirstLine + 4, 0, (focus&&(SidSetupEditPos==3))?0x07:0x08, &"  FilterCurve8580:"[2 - skip], 27 + skip);
	SidSetupDrawBar (cpifaceSession, focus, 4, skip, 100, "", 0, 100, SidSetupFilterCurve8580, SidSetupEditPos == 3, (SidSetupFilter == 0) || strcmp (sidChipModel(0), "MOS8580") /* || (SidSetupUseresidfp == 0)*/);

	cpifaceSession->console->Driver->DisplayStr (SidSetupFirstLine + 5, 0, (focus&&(SidSetupEditPos==4))?0x07:0x08, &"  CombinedWaveformsStrength:"[2 - skip], 27 + skip);
	SidSetupDrawItems (cpifaceSession, focus, 5, skip, combinedwaveforms, 3, SidSetupCombinedWaveformsStrength, SidSetupEditPos==4, (SidSetupFilter == 0) /* || (SidSetupUseresidfp == 0) */); /* always active, libsidfp no longer ships resid */
}

static int SidSetupIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i', "Enable Sid Setup Viewer");
			cpifaceSession->KeyHelp ('I', "Enable Sid Setup Viewer");
			break;
		case 'i': case 'T':
			SidSetupActive=1;
			cpifaceSession->cpiTextSetMode (cpifaceSession, "SIDSetup");
			return 1;
		case 'x': case 'X':
			SidSetupActive=1;
			break;
		case KEY_ALT_X:
			SidSetupActive=0;
			break;
	}
	return 0;
}

static int SidSetupAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	static uint32_t lastpress = 0;
	static int repeat;
	if ((key != KEY_LEFT) && (key != KEY_RIGHT))
	{
		lastpress = 0;
		repeat = 1;
	} else {
		uint32_t newpress = clock_ms();
		if ((newpress-lastpress) > 250) /* 250 ms */
		{
			repeat = 1;
		} else {
			if (SidSetupEditPos==1)
			{
				if (repeat < 20)
				{
					repeat += 1;
				}
			} else {
				if (repeat < 5)
				{
					repeat += 1;
				}
			}
		}
		lastpress = newpress;
	}
	switch (key)
	{
		case 'i': case 'I':
			SidSetupActive=!SidSetupActive;
			cpifaceSession->cpiTextRecalc (cpifaceSession);
			break;

		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('i',       "Disable Sid Setup Viewer");
			cpifaceSession->KeyHelp ('I',       "Disable Sid Setup Viewer");
			cpifaceSession->KeyHelp (KEY_UP,    "Move cursor up");
			cpifaceSession->KeyHelp (KEY_DOWN,  "Move cursor down");
			return 0;
		case KEY_LEFT:
			if (SidSetupEditPos==0)
			{
				if (SidSetupFilter)
				{
					SidSetupFilter = 0;
					sidSetFilter(SidSetupFilter);
				}
			} else if (SidSetupEditPos==1)
			{
				if (SidSetupFilter /* && SidSetupUseresidfp */)
				{
					SidSetupFilterCurve6581 -= repeat;
					if (SidSetupFilterCurve6581 < 0) SidSetupFilterCurve6581 = 0;
					sidSetFilterCurve6581((double)SidSetupFilterCurve6581 / 100.0);
				}
			} else if (SidSetupEditPos==2)
			{
				if (SidSetupFilter /* && SidSetupUseresidfp */)
				{
					SidSetupFilterRange6581 -= repeat;
					if (SidSetupFilterRange6581 < 0) SidSetupFilterRange6581 = 0;
					sidSetFilterRange6581((double)SidSetupFilterRange6581 / 100.0);
				}
			} else if (SidSetupEditPos==3)
			{
				if (SidSetupFilter /* && SidSetupUseresidfp */)
				{
					SidSetupFilterCurve8580 -= repeat;
					if (SidSetupFilterCurve8580 < 0) SidSetupFilterCurve8580 = 0;
					sidSetFilterCurve8580((double)SidSetupFilterCurve8580 / 100.0);
				}
			} else if (SidSetupEditPos==4)
			{
				if (SidSetupFilter /* && SidSetupUseresidfp */ && (SidSetupCombinedWaveformsStrength > 0))
				{
					SidSetupCombinedWaveformsStrength--;
					sidSetCombinedWaveformsStrength(SidSetupCombinedWaveformsStrength);
				}
			}
			break;
		case KEY_RIGHT:
			if (SidSetupEditPos==0)
			{
				if (!SidSetupFilter)
				{
					SidSetupFilter = 1;
					sidSetFilter(SidSetupFilter);
				}
			} else if (SidSetupEditPos==1)
			{
				if (SidSetupFilter /* && SidSetupUseresidfp */)
				{
					SidSetupFilterCurve6581 += repeat;
					if (SidSetupFilterCurve6581 > 100) SidSetupFilterCurve6581 = 100;
					sidSetFilterCurve6581((double)SidSetupFilterCurve6581 / 100.0);
				}
			} else if (SidSetupEditPos==2)
			{
				if (SidSetupFilter /* && SidSetupUseresidfp */)
				{
					SidSetupFilterRange6581 += repeat;
					if (SidSetupFilterRange6581 > 100) SidSetupFilterRange6581 = 100;
					sidSetFilterRange6581((double)SidSetupFilterRange6581 / 100.0);
				}
			} else if (SidSetupEditPos==3)
			{
				if (SidSetupFilter /* && SidSetupUseresidfp */)
				{
					SidSetupFilterCurve8580 += repeat;
					if (SidSetupFilterCurve8580 > 100) SidSetupFilterCurve8580 = 100;
					sidSetFilterCurve8580((double)SidSetupFilterCurve8580 / 100.0);
				}
			} else if (SidSetupEditPos==4)
			{
				if (SidSetupFilter /* && SidSetupUseresidfp */ && (SidSetupCombinedWaveformsStrength < 2))
				{
					SidSetupCombinedWaveformsStrength++;
					sidSetCombinedWaveformsStrength(SidSetupCombinedWaveformsStrength);
				}
			}
			break;
		case KEY_UP:
			if (SidSetupEditPos)
			{
				SidSetupEditPos--;
			}
			break;
		case KEY_DOWN:
			if (SidSetupEditPos < 4)
			{
				SidSetupEditPos++;
			}
			break;
		default:
			return 0;
	}
	return 1;
}

static int SidSetupEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			return 1;
		case cpievInit:
			SidSetupActive=1;
			// Here we can allocate memory, return 0 on error
			break;
		case cpievDone:
			// Here we can free memory
			break;
	}
	return 1;
}

static int CWS_to_int (const char *src)
{
	if (!strcasecmp (src, "AVERAGE")) return 0;
	if (!strcasecmp (src, "WEAK")) return 1;
	if (!strcasecmp (src, "STRONG")) return 2;
	return 0;
}

#if 0
static int float10x_to_int(const char *src)
{
	int retval = atoi (src) * 10;
	char *r = strchr ((char *)src, '.');
	if (r)
	{
		if (r[1] >= '0' && (r[1] <= '9'))
		{
			retval += r[1] - '0';
		}
	}
	return retval;
}
#endif

static int float100x_to_int(const char *src)
{
	int retval = atoi (src) * 100;
	char *r = strchr ((char *)src, '.');
	if (r)
	{
		if (r[1] >= '0' && (r[1] <= '9'))
		{
			retval += (r[1] - '0') * 10;
			if (r[2] >= '0' && (r[2] <= '9'))
			{
				retval += (r[2] - '0');
			}
		}
	}
	return retval;
}

static struct cpitextmoderegstruct cpiSidSetup = {"SIDSetup", SidSetupGetWin, SidSetupSetWin, SidSetupDraw, SidSetupIProcessKey, SidSetupAProcessKey, SidSetupEvent CPITEXTMODEREGSTRUCT_TAIL};

OCP_INTERNAL void cpiSidSetupInit (struct cpifaceSessionAPI_t *cpifaceSession)
{
	SidSetupUseresidfp = !strcmp (                  cpifaceSession->configAPI->GetProfileString ("libsidplayfp", "emulator",        "residfp"), "residfp");
	SidSetupFilter =                                cpifaceSession->configAPI->GetProfileBool   ("libsidplayfp", "filter",          1, 1);
	SidSetupFilterCurve6581 = float100x_to_int     (cpifaceSession->configAPI->GetProfileString ("libsidplayfp", "filtercurve6581", "0.5"));
	SidSetupFilterRange6581 = float100x_to_int     (cpifaceSession->configAPI->GetProfileString ("libsidplayfp", "filterrange6581", "0.5"));
	SidSetupFilterCurve8580 = float100x_to_int     (cpifaceSession->configAPI->GetProfileString ("libsidplayfp", "filtercurve8580", "0.5"));
	SidSetupCombinedWaveformsStrength = CWS_to_int (cpifaceSession->configAPI->GetProfileString ("libsidplayfp", "combinedwaveforms", "Average"));

	if (SidSetupFilterCurve6581 <   0) SidSetupFilterCurve6581 = 0;
	if (SidSetupFilterCurve6581 > 100) SidSetupFilterCurve6581 = 100;
	if (SidSetupFilterRange6581 <   0) SidSetupFilterRange6581 = 0;
	if (SidSetupFilterRange6581 > 100) SidSetupFilterRange6581 = 100;
	if (SidSetupFilterCurve8580 <   0) SidSetupFilterCurve8580 = 0;
	if (SidSetupFilterCurve8580 > 100) SidSetupFilterCurve8580 = 100;

	cpifaceSession->cpiTextRegisterMode (cpifaceSession, &cpiSidSetup);
}

OCP_INTERNAL void cpiSidSetupDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->cpiTextUnregisterMode (cpifaceSession, &cpiSidSetup);
}
