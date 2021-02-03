/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include "types.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"


static int itpGetModuleType(const char *buf)
{
	if (*(uint32_t*)buf==uint32_little(0x4D504D49))
		return mtIT;
	return mtUnRead;
}


static int itpReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	int type;
	int i;

	if (!memcmp(buf, "ziRCONia", 8))
	{
		strcpy(m->modname, "MMCMPed module");
		return 0;
	}

	if ((type=itpGetModuleType(buf))==mtUnRead)
		return 0;
	m->modtype=type;
	switch (type)
	{
		case mtIT:
			if (buf[0x2C]&4)
				if (buf[0x2B]<2)
					return 0;
			memcpy(m->modname, buf+4, 26);
			m->modname[26]=0;
			m->channels=0;
			for (i=0; i<64; i++)
				if (!(buf[64+i]&0x80))
					m->channels++;
			memset(&m->composer, 0, sizeof(m->composer));
			return 1;
	}
	return 0;
}

static int itpReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *bf, size_t len)
{
	return itpReadMemInfo (m, bf, len);
}

struct mdbreadinforegstruct itpReadInfoReg = {itpReadMemInfo, itpReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
