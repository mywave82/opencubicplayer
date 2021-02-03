/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'20 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OGGPlay file type detection routines for fileselector
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
 *  -ss040911  Stian Skjelstad <stian@nixia.no>
 *    -first release
 */


#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "stuff/cp437.h"

void _utf8_to_cp437 (const char *src, size_t srclen, char *dst, size_t dstlen)
{
	if (dst[0])
	{
		while ((*dst) && dstlen)
		{
			dst++;
			dstlen--;
		}
		if (dstlen > 3)
		{
			*(dst++) = ' '; dstlen--;
			*(dst++) = '-'; dstlen--;
			*(dst++) = ' '; dstlen--;
		} else {
			return;
		}
	}
	utf8_to_cp437 (src, srclen, dst, dstlen);
}

static int oggReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	uint8_t offset;
	uint32_t length;
	uint32_t count;
	const char *ptr;
	unsigned int i;
	char const *bufend=buf+len;
	int gottitle = 0;
	int gotartist = 0;
	int gotcomment = 0;
	int gotgenre = 0;

	if (len<35)
		return 0;
	/* OggS   Ogg data header */
	if (*(uint32_t*)buf!=int32_little(0x5367674f))
		return 0;
	/* Vorbis audio ? */
	if ( (*(uint32_t*)(buf+28)!=int32_little(0x726f7601)) || (*(uint16_t *)(buf+32)!=int16_little(0x6962)) || (*(uint8_t *)(buf+34)!=0x73))
		return 0;
	m->modtype=mtOGG;
	if (len<85)
		return 1;
	offset=((uint8_t *)buf)[84];
	/* This is for sure an Ogg Vorbis sound stream */

	ptr=(char *)buf+offset+85;
	if ((ptr+7)>bufend)
		return 1;
	if (strncmp(ptr, "\003vorbis", 7))
		return 1;
	ptr+=7;
	if ((ptr+4)>bufend)
		return 1;
	length=int32_little(*(uint32_t *)ptr);
	ptr+=length+sizeof(uint32_t);
	if ((ptr+4)>bufend)
		return 1;
	count=int32_little(*(uint32_t *)ptr);
	ptr+=sizeof(uint32_t);

	for (i=0;i<count;i++)
	{
		if ((ptr+4)>bufend)
			return 1;
		length=int32_little(*(uint32_t *)ptr);
		if ((ptr+4+length)>bufend)
			return 1;
		ptr+=sizeof(uint32_t);
		if ((length >= 6) && (!strncasecmp(ptr, "title=", 6)))
		{
			if (!gottitle)
			{
				m->modname[0] = 0;
			}
			_utf8_to_cp437 (ptr+6, length-6, m->modname, sizeof (m->modname));
			gottitle = 1;
		} else if ((length >= 7) && (!strncasecmp(ptr, "artist=", 7)))
		{
			if (!gotartist)
			{
				m->composer[0] = 0;
			}
			_utf8_to_cp437 (ptr+7, length-7, m->composer, sizeof (m->composer));
			gotartist = 1;
		} else if ((length >= 6) && (!strncasecmp(ptr, "album=", 6)))
		{
			if (!gotcomment)
			{
				m->comment[0] = 0;
			}
			_utf8_to_cp437 (ptr+6, length-6, m->comment, sizeof (m->comment));
			gotcomment = 1;
		} else if ((length >= 6) &&(!strncasecmp(ptr, "genre=", 6)))
		{
			if (!gotgenre)
			{
				m->style[0] = 0;
			}
			_utf8_to_cp437 (ptr+6, length-6, m->style, sizeof (m->style));
			gotgenre=1;
		}

		ptr+=length;
	}
	return 1;
}

static int oggReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *buf, size_t len)
{
	return oggReadMemInfo(m, buf, len);
}

struct mdbreadinforegstruct oggReadInfoReg = {oggReadMemInfo, oggReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
