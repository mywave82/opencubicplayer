/* OpenCP Module Player
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "stuff/cp437.h"
#include "stuff/compat.h"

static struct mdbreadinforegstruct ayReadInfoReg;

static int ayReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *buf, size_t len)
{
	int authorptr;
	const char *authorstr;

	int miscptr;
	const char *miscstr;

	signed int left;

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

	m->modtype.integer.i = MODULETYPE("AY");

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

	if ((left=(signed int)len-(authorstr-buf))>0) /* outside buffer-space */
	{
		if (memchr(authorstr, 0, left)) /* we have \0 before end of buffer */
		{
			cp437_f_to_utf8_z (authorstr, strlen (authorstr), m->composer, sizeof (m->composer));
		} else {
			cp437_f_to_utf8_z (authorstr, left, m->composer, sizeof (m->composer));
		}
	}

	if ((left=(signed int)len-(miscstr-buf))>0) /* outside buffer-space */
	{
		if (memchr(miscstr, 0, left)) /* we have \0 before end of buffer */
		{
			cp437_f_to_utf8_z (miscstr, strlen (miscstr), m->comment, sizeof (m->comment));
		} else {
			cp437_f_to_utf8_z (miscstr, left, m->comment, sizeof (m->comment));
		}
	}

	return 1;
}

static const char *AY_description[] =
{
	//                                                                          |
	"AY files are executable code that runs a virtual Z80 machine with a virtual",
	"AY-3-8910 sound IC. This IC a 3 channel programmable sound generator (PSG)",
	"that can generate sawtooth and pulse-wave (square) sounds.",
	NULL
};

static const struct interfaceparameters AY_p =
{
	"playay", "ayPlayer",
	0, 0
};


static void ayEvent(int event)
{
	switch (event)
	{
		case mdbEvInit:
		{
			struct moduletype mt;
			fsRegisterExt("ay");
			mt.integer.i = MODULETYPE("AY");
			fsTypeRegister (mt, AY_description, "plOpenCP", &AY_p);
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

static struct mdbreadinforegstruct ayReadInfoReg = {"AY", ayReadInfo, ayEvent MDBREADINFOREGSTRUCT_TAIL};
char *dllinfo = "";
struct linkinfostruct dllextinfo = {.name = "aytype", .desc = "OpenCP AY Detection (c) 2005-'22 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
