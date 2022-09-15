/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "filesel/pfilesel.h"
#include "stuff/err.h"
#include "oggtype.h"

static int oggReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *buf, size_t len)
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
	m->modtype.integer.i=MODULETYPE("OGG");
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
		if ((length >= 7) && (!strncasecmp(ptr, "artist=", 7)))
		{
			int copy = length - 7;
			if (copy >= sizeof (m->artist))
			{
				copy = sizeof (m->artist)-1;
			}
			bzero (m->artist, sizeof (m->artist));
			memcpy (m->artist, ptr + 7, copy);
		} else if ((length >= 6) && (!strncasecmp(ptr, "title=", 6)))
		{
			int copy = length - 6;
			if (copy >= sizeof (m->title))
			{
				copy = sizeof (m->title)-1;
			}
			bzero (m->title, sizeof (m->title));
			memcpy (m->title, ptr + 6, copy);
		} else if ((length >= 6) && (!strncasecmp(ptr, "album=", 6)))
		{
			int copy = length - 6;
			if (copy >= sizeof (m->album))
			{
				copy = sizeof (m->album)-1;
			}
			bzero (m->album, sizeof (m->album));
			memcpy (m->album, ptr + 6, copy);
		} else if ((length >= 6) &&(!strncasecmp(ptr, "genre=", 6)))
		{
			int copy = length - 6;
			if (copy >= sizeof (m->style))
			{
				copy = sizeof (m->style)-1;
			}
			bzero (m->style, sizeof (m->style));
			memcpy (m->style, ptr + 6, copy);
		} else if ((length >= 9) &&(!strncasecmp(ptr, "composer=", 9)))
		{
			int copy = length - 9;
			if (copy >= sizeof (m->composer))
			{
				copy = sizeof (m->composer)-1;
			}
			bzero (m->composer, sizeof (m->composer));
			memcpy (m->composer, ptr + 9, copy);
		}

		ptr+=length;
	}
	return 1;
}

static const char *OGG_description[] =
{
	//                                                                          |
	"OGG Vorbis is an open format, royalty free, lossy, audio compressed file",
	"format.",
	NULL
};

static const struct interfaceparameters OGG_p =
{
	"autoload/40-playogg", "oggPlayer",
	0, 0
};

static struct mdbreadinforegstruct oggReadInfoReg = {"OGG", oggReadInfo MDBREADINFOREGSTRUCT_TAIL};

int __attribute__ ((visibility ("internal"))) ogg_type_init (void)
{
	struct moduletype mt;
	fsRegisterExt ("OGA");
	fsRegisterExt ("OGG");
	mt.integer.i = MODULETYPE("OGG");
	fsTypeRegister (mt, OGG_description, "plOpenCP", &OGG_p);

	mdbRegisterReadInfo(&oggReadInfoReg);
	return errOk;
}

void __attribute__ ((visibility ("internal"))) ogg_type_done (void)
{
	mdbUnregisterReadInfo(&oggReadInfoReg);
}
