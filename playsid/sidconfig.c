/* OpenCP Module Player
 * copyright (c) 2022 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-mem.h"
#include "filesel/filesystem-setup.h"
#include "filesel/filesystem-unix.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/framelock.h"
#include "stuff/imsrtns.h"
#include "stuff/utf-8.h"
#include "stuff/poutput.h"

#include "md5.inc.c"

struct browser_t
{
	int isdir;
	int isparent;
	uint32_t dirdb_ref;
	char hash_4096[33];
	char hash_8192[33];
};

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
static int config_filtercurve8580;
static int config_digiboost;
static char *config_kernal;
static char *config_basic;
static char *config_chargen;

struct browser_t entry_kernal;
struct browser_t entry_basic;
struct browser_t entry_chargen;

static int sidConfigInit (struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct interfaceparameters *ip)
{
	return 1;
}

static void ConfigDrawItems (const int lineno, int xpos, const int width, const char **list, const int listlength, const int selected, const int active)
{
	int i;
	int origxpos = xpos;
	for (i=0; i < listlength; i++)
	{
		int l = strlen (list[i]);
		if (selected == i)
		{
			display_nprintf (lineno, xpos, (active)?0x09:0x01, l + 2, "[%.*o%s%.*o]", (active)?0x0f:0x07, list[i], (active)?0x09:0x01);
		} else {
			display_nprintf (lineno, xpos, 0x00, l + 2, " %.*o%s%.0o ", (active)?0x07:0x08, list[i]);
		}
		xpos += l + 2;
	}
	displayvoid (lineno, xpos, width - xpos + origxpos);
}

static void ConfigDrawMenuItems (const int lineno, int xpos, const int width, const char *item, const char **list, const int listlength, const int selected, const int active)
{
	display_nprintf (lineno, xpos, 0x09, 23, "\xb3%.7o %s:", item);
	ConfigDrawItems (lineno, xpos + 23, width - 24, list, listlength, selected, active);
	displaychr (lineno, xpos + width - 1, 0x09, '\xb3', 1);
}

static void ConfigDrawBar (const int lineno, int xpos, int width, int scale, const char *suffix, int minlevel, int maxlevel, int level, const int active)
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
		maxlevel = saturate (maxlevel, -99999, 99999);
	} else {
		level    = saturate (level,    -9999, 9999);
		minlevel = saturate (minlevel, -9999, 9999);
		maxlevel = saturate (maxlevel, -9999, 9999);
	}

	abslevel = abs(level);
	absmin   = abs(minlevel);


	snprintf (prefix, sizeof (prefix), "%4d.%0*d%s",
		level / scale,
		(scale == 100) ? 2 : 1,
		abslevel % scale,
		suffix);

	snprintf (min, sizeof (min), "%4d.%0*d",
		minlevel / scale,
		(scale == 100) ? 2 : 1,
		absmin % scale);
	snprintf (max, sizeof (max), "%3d.%0*d",
		maxlevel / scale,
		(scale == 100) ? 2 : 1,
		maxlevel % scale);

	pos = (/*(maxlevel - minlevel / 46) - 1 +*/ (level - minlevel) * 22) / (maxlevel - minlevel);

	p1 = pos;
        p2 = 22 - pos;

	display_nprintf (lineno, xpos, (active)?0x07:0x08, width, "%10s%-7s [%*C.#%*C.] %-6s", prefix, min, p1, p2, max);
}

static void ConfigDrawMenuBar (const int lineno, int xpos, int width, const char *item, int scale, const char *suffix, int minlevel, int maxlevel, int level, const int active)
{
	display_nprintf (lineno, xpos, 0x09, 23, "\xb3%.7o %s:", item);
	ConfigDrawBar (lineno, xpos + 23, width - 24, scale, suffix, minlevel, maxlevel, level, active);
	displaychr (lineno, xpos + width - 1, 0x09, '\xb3', 1);
}

static void ConfigDrawMenuRom (const int lineno, int xpos, int width, const char *item, int active, const char *path)
{
	display_nprintf (lineno, xpos, 0x09, width, "\xb3%.7o %20s %.*o%*S%.9o\xb3",
		item,
		active?0x0f:0x08,
		width - 24,
		path);
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

static void ConfigDrawHashInfo (const int lineno, int xpos, int width, const char *hash_8192, const char *hash_4096, int expect)
{
	const uint8_t COK = 0x02;
	const uint8_t CERR = 0x04;
	int j;

	for (j=0; j < (sizeof (hash_kernal) / sizeof (hash_kernal[0])); j++)
	{
		if (!strcmp (hash_8192, hash_kernal[j].hash))
		{
			displaystr (lineno, xpos, (expect==0)?COK:CERR, hash_kernal[j].description, width);
			return;
		}
	}
	for (j=0; j < (sizeof (hash_basic) / sizeof (hash_basic[0])); j++)
	{
		if (!strcmp (hash_8192, hash_basic[j].hash))
		{
			displaystr (lineno, xpos, (expect==1)?COK:CERR, hash_basic[j].description, width);
			return;
		}
	}
	for (j=0; j < (sizeof (hash_chargen) / sizeof (hash_chargen[0])); j++)
	{
		if (!strcmp (hash_4096, hash_chargen[j].hash))
		{
			displaystr (lineno, xpos, (expect==2)?COK:CERR, hash_chargen[j].description, width);
			return;
		}
	}

	displaystr (lineno, xpos, CERR, "Unknown ROM file", width); return;
}

static void ConfigDrawHashMenuInfo (const int lineno, int xpos, int width, const char *hash_8192, const char *hash_4096, int expect)
{
	display_nprintf (lineno, xpos, 0x09, 25, "\xb3%.7o");
	ConfigDrawHashInfo (lineno, xpos + 25, width - 26, hash_8192, hash_4096, expect);
	displaychr (lineno, xpos + width - 1, 0x09, '\xb3', 1);

}

#if 0
 +----------------------------------------------------------------------------\
 |                     libsidplayfp configuration                             |
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

static void sidConfigDraw (int EditPos)
{
	const int mlHeight = 25;
	int mlTop, mlLeft, mlWidth;
	const char *offon[] = {"off", "on"};
	const char *emulators[] = {"resid", "residfp"};
	const char *C64models[] = {"PAL", "NTSC", "OLD-NTSC", "DREAN", "PAL-M"};
	const char *SIDmodels[] = {"MOS6581", "MOS8580"};
	const char *CIAmodels[] = {"MOS6526", "MOS6526W4485", "MOS8521"};

	mlWidth = 78 + (plScrWidth - 80) * 2 / 3;
	mlTop = (plScrHeight - mlHeight) / 2;
	mlLeft = (plScrWidth - mlWidth) / 2;

	display_nprintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xda%*C\xc4\xbf", mlWidth - 2);

	display_nprintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o                     libsidplayfp configuration%*C %.9o\xb3", mlWidth - 49);
	display_nprintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%.15o  Navigate with  %.15o<\x18>%.7o,%.15o<\x19>%.7o,%.15o<\x1a>%.7o,%.15o<\x1b>%.7o and %.15o<ENTER>%.7o; hit %.15o<ESC>%.7o to save and exit.%*C %.9o\xb3", mlWidth - 75);

	display_nprintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2);

	ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, " 1: emulator", emulators, 2, config_emulator, EditPos==0);

	ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, " 2: default C64", C64models, 5, config_defaultC64, EditPos==1);

	ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, " 3: force C64 model", offon, 2, config_forceC64, EditPos==2);

	ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, " 4: default SID", SIDmodels, 2, config_defaultSID, EditPos==3);

	ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, " 5: force SID", offon, 2, config_forceSID, EditPos==4);

	ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, " 6: CIA", CIAmodels, 3, config_CIA, EditPos==5);

	ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, " 7: filter", offon, 2, config_filter, EditPos==6);

	ConfigDrawMenuBar      (mlTop++, mlLeft, mlWidth, " 8: filterbias", 10, "mv", -5000, 5000, config_filterbias, EditPos==7);

	ConfigDrawMenuBar      (mlTop++, mlLeft, mlWidth, " 9: filtercurve6581", 100, "", -0, 100, config_filtercurve6581, EditPos==8);

	ConfigDrawMenuBar      (mlTop++, mlLeft, mlWidth, "10: filtercurve8580", 100, "", 0, 100, config_filtercurve8580, EditPos==9);

	ConfigDrawMenuItems    (mlTop++, mlLeft, mlWidth, "11: digiboost", offon, 2, config_digiboost, EditPos==10);

	ConfigDrawMenuRom      (mlTop++, mlLeft, mlWidth, "12: kernal.rom:", EditPos==11, config_kernal);

	ConfigDrawHashMenuInfo (mlTop++, mlLeft, mlWidth, entry_kernal.hash_8192, entry_kernal.hash_4096, 0);

	ConfigDrawMenuRom      (mlTop++, mlLeft, mlWidth, "13: basic.rom:", EditPos==12, config_basic);

	ConfigDrawHashMenuInfo (mlTop++, mlLeft, mlWidth, entry_basic.hash_8192, entry_basic.hash_4096, 1);

	ConfigDrawMenuRom      (mlTop++, mlLeft, mlWidth, "14: chargen.rom", EditPos==13, config_chargen);

	ConfigDrawHashMenuInfo (mlTop++, mlLeft, mlWidth, entry_chargen.hash_8192, entry_chargen.hash_4096, 2);

	display_nprintf        (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2);
	switch (EditPos)
	{
		case 0:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o resid   = standard integer emulator - fastest.%*C %.9o\xb3", mlWidth - 49);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o residfp = floating point emulator - better quality but slower.%*C %.9o\xb3", mlWidth - 65);
			break;
		case 1:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o For SID files that does not specify C64 model.%*C %.9o\xb3", mlWidth - 49);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 2:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Override C64 model specified in SID files (if specified).%*C %.9o\xb3", mlWidth - 60);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 3:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o For SID files that does not specify SID model.%*C %.9o\xb3", mlWidth - 49);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 4:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Override SID model specified in SID files (if specified).%*C %.9o\xb3", mlWidth - 60);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 5:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Specify which CIA chip to emulate. MOS6526 is the classic model where%*C %.9o\xb3", mlWidth - 72);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o MOS6526W4485 is a specific batch. MOS8521 is the modern chip model.%*C %.9o\xb3", mlWidth - 70);
			break;
		case 6:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Enable post-processing filtering.%*C %.9o\xb3", mlWidth - 36);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 7:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Default is 0.0mV%*C %.9o\xb3", mlWidth - 19);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o%*C %.9o\xb3", mlWidth - 2);
			break;
		case 8:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Value to use if SID is MOS6581.%*C %.9o\xb3", mlWidth - 34);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Default is 0.5%*C %.9o\xb3", mlWidth - 17);
			break;
		case 9:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Value to use if SID is MOS8580.%*C %.9o\xb3", mlWidth - 34);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Default is 0.5%*C %.9o\xb3", mlWidth - 17);
			break;
		case 10:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Digi-Boost is a hardware feature only available on MOS8580, where the%*C %.9o\xb3", mlWidth - 72);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o digital playback would be boosted.%*C %.9o\xb3", mlWidth - 37);
			break;
		case 11:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o KERNEL.ROM images can be found online. Some SID files requires this file%*C %.9o\xb3", mlWidth - 75);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o in order to play correctly.%*C %.9o\xb3", mlWidth - 30);
			break;
		case 12:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o BASIC.ROM images can be found online. Some SID files requires this file%*C %.9o\xb3", mlWidth - 74);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o in order to play correctly.%*C %.9o\xb3", mlWidth - 30);
			break;
		case 13:
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o CHARGEN.ROM images can be found online. Some SID files requires this file%*C %.9o\xb3", mlWidth - 76);
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o in order to play correctly.%*C %.9o\xb3", mlWidth - 30);
			break;
		default: /* should not be reachable */
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%76C \xb3");
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%76C \xb3");
			break;
	}
	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
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
	char *r = index (src, '.');
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
	char *r = index (src, '.');
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

static void rom_md5 (char md5[33], uint32_t dirdb_ref, int size)
{
	char *romPath = 0;
	int fd;
	uint8_t buffer[4096];
	MD5_CTX ctx;

	md5[0] = '-';
	md5[1] = 0;
	md5[32] = 0;

#ifdef __W32__
	#error we need to make flags, so we can reverse the slashes
	dirdbGetFullname_malloc (dirdb_ref, &romPath, DIRDB_FULLNAME_DRIVE);
#else
	dirdbGetFullname_malloc (dirdb_ref, &romPath, DIRDB_FULLNAME_NODRIVE);
#endif

	fd = open (romPath, O_RDONLY);
	free (romPath);
	if (fd < 0)
	{
		return;
	}

	MD5Init (&ctx);

	while (size)
	{
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
	}
	close (fd);
	MD5Final (md5, &ctx);
}

static void sidDrawDir (const int esel, const int expect)
{
	int contentheight = entries_count;
	int i;
	int files = 0;
	int dirs = 0;
	int contentsel;

	int half;
	int skip;
	int dot;

	const int mlHeight = 24;
	const int LINES_NOT_AVAILABLE = 2;
	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlWidth = 78 + (plScrWidth - 80) * 2 / 3;
	int mlLeft = (plScrWidth - mlWidth) / 2;

	for (i=0; i < entries_count; i++)
	{
		if (entries_data[i].isdir)
		{
			dirs++;
		} else {
			files++;
		}
	}
	contentheight = dirs + files * 2;

	if (esel >= dirs)
	{
		contentsel = dirs + (esel - dirs - 1) * 2;
	} else {
		contentsel = esel;
	}

	half = (mlHeight - LINES_NOT_AVAILABLE) / 2;

	if (contentheight <= (mlHeight - LINES_NOT_AVAILABLE))
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (contentsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (contentsel >= (contentheight - half))
	{ /* we are at the bottom part */
		skip = contentheight - (mlHeight - LINES_NOT_AVAILABLE);
		dot = mlHeight - LINES_NOT_AVAILABLE - 1;
	} else {
		skip = contentsel - half;
		dot = skip * (mlHeight - LINES_NOT_AVAILABLE) / (contentheight - (mlHeight - LINES_NOT_AVAILABLE));
	}

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda%*C\xc4\xbf", mlWidth - 2);

	for (i = 0; i < (mlHeight-LINES_NOT_AVAILABLE); i++)
	{
		int line = skip + i;
		int masterindex;
		int subindex = 0;
		if (line < dirs)
		{
			masterindex = line;
		} else {
			masterindex = dirs + (line - dirs) / 2;
			subindex = (line - dirs) % 2;
		}
		if (masterindex >= entries_count)
		{
			display_nprintf  (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C %c", mlWidth - 2, (i==dot)?'\xdd':'\xb3');
		} else if (entries_data[masterindex].isdir)
		{
			if (entries_data[masterindex].isparent)
			{
				display_nprintf  (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*.*o..%*C %0.9o%c", (masterindex==esel)?0x6:0x0, (masterindex==esel)?0x1:0x1, mlWidth - 4, (i==dot)?'\xdd':'\xb3');
			} else {
				const char *dirname;
				dirdbGetName_internalstr (entries_data[masterindex].dirdb_ref, &dirname);
				display_nprintf  (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*.*o%*S%0.9o%c", (masterindex==esel)?0x6:0x0, (masterindex==esel)?0x1:0x1, mlWidth - 2, dirname, (i==dot)?'\xdd':'\xb3');
			}
		} else {
			if (subindex)
			{
				displaychr (mlTop, mlLeft, 0x09, '\xb3', 1);
				ConfigDrawHashInfo (mlTop, mlLeft + 1, mlWidth - 2, entries_data[masterindex].hash_8192, entries_data[masterindex].hash_4096, expect);
				displaychr (mlTop++, mlLeft+mlWidth-1, 0x09, (i==dot)?'\xdd':'\xb3', 1);
			} else {
				const char *filename;
				dirdbGetName_internalstr (entries_data[masterindex].dirdb_ref, &filename);
				display_nprintf  (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*.*o%*S%0.9o%c", (masterindex==esel)?0x6:0x0, (masterindex==esel)?0x0:0x7, mlWidth - 2, filename, (i==dot)?'\xdd':'\xb3');
			}
		}
	}

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

static void entries_clear (void)
{
	int i;
	for (i=0; i < entries_count; i++)
	{
		dirdbUnref (entries_data[i].dirdb_ref, dirdb_use_file);
	}
	free (entries_data);
	entries_count = 0;
	entries_size = 0;
	entries_data = 0;
}

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

	dirdbGetName_internalstr (p1->dirdb_ref, &n1);
	dirdbGetName_internalstr (p2->dirdb_ref, &n2);
	return strcmp (n1, n2);
}

static void entries_sort (void)
{
	qsort (entries_data, entries_count, sizeof (entries_data[0]), cmp);
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

static void entries_append_parent (uint32_t ref)
{
	if (entries_append())
	{
		dirdbUnref (ref, dirdb_use_file);
		return;
	}
	entries_data[entries_count].isdir = 1;
	entries_data[entries_count].isparent = 1;
	entries_data[entries_count].dirdb_ref = ref;
	entries_data[entries_count].hash_4096[0] = 0;
	entries_data[entries_count].hash_8192[0] = 0;
	entries_count++;
}

static void entries_append_dir (uint32_t ref)
{
	if (entries_append())
	{
		dirdbUnref (ref, dirdb_use_file);
		return;
	}
	entries_data[entries_count].isdir = 1;
	entries_data[entries_count].isparent = 0;
	entries_data[entries_count].dirdb_ref = ref;
	entries_data[entries_count].hash_4096[0] = 0;
	entries_data[entries_count].hash_8192[0] = 0;
	entries_count++;
}

static void entries_append_file (uint32_t ref, const char *hash_4096, const char *hash_8192)
{
	if (entries_append())
	{
		dirdbUnref (ref, dirdb_use_file);
		return;
	}
	entries_data[entries_count].isdir = 0;
	entries_data[entries_count].isparent = 0;
	entries_data[entries_count].dirdb_ref = ref;
	strcpy (entries_data[entries_count].hash_4096, hash_4096);
	strcpy (entries_data[entries_count].hash_8192, hash_8192);
	entries_count++;
}

static void refresh_dir (uint32_t ref, uint32_t old, int *esel)
{
	char *romPath = 0;
	DIR *d;
	int i;

	*esel = 0;

#ifdef __W32__
	#error we need to make flags, so we can reverse the slashes
	dirdbGetFullname_malloc (ref, &romPath, DIRDB_FULLNAME_DRIVE);
#else
	dirdbGetFullname_malloc (ref, &romPath, DIRDB_FULLNAME_NODRIVE);
#endif

	entries_clear();

	{
		uint32_t dir_ref = dirdbGetParentAndRef (ref, dirdb_use_file);
		if (dir_ref != UINT32_MAX)
		{
			entries_append_parent (dir_ref);
		}
	}

	d = opendir (romPath);
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
				char *temp = malloc (strlen (romPath) + 1 + strlen (de->d_name) + 1);
				int res;
				if (!temp)
				{
					continue;
				}
				sprintf (temp, "%s/%s", romPath, de->d_name);
				res = stat (temp, &st);
				free (temp);
				if (res < 0)
				{
					continue;
				}
			}
			if ((st.st_mode & S_IFMT) == S_IFDIR)
			{
				entries_append_dir (dirdbFindAndRef (ref, de->d_name, dirdb_use_file));
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
				newref = dirdbFindAndRef (ref, de->d_name, dirdb_use_file);
				rom_md5 (md5_4096, newref, 4096);
				rom_md5 (md5_8192, newref, 8192);
				entries_append_file (newref, md5_4096, md5_8192);
			}
		}
		closedir (d);
	}
	free (romPath);
	entries_sort ();
	for (i=0; i < entries_count; i++)
	{
		if (entries_data[i].dirdb_ref == old)
		{
			*esel = i;
			return;
		}
	}
}

static interfaceReturnEnum sidConfigRun (void)
{
	int esel = 0;

	uint32_t dirdb_base = cfConfigDir_dirdbref;

	config_emulator = emulator_to_int         (cfGetProfileString ("libsidplayfp", "emulator",        "residfp"));
	config_defaultC64 = defaultC64_to_int     (cfGetProfileString ("libsidplayfp", "defaultC64",      "PAL"));
	config_forceC64 =                          cfGetProfileBool   ("libsidplayfp", "forceC64",        0, 0);
	config_defaultSID = defaultSID_to_int     (cfGetProfileString ("libsidplayfp", "defaultSID",      "MOS6581"));
	config_forceSID =                          cfGetProfileBool   ("libsidplayfp", "forceSID",        0, 0);
	config_CIA = CIA_to_int                   (cfGetProfileString ("libsidplayfp", "CIA",             "MOS6526"));
	config_filter =                            cfGetProfileBool   ("libsidplayfp", "filter",          1, 1);
	config_filterbias = float10x_to_int       (cfGetProfileString ("libsidplayfp", "filterbias",      "0.0"));
	config_filtercurve6581 = float100x_to_int (cfGetProfileString ("libsidplayfp", "filtercurve6581", "0.5"));
	config_filtercurve8580 = float100x_to_int (cfGetProfileString ("libsidplayfp", "filtercurve8580", "0.5"));
	config_digiboost =                         cfGetProfileBool   ("libsidplayfp", "digiboost",       0, 0);
	config_kernal = strdup                    (cfGetProfileString ("libsidplayfp", "kernal",          "KERNEL.ROM"));
	config_basic = strdup                     (cfGetProfileString ("libsidplayfp", "basic",           "BASIC.ROM"));
	config_chargen = strdup                   (cfGetProfileString ("libsidplayfp", "chargen",         "CHARGEN.ROM"));

	if (config_filterbias < -5000) config_filterbias = -5000;
	if (config_filterbias > 5000) config_filterbias = 5000;
	if (config_filtercurve6581 < 0) config_filtercurve6581 = 0;
	if (config_filtercurve6581 > 100) config_filtercurve6581 = 100;
	if (config_filtercurve8580 < 0) config_filtercurve8580 = 0;
	if (config_filtercurve8580 > 100) config_filtercurve8580 = 100;

	entry_kernal.dirdb_ref  = dirdbResolvePathWithBaseAndRef (dirdb_base, config_kernal,  DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
	entry_basic.dirdb_ref   = dirdbResolvePathWithBaseAndRef (dirdb_base, config_basic,   DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
	entry_chargen.dirdb_ref = dirdbResolvePathWithBaseAndRef (dirdb_base, config_chargen, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);

	rom_md5 (entry_kernal.hash_8192, entry_kernal.dirdb_ref, 8192);
	rom_md5 (entry_kernal.hash_4096, entry_kernal.dirdb_ref, 4096);
	rom_md5 (entry_basic.hash_8192, entry_basic.dirdb_ref, 8192);
	rom_md5 (entry_basic.hash_4096, entry_basic.dirdb_ref, 4096);
	rom_md5 (entry_chargen.hash_8192, entry_chargen.dirdb_ref, 8192);
	rom_md5 (entry_chargen.hash_4096, entry_chargen.dirdb_ref, 4096);

	while (1)
	{
		fsDraw();
		sidConfigDraw (esel);
		while (ekbhit())
		{
			int key = egetch();
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
						if (repeat < 100)
						{
							repeat += 1;
						}
					} else {
						if (repeat < 10)
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
					if ((esel == 11) || (esel == 12) || (esel == 13))
					{
						uint32_t dir_ref;
						int dsel = 0;
						int inner = 1;

						if (esel == 11)
						{
							dir_ref = dirdbGetParentAndRef (entry_kernal.dirdb_ref, dirdb_use_dir);
							refresh_dir (dir_ref, entry_kernal.dirdb_ref, &dsel);
						} else if (esel == 12)
						{
							dir_ref = dirdbGetParentAndRef (entry_basic.dirdb_ref, dirdb_use_dir);
							refresh_dir (dir_ref, entry_basic.dirdb_ref, &dsel);
						} else {
							dir_ref = dirdbGetParentAndRef (entry_chargen.dirdb_ref, dirdb_use_dir);
							refresh_dir (dir_ref, entry_chargen.dirdb_ref, &dsel);
						}

#if 0
						{ /* preselect dsel */
							const char *configfile = cfGetProfileString ("timidity", "configfile", "");
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
							fsDraw();
							sidDrawDir (dsel, esel - 11);
							while (inner && ekbhit())
							{
								int key = egetch();
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
										if (entries_data[dsel].isdir)
										{
											uint32_t dir = entries_data[dsel].dirdb_ref;
											dirdbRef (dir, dirdb_use_file);
											refresh_dir (dir, dir_ref, &dsel);
											dirdbUnref (dir_ref, dirdb_use_file);
											dir_ref = dir;
										} else {
											char *newpath = 0;
											char *home = 0;
											char *config = 0;
											char *path = 0;
											dirdbGetFullname_malloc (entries_data[dsel].dirdb_ref, &path, DIRDB_FULLNAME_NODRIVE);
											dirdbGetFullname_malloc (dirdb_base, &config, DIRDB_FULLNAME_NODRIVE | DIRDB_FULLNAME_ENDSLASH);
											{
												char *_home = getenv("HOME");
												if (_home && strlen (_home))
												{
													home = malloc (strlen (_home) + 2);
													if (home)
													{
														sprintf(home, "%s%s", _home, (_home[strlen(_home)-1]=='/')?"":"/");
													}
												}
											}
											if (!strncmp (path, config, strlen (config)))
											{
												newpath = malloc (1 + strlen (path) - strlen (config));
												if (newpath)
												{
													sprintf (newpath, "%s", path + strlen (config));
												}
											} else if (home && (!strncmp (path, home, strlen (home))))
											{
												newpath = malloc (3 + strlen (path) - strlen (home));
												if (newpath)
												{
													sprintf (newpath, "~/%s", path + strlen (home));
												}
											} else {
												newpath = path;
												path = 0;
											}
											if (newpath)
											{
												if (esel == 11)
												{
													dirdbUnref (entry_kernal.dirdb_ref, dirdb_use_file);
													entry_kernal = entries_data[dsel];
													dirdbRef (entry_kernal.dirdb_ref, dirdb_use_file);
													free (config_kernal);
													config_kernal = newpath;
												} else if (esel == 12)
												{
													dirdbUnref (entry_basic.dirdb_ref, dirdb_use_file);
													entry_basic = entries_data[dsel];
													dirdbRef (entry_basic.dirdb_ref, dirdb_use_file);
													free (config_basic);
													config_basic = newpath;
												} else { /* esel == 13 */
													dirdbUnref (entry_chargen.dirdb_ref, dirdb_use_file);
													entry_chargen = entries_data[dsel];
													dirdbRef (entry_chargen.dirdb_ref, dirdb_use_file);
													free (config_chargen);
													config_chargen = newpath;
												}
											}
											free (path);
											free (home);
											free (config);
											inner = 0;
										}
										break;

								}
							}
							framelock ();
						}
						entries_clear();
						dirdbUnref (dir_ref, dirdb_use_dir);
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
					esel = key - 'a' + 9;
					break;
				case 'A':
				case 'B':
				case 'C':
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
							config_filtercurve8580 -= repeat;
							if (config_filtercurve8580 < 0) config_filtercurve8580 = 0;
							break;
						case 10: config_digiboost = 0; break;
						case 11:
						case 12:
						case 13:
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
							config_filtercurve8580 += repeat;
							if (config_filtercurve8580 > 100) config_filtercurve8580 = 100;
							break;
						case 10: config_digiboost = 1; break;
						case 11:
						case 12:
						case 13:
							break;
					}
					break;
				case KEY_DOWN:
					if (esel < 13)
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
					cfStoreConfig();
					goto superexit;
					break;
			}
		}
		framelock ();
	}

superexit:

	cfSetProfileString ("libsidplayfp", "emulator", emulator_from_int (config_emulator));
	cfSetProfileString ("libsidplayfp", "defaultC64", defaultC64_from_int (config_defaultC64));
	cfSetProfileBool   ("libsidplayfp", "forceC64", config_forceC64);
	cfSetProfileString ("libsidplayfp", "defaultSID", defaultSID_from_int (config_defaultSID));
	cfSetProfileBool   ("libsidplayfp", "forceSID", config_forceSID);
	cfSetProfileString ("libsidplayfp", "CIA", CIA_from_int (config_CIA));
	cfSetProfileBool   ("libsidplayfp", "filter", config_filter);
	cfSetProfileString ("libsidplayfp", "filterbias", int_to_float10x(config_filterbias));
	cfSetProfileString ("libsidplayfp", "filtercurve6581", int_to_float100x(config_filtercurve6581));
	cfSetProfileString ("libsidplayfp", "filtercurve8580", int_to_float100x(config_filtercurve8580));
	cfSetProfileBool   ("libsidplayfp", "digiboost", config_digiboost);
	cfSetProfileString ("libsidplayfp", "kernal",    config_kernal); free (config_kernal);
	cfSetProfileString ("libsidplayfp", "basic",     config_basic); free (config_basic);
	cfSetProfileString ("libsidplayfp", "chargen",   config_chargen); free (config_chargen);
	cfStoreConfig ();

	dirdbUnref (entry_kernal.dirdb_ref, dirdb_use_file);
	dirdbUnref (entry_basic.dirdb_ref, dirdb_use_file);
	dirdbUnref (entry_chargen.dirdb_ref, dirdb_use_file);

	return interfaceReturnNextAuto;
}

static struct ocpfile_t      *sidconfig; // needs to overlay an dialog above filebrowser, and after that the file is "finished"   Special case of DEVv

static int                    sidConfigInit (struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct interfaceparameters *ip);
static interfaceReturnEnum    sidConfigRun  (void);
static struct interfacestruct sidConfigIntr = {sidConfigInit, sidConfigRun, 0, "libsidplayfp Config" INTERFACESTRUCT_TAIL};

static int sid_config_init (void)
{
	struct moduleinfostruct m;
	uint32_t mdbref;

	sidconfig= mem_file_open (dmSetup->basedir, dirdbFindAndRef (dmSetup->basedir->dirdb_ref, "sidconfig.dev", dirdb_use_file), strdup (sidConfigIntr.name), strlen (sidConfigIntr.name));
	dirdbUnref (sidconfig->dirdb_ref, dirdb_use_file);
	mdbref = mdbGetModuleReference2 (sidconfig->dirdb_ref, strlen (sidConfigIntr.name));
	mdbGetModuleInfo (&m, mdbref);
	m.modtype.integer.i = MODULETYPE("DEVv");
	strcpy (m.title, "libsidplayfp Configuration");
	mdbWriteModuleInfo (mdbref, &m);
	filesystem_setup_register_file (sidconfig);
	plRegisterInterface (&sidConfigIntr);

	return errOk;
}

static void sid_config_done (void)
{
	plUnregisterInterface (&sidConfigIntr);
	if (sidconfig)
	{
		filesystem_setup_unregister_file (sidconfig);
		sidconfig = 0;
	}
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "sidconfig", .desc = "OpenCP libsidplayfp configuration (c) 2022 Stian Skjelstad", .ver = DLLVERSION, .size = 0, .Init = sid_config_init, .Close = sid_config_done};
