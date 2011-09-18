/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * WAVPlay file type detection routines for the fileselector
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
#include <stdlib.h>
#include "types.h"
#include "filesel/mdb.h"

static unsigned char wavGetModuleType(const char *buf)
{
	if ((*(uint32_t *)buf==uint32_little(0x46464952))&&(*(uint32_t *)(buf+8)==uint32_little(0x45564157))&&(*(uint32_t *)(buf+12)==uint32_little(0x20746D66))&&(*(uint16_t *)(buf+20)==uint16_little(1)))
		return mtWAV;
	return mtUnRead;
}


static int wavReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	int type=wavGetModuleType(buf);
	int i,j;

	if (type==mtUnRead)
		return 0;

	m->modtype=type;

	switch (type)
	{
		case mtWAV:
			{
				char rate[10];
				i=20;
				m->modname[0]=0;
				sprintf(rate, "%d", (int)int32_little((*(uint32_t *)(buf+i+4))));
				for (j=strlen(rate); j<5; j++)
					strcat(m->modname, " ");
				strcat(m->modname, rate);
				if (*(uint16_t *)(buf+i+14)==uint16_little(8))
					strcat(m->modname, "Hz,  8 bit, ");
				else
					strcat(m->modname, "Hz, 16 bit, ");
				if (*(uint16_t *)(buf+i+2)==uint16_little(1))
					strcat(m->modname, "mono");
				else
					strcat(m->modname, "stereo");
				m->channels=uint16_little(*(uint16_t *)(buf+i+2));
				if (*(uint32_t *)(buf+i+16)==uint32_little(61746164))
					m->playtime=uint32_little(*(uint32_t *)(buf+i+20))/ uint32_little(*(uint32_t *)(buf+i+8));
				memset(&m->composer, 0, sizeof(m->composer));
				return 1;
			}
	}
	return 0;
}

static int wavReadInfo(struct moduleinfostruct *m, FILE *fp, const char *mem, size_t len)
{
	return 0;
}

struct mdbreadinforegstruct wavReadInfoReg = {wavReadMemInfo, wavReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
