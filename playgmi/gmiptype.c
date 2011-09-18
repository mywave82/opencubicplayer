/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay file type detection routines for the fileselector
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "filesel/mdb.h"
#include "stuff/compat.h"

#define _EXT_MAX 5

static void getext(char *ext, char *name)
{
	int i;
	name+=8;
	for (i=0; i<(_EXT_MAX-1); i++)
		if (*name==' ')
			break;
		else
			*ext++=*name++;
	*ext=0;
}

static int gmiGetModuleType(const char *buf, const char *ext)
{
	if (!strcmp(ext, ".MID"))
		return mtMID;

	if (*(uint32_t*)buf==uint32_little(0x6468544D))
		return mtMID;
	if ((*(uint32_t*)buf==uint32_little(0x46464952))&&(*(uint32_t*)(buf+8)==uint32_little(0x44494D52)))
		return mtMID;

	return mtUnRead;
}

static int gmiReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	char ext[_EXT_MAX];
	int type;
	unsigned int i;

	if (len<12)
		return 0;

	getext(ext, m->name);

	if ((type=gmiGetModuleType(buf, ext))==mtUnRead)
		return 0;
	m->modtype=type;

	switch (type)
	{
		unsigned long len;

		case mtMID:
			len=0;
			m->channels=16;

			i=0;
			if (*(uint32_t*)buf==uint32_little(0x46464952))
			{
				i=12;
				while (i<800)
				{
					i+=8;
					if (*(uint32_t*)(buf+i-8)==uint32_little(0x61746164))
						break;
					i+=uint32_little(*(uint32_t*)(buf+i-4));
				}
			}
			while (i<800)
			{
				i+=8;
				len=(buf[i-4]<<24)|(buf[i-3]<<16)|(buf[i-2]<<8)|(buf[i-1]);
				if (!memcmp(buf+i-8, "MTrk", 4))
					break;
				i+=len;
			}
			len+=i;
			if (len>800)
				len=800;
			while (i<len)
			{
				if (*(uint16_t*)(buf+i)!=uint16_little(0xFF00))
					break;
				if (buf[i+2]!=0x03)
				{
					i+=4+buf[i+3];
					continue;
				}
				len=buf[i+3];
				if (len>31)
					len=31;

				memcpy(m->modname, buf+i+4, len);
				m->modname[len]=0;
				break;
			}
			memset(&m->composer, 0, sizeof(m->composer));
			return 1;
	}
	return 0;
}

static int gmiReadInfo(struct moduleinfostruct *m, FILE *fp, const char *mem, size_t len)
{
	return 0;
}

struct mdbreadinforegstruct gmiReadInfoReg = {gmiReadMemInfo, gmiReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
