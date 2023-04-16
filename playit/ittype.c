/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
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
	uint16_t ver;

	if (!memcmp(buf, "ziRCONia", 8))
	{
		strcpy(m->title, "MMCMPed module");
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

	ver = uint16_little(*(uint16_t *)(buf + 0x28));
	if ( ((ver >= 0x0100) && (ver <= 0x0106)) ||
	     ((ver >= 0x0200) && (ver >= 0x020f)) )
	{
		snprintf (m->comment, sizeof (m->comment), "Impulse Tracker v%d.%02d", ver >> 8, ver & 0x00ff);
	} else if (ver == 0x0020)
	{
		snprintf (m->comment, sizeof (m->comment), "Schism Tracker v0.2a");
	} else if (ver == 0x0050)
	{
		snprintf (m->comment, sizeof (m->comment), "Schism Tracker v2007-04-17<=>v2009-10-31");
	} else if ((ver >= 0x0050) && (ver < 0x0fff))
	{
		struct tm version,     epoch = { .tm_year = 109, .tm_mon = 9, .tm_mday = 31 }; /* 2009-10-31 */
		time_t    version_sec, epoch_sec;

		epoch_sec = mktime(&epoch);
		version_sec = (ver - 0x050) * 86400 + epoch_sec;

		if (localtime_r(&version_sec, &version) != 0)
		{
			snprintf(m->comment, sizeof (m->comment), "Schism Tracker v%04d-%02d-%02d",
				version.tm_year + 1900, version.tm_mon + 1, version.tm_mday);
		}
	} else {
		struct tm version,     epoch = { .tm_year = 109, .tm_mon = 9, .tm_mday = 31 }; /* 2009-10-31 */
		time_t    version_sec, epoch_sec;

		epoch_sec = mktime(&epoch);
		version_sec = (*(uint32_t *)(buf + 0x3c)) * 86400 + epoch_sec;

		if (localtime_r(&version_sec, &version) != 0)
		{
			snprintf(m->comment, sizeof (m->comment), "Schism Tracker v%04d-%02d-%02d",
				version.tm_year + 1900, version.tm_mon + 1, version.tm_mday);
		}
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
