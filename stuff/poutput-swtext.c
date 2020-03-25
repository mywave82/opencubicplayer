/* OpenCP Module Player
 * copyright (c) 2020 Stian Skjelstad <stian.skjelestad@gmail.com>
 *
 * Text render functions that can be used if the text-rendering is done
 * virtually using plVidMem API
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

#define _CONSOLE_DRIVER
#include "config.h"
#include "types.h"
#include "pfonts.h"
#include "poutput.h"
#include "poutput-swtext.h"

static void swtext_displaycharattr_cpfont_8x8(uint16_t y, uint16_t x, const uint8_t ch, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t *cp;
	uint8_t f, b;

	target = plVidMem + y * 8 * plScrLineBytes + x * 8;

	cp = plFont88[ch];
	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 8; i++)
	{
		uint8_t bitmap=*cp++;
		for (j=0; j < 8; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 8;
		target += plScrLineBytes;
	}
}

/* codepage is optional, to translate...*/
void swtext_displaystrattr_cpfont_8x8(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len, const uint8_t *codepage)
{
	while (len)
	{
		uint8_t ch;

		if (x >= plScrWidth) return;

		ch = (*buf)&0x0ff;
		if (codepage)
		{
			ch = codepage[ch];
		}
		swtext_displaycharattr_cpfont_8x8 (y, x, ch, plpalette[((*buf)>>8)]);
		buf++;
		len--;
		x++;
	}
}

void swtext_displaystr_cpfont_8x8(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len, const uint8_t *codepage)
{
	while (len)
	{
		uint8_t ch;

		if (x >= plScrWidth) return;

		ch = *str;
		if (codepage)
		{
			ch = codepage[ch];
		}

		swtext_displaycharattr_cpfont_8x8 (y, x, ch, attr);

		if (*str) str++;
		len--;
		x++;
	}
}

static void swtext_displaycharattr_cpfont_4x4(uint16_t y, uint16_t x, const uint8_t ch, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t *cp;
	uint8_t f, b;

	target = plVidMem + y * 4 * plScrLineBytes + x * 4;

	cp = plFont44[ch];
	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 2; i++)
	{ /* we get two lines of data per byte in the font */
		uint8_t bitmap=*cp++;
		for (j=0; j < 4; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 4;
		target += plScrLineBytes;
		for (j=0; j < 4; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 4;
		target += plScrLineBytes;
	}
}

void swtext_displaystrattr_cpfont_4x4(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len, const uint8_t *codepage)
{
	while (len)
	{
		uint8_t ch;

		if (x >= plScrWidth) return;

		ch = (*buf)&0x0ff;
		if (codepage)
		{
			ch = codepage[ch];
		}
		swtext_displaycharattr_cpfont_4x4 (y, x, ch, plpalette[((*buf)>>8)]);
		buf++;
		len--;
		x++;
	}
}

void swtext_displaystr_cpfont_4x4(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len, const uint8_t *codepage)
{
	while (len)
	{
		uint8_t ch;

		if (x >= plScrWidth) return;

		ch = *str;
		if (codepage)
		{
			ch = codepage[ch];
		}

		swtext_displaycharattr_cpfont_4x4 (y, x, ch, attr);

		if (*str) str++;
		len--;
		x++;
	}
}
