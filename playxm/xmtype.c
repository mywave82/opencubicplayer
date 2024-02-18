/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * XMPlay file type detection routines for the fileselector
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
 *
 * revision history: (please note changes here)
 *  -kb980717   Tammo Hinrichs <opencp@groove.org>
 *    -first release
 *    -separated this code from gmdptype.cpp
 *    -added 0x1a and version number checking
 *    -added MXM file type
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/pfilesel.h"
#include "filesel/mdb.h"
#include "stuff/cp437.h"
#include "stuff/err.h"
#include "xmtype.h"

static uint32_t xmpGetModuleType(const char *buf, int len, const char *filename)
{
	if ((strlen(filename)>4) &&
	    (!strcasecmp(filename + strlen (filename) - 4,".WOW")) &&
	    (*(uint32_t *)(buf+1080)==int32_little(0x2E4B2E4D))) /* "M.K." */
	{
		return MODULETYPE("WOW");
	}

	if (len >= 1084)
	{
		switch (int32_little(*(uint32_t *)(buf+1080)))
		{
			case 0x2E542E4E: /* N.T. */
				return MODULETYPE("MODt");

			case 0x2E4B2E4D: case 0x214B214D: case 0x34544C46:                  /* "M.K." "M!K!" "FLT4" */
			case 0x4E484331: case 0x4E484332: case 0x4E484333: case 0x4E484334: /* "1CHN" "2CHN" "3CHN" "4CHN" */
			case 0x4E484335: case 0x4E484336: case 0x4E484337: case 0x4E484338: /* "5CHN" "6CHN" "7CHN" "8CHN" */
			case 0x4E484339: case 0x48433031: case 0x48433131: case 0x48433231: /* "9CHN" "10CH" "11CH" "12CH" */
			case 0x48433331: case 0x48433431: case 0x48433531: case 0x48433631: /* "13CH" "14CH" "15CH" "16CH" */
			case 0x48433731: case 0x48433831: case 0x48433931: case 0x48433032: /* "17CH" "18CH" "19CH" "20CH" */
			case 0x48433132: case 0x48433232: case 0x48433332: case 0x48433432: /* "21CH" "22CH" "23CH" "24CH" */
			case 0x48433532: case 0x48433632: case 0x48433732: case 0x48433832: /* "25CH" "26CH" "27CH" "28CH" */
			case 0x48433932: case 0x48433033: case 0x48433133: case 0x48433233: /* "29CH" "30CH" "31CH" "32CH" */
			{
				return MODULETYPE("MOD");
			}
		}
	}

	if (!memcmp(buf, "Extended Module: ", 17)/* && buf[37]==0x1a*/) /* some malformed trackers doesn't save the magic 0x1a at offset 37 */
	{
		return MODULETYPE("XM");
	}

	if (!memcmp(buf, "MXM", 4))
	{
		return MODULETYPE("MXM");
	}

	if ((strlen(filename)>4) &&
	    (!strcasecmp(filename + strlen (filename) - 4,".MOD")))
	{
		int i,j;

		/* Check title for ASCII */
		for (i=0; i<20; i++)
		{
			if (buf[i]) /* string is zero-terminated */
			{
				break;
			} else {
				if (buf[i]<0x20) /* non-ASCII?, can not be mtM15/mtM31 */
				{
					return 0;
				}
			}
		}
		/* Check instruments for ASCII*/
		for (i=0; i<31; i++)
		{
			for (j=0; j<21; j++)
			{
				if ((20+i*30+j) >= len)
				{
					//fprintf (stderr, "buffer too small");
					return 0;
				}
				if (!(buf[20+i*30+j]))
				{
					break;
				}
				if (((signed char *)buf)[20+i*30+j]<0x20) /* non-ASCII? */
				{
					//fprintf (stderr, "instrument %d has problem at character %d\n", i, j);
					goto out;
				}
			}
		}
out:
		if (i >= 31) return MODULETYPE("M31");
		if (i >= 15) return MODULETYPE("M15");
	}
	return 0;
}

static int xmpReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	const char *filename = 0;
	uint32_t type;
	typedef struct __attribute__((packed))
	{
		char sig[17];
		char name[20];
		char eof;
		char tracker[20];
		uint16_t ver;
		uint32_t hdrsize;
	} head1;
	head1 *xmhdr;

	if (!memcmp(buf, "ziRCONia", 8))
	{
		strcpy(m->title, "MMCMPed module");
		return 0;
	}

	API->dirdb->GetName_internalstr (fp->dirdb_ref, &filename);
	type=xmpGetModuleType(buf, len, filename);
	if (!type)
	{
		return 0;
	}
	m->modtype.integer.i=type;

	if ((type == MODULETYPE("M15")) ||
	    (type == MODULETYPE("M15t")) ||
	    (type == MODULETYPE("M31")) )
	{
		m->channels=4;
		API->cp437_f_to_utf8_z (buf + 0, 20, m->title, sizeof (m->title));
		return 0; /* not a hard hit.... */
	} else if (type == MODULETYPE("MODt"))
	{
		m->channels=4;
		API->cp437_f_to_utf8_z (buf + 0, 20, m->title, sizeof (m->title));
		return 1;
	} else if (type == MODULETYPE("WOW"))
	{
		m->channels=8;
		API->cp437_f_to_utf8_z (buf + 0, 20, m->title, sizeof (m->title));
		snprintf (m->comment, sizeof (m->comment), "%s", "Converted from .669 with Mod's Grave");
		return 1;
	} else if (type == MODULETYPE("MOD"))
	{
		if (len >= 1084)
		{
			switch (int32_little(*(uint32_t *)(buf+1080)))
			{
				case 0x2E4B2E4D: /* M.K. */
				case 0x214B214D: /* M!K! */
				case 0x2E542E4E: /* N.T. */
				case 0x34544C46: m->channels=4; break;  /* FLT4 */
				case 0x4E484331: m->channels=1; break;  /* 1CHN */
				case 0x4E484332: m->channels=2; break;  /* 2CHN */
				case 0x4E484333: m->channels=3; break;  /* 3CHN */
				case 0x4E484334: m->channels=4; break;  /* 4CHN */
				case 0x4E484335: m->channels=5; break;  /* 5CHN */
				case 0x4E484336: m->channels=6; break;  /* 6CHN */
				case 0x4E484337: m->channels=7; break;  /* 7CHN */
				case 0x4E484338: m->channels=8; break;  /* 8CHN */
				case 0x4E484339: m->channels=9; break;  /* 9CHN */
				case 0x48433031: m->channels=10; break; /* 10CH */
				case 0x48433131: m->channels=11; break; /* 11CH */
				case 0x48433231: m->channels=12; break; /* 12CH */
				case 0x48433331: m->channels=13; break; /* 13CH */
				case 0x48433431: m->channels=14; break; /* 14CH */
				case 0x48433531: m->channels=15; break; /* 15CH */
				case 0x48433631: m->channels=16; break; /* 16CH */
				case 0x48433731: m->channels=17; break; /* 17CH */
				case 0x48433831: m->channels=18; break; /* 18CH */
				case 0x48433931: m->channels=19; break; /* 19CH */
				case 0x48433032: m->channels=20; break; /* 20CH */
				case 0x48433132: m->channels=21; break; /* 21CH */
				case 0x48433232: m->channels=22; break; /* 22CH */
				case 0x48433332: m->channels=23; break; /* 23CH */
				case 0x48433432: m->channels=24; break; /* 24CH */
				case 0x48433532: m->channels=25; break; /* 25CH */
				case 0x48433632: m->channels=26; break; /* 26CH */
				case 0x48433732: m->channels=27; break; /* 27CH */
				case 0x48433832: m->channels=28; break; /* 28CH */
				case 0x48433932: m->channels=29; break; /* 29CH */
				case 0x48433033: m->channels=30; break; /* 30CH */
				case 0x48433133: m->channels=31; break; /* 31CH */
				case 0x48433233: m->channels=32; break; /* 32CH */
			}
		}

		API->cp437_f_to_utf8_z (buf + 0, 20, m->title, sizeof (m->title));
		return 1;
	} else if (type == MODULETYPE("XM"))
	{
		xmhdr =  (head1 *)buf;
		if (xmhdr->ver<int16_little(0x104))
		{
			m->modtype.integer.i = 0;
			strcpy(m->title,"too old version");
			return 0;
		} else {
			API->cp437_f_to_utf8_z (xmhdr->name, 20, m->title, sizeof (m->title));
			m->channels=buf[68];
		}
		snprintf (m->comment, sizeof (m->comment), "Fast Tracker II v%d.%02d", uint16_little(xmhdr->ver) >> 8, uint16_little(xmhdr->ver) & 0xff);
		return 1;
	} else if (type == MODULETYPE("MXM"))
	{
		strcpy(m->title,"MXMPlay module");
		m->channels=buf[12];
		return 1;
	}
	return 0;
}

static const char *M15_description[] =
{
	//                                                                          |
	"M15 files are loaded as amiga NoiseTracker 15 instruments modules with no",
	"signature (but plays like ProTracker 1.1b). Open Cubic Player loads this an",
	"amiga module and played back using the XM/FastTracker support.",
	NULL
};

static const char *M15t_description[] =
{
	//                                                                          |
	"M15t files are loaded as amiga NoiseTracker 15 instruments modules with no",
	"signature (but plays like ProTracker 1.1b). Quirk enabled for old ProTracker",
	"compatible mode that only had tempo (no speed) command. Open Cubic Player",
	"loads this an amiga module and played back using the XM/FastTracker support.",
	NULL
};

static const char *M31_description[] =
{
	//                                                                          |
	"M31 files are loaded as amiga NoiseTracker 31 instruments modules with no",
	"signature (but plays like ProTracker 1.1b). Open Cubic Player loads this an",
	"amiga module and played back using the XM/FastTracker support.",
	NULL
};

static const char *MOD_description[] =
{
	//                                                                          |
	"MOD files are loaded as amiga ProTracker 1.1b modules with valid signature.",
	"F00 commands (end of tune) commands are ignored due to compatibility with",
	"some trackers this being a no-operation. Open Cubic Player loads this an",
	"amiga module and played back using the XM/FastTracker support.",
	NULL
};

static const char *MODd_description[] =
{
	//                                                                          |
	"MODd files are loaded as amiga ProTracker 1.1b modules with valid signature,",
	"but quirk enabled for DMP (Dual Module Player) style interpretation of the",
	"panning command. Open Cubic Player loads this an amiga module and played",
	"back using the XM/FastTracker support.",
	NULL
};

static const char *MODf_description[] =
{
	//                                                                          |
	"MODf files are loaded as Fast Tracker II modules with valid signature, with",
	"quirk enabled for Fast Tracker II compatible mode (when saved as .MOD). Open",
	"Cubic Player loads this an amiga module and played back using the",
	"XM/FastTracker support.",
	NULL
};

static const char *MODt_description[] =
{
	//                                                                          |
	"MODt files are loaded as amiga ProTracker 1.1b modules with valid signature,",
	"but quirk enabled for old ProTracker compatible mode that only had tempo (no",
	"speed) command. Open Cubic Player loads this an amiga module and played back",
	"using the XM/FastTracker support.",
	NULL
};

static const char *MXM_description[] =
{
	//                                                                          |
	"MXM files are created by XM2XMX by Niklas Beisert / pascal, and used by",
	"MXMPlay a tiny XM player for GUS soundcards used by various PC intro's",
	"(demos up to 64K). Open Cubic Player has a special loader for this file",
	"format and playback is done using the XM/FastTracker support.",
	NULL
};

static const char *WOW_description[] =
{
	//                                                                          |
	"WOW files are converted from Composer 669 files using Mod's Grave utility",
	"by JAS (Jan Ole Suhr). WOW files has a \"M.K.\" header which normally should",
	"be a 4 channel file. Open Cubic Player loads this an amiga module, forced to",
	"8 channels and played back using the XM/FastTracker support.",
	NULL
};

static const char *XM_description[] =
{
	//                                                                          |
	"XM - eXtended Module - files are created by Fast Tracker II by Fredrik",
	"\"Mr. H\" Huss and Magnus \"Vogue\" Högdahl from Triton. XM supports up to 32",
	"channels. Open Cubic Player has a special loader for this file format and",
	"playback is done using the XM/FastTracker support.",
	NULL
};

static struct mdbreadinforegstruct xmpReadInfoReg = {"MOD/XM", xmpReadInfo MDBREADINFOREGSTRUCT_TAIL};

OCP_INTERNAL int xm_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("NST");
	API->fsRegisterExt ("MOD");
	API->fsRegisterExt ("MXM");
	API->fsRegisterExt ("WOW");
	API->fsRegisterExt ("XM");

	mt.integer.i = MODULETYPE("M15");
	API->fsTypeRegister (mt, M15_description, "plOpenCP", &xmpPlayer);

	mt.integer.i = MODULETYPE("M15t");
	API->fsTypeRegister (mt, M15t_description, "plOpenCP", &xmpPlayer);

	mt.integer.i = MODULETYPE("M31");
	API->fsTypeRegister (mt, M31_description, "plOpenCP", &xmpPlayer);

	mt.integer.i = MODULETYPE("MOD");
	API->fsTypeRegister (mt, MOD_description, "plOpenCP", &xmpPlayer);

	mt.integer.i = MODULETYPE("MODd");
	API->fsTypeRegister (mt, MODd_description, "plOpenCP", &xmpPlayer);

	mt.integer.i = MODULETYPE("MODf");
	API->fsTypeRegister (mt, MODf_description, "plOpenCP", &xmpPlayer);

	mt.integer.i = MODULETYPE("MODt");
	API->fsTypeRegister (mt, MODt_description, "plOpenCP", &xmpPlayer);

	mt.integer.i = MODULETYPE("MXM");
	API->fsTypeRegister (mt, MXM_description, "plOpenCP", &xmpPlayer);

	mt.integer.i = MODULETYPE("WOW");
	API->fsTypeRegister (mt, WOW_description, "plOpenCP", &xmpPlayer);

	mt.integer.i = MODULETYPE("XM");
	API->fsTypeRegister (mt, XM_description, "plOpenCP", &xmpPlayer);

	API->mdbRegisterReadInfo(&xmpReadInfoReg);

	return errOk;
}

OCP_INTERNAL void xm_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("M15");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("M15t");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("M31");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("MOD");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("MODd");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("MODf");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("MODt");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("MXM");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("WOW");
	API->fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("XM");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&xmpReadInfoReg);
}
