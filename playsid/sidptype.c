/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * SIDPlay file type detection routines for the fileselector
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
 *  -kb980717  Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/mdb.h"

static const char latin1_table[256] = /* table to convert latin1 into OCP style cp437 */
{/*
	   0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f
*/
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // 0x
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // 1x
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, // 2x
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, // 3x
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, // 4x
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, // 5x
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, // 6x
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x20, // 7x
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // 8x
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // 9x
	0x20, 0xad, 0x9b, 0x9c,  'o', 0x9d,  '|', 0x15,  ' ',  'c', 0xa6, 0xae, 0xaa, 0xc4,  'r',  '-', // ax
	0xf8, 0xf1, 0xfd,  '3', '\'', 0xe6, 0x14, 0xfa, '\'',  '1', 0xa7, 0xaf, 0xac, 0xab,  ' ', 0xa8, // bx
	 'A',  'A',  'A',  'A', 0x8e, 0x8f, 0x92, 0x80,  'E', 0x90,  'E',  'E',  'I',  'I',  'I',  'I', // cx
	 'D', 0xa5,  'O',  'O',  'O',  'O', 0x99,  'x',  'O',  'U',  'U',  'U', 0x9a,  'Y',  'p', 0xe1, // dx
	0x86, 0xa0, 0x83,  'a', 0x85, 0x86, 0x91, 0x87, 0x8a, 0x82, 0x88, 0x89, 0x8d, 0xa1, 0x8c, 0x8b, // ex
	 'd', 0xa4, 0x95, 0xa2, 0x94,  'o', 0x95, 0xf6,  'o', 0x97, 0xa3, 0x96, 0x81,  'y',  'p', 0x98  // fx
};

struct __attribute__((packed)) psidHeader
{
	/* All values in big-endian order. */
	char id[4];          /* 'PSID'                                         */
	char version[2];     /* 0x0001 or 0x0002                               */
	char data[2];        /* 16-bit offset to binary data in file           */
	char load[2];        /* 16-bit C64 address to load file to             */
	char init[2];        /* 16-bit C64 address of init subroutine          */
	char play[2];        /* 16-bit C64 address of play subroutine          */
	char songs[2];       /* number of songs                                */
	char start[2];       /* start song (1-256 !)                           */
	char speed[4];       /* 32-bit speed info                              */
	                     /* bit: 0=50 Hz, 1=CIA 1 Timer A (default: 60 Hz) */
	char name[32];       /* ASCII strings, 31 characters long and          */
	char author[32];     /* terminated by a trailing zero                  */
	char copyright[32];
	char flags[2];       /* only version 0x0002                            */
	char reserved[4];    /* only version 0x0002                            */
};

static void latin1(char *dst, int dstlen, char *src)
{
	while ((dstlen-1) && (*src))
	{
		*dst = latin1_table[*(unsigned char *)src];
		dstlen--;
		dst++;
		src++;
	}
	*dst = 0;
}

static int sidReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	int i;
	struct psidHeader *ph;

	if (len<sizeof(struct psidHeader)+2)
		return 0;

	ph = (struct psidHeader *) buf;

	if ((!memcmp(ph->id,"PSID",4))||(!memcmp(ph->id,"RSID",4)))
	{
		m->modtype=mtSID;
		m->channels=ph->songs[1];
		latin1 (m->modname,  sizeof (m->modname),  ph->name);
		latin1 (m->composer, sizeof (m->composer), ph->author);
		if (ph->copyright[0])
		{
			strcpy (m->comment, "(C)");
			latin1 (m->comment + 3, sizeof (m->comment) - 3, ph->copyright);
		}
		return 1;
	}

	if ((buf[0]==0x00) & (buf[1]>=0x03) & (buf[2]==0x4c) &
	    (buf[4]>=buf[1]) & (buf[5]==0x4c) & (buf[7]>=buf[1]) )
	{
		char snginfo[33];

		m->modtype=mtSID;
		m->channels=1;

		snginfo[32]=0;
		memcpy(snginfo,buf+0x22,0x20);
		for (i=0; i<0x20; i++)
		{
			if (snginfo[i] && snginfo[i]<27)
				snginfo[i]|=0x40;
			if (snginfo[i]>=0x60)
				snginfo[i]=0;
		}
		if (strlen(snginfo)<6)
			strcpy(snginfo,"raw SID file");

		strcpy(m->modname, snginfo);
		m->composer[0] = 0;
		m->comment[0] = 0;
		return 1;
	}

	if (!memcmp(buf,"SIDPLAY INFOFILE",16) && (((char *)buf)[16]==0x0a || ((char *)buf)[16]==0x0d))
	{
		strcpy(m->modname, "SIDPlay info file");
		m->modtype=mtUnRead;
		return 1;
	}

	return 0;
}


static int sidReadInfo(struct moduleinfostruct *m, FILE *fp, const char *mem, size_t len)
{
	return 0;
}

static struct mdbreadinforegstruct sidReadInfoReg = {sidReadMemInfo, sidReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	mdbRegisterReadInfo(&sidReadInfoReg);
}

static void __attribute__((destructor))done(void)
{
	mdbUnregisterReadInfo(&sidReadInfoReg);
}

char *dllinfo = "";
struct linkinfostruct dllextinfo = {.name = "sidtype", .desc = "OpenCP SID Detection (c) 2005-09 Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
