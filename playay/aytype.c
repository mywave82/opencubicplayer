/* OpenCP Module Player
 * copyright (c) '05-'10 Stian Skjelstad <stian@nixia.no>
 *
 * .ay file type detection routines for the file selector
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
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"

static struct mdbreadinforegstruct ayReadInfoReg;

static int ayReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	int authorptr;
	const char *authorstr;

	int miscptr;
	const char *miscstr;

	unsigned int left;

/*
	uint8_t tracks, default_track;*/

	/*int i;*/

	if (len<14)
	{
/*
		fprintf(stderr, "len<14 (len=%d)\n", len);*/
		return 0;
	}

	if (memcmp(buf, "ZXAYEMUL", 8))
	{
/*
		fprintf(stderr, "AY header failed\n");*/
		return 0;
	}

	m->modtype=mtAY;

	/* filever at offset 8 */
	/* playerver at offset 9 */

	/* custom-player at offset 10/11 */

	authorptr=256*(uint8_t)buf[12]+(uint8_t)buf[13] + 12;

/*
	fprintf(stderr, "authorptr=%x\n", authorptr);*/
	authorstr=buf+authorptr;

	miscptr=256*(uint8_t)buf[14]+(uint8_t)buf[15] + 14;
	miscstr=buf+miscptr;

	/* default track is at offset 16 */
	m->channels=(uint8_t)buf[17];

	if ((left=len-(authorstr-buf))<=0) /* outside buffer-space */
	{
		m->composer[0]=0;
	} else if (left>sizeof(m->composer)) /* we have all the space we need anyway */
	{
		strncpy(m->composer, authorstr, sizeof(m->composer));
	} else if (memchr(authorstr, 0, left)) /* we have \0 before end of buffer */
	{
		strncpy(m->composer, authorstr, sizeof(m->composer));
	} else {
		m->composer[0]=0;
	}

	if ((left=len-(miscstr-buf))<=0) /* outside buffer-space */
	{
		m->comment[0]=0;
	} else if (left>sizeof(m->comment)) /* we have all the space we need anyway */
	{
		strncpy(m->comment, miscstr, sizeof(m->comment));
	} else if (memchr(miscstr, 0, left)) /* we have \0 before end of buffer */
	{
		strncpy(m->comment, miscstr, sizeof(m->comment));
	} else {
		m->comment[0]=0;
	}

	return 1;
}

static int ayReadInfo(struct moduleinfostruct *m, FILE *f, const char *buf, size_t len)
{
	return ayReadMemInfo(m, buf, len);
}

static void ayEvent(int event)
{
	switch (event)
	{
		case mdbEvInit:
		{
			fsRegisterExt("ay");
		}
	}
}

static void __attribute__((constructor))init(void)
{
	mdbRegisterReadInfo(&ayReadInfoReg);
}

static void __attribute__((destructor))done(void)
{
	mdbUnregisterReadInfo(&ayReadInfoReg);
}


static struct mdbreadinforegstruct ayReadInfoReg = {ayReadMemInfo, ayReadInfo, ayEvent MDBREADINFOREGSTRUCT_TAIL};
char *dllinfo = "";
struct linkinfostruct dllextinfo = {.name = "aytype", .desc = "OpenCP AY Detection (c) 2005-09 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
