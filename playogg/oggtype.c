/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include "filesel/mdb.h"

/* try to ignore utf8-characters... */
static char *_strncpy(char *dest, const char *src, size_t n)
{
	while (n)
	{
		if (*src&0x80)
		{
			src++;
			continue;
		}
		*dest=*src;
		n--;
		if (!*src)
			break;
		src++;
		dest++;
	}
	return dest;
}

static int oggReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	uint8_t offset;
	uint32_t length;
	uint32_t count;
	const char *ptr;
	unsigned int i;
	char const *bufend=buf+len;

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
		if(!strncasecmp(ptr, "title=", 6))
		{
			unsigned int len=length-6;
			if (len>(sizeof(m->modname)-1))
				len=sizeof(m->modname)-1;
			_strncpy(m->modname, ptr+6, len);
			m->modname[len]=0;
		} else if(!strncasecmp(ptr, "artist=", 7))
		{
			unsigned int len=length-7;
			if (len>(sizeof(m->composer)-1))
				len=sizeof(m->composer)-1;
			_strncpy(m->composer, ptr+7, len);
			m->composer[len]=0;
		} else if(!strncasecmp(ptr, "album=", 6))
		{
			unsigned int len=length-6;
			if (len>(sizeof(m->comment)-1))
				len=sizeof(m->comment)-1;
			_strncpy(m->comment, ptr+6, len);
			m->comment[len]=0;
		}
		ptr+=length;
	}
	return 1;
}

static int oggReadInfo(struct moduleinfostruct *m, FILE *f, const char *buf, size_t len)
{
	return oggReadMemInfo(m, buf, len);
}

struct mdbreadinforegstruct oggReadInfoReg = {oggReadMemInfo, oggReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
