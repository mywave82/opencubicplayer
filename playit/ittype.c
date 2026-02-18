/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ITPlay file type detection routines for the file selector
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 */

#include "config.h"
#include <string.h>
#include <time.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/cp437.h"
#include "stuff/err.h"
#include "ittype.h"

static uint32_t itpGetModuleType(const char *buf)
{
	if (*(uint32_t*)buf==uint32_little(0x4D504D49))
		return MODULETYPE("IT");
	return 0;
}

static int itpReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	uint32_t type;
	int i;
	uint8_t hilight_minor;
	uint8_t hilight_major;
	uint16_t ordnum;
	uint16_t insnum;
	uint16_t smpnum;
	uint16_t patnum;
	uint16_t cwtv;
	uint16_t cmwt;
	uint16_t flags;
	uint16_t special;
	uint8_t  globalvol;
	uint8_t  mv;
	uint8_t  speed;
	uint8_t  sep;
	uint8_t  pwd;
	uint16_t msglength;
	uint32_t msgoffset;
	uint32_t reserved;

	uint16_t hist = 0;

#if 0 /* handled by libancient */
	if (!memcmp(buf, "ziRCONia", 8))
	{
		strcpy(m->title, "MMCMPed module");
		return 0;
	}
#endif

	if (len < 0x40)
	{
		return 0;
	}

	if (!(type=itpGetModuleType(buf)))
		return 0;
	m->modtype.integer.i=type;

	if (buf[0x2C]&4)
		if (buf[0x2B]<2)
			return 0;

	API->cp437_f_to_utf8_z (buf + 4, 26, m->title, sizeof (m->title));
	m->channels=0;
	for (i=0; i<64; i++)
		if (!(buf[64+i]&0x80))
			m->channels++;

	hilight_minor = *(uint8_t *)(buf + 0x1e);
	hilight_major = *(uint8_t *)(buf + 0x1f);
	ordnum    = uint16_little(*(uint16_t *)(buf + 0x20));
	insnum    = uint16_little(*(uint16_t *)(buf + 0x22));
	smpnum    = uint16_little(*(uint16_t *)(buf + 0x24));
	patnum    = uint16_little(*(uint16_t *)(buf + 0x26));
	cwtv      = uint16_little(*(uint16_t *)(buf + 0x28)); // created with tracker version
	cmwt      = uint16_little(*(uint16_t *)(buf + 0x2a)); // compatible with
	flags     = uint16_little(*(uint16_t *)(buf + 0x2c));
	special   = uint16_little(*(uint16_t *)(buf + 0x2e));
	globalvol = *(uint8_t *)(buf + 0x30);
	mv        = *(uint8_t *)(buf + 0x31);
	speed     = *(uint8_t *)(buf + 0x32);
	sep       = *(uint8_t *)(buf + 0x34);
	pwd       = *(uint8_t *)(buf + 0x35);
	msglength = uint16_little(*(uint16_t *)(buf + 0x36));
	msgoffset = uint32_little(*(uint16_t *)(buf + 0x38));
	reserved  = uint32_little(*(uint16_t *)(buf + 0x3c));

	if (special & 0x0002) /* history should be present */
	{
		uint32_t offset_min = 0xffffffff;
		uint32_t offset_ins, offset_smp, offset_pat, offset_hist;
		int i;
		if ((special & 1) && msglength) /* message is present */
		{
			offset_min = msgoffset;
		}

		if (len < (0x0c + ordnum + 0x08 + 0x0a))
		{
			goto skip_hist_check;
		}
		offset_ins = 0xc0 + ordnum + 0x00;
		offset_smp = offset_ins + insnum * 4;
		offset_pat = offset_smp + smpnum * 4;
		offset_hist = offset_pat + patnum * 4;

		if (len < (offset_hist + 2))
		{
			goto skip_hist_check;
		}

		for (i=0; i < insnum; i++)
		{
			uint32_t ins_pos = uint32_little(*(uint16_t *)(buf + offset_ins + i * 4));
			if (ins_pos < offset_min)
			{
				offset_min = ins_pos;
			}
		}

		for (i=0; i < smpnum; i++)
		{
			uint32_t smp_pos = uint32_little(*(uint16_t *)(buf + offset_smp + i * 4));
			if (smp_pos < offset_min)
			{
				offset_min = smp_pos;
			}
		}
		for (i=0; i < patnum; i++)
		{
			uint32_t pat_pos = uint32_little(*(uint16_t *)(buf + offset_pat + i * 4));
			if (pat_pos < offset_min)
			{
				offset_min = pat_pos;
			}
		}
		hist = uint32_little(*(uint16_t *)(buf + offset_hist)); /* each entry in the history is 8 bytes, can they fit before any other data */

		if (offset_min < ((uint32_t)hist) * 8)
		{
			hist = 0;
		}
	}
skip_hist_check:

	switch (cwtv >> 12)
	{
		case 0:
			/* TODO, detect BeRoTracker that is not tagged with non 0x6nnn ? */
			/* if "MODU" chunk is present before "IMPI", "IMPS", "XTPM" or "STPM", and version cwtv is 0x0nnn we have BeRoTracker too */

			if (hist && !reserved)
			{

			} else if ((cwtv == 0x0214) && (cmwt == 0x0200) && (flags == 9) && (special == 0) &&
			           (hilight_major == 0) && (hilight_minor == 0) &&
			           (insnum == 0) && ((patnum + 1) == ordnum) &&
			           (globalvol == 128) && (mv == 100) && (speed == 1) && (sep == 128) && (pwd == 0) &&
			           (msglength == 0) && (msgoffset == 0) && (reserved == 0))
			{
				snprintf (m->comment, sizeof (m->comment), "OpenSPC conversion");
				break;
			} else if ((cwtv == 0x0888) && (cmwt == 0x0888) && (reserved == 0))
			{
				snprintf (m->comment, sizeof (m->comment), "OpenMPT 1.17+");
				break;
			} else if ((cwtv == 0x0300) && (cmwt == 0x0300) && (reserved == 0) && (ordnum == 256) && (sep == 128) && (pwd == 0))
			{
				snprintf (m->comment, sizeof (m->comment), "OpenMPT 1.17.02.20 - 1.17.02.25");
				break;
			} else if ((cwtv == 0x0217) && (cmwt == 0x0200) && (reserved == 0))
			{
				int ompt = 0;
				if (insnum > 0)
				{
					uint32_t offset_ins = 0xc0 + ordnum + 0x00;
					if (len >= (offset_ins + 4))
					{
						uint32_t ins_pos = uint32_little(*(uint16_t *)(buf + offset_ins + 0 * 4)); /* instrument 0 */
						if (len >= (ins_pos + 0x1c + 4))
						{
							uint16_t trkvers = uint16_little(*(uint16_t *)(buf + ins_pos + 0x1c));
							// check trkvers -- OpenMPT writes 0x0220; older MPT writes 0x0211
							if (trkvers == 0x0220)
							{
								ompt = 1;
							}
						}
					}
				}
				if (len >= (0x40 + 64))
				{
					if (!ompt && (memchr(buf + 0x40 /* channel panning */, 0xff, 64) == NULL))
					{
						// MPT 1.16 writes 0xff for unused channels; OpenMPT never does this
						// XXX this is a false positive if all 64 channels are actually in use
						// -- but then again, who would use 64 channels and not instrument mode?
						ompt = 1;
					}
				}
				snprintf (m->comment, sizeof (m->comment), ompt ? "OpenMPT (compatibility mode)" : "Modplug Tracker 1.09 - 1.16");
				break;
			} else if ((cwtv == 0x0214) && (cmwt == 0x0200) && (reserved == 0))
			{
				// instruments 560 bytes apart
				snprintf (m->comment, sizeof (m->comment), "Modplug Tracker 1.00a5");
				break;
			} else if ((cwtv == 0x0214) && (cmwt == 0x0202) && (reserved == 0))
			{
				// instruments 557 bytes apart
				snprintf (m->comment, sizeof (m->comment), "Modplug Tracker b3.3 - 1.07");
				break;
			} else if ((cwtv == 0x0214) && (cmwt == 0x0214) && (reserved == 0x49424843))
			{
				// sample data stored directly after header
				// all sample/instrument filenames say "-DEPRECATED-"
				// 0xa for message newlines instead of 0xd
				snprintf (m->comment, sizeof (m->comment), "ChibiTracker");
				break;
			} else if ((cwtv == 0x0214) && (cmwt == 0x0214) && ((flags & 0x10C6) == 4) && (special <= 1) && (reserved == 0))
			{
				// sample data stored directly after header
				// all sample/instrument filenames say "XXXXXXXX.YYY"
				snprintf (m->comment, sizeof (m->comment), "CheeseTracker?");
				break;
			}
			if (cmwt > 0x0214)
			{
				snprintf (m->comment, sizeof (m->comment), "Impulse Tracker v2.15");
				break;
			}
			if ((cwtv > 0x0215) && (cwtv <= 0x0217))
			{
				const char *versions[] = { "1 or 2", "3", "4 or 5" };
				snprintf(m->comment, sizeof (m->comment), "Impulse Tracker v2.14 patch %s", versions[cwtv - 0x0215]);
				break;
			}
			if ( ((cwtv >= 0x0100) && (cwtv <= 0x0106)) ||
			     ((cwtv >= 0x0200) && (cwtv <= 0x0215)) )
			{
				snprintf (m->comment, sizeof (m->comment), "Impulse Tracker v%d.%02x", cwtv >> 8, cwtv & 0x00ff);
			}
			break;
		case 1:
			if (cwtv == 0x1020)
			{
				snprintf (m->comment, sizeof (m->comment), "Schism Tracker v0.2a");
			} else if (cwtv < 0x1050)
			{
				snprintf (m->comment, sizeof (m->comment), "Schism Tracker v0.%x", cwtv & 0xff);
			} else if (cwtv == 0x1050)
			{
				snprintf (m->comment, sizeof (m->comment), "Schism Tracker v2007-04-17<=>v2009-10-31");
			} else if (cwtv == 0x1fff)
			{
				struct tm version,     epoch = { .tm_year = 109, .tm_mon = 9, .tm_mday = 31 }; /* 2009-10-31 */
				time_t    version_sec, epoch_sec;

				epoch_sec = mktime(&epoch);
				version_sec = reserved * 86400 + epoch_sec;

				if (localtime_r(&version_sec, &version) != 0)
				{
					snprintf(m->comment, sizeof (m->comment), "Schism Tracker v%04d-%02d-%02d",
						version.tm_year + 1900, version.tm_mon + 1, version.tm_mday);
				}
			} else {
				struct tm version,     epoch = { .tm_year = 109, .tm_mon = 9, .tm_mday = 31 }; /* 2009-10-31 */
				time_t    version_sec, epoch_sec;

				epoch_sec = mktime(&epoch);
				version_sec = (cwtv - 0x1050) * 86400 + epoch_sec;

				if (localtime_r(&version_sec, &version) != 0)
				{
					snprintf(m->comment, sizeof (m->comment), "Schism Tracker v%04d-%02d-%02d",
						version.tm_year + 1900, version.tm_mon + 1, version.tm_mday);
				}
			}
			break;
		case 4:
			snprintf (m->comment, sizeof (m->comment), "pyIT v%d.%02d", (cwtv >> 8) & 0x0f, cwtv & 0xff);
			break;
		case 5:
			snprintf (m->comment, sizeof (m->comment), "OpenMPT v%d.%02d", (cwtv >> 8) & 0x0f, cwtv & 0xff);
			break;
		case 6:
			snprintf (m->comment, sizeof (m->comment), "BeRoTracker v%d.%02d", (cwtv >> 8) & 0x0f, cwtv & 0xff);
			break;
		case 7:
			if (cmwt == 0x215)
			{
				snprintf (m->comment, sizeof (m->comment), "munch.py");
				break;
			}
			snprintf (m->comment, sizeof (m->comment), "ITMCK v%d.%d.%d", (cwtv >> 8) & 0x0f, (cwtv >> 4) & 0x0f, cwtv & 0x0f);
			break;
		case 8:
			if (cwtv == 0x8000)
			{
				snprintf (m->comment, sizeof (m->comment), "Tralala before first release");
			} else {
				snprintf (m->comment, sizeof (m->comment), "Tralala v%d.%02d", (cwtv >> 8) & 0x0f, cwtv & 0xff);
			}
			break;
		case 0xc:
			snprintf (m->comment, sizeof (m->comment), "ChickDune ChipTune Tracker v%d.%02d", (cwtv >> 8) & 0x0f, cwtv & 0xff);
			break;
		case 0xd:
			if (cwtv == 0xdaeb)
			{
				snprintf (m->comment, sizeof (m->comment), "spc2it");
			} else if (cwtv == 0xd1ce)
			{
				snprintf (m->comment, sizeof (m->comment), "itwriter Javascript library");
			}
			break;
	}
	return 1;
}

static const char *IT_description[] =
{
	//                                                                          |
	"IT files are created by Impulse Tracker or the modern remake Schism Tracker.",
	"Impulse Tracker was only for MSDOS while Schism Tracker made using SDL works",
	"on most operating systems. IT files features 16bit samples and 64 channels.",
	NULL
};

static struct mdbreadinforegstruct itpReadInfoReg = {"IT", itpReadInfo MDBREADINFOREGSTRUCT_TAIL};

OCP_INTERNAL int it_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("IT");

	mt.integer.i = MODULETYPE("IT");
	API->fsTypeRegister (mt, IT_description, "plOpenCP", &itPlayer);

	API->mdbRegisterReadInfo(&itpReadInfoReg);

	return errOk;
}

OCP_INTERNAL void it_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("IT");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&itpReadInfoReg);
}
