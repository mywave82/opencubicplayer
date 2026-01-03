/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
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

#define _POUTPUT_C

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "imsrtns.h"
#include "framelock.h"
#include "pfonts.h"
#include "poutput.h"
#include "poutput-keyboard.h"

unsigned char plpalette[256];

const struct FontSizeInfo_t FontSizeInfo[] =
{
	{8, 8},
	{8, 16},
	{16, 32}
};

void make_title (const char *part, int escapewarning)
{
	char prebuf[32];
	char buf[CONSOLE_MAX_X];
	const char *title = "Open Cubic Player v" VERSION;
	const char *copyright = "(c) 1994-'26 Stian Skjelstad";

	int spacem = plScrWidth - 2 - strlen (title) - strlen (copyright) - 2 - strlen (part);
	int space1 = spacem / 2;
	int space2 = spacem - space1;

	snprintf (prebuf, sizeof (prebuf), "  %%s%%%ds%%s%%%ds%%s  ", space1, space2);
	snprintf (buf, sizeof (buf), prebuf, title, "", part, "", copyright);

	if (plScrMode<100)
	{
		displaystr (0, 0, escapewarning?0xc0:0x30, buf, plScrWidth);
	} else {
		gdrawstr (0, 0, escapewarning?0xc0:0x30, buf, plScrWidth);
	}
}

void generic_gdrawchar8 (uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b)
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

void generic_gdrawchar8p (uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp)
{
	unsigned char *cp=plFont88[c];
	unsigned long p=y*plScrLineBytes+x;
	uint8_t *scr;
	uint8_t *pic;
	short i,j;

	if (!picp)
	{
		generic_gdrawchar8 (x,y,c,f,0);
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

void generic_gdrawstr (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	unsigned long p=16*y*plScrLineBytes+x*8;
	uint8_t *sp;
	uint16_t i,j,k;
	uint8_t f, b;

	sp=plVidMem+p;

	b=plpalette[attr >> 4  ]&0x0f;
	f=plpalette[attr & 0x0f]&0x0f;
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

void generic_gdrawcharp (uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp)
{

	unsigned char *cp=plFont816[c];
	unsigned long p=y*plScrLineBytes+x;
	uint8_t *pic=(uint8_t *)picp+p;
	uint8_t *scr;
	short i,j;

	if (!picp)
	{
		generic_gdrawchar (x,y,c,f,0);
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

void generic_gdrawchar (uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b)
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

void generic_gupdatestr (uint16_t y, uint16_t x, const uint16_t *str, uint16_t len, uint16_t *old)
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
