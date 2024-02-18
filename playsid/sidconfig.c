/* OpenCP Module Player
 * copyright (c) 2022-'23 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
 *
 * libsidplay setup config dialog
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
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef _WIN32
# include <windows.h>
# include <fileapi.h>
# include <windows.h>
#endif
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/filesystem-setup.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
#include "stuff/utf-8.h"
#include "stuff/poutput.h"

#include "sidconfig.h"
#include "md5.inc.c"

#ifndef MIN
# define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
# define MAX(a,b) (((a)>(b))?(a):(b))
#endif

struct browser_t
{
	int isdir;
	int isparent;
#ifdef _WIN32
	int isdrive;
#endif
	uint32_t dirdb_ref;
	char hash_4096[33];
	char hash_8192[33];
};

static char             *entries_location;
static struct browser_t *entries_data;
static int               entries_count;
static int               entries_size;

static int config_emulator;
static int config_defaultC64;
static int config_forceC64;
static int config_defaultSID;
static int config_forceSID;
static int config_CIA;
static int config_filter;
static int config_filterbias;
static int config_filtercurve6581;
static int config_filterrange6581;
static int config_filtercurve8580;
static int config_combinedwaveforms;
static int config_digiboost;
static char *config_kernal;
static char *config_basic;
static char *config_chargen;

struct browser_t entry_kernal;
struct browser_t entry_basic;
struct browser_t entry_chargen;

static void ConfigDrawItems (const int lineno, int xpos, const int width, const char **list, const int listlength, const int selected, const int active, const struct DevInterfaceAPI_t *API)
{
	int i;
	int origxpos = xpos;
	for (i=0; i < listlength; i++)
	{
		int l = strlen (list[i]);
		if (selected == i)
		{
			API->console->DisplayPrintf (lineno, xpos, (active)?0x09:0x01, l + 2, "[%.*o%s%.*o]", (active)?0x0f:0x07, list[i], (active)?0x09:0x01);
		} else {
			API->console->DisplayPrintf (lineno, xpos, 0x00, l + 2, " %.*o%s%.0o ", (active)?0x07:0x08, list[i]);
		}
		xpos += l + 2;
	}
	API->console->Driver->DisplayVoid (lineno, xpos, width - xpos + origxpos);
}

static void ConfigDrawMenuItems (const int lineno, int xpos, const int width, const int dot, const char *item, const char **list, const int listlength, const int selected, const int active, const struct DevInterfaceAPI_t *API)
{
	API->console->DisplayPrintf (lineno, xpos, 0x09, 23, "\xb3%.7o %s:", item);
	ConfigDrawItems (lineno, xpos + 23, width - 24, list, listlength, selected, active, API);
	API->console->Driver->DisplayChr (lineno, xpos + width - 1, 0x09, (lineno == dot) ? '\xdd' : '\xb3', 1);
}

static void ConfigDrawBar (const int lineno, int xpos, int width, int scale, const char *suffix, int minlevel, int maxlevel, int level, const int active, const struct DevInterfaceAPI_t *API)
{
	char prefix[11];
	char min[8];
	char max[7];

	unsigned int abslevel;
	unsigned int absmin;

	int pos, p1, p2;

	assert ((scale == 10) || (scale == 100));

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

	pos = (/*(maxlevel - minlevel / 46) - 1 +*/ (level - minlevel) * 22) / (maxlevel - minlevel);

	p1 = pos;
        p2 = 22 - pos;

	API->console->DisplayPrintf (lineno, xpos, (active)?0x07:0x08, width, "%10s%-7s [%*C.#%*C.] %-6s", prefix, min, p1, p2, max);
}

static void ConfigDrawMenuBar (const int lineno, int xpos, int width, const int dot, const char *item, int scale, const char *suffix, int minlevel, int maxlevel, int level, const int active, const struct DevInterfaceAPI_t *API)
{
	API->console->DisplayPrintf (lineno, xpos, 0x09, 23, "\xb3%.7o %s:", item);
	ConfigDrawBar (lineno, xpos + 23, width - 24, scale, suffix, minlevel, maxlevel, level, active, API);
	API->console->Driver->DisplayChr (lineno, xpos + width - 1, 0x09, (lineno == dot) ? '\xdd' : '\xb3', 1);
}

static void ConfigDrawMenuRom (const int lineno, int xpos, int width, const int dot, const char *item, int active, const char *path, const struct DevInterfaceAPI_t *API)
{
	API->console->DisplayPrintf (lineno, xpos, 0x09, width, "\xb3%.7o %20s %.*o%*S%.9o%c",
		item,
		active?0x0f:0x08,
		width - 24,
		path,
		(lineno == dot) ? '\xdd' : '\xb3');
}

struct hash_pairs_t
{
	const char *hash; const char *description;
};

static struct hash_pairs_t hash_kernal[] =
{
	/* src is libsidplayfp-git/src/romCheck.h */
	{"1ae0ea224f2b291dafa2c20b990bb7d4", "C64 KERNAL first revision"},
	{"7360b296d64e18b88f6cf52289fd99a1", "C64 KERNAL second revision"},
	{"479553fd53346ec84054f0b1c6237397", "C64 KERNAL second revision (Japanese)"},
	{"39065497630802346bce17963f13c092", "C64 KERNAL third revision"},
	{"27e26dbb267c8ebf1cd47105a6ca71e7", "C64 KERNAL third revision (Swedish)"},
	{"27e26dbb267c8ebf1cd47105a6ca71e7", "C64 KERNAL third revision (Swedish C2G007)"},
	{"e4aa56240fe13d8ad8d7d1dc8fec2395", "C64 KERNAL third revision (Danish)"},
	{"174546cf655e874546af4eac5f5bf61b", "C64 KERNAL third revision (Turkish)"},
	{"187b8c713b51931e070872bd390b472a", "Commodore SX-64 KERNAL"},
	{"b7b1a42e11ff8efab4e49afc4faedeee", "Commodore SX-64 KERNAL (Swedish)"},
	{"3abc938cac3d622e1a7041c15b928707", "Cockroach Turbo-ROM"},
	{"631ea2ca0dcda414a90aeefeaf77fe45", "Cockroach Turbo-ROM (SX-64)"},
	{"a9de1832e9be1a8c60f4f979df585681", "Datel DOS-ROM 1.2"},
	{"da43563f218b46ece925f221ef1f4bc2", "Datel Mercury 3 (NTSC)"},
	{"b7dc8ed82170c81773d4f5dc8069a000", "Datel Turbo ROM II (PAL)"},
	{"6b309c76473dcf555c52c598c6a51011", "Dolphin DOS v1.0"},
	{"c3c93b9a46f116acbfe7ee147c338c60", "Dolphin DOS v2.0-1 AU"},
	{"2a441f4abd272d50f94b43c7ff3cc629", "Dolphin DOS v2.0-1"},
	{"c7a175217e67dcb425feca5fcf2a01cc", "Dolphin DOS v2.0-2"},
	{"7a9b1040cfbe769525bb9cdc28427be6", "Dolphin DOS v2.0-3"},
	{"fc8fb5ec89b34ae41c8dc20907447e06", "Dolphin DOS v3.0"},
	{"9a6e1c4b99c6f65323aa96940c7eb7f7", "ExOS v3 fertig"},
	{"3241a4fcf2ba28ba3fc79826bc023814", "ExOS v3"},
	{"cffd2616312801da56bcc6728f0e39ca", "ExOS v4"},
	{"e6e2bb24a0fa414182b0fd149bde689d", "TurboAccess"},
	{"c5c5990f0826fcbd372901e761fab1b7", "TurboTrans v3.0-1"},
	{"042ffc11383849bdf0e600474cefaaaf", "TurboTrans v3.0-2"},
	{"9d62852013fc2c29c3111c765698664b", "Turbo-Process US"},
	{"f9c9838e8d6752dc6066a8c9e6c2e880", "Turbo-Process"},
};
static struct hash_pairs_t hash_basic[] =
{
	{"57af4ae21d4b705c2991d98ed5c1f7b8", "C64 BASIC V2"},
};
static struct hash_pairs_t hash_chargen[] =
{
	{"12a4202f5331d45af846af6c58fba946", "C64 character generator"},
	{"cf32a93c0a693ed359a4f483ef6db53d", "C64 character generator (Japanese)"},
	{"7a1906cd3993ad17a0a0b2b68da9c114", "C64 character generator (Swedish)"},
	{"5973267e85b7b2b574e780874843180b", "C64 character generator (Swedish C2G007)"},
	{"81a1a8e6e334caeadd1b8468bb7728d3", "C64 character generator (Spanish)"},
	{"b3ad62b41b5f919fc56c3a40e636ec29", "C64 character generator (Danish)"},
	{"7d82b1f8f750665b5879c16b03c617d9", "C64 character generator (Turkish)"},
};

static void ConfigDrawHashInfo (const int lineno, int xpos, int width, const char *hash_8192, const char *hash_4096, int expect, const struct DevInterfaceAPI_t *API)
{
	const uint8_t COK = 0x02;
	const uint8_t CERR = 0x04;
	int j;

	for (j=0; j < (sizeof (hash_kernal) / sizeof (hash_kernal[0])); j++)
	{
		if (!strcmp (hash_8192, hash_kernal[j].hash))
		{
			API->console->Driver->DisplayStr (lineno, xpos, (expect==0)?COK:CERR, hash_kernal[j].description, width);
			return;
		}
	}
	for (j=0; j < (sizeof (hash_basic) / sizeof (hash_basic[0])); j++)
	{
		if (!strcmp (hash_8192, hash_basic[j].hash))
		{
			API->console->Driver->DisplayStr (lineno, xpos, (expect==1)?COK:CERR, hash_basic[j].description, width);
			return;
		}
	}
	for (j=0; j < (sizeof (hash_chargen) / sizeof (hash_chargen[0])); j++)
	{
		if (!strcmp (hash_4096, hash_chargen[j].hash))
		{
			API->console->Driver->DisplayStr (lineno, xpos, (expect==2)?COK:CERR, hash_chargen[j].description, width);
			return;
		}
	}

	API->console->Driver->DisplayStr (lineno, xpos, CERR, "Unknown ROM file", width); return;
}

static void ConfigDrawHashMenuInfo (const int lineno, int xpos, int width, const int dot, const char *hash_8192, const char *hash_4096, int expect,  const struct DevInterfaceAPI_t *API)
{
	API->console->DisplayPrintf (lineno, xpos, 0x09, 25, "\xb3%.7o");
	ConfigDrawHashInfo (lineno, xpos + 25, width - 26, hash_8192, hash_4096, expect, API);
	API->console->Driver->DisplayChr (lineno, xpos + width - 1, 0x09, (lineno == dot) ? '\xdd' : '\xb3', 1);
}

#if 0
 +--------------------- libsidplayfp configuration ---------------------------\
 |  Navigate with <U>,<D>,<L>,<R> and <ENTER>; hit <ESC> to save and exit.    |
 +----------------------------------------------------------------------------+
 |  1: emulator:        [resid] [residfp]                                     | fp=floating point - better but slower
 |  2: default C64:     [PAL] [NTSC] [OLD-NTSC] [DREAN] [PAL-M]               | for SID files that does not specify C64 model
 |  3: force C64 model: [yes] [no]                                            | ignore information in SID files?
 |  4: default SID:     [MOS6581] [MOS8580]                                   | for SID files that does not specify SID model
 |  5: force SID:       [yes] [no]                                            | ignore information in SID files?
 |  6: CIA:             [MOS6526] [MOS6526W4485] [MOS8521]                    | MOS6526 is the classic chip, MOS6526W4485 is a specific batch while MOS8521 is the modern chip
 |  7: filter:          [yes] [no]                                            |
                                          12345678901 12345678901 = 23
                                  123456
 |  8: filterbias:         0.0mV  -500.0 [...........#...........] 500.0      | Default is 0.0mV
 |  9: filtercurce6581:    0.50      0.0 [           #           ]   1.0      | Default is 0.5
 | 10: filtercurce8580:    0.50      0.0 [           #           ]   1.0      | Default is 0.5
 | 11: digiboost:       [yes] [no]                                            |
 | 12: kernal.rom:      sadfasdfsdfsadf/sdfasdf/sdfasdf/sdfasdf.ROM           | KERNEL.ROM images can be found online. Some SID files requires this file in order to play correctly
 |                           sadfløjhsdflkhjasdf
 | 13: basic.rom:       sadfasdfsdfsadf/sdfasdf/sdfasdf/sdfasdf.ROM           | BASIC.ROM images can be found online. Some SID files requires this file in order to play correctly
 |                           sadfløjhsdflkhjasdf
 | 14: chargen.rom:     sadfasdfsdfsadf/sdfasdf/sdfasdf/sdfasdf.ROM           | CHARGEN.ROM images can be found online. Some SID files requires this file in order to play correctly
 |                           sadfløjhsdflkhjas
 +----------------------------------------------------------------------------|
 |                                                                            |
 |                                                                            |
 \----------------------------------------------------------------------------/
#endif

static void sidConfigDraw (int EditPos, const struct DevInterfaceAPI_t *API)
{
	const int LINES_NOT_AVAILABLE_START = 3;
	const int LINES_NOT_AVAILABLE_STOP = 4;
	const int LINES_NOT_AVAILABLE = LINES_NOT_AVAILABLE_START + LINES_NOT_AVAILABLE_STOP;
	const int maxcontentheight = 19;
	      int contentheight = MIN(maxcontentheight, MAX(API->console->TextHeight - 1 - LINES_NOT_AVAILABLE, LINES_NOT_AVAILABLE + 1));
	const int mlHeight = contentheight + LINES_NOT_AVAILABLE;
	int mlTop, mlLeft, mlWidth;
	const char *offon[] = {"off", "on"};
	const char *emulators[] = {"resid", "residfp"};
	const char *C64models[] = {"PAL", "NTSC", "OLD-NTSC", "DREAN", "PAL-M"};
	const char *SIDmodels[] = {"MOS6581", "MOS8580"};
	const char *CIAmodels[] = {"MOS6526", "MOS6526W4485", "MOS8521"};
	const char *combinedwaveforms[] = {"Average", "Weak", "Strong" };
	int s;

	const int Pos = EditPos + ((EditPos > 13) ? (EditPos - 13) * 2 : 0); /* 3 last lines count double speed */
	int half;
	int skip;
	int dot;

	half = contentheight / 2;
	if (contentheight >= maxcontentheight)
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (Pos < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (Pos >= (maxcontentheight - half))
	{ /* we are at the bottom part */
		skip = maxcontentheight - contentheight;
		dot = contentheight - 1;
	} else {
		skip = Pos - half;
		dot = skip * (contentheight) / (maxcontentheight - (contentheight));
	}

	mlWidth = MIN (78 + (API->console->TextWidth - 80) * 2 / 3, 120);
	mlTop = MAX(1,(API->console->TextHeight - mlHeight) / 2);
	mlLeft = (API->console->TextWidth - mlWidth) / 2;

	if (dot >= 0)
	{
		dot += mlTop + 3;
	}

	s = (mlWidth - 2 - 28) / 2; API->console->DisplayPrintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xda%*C\xc4 libsidplayfp configuration %*C\xc4\xbf", s, mlWidth - 2 - 28 - s);
	s = (mlWidth - 2 - 70) / 2; API->console->DisplayPrintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%.15o%*C Navigate with %.15o<\x18>%.7o,%.15o<\x19>%.7o,%.15o<\x1a>%.7o,%.15o<\x1b>%.7o and %.15o<ENTER>%.7o; hit %.15o<ESC>%.7o to save and exit.%*C %.9o\xb3", s, mlWidth - 2 - 70 - s);

	API->console->DisplayPrintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2);

#undef _A
#undef _B
#define _A \
if (skip)                   \
{                           \
  skip --;                  \
} else if (contentheight) {

#define _B \
  contentheight--;          \
}

	_A ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, dot, " 1: emulator", emulators, 2, config_emulator, EditPos==0, API);                   _B
	_A ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, dot, " 2: default C64", C64models, 5, config_defaultC64, EditPos==1, API);              _B
	_A ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, dot, " 3: force C64 model", offon, 2, config_forceC64, EditPos==2, API);                _B
	_A ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, dot, " 4: default SID", SIDmodels, 2, config_defaultSID, EditPos==3, API);              _B
	_A ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, dot, " 5: force SID", offon, 2, config_forceSID, EditPos==4, API);                      _B
	_A ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, dot, " 6: CIA", CIAmodels, 3, config_CIA, EditPos==5, API);                             _B
	_A ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, dot, " 7: filter", offon, 2, config_filter, EditPos==6, API);                           _B
	_A ConfigDrawMenuBar      (mlTop++, mlLeft, mlWidth, dot, " 8: filterbias", 10, "mV", -5000, 5000, config_filterbias, EditPos==7, API);      _B
	_A ConfigDrawMenuBar      (mlTop++, mlLeft, mlWidth, dot, " 9: filtercurve6581", 100, "", 0, 100, config_filtercurve6581, EditPos==8, API);  _B
	_A ConfigDrawMenuBar      (mlTop++, mlLeft, mlWidth, dot, "10: filterrange6581", 100, "", 0, 100, config_filterrange6581, EditPos==9, API);  _B
	_A ConfigDrawMenuBar      (mlTop++, mlLeft, mlWidth, dot, "11: filtercurve8580", 100, "", 0, 100, config_filtercurve8580, EditPos==10, API); _B
	_A ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, dot, "12: CWS", combinedwaveforms, 3, config_combinedwaveforms, EditPos==11, API);      _B
	_A ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, dot, "13: digiboost", offon, 2, config_digiboost, EditPos==12, API);                    _B
	_A ConfigDrawMenuRom      (mlTop++, mlLeft, mlWidth, dot, "14: kernal.rom:", EditPos==13, config_kernal, API);                               _B
	_A ConfigDrawHashMenuInfo (mlTop++, mlLeft, mlWidth, dot, entry_kernal.hash_8192, entry_kernal.hash_4096, 0, API);                           _B
	_A ConfigDrawMenuRom      (mlTop++, mlLeft, mlWidth, dot, "15: basic.rom:", EditPos==14, config_basic, API);                                 _B
	_A ConfigDrawHashMenuInfo (mlTop++, mlLeft, mlWidth, dot, entry_basic.hash_8192, entry_basic.hash_4096, 1, API);                             _B
	_A ConfigDrawMenuRom      (mlTop++, mlLeft, mlWidth, dot, "16: chargen.rom", EditPos==15, config_chargen, API);                              _B
	_A ConfigDrawHashMenuInfo (mlTop++, mlLeft, mlWidth, dot, entry_chargen.hash_8192, entry_chargen.hash_4096, 2, API);                         _B

#undef _A
#undef _B

	API->console->DisplayPrintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2);
	switch (EditPos)
	{
		case 0:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o resid   = standard integer emulator - fastest.%*C %.9o\xb3", mlWidth - 49);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o residfp = floating point emulator - better quality but slower.%*C %.9o\xb3", mlWidth - 65);
			break;
		case 1:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o For SID files that does not specify C64 model.%*C %.9o\xb3", mlWidth - 49);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 2:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Override C64 model specified in SID files (if specified).%*C %.9o\xb3", mlWidth - 60);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 3:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o For SID files that does not specify SID model.%*C %.9o\xb3", mlWidth - 49);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 4:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Override SID model specified in SID files (if specified).%*C %.9o\xb3", mlWidth - 60);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 5:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Specify which CIA chip to emulate. MOS6526 is the classic model where%*C %.9o\xb3", mlWidth - 72);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o MOS6526W4485 is a specific batch. MOS8521 is the modern chip model.%*C %.9o\xb3", mlWidth - 70);
			break;
		case 6:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Enable post-processing filtering.%*C %.9o\xb3", mlWidth - 36);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 7:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Only used by \"resid\".%*C %.9o\xb3", mlWidth - 24);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Default is 0.0mV%*C %.9o\xb3", mlWidth - 19);
			break;
		case 8:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Only used by \"residfp\". Value to use if SID is MOS6581.%*C %.9o\xb3", mlWidth - 58);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Default is 0.5%*C %.9o\xb3", mlWidth - 17);
			break;
		case 9:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Only used by \"residfp\". MOS6581 only: 0=\"bright\", 1=\"dark\".%*C %.9o\xb3", mlWidth - 62);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Default is 0.5%*C %.9o\xb3", mlWidth - 17);
			break;
		case 10:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Only used by \"residfp\". Value to use if SID is MOS8580.%*C %.9o\xb3", mlWidth - 58);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Default is 0.5%*C %.9o\xb3", mlWidth - 17);
			break;
		case 11:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Only used by \"residfp\". Combined waveforms strength.%*C %.9o\xb3", mlWidth - 55);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Default is average.%*C %.9o\xb3", mlWidth - 22);
			break;
		case 12:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Digi-Boost is a hardware feature only available on MOS8580, where the%*C %.9o\xb3", mlWidth - 72);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o digital playback would be boosted.%*C %.9o\xb3", mlWidth - 37);
			break;
		case 13:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o KERNEL.ROM images can be found online. Some SID files requires this file%*C %.9o\xb3", mlWidth - 75);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o in order to play correctly.%*C %.9o\xb3", mlWidth - 30);
			break;
		case 14:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o BASIC.ROM images can be found online. Some SID files requires this file%*C %.9o\xb3", mlWidth - 74);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o in order to play correctly.%*C %.9o\xb3", mlWidth - 30);
			break;
		case 15:
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o CHARGEN.ROM images can be found online. Some SID files requires this file%*C %.9o\xb3", mlWidth - 76);
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o in order to play correctly.%*C %.9o\xb3", mlWidth - 30);
			break;
		default: /* should not be reachable */
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%76C \xb3");
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%76C \xb3");
			break;
	}
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

static int emulator_to_int (const char *src)
{
	if (!strcasecmp (src, "resid")) return 0;
	if (!strcasecmp (src, "residfp")) return 1;
	return 1;
}

static const char *emulator_from_int (const int src)
{
	switch (src)
	{
		default:
		case 0: return "resid";
		case 1: return "residfp";
	}
}

static int defaultC64_to_int (const char *src)
{
	if (!strcasecmp (src, "PAL")) return 0;
	if (!strcasecmp (src, "NTSC")) return 1;
	if (!strcasecmp (src, "OLD-NTSC")) return 2;
	if (!strcasecmp (src, "DREAN")) return 3;
	if (!strcasecmp (src, "PAL-M")) return 4;
	return 0;
}

static const char *defaultC64_from_int (const int src)
{
	switch (src)
	{
		default:
		case 0: return "PAL";
		case 1: return "NTSC";
		case 2: return "OLD-NTSC";
		case 3: return "DREAN";
		case 4: return "PAL-M";
	}
}

static int defaultSID_to_int (const char *src)
{
	if (!strcasecmp (src, "MOS6581")) return 0;
	if (!strcasecmp (src, "MOS8580")) return 1;
	return 0;
}

static const char *defaultSID_from_int (const int src)
{
	switch (src)
	{
		default:
		case 0: return "MOS6581";
		case 1: return "MOS8580";
	}
}

static int CIA_to_int (const char *src)
{
	if (!strcasecmp (src, "MOS6526")) return 0;
	if (!strcasecmp (src, "MOS6526W4485")) return 1;
	if (!strcasecmp (src, "MOS8521")) return 2;
	return 0;
}

static const char *CIA_from_int (const int src)
{
	switch (src)
	{
		default:
		case 0: return "MOS6526";
		case 1: return "MOS6526W4485";
		case 2:	return "MOS8521";
	}
}

static int CWS_to_int (const char *src)
{
	if (!strcasecmp (src, "AVERAGE")) return 0;
	if (!strcasecmp (src, "WEAK")) return 1;
	if (!strcasecmp (src, "STRONG")) return 2;
	return 0;
}

static const char *CWS_from_int (const int src)
{
	switch (src)
	{
		default:
		case 0: return "Average";
		case 1: return "Weak";
		case 2: return "Strong";
	}
}

static const char *int_to_float10x(int src)
{
	static char retval[32];
	int src_abs = (src >= 0) ? src : -src;
	snprintf (retval, sizeof (retval), "%d.%01d", src / 10, src_abs % 10);
	return retval;
}

static const char *int_to_float100x(int src)
{
	static char retval[32];
	int src_abs = (src >= 0) ? src : -src;
	snprintf (retval, sizeof (retval), "%d.%02d", src / 100, src_abs % 100);
	return retval;
}

static int float10x_to_int(const char *src)
{
	int retval = atoi (src) * 10;
	char *r = strchr (src, '.');
	if (r)
	{
		if (r[1] >= '0' && (r[1] <= '9'))
		{
			retval += r[1] - '0';
		}
	}
	return retval;
}

static int float100x_to_int(const char *src)
{
	int retval = atoi (src) * 100;
	char *r = strchr (src, '.');
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

static void rom_md5 (char md5[33], uint32_t dirdb_ref, int size, const struct DevInterfaceAPI_t *API)
{
	char *romPath = 0;
#ifdef _WIN32
	HANDLE f;
#else
	int fd;
#endif
	uint8_t buffer[4096];
	MD5_CTX ctx;

	md5[0] = '-';
	md5[1] = 0;
	md5[32] = 0;

#ifdef _WIN32
	API->dirdb->GetFullname_malloc (dirdb_ref, &romPath, DIRDB_FULLNAME_DRIVE | DIRDB_FULLNAME_BACKSLASH);

	f = CreateFile (romPath,             /* lpFileName */
	                GENERIC_READ,          /* dwDesiredAccess */
	                FILE_SHARE_READ,       /* dwShareMode */
	                0,                     /* lpSecurityAttributes */
	                OPEN_EXISTING,         /* dwCreationDisposition */
	                FILE_ATTRIBUTE_NORMAL, /* dwFlagsAndAttributes */
	                0);                    /* hTemplateFile */
	free (romPath);
	if (f == INVALID_HANDLE_VALUE)
	{
		return;
	}
#else
	API->dirdb->GetFullname_malloc (dirdb_ref, &romPath, DIRDB_FULLNAME_NODRIVE);
	fd = open (romPath, O_RDONLY);
	free (romPath);
	if (fd < 0)
	{
		return;
	}
#endif

	MD5Init (&ctx);

	while (size)
	{
#ifdef _WIN32
		DWORD NumberOfBytesRead = 0;
		BOOL Result;
		Result = ReadFile (f, buffer, 4096, &NumberOfBytesRead, 0);
		if ((!Result) || (NumberOfBytesRead != 4096))
		{
			CloseHandle (f);
			return;
		}
		MD5Update (&ctx, buffer, 4096);
		size -= 4096;
#else
		int result = read (fd, buffer, 4096);
		if (result < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			close (fd);
			return;
		}
		if (result == 0)
		{
			close (fd);
			return;
		}
		MD5Update (&ctx, buffer, 4096);
		size -= result;
#endif
	}

#ifdef _WIN32
	CloseHandle (f);
#else
	close (fd);
#endif
	MD5Final (md5, &ctx);
}

static void sidDrawDir (const int esel, const int expect, const struct DevInterfaceAPI_t *API)
{
	int contentheight = entries_count;
	int i;
	int files = 0;
	int dirs = 0;
#ifdef _WIN32
	int drives = 0;
#endif

	int half;
	int skip;
	int dot;
	int s;
	const char *title;

	const int mlHeight = 24;
	const int LINES_NOT_AVAILABLE = 4;
	int mlTop = (API->console->TextHeight - mlHeight) / 2;
	int mlWidth = MIN (78 + (API->console->TextWidth - 80) * 2 / 3, 120);
	int mlLeft = (API->console->TextWidth - mlWidth) / 2;

	switch (expect)
	{
		case 0:  title = "Select a KERNEL ROM";  break;
		case 1:  title = "Select a BASIC ROM";   break;
		default: title = "Select a CHARGEN ROM"; break;
	}

	for (i=0; i < entries_count; i++)
	{
		if (entries_data[i].isdir)
		{
			dirs++;
#ifdef _WIN32
		} else if (entries_data[i].isdrive)
		{
			drives++;
#endif
		} else {
			files++;
		}
	}
	contentheight = dirs + files;
#ifdef _WIN32
	contentheight += drives;
#endif

	half = (mlHeight - LINES_NOT_AVAILABLE) / 2;

	if (contentheight <= (mlHeight - LINES_NOT_AVAILABLE))
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (esel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (esel >= (contentheight - half))
	{ /* we are at the bottom part */
		skip = contentheight - (mlHeight - LINES_NOT_AVAILABLE);
		dot = mlHeight - LINES_NOT_AVAILABLE - 1;
	} else {
		skip = esel - half;
		dot = skip * (mlHeight - LINES_NOT_AVAILABLE) / (contentheight - (mlHeight - LINES_NOT_AVAILABLE));
	}

	s = (mlWidth - 2 - strlen (title) - 2) / 2; API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda%*C\xc4 %s %*C\xc4\xbf", s, title, mlWidth - 2 - strlen (title) - 2 - s);

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*S%.9o\xb3", mlWidth - 2, entries_location ? entries_location : "");

	API->console->DisplayPrintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2);

	for (i = 0; i < (mlHeight-LINES_NOT_AVAILABLE); i++)
	{
		int masterindex = skip + i;
		if (masterindex >= entries_count)
		{
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C %c", mlWidth - 2, (i==dot)?'\xdd':'\xb3');
		} else if (entries_data[masterindex].isdir)
		{
			if (entries_data[masterindex].isparent)
			{
				API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*.*o..%*C %0.9o%c", (masterindex==esel)?0x6:0x0, (masterindex==esel)?0x1:0x1, mlWidth - 4, (i==dot)?'\xdd':'\xb3');
			} else {
				const char *dirname;
				API->dirdb->GetName_internalstr (entries_data[masterindex].dirdb_ref, &dirname);
				API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*.*o%*S%0.9o%c", (masterindex==esel)?0x6:0x0, (masterindex==esel)?0x1:0x1, mlWidth - 2, dirname, (i==dot)?'\xdd':'\xb3');
			}
		} else {
			const char *filename;
			API->dirdb->GetName_internalstr (entries_data[masterindex].dirdb_ref, &filename);
			API->console->DisplayPrintf (mlTop,   mlLeft,                0x09, mlWidth - 43, "\xb3%*.*o%*S%0.9o%c", (masterindex==esel)?0x6:0x0, (masterindex==esel)?0x0:0x7, mlWidth - 2 - 42, filename);
			ConfigDrawHashInfo          (mlTop,   mlLeft + mlWidth - 43,                 42, entries_data[masterindex].hash_8192, entries_data[masterindex].hash_4096, expect, API);
			API->console->DisplayPrintf (mlTop++, mlLeft + mlWidth -  1, 0x09,            1, "%c", (i==dot)?'\xdd':'\xb3');
		}
	}

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

static void entries_clear (const struct DevInterfaceAPI_t *API)
{
	int i;
	free (entries_location);
	for (i=0; i < entries_count; i++)
	{
		API->dirdb->Unref (entries_data[i].dirdb_ref, dirdb_use_file);
	}
	free (entries_data);
	entries_location = 0;
	entries_count = 0;
	entries_size = 0;
	entries_data = 0;
}

static const struct DevInterfaceAPI_t *cmp_API;
static int cmp(const void *a, const void *b)
{
	struct browser_t *p1 = (struct browser_t *)a;
	struct browser_t *p2 = (struct browser_t *)b;

	const char *n1;
	const char *n2;

	if (p1->isparent)
	{
		return -1;
	}
	if (p2->isparent)
	{
		return 1;
	}
	if (p1->isdir && (!p2->isdir))
	{
		return -1;
	}
	if (p2->isdir && (!p1->isdir))
	{
		return 1;
	}
#ifdef _WIN32
	if ((p1->isdrive) && (!p2->isdrive))
	{
		return -1;
	}
	if (p2->isdrive && (!p1->isdrive))
	{
		return 1;
	}
#endif

	cmp_API->dirdb->GetName_internalstr (p1->dirdb_ref, &n1);
	cmp_API->dirdb->GetName_internalstr (p2->dirdb_ref, &n2);
	return strcmp (n1, n2);
}

static void entries_sort (const struct DevInterfaceAPI_t *API)
{
	cmp_API = API;
	qsort (entries_data, entries_count, sizeof (entries_data[0]), cmp);
	cmp_API = 0;
}

static int entries_append (void)
{
	void *temp;
	if (entries_count < entries_size)
	{
		return 0;
	}
	temp = realloc (entries_data, sizeof (entries_data[0]) * (entries_size + 16));
	if (!temp)
	{
		return -1;
	}
	entries_data = temp;
	entries_size += 16;
	return 0;
}

static void entries_append_parent (uint32_t ref, const struct DevInterfaceAPI_t *API)
{
	if (entries_append())
	{
		API->dirdb->Unref (ref, dirdb_use_file);
		return;
	}
	entries_data[entries_count].isdir = 1;
	entries_data[entries_count].isparent = 1;
#ifdef _WIN32
	entries_data[entries_count].isdrive = 0;
#endif
	entries_data[entries_count].dirdb_ref = ref;
	entries_data[entries_count].hash_4096[0] = 0;
	entries_data[entries_count].hash_8192[0] = 0;
	entries_count++;
}

static void entries_append_dir (uint32_t ref, const struct DevInterfaceAPI_t *API)
{
	if (entries_append())
	{
		API->dirdb->Unref (ref, dirdb_use_file);
		return;
	}
	entries_data[entries_count].isdir = 1;
	entries_data[entries_count].isparent = 0;
#ifdef _WIN32
	entries_data[entries_count].isdrive = 0;
#endif
	entries_data[entries_count].dirdb_ref = ref;
	entries_data[entries_count].hash_4096[0] = 0;
	entries_data[entries_count].hash_8192[0] = 0;
	entries_count++;
}

static void entries_append_file (uint32_t ref, const char *hash_4096, const char *hash_8192, const struct DevInterfaceAPI_t *API)
{
	if (entries_append())
	{
		API->dirdb->Unref (ref, dirdb_use_file);
		return;
	}
	entries_data[entries_count].isdir = 0;
	entries_data[entries_count].isparent = 0;
	entries_data[entries_count].dirdb_ref = ref;
	strcpy (entries_data[entries_count].hash_4096, hash_4096);
	strcpy (entries_data[entries_count].hash_8192, hash_8192);
	entries_count++;
}

#ifdef _WIN32
static void entries_append_drive (uint32_t ref, const struct DevInterfaceAPI_t *API)
{
	if (ref == DIRDB_CLEAR)
	{
		return;
	}
	if (entries_append())
	{
		return;
	}
	entries_data[entries_count].isdir = 0;
	entries_data[entries_count].isparent = 0;
	entries_data[entries_count].isdrive = 1;
	API->dirdb->Unref(ref, dirdb_use_file);
	entries_data[entries_count].dirdb_ref = ref;
	entries_data[entries_count].hash_4096[0] = 0;
	entries_data[entries_count].hash_8192[0] = 0;
	entries_count++;
}
#endif

static void refresh_dir (uint32_t ref, uint32_t old, int *esel, const struct DevInterfaceAPI_t *API)
{
	DIR *d;
	int i;

	*esel = 0;

	entries_clear (API);

#ifdef _WIN32
	API->dirdb->GetFullname_malloc (ref, &entries_location, DIRDB_FULLNAME_DRIVE | DIRDB_FULLNAME_BACKSLASH);
#else
	API->dirdb->GetFullname_malloc (ref, &entries_location, DIRDB_FULLNAME_NODRIVE);
#endif

	{
		uint32_t dir_ref = API->dirdb->GetParentAndRef (ref, dirdb_use_file);
		if (dir_ref != UINT32_MAX)
		{
			entries_append_parent (dir_ref, API);
		}
	}

	d = opendir (entries_location);
	if (d)
	{
		struct dirent *de;
		while ((de = readdir (d)))
		{
			struct stat st;
			if (!strcmp (de->d_name, "."))
			{
				continue;
			}
			if (!strcmp (de->d_name, ".."))
			{
				continue;
			}
			{
				char *temp = malloc (strlen (entries_location) + 1 + strlen (de->d_name) + 1);
				int res;
				if (!temp)
				{
					continue;
				}
				sprintf (temp, "%s/%s", entries_location, de->d_name);
				res = stat (temp, &st);
				free (temp);
				if (res < 0)
				{
					continue;
				}
			}
			if ((st.st_mode & S_IFMT) == S_IFDIR)
			{
				entries_append_dir (API->dirdb->FindAndRef (ref, de->d_name, dirdb_use_file), API);
				continue;
			} else if ((st.st_mode & S_IFMT) == S_IFREG)
			{
				int len = strlen (de->d_name);
				char md5_4096[33];
				char md5_8192[33];
				uint32_t newref;
				if (len < 4)
				{
					continue;
				}
				if (strcasecmp (de->d_name + len - 4, ".rom") &&
				    strcasecmp (de->d_name + len - 4, ".bin"))
				{
					continue;
				}
				newref = API->dirdb->FindAndRef (ref, de->d_name, dirdb_use_file);
				rom_md5 (md5_4096, newref, 4096, API);
				rom_md5 (md5_8192, newref, 8192, API);
				entries_append_file (newref, md5_4096, md5_8192, API);
			}
		}
		closedir (d);
	}
#ifdef _WIN32
	for (i=0; i < 26; i++)
	{
		if (API->dmDriveLetters[i])
		{
			entries_append_drive (API->dmDriveLetters[i]->cwd->dirdb_ref, API);
		}
	}
#endif
	entries_sort (API);
	for (i=0; i < entries_count; i++)
	{
		if (entries_data[i].dirdb_ref == old)
		{
			*esel = i;
			return;
		}
	}
}

static void sidConfigRun (void **token, const struct DevInterfaceAPI_t *API)
{
	int esel = 0;
	uint32_t dirdb_base = API->configAPI->DataHomeDir->dirdb_ref;

	config_emulator = emulator_to_int         (API->configAPI->GetProfileString ("libsidplayfp", "emulator",        "residfp"));
	config_defaultC64 = defaultC64_to_int     (API->configAPI->GetProfileString ("libsidplayfp", "defaultC64",      "PAL"));
	config_forceC64 =                          API->configAPI->GetProfileBool   ("libsidplayfp", "forceC64",        0, 0);
	config_defaultSID = defaultSID_to_int     (API->configAPI->GetProfileString ("libsidplayfp", "defaultSID",      "MOS6581"));
	config_forceSID =                          API->configAPI->GetProfileBool   ("libsidplayfp", "forceSID",        0, 0);
	config_CIA = CIA_to_int                   (API->configAPI->GetProfileString ("libsidplayfp", "CIA",             "MOS6526"));
	config_filter =                            API->configAPI->GetProfileBool   ("libsidplayfp", "filter",          1, 1);
	config_filterbias = float10x_to_int       (API->configAPI->GetProfileString ("libsidplayfp", "filterbias",      "0.0"));
	config_filtercurve6581 = float100x_to_int (API->configAPI->GetProfileString ("libsidplayfp", "filtercurve6581", "0.5"));
	config_filterrange6581 = float100x_to_int (API->configAPI->GetProfileString ("libsidplayfp", "filterrange6581", "0.5"));
	config_filtercurve8580 = float100x_to_int (API->configAPI->GetProfileString ("libsidplayfp", "filtercurve8580", "0.5"));
	config_combinedwaveforms = CWS_to_int     (API->configAPI->GetProfileString ("libsidplayfp", "combinedwaveforms", "Average"));
	config_digiboost =                         API->configAPI->GetProfileBool   ("libsidplayfp", "digiboost",       0, 0);
	config_kernal = strdup                    (API->configAPI->GetProfileString ("libsidplayfp", "kernal",          "KERNEL.ROM"));
	config_basic = strdup                     (API->configAPI->GetProfileString ("libsidplayfp", "basic",           "BASIC.ROM"));
	config_chargen = strdup                   (API->configAPI->GetProfileString ("libsidplayfp", "chargen",         "CHARGEN.ROM"));

	if (config_filterbias < -5000) config_filterbias = -5000;
	if (config_filterbias > 5000) config_filterbias = 5000;
	if (config_filtercurve6581 < 0) config_filtercurve6581 = 0;
	if (config_filtercurve6581 > 100) config_filtercurve6581 = 100;
	if (config_filterrange6581 < 0) config_filterrange6581 = 0;
	if (config_filterrange6581 > 100) config_filterrange6581 = 100;
	if (config_filtercurve8580 < 0) config_filtercurve8580 = 0;
	if (config_filtercurve8580 > 100) config_filtercurve8580 = 100;

	entry_kernal.dirdb_ref  = API->dirdb->ResolvePathWithBaseAndRef (dirdb_base, config_kernal,  DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
	entry_basic.dirdb_ref   = API->dirdb->ResolvePathWithBaseAndRef (dirdb_base, config_basic,   DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
	entry_chargen.dirdb_ref = API->dirdb->ResolvePathWithBaseAndRef (dirdb_base, config_chargen, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);

	rom_md5 (entry_kernal .hash_8192, entry_kernal .dirdb_ref, 8192, API);
	rom_md5 (entry_kernal .hash_4096, entry_kernal .dirdb_ref, 4096, API);
	rom_md5 (entry_basic  .hash_8192, entry_basic  .dirdb_ref, 8192, API);
	rom_md5 (entry_basic  .hash_4096, entry_basic  .dirdb_ref, 4096, API);
	rom_md5 (entry_chargen.hash_8192, entry_chargen.dirdb_ref, 8192, API);
	rom_md5 (entry_chargen.hash_4096, entry_chargen.dirdb_ref, 4096, API);

	while (1)
	{
		API->fsDraw();
		sidConfigDraw (esel, API);
		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();
			static uint32_t lastpress = 0;
			static uint16_t lastkey = 0;
			static int repeat;
			if ((key != KEY_LEFT) && (key != KEY_RIGHT) && (key != lastkey))
			{
				lastkey = key;
				lastpress = 0;
				repeat = 1;
			} else {
				uint32_t newpress = clock_ms();
				if ((newpress-lastpress) > 250) /* 125 ms */
				{
					repeat = 1;
				} else {
					if (esel == 7)
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
				case _KEY_ENTER:
					if ((esel == 13) || (esel == 14) || (esel == 15))
					{
						uint32_t dir_ref;
						int dsel = 0;
						int inner = 1;

						if (esel == 13)
						{
							dir_ref = API->dirdb->GetParentAndRef (entry_kernal.dirdb_ref, dirdb_use_dir);
							refresh_dir (dir_ref, entry_kernal.dirdb_ref, &dsel, API);
						} else if (esel == 14)
						{
							dir_ref = API->dirdb->GetParentAndRef (entry_basic.dirdb_ref, dirdb_use_dir);
							refresh_dir (dir_ref, entry_basic.dirdb_ref, &dsel, API);
						} else {
							dir_ref = API->dirdb->GetParentAndRef (entry_chargen.dirdb_ref, dirdb_use_dir);
							refresh_dir (dir_ref, entry_chargen.dirdb_ref, &dsel, API);
						}

#if 0
						{ /* preselect dsel */
							const char *configfile = API->configAPI->GetProfileString ("timidity", "configfile", "");
							int i;
							if (configfile[0])
							{
								for (i=0; i < global_timidity_count; i++)
								{
									if (!strcmp (configfile, global_timidity_path[i]))
									{
										dsel = i + 1;
										break;
									}
								}
								if (!dsel)
								{
									for (i=0; i < sf2_files_count; i++)
									{
										if (!strcmp (configfile, sf2_files_path[i]))
										{
											dsel = i + 1 + global_timidity_count;
										}
									}
								}
							}
						}
#endif
						while (inner)
						{
							API->fsDraw();
							sidDrawDir (dsel, esel - 13, API);
							while (inner && API->console->KeyboardHit())
							{
								int key = API->console->KeyboardGetChar();
								switch (key)
								{
									case KEY_DOWN:
										if ((dsel + 1) < entries_count)
										{
											dsel++;
										}
										break;
									case KEY_UP:
										if (dsel)
										{
											dsel--;
										}
										break;
									case KEY_EXIT:
									case KEY_ESC:
										inner = 0;
										break;

									case _KEY_ENTER:
										if (
											entries_data[dsel].isdir
#ifdef _WIN32
											|| entries_data[dsel].isdrive
#endif
										)
										{
											uint32_t dir = entries_data[dsel].dirdb_ref;
											API->dirdb->Ref (dir, dirdb_use_file);
											refresh_dir (dir, dir_ref, &dsel, API);
											API->dirdb->Unref (dir_ref, dirdb_use_file);
											dir_ref = dir;
										} else {
											char *newpath = 0;
											char *config = 0;
											char *path = 0;
											API->dirdb->GetFullname_malloc (entries_data[dsel].dirdb_ref, &path, DIRDB_FULLNAME_NODRIVE);
											API->dirdb->GetFullname_malloc (dirdb_base, &config, DIRDB_FULLNAME_NODRIVE | DIRDB_FULLNAME_ENDSLASH);
											if (!strncmp (path, config, strlen (config)))
											{
												newpath = malloc (1 + strlen (path) - strlen (config));
												if (newpath)
												{
													sprintf (newpath, "%s", path + strlen (config));
												}
#ifdef _WIN32
											} else if (!strncasecmp (path, API->configAPI->HomePath, strlen (API->configAPI->HomePath)))
#else
											} else if (!strncmp (path, API->configAPI->HomePath, strlen (API->configAPI->HomePath)))
#endif
											{
												newpath = malloc (3 + strlen (path) - strlen (API->configAPI->HomePath));
												if (newpath)
												{
													sprintf (newpath, "~/%s", path + strlen (API->configAPI->HomePath));
												}
											} else {
												newpath = path;
												path = 0;
											}
											if (newpath)
											{
												if (esel == 13)
												{
													API->dirdb->Unref (entry_kernal.dirdb_ref, dirdb_use_file);
													entry_kernal = entries_data[dsel];
													API->dirdb->Ref (entry_kernal.dirdb_ref, dirdb_use_file);
													free (config_kernal);
													config_kernal = newpath;
												} else if (esel == 14)
												{
													API->dirdb->Unref (entry_basic.dirdb_ref, dirdb_use_file);
													entry_basic = entries_data[dsel];
													API->dirdb->Ref (entry_basic.dirdb_ref, dirdb_use_file);
													free (config_basic);
													config_basic = newpath;
												} else { /* esel == 15 */
													API->dirdb->Unref (entry_chargen.dirdb_ref, dirdb_use_file);
													entry_chargen = entries_data[dsel];
													API->dirdb->Ref (entry_chargen.dirdb_ref, dirdb_use_file);
													free (config_chargen);
													config_chargen = newpath;
												}
											}
											free (path);
											free (config);
											inner = 0;
										}
										break;

								}
							}
							API->console->FrameLock ();
						}
						entries_clear (API);
						API->dirdb->Unref (dir_ref, dirdb_use_dir);
					}
					break;
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					esel = key - '1';
					break;
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
					esel = key - 'a' + 9;
					break;
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
					esel = key - 'A' + 9;
					break;

				case KEY_LEFT:
					switch (esel)
					{
						case 0: config_emulator = 0; break;
						case 1: config_defaultC64 -= (!!config_defaultC64); break;
						case 2: config_forceC64 = 0; break;
						case 3: config_defaultSID -= (!!config_defaultSID); break;
						case 4: config_forceSID = 0; break;
						case 5: config_CIA -= (!!config_CIA); break;
						case 6: config_filter = 0; break;
						case 7:
							config_filterbias -= repeat;
							if (config_filterbias < -5000) config_filterbias = -5000;
							break;
						case 8:
							config_filtercurve6581 -= repeat;
							if (config_filtercurve6581 < 0) config_filtercurve6581 = 0;
							break;
						case 9:
							config_filterrange6581 -= repeat;
							if (config_filterrange6581 < 0) config_filterrange6581 = 0;
							break;
						case 10:
							config_filtercurve8580 -= repeat;
							if (config_filtercurve8580 < 0) config_filtercurve8580 = 0;
							break;
						case 11:
							config_combinedwaveforms -= 1;
							if (config_combinedwaveforms < 0) config_combinedwaveforms = 0;
							break;
						case 12: config_digiboost = 0; break;
						case 13:
						case 14:
						case 15:
							break;
					}
					break;
				case KEY_RIGHT:
					switch (esel)
					{
						case 0: config_emulator = 1; break;
						case 1: config_defaultC64 += (config_defaultC64 != 4); break;
						case 2: config_forceC64 = 1; break;
						case 3: config_defaultSID = 1; break;
						case 4: config_forceSID = 1; break;
						case 5: config_CIA += (config_CIA != 2); break;
						case 6: config_filter = 1; break;
						case 7:
							config_filterbias += repeat;
							if (config_filterbias > 5000) config_filterbias = 5000;
							break;
						case 8:
							config_filtercurve6581 += repeat;
							if (config_filtercurve6581 > 100) config_filtercurve6581 = 100;
							break;
						case 9:
							config_filterrange6581 += repeat;
							if (config_filterrange6581 > 100) config_filterrange6581 = 100;
							break;
						case 10:
							config_filtercurve8580 += repeat;
							if (config_filtercurve8580 > 100) config_filtercurve8580 = 100;
							break;
						case 11:
							config_combinedwaveforms += 1;
							if (config_combinedwaveforms > 2) config_combinedwaveforms = 2;
							break;
						case 12: config_digiboost = 1; break;
						case 13:
						case 14:
						case 15:
							break;
					}
					break;
				case KEY_DOWN:
					if (esel < 15)
					{
						esel++;
					}
					break;
				case KEY_UP:
					if (esel)
					{
						esel--;
					}
					break;
				case KEY_EXIT:
				case KEY_ESC:
					API->configAPI->StoreConfig();
					goto superexit;
					break;
			}
		}
		API->console->FrameLock ();
	}

superexit:

	API->configAPI->SetProfileString ("libsidplayfp", "emulator", emulator_from_int (config_emulator));
	API->configAPI->SetProfileString ("libsidplayfp", "defaultC64", defaultC64_from_int (config_defaultC64));
	API->configAPI->SetProfileBool   ("libsidplayfp", "forceC64", config_forceC64);
	API->configAPI->SetProfileString ("libsidplayfp", "defaultSID", defaultSID_from_int (config_defaultSID));
	API->configAPI->SetProfileBool   ("libsidplayfp", "forceSID", config_forceSID);
	API->configAPI->SetProfileString ("libsidplayfp", "CIA", CIA_from_int (config_CIA));
	API->configAPI->SetProfileBool   ("libsidplayfp", "filter", config_filter);
	API->configAPI->SetProfileString ("libsidplayfp", "filterbias", int_to_float10x(config_filterbias));
	API->configAPI->SetProfileString ("libsidplayfp", "filtercurve6581", int_to_float100x(config_filtercurve6581));
	API->configAPI->SetProfileString ("libsidplayfp", "filterrange6581", int_to_float100x(config_filterrange6581));
	API->configAPI->SetProfileString ("libsidplayfp", "filtercurve8580", int_to_float100x(config_filtercurve8580));
	API->configAPI->SetProfileString ("libsidplayfp", "combinedwaveforms", CWS_from_int (config_combinedwaveforms));
	API->configAPI->SetProfileBool   ("libsidplayfp", "digiboost", config_digiboost);
	API->configAPI->SetProfileString ("libsidplayfp", "kernal",    config_kernal); free (config_kernal);
	API->configAPI->SetProfileString ("libsidplayfp", "basic",     config_basic); free (config_basic);
	API->configAPI->SetProfileString ("libsidplayfp", "chargen",   config_chargen); free (config_chargen);
	API->configAPI->StoreConfig ();

	API->dirdb->Unref (entry_kernal.dirdb_ref, dirdb_use_file);
	API->dirdb->Unref (entry_basic.dirdb_ref, dirdb_use_file);
	API->dirdb->Unref (entry_chargen.dirdb_ref, dirdb_use_file);
}

static struct ocpfile_t *sidconfig; // needs to overlay an dialog above filebrowser, and after that the file is "finished"   Special case of DEVv

static void sidConfigRun (void **token, const struct DevInterfaceAPI_t *API);

OCP_INTERNAL int sid_config_init (struct PluginInitAPI_t *API)
{
	sidconfig = API->dev_file_create (
		API->dmSetup->basedir,
		"sidconfig.dev",
		"libsidplayfp Configuration",
		"",
		0, /* token */
		0, /* Init */
		sidConfigRun,
		0, /* Close */
		0  /* Destructor */
	);

	API->filesystem_setup_register_file (sidconfig);

	return errOk;
}

OCP_INTERNAL void sid_config_done (struct PluginCloseAPI_t *API)
{
	if (sidconfig)
	{
		API->filesystem_setup_unregister_file (sidconfig);
		sidconfig->unref (sidconfig);
		sidconfig = 0;
	}
}
