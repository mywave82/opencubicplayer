/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Fileselector file type detection routines (covers play lists and internal
 * cache files)
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "types.h"
#include "dirdb.h"
#include "mdb.h"

static int fsReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
  /* check for PLS play list */

	char *b=(char *)buf;
	unsigned int pos=10;
	int num=0;

	if (!memcmp(buf,"[playlist]", 10))
	{
		while (pos<len)
		{
			if (b[pos]!=0x0a && b[pos]!=0x0d)
				pos++;
			else {
				while ((pos<len) && isspace(b[pos]))
					pos++;
				if (len-pos>18 && !memcmp(b+pos,"NumberOfEntries=",16))
				{
					pos+=16;
					num=strtol(b+pos,0,10);
					pos=len;
				}
			}
		}
		if (num)
		{
			sprintf(m->modname, "PLS style playlist (%d entries)", num);
			m->modtype=mtPLS;
			m->flags1|=MDB_PLAYLIST;
			return 1;
		} else {
			strcpy(m->modname,"PLS style playlist ?");
			m->modtype=mtPLS;
			m->flags1|=MDB_PLAYLIST;
			return 1;
		}
	}

 /* check for M3U-style play list */
	if (!memcmp(buf,"#EXTM3U", 7))
	{
		strcpy(m->modname,"M3U playlist");
		m->modtype=mtM3U;
		m->flags1|=MDB_PLAYLIST;
		return 1;
	}

	if (!strncasecmp(m->name+8, ".M3U", 4))
	{
		strcpy(m->modname, "Non-standard M3U playlist");
		m->modtype=mtM3U;
		m->flags1|=MDB_PLAYLIST;
		return 1;
	}

	if (!strncasecmp(m->name+8, ".PLS", 4))
	{
		strcpy(m->modname, "Non-standard PLS playlist");
		m->modtype=mtPLS;
		m->flags1|=MDB_PLAYLIST;
		return 1;
	}

	if (!memcmp(buf, "CPArchiveCache\x1B\x00", 16))
		strcpy(m->modname,"openCP archive data base (old)");
	if (!memcmp(buf, "CPArchiveCache\x1B\x01", 16))
		strcpy(m->modname,"openCP archive data base");
	if (!memcmp(buf, "Cubic Player Module Information Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 60))
		strcpy(m->modname,"openCP module info data base");
	if (!memcmp(buf, dirdbsigv1, sizeof(dirdbsigv1)))
		strcpy(m->modname,"openCP dirdb/medialib: db v1");
	if (!memcmp(buf, dirdbsigv2, sizeof(dirdbsigv2)))
		strcpy(m->modname,"openCP dirdb/medialib: db v2");
	if (!memcmp(buf, "MDZTagList\x1A\x00", 12))
		strcpy(m->modname,"openCP MDZ file cache");

	return 0;
}

static int fsReadInfo(struct moduleinfostruct *m, FILE *fp, const char *buf, size_t len)
{
	return 0;
}

struct mdbreadinforegstruct fsReadInfoReg = {fsReadMemInfo, fsReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
