/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Routines for screen output
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
 *  -kb980717   Tammo Hinrichs <kb@nwn.de>
 *    -did a LOT to avoid redundant mode switching (really needed with
 *     today's monitors)
 *    -added color look-up table for redefining the palette
 *    -added MDA output mode for all Hercules enthusiasts out there
 *     (really rocks in Windows 95 background :)
 *  -fd981220   Felix Domke    <tmbinc@gmx.net>
 *    -faked in a LFB-mode (required some other changes)
 *  -doj20020418 Dirk Jagdmann <doj@cubic.org>
 *    -added screenshot routines
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -more or less killed the entire file due to the unix implementation, but
 *     the generic functions is still kept in this file.
 */

#define _CONSOLE_DRIVER
#include "config.h"
#include <string.h>
#include "types.h"
#include "pfonts.h"
#include "imsrtns.h"
#include "poutput.h"

unsigned char plpalette[256];
int plScrLineBytes;
int plScrLines;

FontSizeEnum plCurrentFont;
const struct FontSizeInfo_t FontSizeInfo[] =
{
	{8, 8},
	{8, 16}
};


void make_title(char *part, int escapewarning)
{
// DEBUG TODO sprintf(tstr, "%02i%% %08X %s", tmGetCpuUsage(),/* debugint, debugstr*/ 0, "");
	char prebuf[32];
	char buf[CONSOLE_MAX_X];
	const char *title = "Open Cubic Player v" VERSION;
	const char *copyright = "(c) 1994-2021 Stian Skjelstad";

	int spacem = plScrWidth - 2 - strlen (title) - strlen (copyright) - 2 - strlen (part);
	int space1 = spacem / 2;
	int space2 = spacem - space1;

	snprintf (prebuf, sizeof (prebuf), "  %%s%%%ds%%s%%%ds%%s  ", space1, space2);
	snprintf (buf, sizeof (buf), prebuf, title, "", part, "", copyright);

	if (plScrMode<100)
	{
		_displaystr (0, 0, escapewarning?0xc0:0x30, buf, plScrWidth);
	} else {
		_gdrawstr (0, 0, escapewarning?0xc0:0x30, buf, plScrWidth);
	}
}

char *convnum(unsigned long num, char *buf, unsigned char radix, unsigned short len, char clip0)
{
	int i;
	for (i=0; i<len; i++)
	{
		buf[len-1-i]="0123456789ABCDEF"[num%radix];
		num/=radix;
	}
	buf[len]=0;
	if (clip0)
		for (i=0; i<(len-1); i++)
		{
			if (buf[i]!='0')
				break;
			buf[i]=' ';
		}
	return buf;
}

void writenum(uint16_t *buf, unsigned short ofs, unsigned char attr, unsigned long num, unsigned char radix, unsigned short len, char clip0)
{
	char convbuf[20];
	uint16_t *p=buf+ofs;
	char *cp=convbuf+len;
	int i;
	for (i=0; i<len; i++)
	{
		*--cp="0123456789ABCDEF"[num%radix];
		num/=radix;
	}
	for (i=0; i<len; i++)
	{
		if (clip0&&(convbuf[i]=='0')&&(i!=(len-1)))
		{
			*p++=' '|(attr<<8);
			cp++;
		} else {
			*p++=(*cp++)|(attr<<8);
			clip0=0;
		}
	}
}

void writestring(uint16_t *buf, unsigned short ofs, unsigned char attr, const char *str, unsigned short len)
{
	uint16_t *p=buf+ofs;
	int i;
	for (i=0; i<len; i++)
	{
		*p++=(*((unsigned char *)(str)))|(attr<<8);
		if (*str)
			str++;
	}
}

void writestringattr(uint16_t *buf, unsigned short ofs, const uint16_t *str, unsigned short len)
{
	memcpyb(buf+ofs, (void *)str, len*2);
}

void fillstr(uint16_t *buf, const unsigned short ofs, const unsigned char chr, const unsigned char attr, unsigned short len)
{
	buf+=ofs;
	while(len)
	{
		*buf=(chr<<8)+attr;
		buf++;
		len--;
	}

}

void generic_gdrawchar8(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b)
{
	unsigned char *cp=plFont88[c];
	unsigned long p=y*plScrLineBytes+x;
	uint8_t *scr;
	short i,j;

	scr=plVidMem+p;

	f=plpalette[f]&0x0f;
	b=plpalette[b]&0x0f;

	for (i=0; i<8; i++)
	{
		unsigned char bitmap=*cp++;
		for (j=0; j<8; j++)
		{
			*scr++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		scr+=plScrLineBytes-8;
	}
}

void generic_gdrawchar8p(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp)
{
	unsigned char *cp=plFont88[c];
	unsigned long p=y*plScrLineBytes+x;
	uint8_t *scr;
	uint8_t *pic;
	short i,j;

	if (!picp)
	{
		_gdrawchar8(x,y,c,f,0);
		return;
	}

	f=plpalette[f]&0x0f;
	scr=plVidMem+p;
	pic=(uint8_t *)picp+p;

	for (i=0; i<8; i++)
	{
		unsigned char bitmap=*cp++;
		for (j=0; j<8; j++)
		{
			if (bitmap&128)
				*scr=f;
			else
				*scr=*pic;
			scr++;
			pic++;
			bitmap<<=1;
		}
		scr+=plScrLineBytes-8;
		pic+=plScrLineBytes-8;
	}
}

void generic_gdrawstr(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	unsigned long p=16*y*plScrLineBytes+x*8;
	uint8_t *sp;
	uint16_t i,j,k;
	uint8_t f, b;

	sp=plVidMem+p;

	f=plpalette[attr >> 4  ]&0x0f;
	b=plpalette[attr & 0x0f]&0x0f;
	for (i=0; i<16; i++)
	{
		const unsigned char *s=(unsigned char *)str;
		for (k=0; k<len; k++)
		{
			unsigned char bitmap=plFont816[*s][i];
			for (j=0; j<8; j++)
			{
				*sp++=(bitmap&128)?f:b;
				bitmap<<=1;
			}
			if (*s)
				s++;
		}
		sp+=plScrLineBytes-8*len;
	}
}

void generic_gdrawcharp(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp)
{

	unsigned char *cp=plFont816[c];
	unsigned long p=y*plScrLineBytes+x;
	uint8_t *pic=(uint8_t *)picp+p;
	uint8_t *scr;
	short i,j;

	if (!picp)
	{
		_gdrawchar(x,y,c,f,0);
		return;
	}

	scr=plVidMem+p;

	f=plpalette[f]&0x0f;

	for (i=0; i<16; i++)
	{
		unsigned char bitmap=*cp++;
		for (j=0; j<8; j++)
		{
			if (bitmap&128)
				*scr=f;
			else
				*scr=*pic;
			scr++;
			pic++;
			bitmap<<=1;
		}
		pic+=plScrLineBytes-8;
		scr+=plScrLineBytes-8;
	}
}

void generic_gdrawchar(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b)
{
	unsigned char *cp=plFont816[c];
	unsigned long p=y*plScrLineBytes+x;
	uint8_t *scr;
	short i,j;

	f=plpalette[f]&0x0f;
	b=plpalette[b]&0x0f;
	scr=plVidMem+p;

	for (i=0; i<16; i++)
	{
		unsigned char bitmap=*cp++;
		for (j=0; j<8; j++)
		{
			*scr++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		scr+=plScrLineBytes-8;
	}
}

void generic_gupdatestr(unsigned short y, unsigned short x, const uint16_t *str, unsigned short len, uint16_t *old)
{
	unsigned long p=16*y*plScrLineBytes+x*8;
	uint8_t *sp;
	short i,j,k;

	sp=plVidMem+p;

	for (k=0; k<len; k++, str++, old++)
		if (*str!=*old)
		{
			uint8_t a = (*str)>>8;
			unsigned char *bitmap0=plFont816[(*str)&0xff];
			unsigned char f=plpalette[a]&0x0F;
			unsigned char b=plpalette[a]>>4;
			*old=*str;

			for (i=0; i<16; i++)
			{
				unsigned char bitmap=bitmap0[i];
				for (j=0; j<8; j++)
				{
					*sp++=(bitmap&128)?f:b;
					bitmap<<=1;
				}
				sp+=plScrLineBytes-8;
			}
			sp-=16*plScrLineBytes-8;
		} else
			sp+=8;
}
