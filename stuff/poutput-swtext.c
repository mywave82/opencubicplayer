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
#include <string.h>
#include "types.h"
#include "latin1.h"
#include "pfonts.h"
#include "poutput.h"
#include "poutput-fontengine.h"
#include "poutput-swtext.h"

/* GNU unifont "poutput-fontengine" supports 8x16 (some glyphs are 16x16) */
/* OpenCubicPlayer built-in font supports (8x16) 8x8 and 4x4, in CP437 only */

#if 0
//not needed until we add UTF-8 support
static void swtext_displaycharattr_double8x16(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t f, b;

	target = plVidMem + y * 16 * plScrLineBytes + x * 8;

	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 16; i++)
	{
		uint8_t bitmap=*cp++;
		for (j=0; j < 8; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		bitmap=*cp++;
		for (j=0; j < 8; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 16;
		target += plScrLineBytes;
	}
}

static void swtext_displaycharattr_doublefirsthalf8x16(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t f, b;

	target = plVidMem + y * 16 * plScrLineBytes + x * 8;

	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 16; i++)
	{
		uint8_t bitmap=*cp++;
		cp++; /* skip one byte of the source font bitmap */
		for (j=0; j < 8; j++)
		{
			*target++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		target -= 8;
		target += plScrLineBytes;
	}
}
#endif

static void swtext_displaycharattr_single8x16(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t f, b;

	target = plVidMem + y * 16 * plScrLineBytes + x * 8;

	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 16; i++)
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

#if 0
//not needed until we add UTF-8 support
static int swtext_displaycharattr_unifont_8x16(uint16_t y, uint16_t x, const uint32_t codepoint, uint8_t attr, int width_left)
{
	uint8_t *cp;
	int fontwidth;

	cp = fontengine_8x16 (codepoint, &fontwidth);

	if (fontwidth == 16)
	{
		if (width_left >= 2)
		{
			swtext_displaycharattr_double8x16 (y, x, cp, attr);
			return 2;
		} else { /* we can only fit the first half */
			swtext_displaycharattr_doublefirsthalf8x16 (y, x, cp, attr);
			return 1;
		}
	} else if (fontwidth == 8)
	{
		swtext_displaycharattr_single8x16 (y, x, cp, attr);
		return 1;
	}
	return 0;
}
#endif

static void swtext_displaystrattr_unifont_8x16(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len, const uint16_t *codepage)
{
	if (codepage)
	{
		while (len)
		{
			uint8_t *cp;
			int fontwidth;

			if (x >= plScrWidth) return;

			cp = fontengine_8x16 (codepage[(*buf)&0x0ff], &fontwidth);
			/* all these codepoints should always use only one CELL */
			swtext_displaycharattr_single8x16 (y, x, cp, plpalette[((*buf)>>8)]);
			x += 1;
			len -= 1;
			buf++;
		}
	} else { /* codepage == NULL => optimization, ocp-cp437 is in the start of the unifont cache */
		while (len)
		{
			if (x >= plScrWidth) return;
			/* all these codepoints should always use only one CELL */
			swtext_displaycharattr_single8x16 (y, x, cp437_8x16[(*buf)&0x0ff].data, plpalette[((*buf)>>8)]);
			x += 1;
			len -= 1;
			buf++;
		}
	}
}

static void swtext_displaystr_unifont_8x16(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len, const uint16_t *codepage)
{
	if (codepage)
	{
		while (len)
		{
			uint8_t *cp;
			int fontwidth;

			if (x >= plScrWidth) return;

			/* all these codepoints should always use only one CELL */
			cp = fontengine_8x16 (codepage[*(uint8_t *)str], &fontwidth);
			swtext_displaycharattr_single8x16 (y, x, cp, attr);
			x += 1;
			len -= 1;
			if (*str) str++;
		}
	} else { /* codepage == NULL => optimization, ocp-cp437 is in the start of the unifont cache */
		while (len)
		{

			if (x >= plScrWidth) return;

			/* all these codepoints should always use only one CELL */
			swtext_displaycharattr_single8x16 (y, x, cp437_8x16[*(uint8_t *)str].data, attr);
			x += 1;
			len -= 1;
			if (*str) str++;
		}
	}
}

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

void swtext_displayvoid(uint16_t y, uint16_t x, uint16_t len)
{
	uint8_t *target;
	unsigned int length;
	unsigned int count;
	int i;
	switch (plCurrentFont)
	{
		default:
		case _8x16:
			target = plVidMem + y * 16 * plScrLineBytes + x * 8;
			length = len * 8;
			count = 16;
			break;
		case _8x8:
			target = plVidMem + y * 8 * plScrLineBytes + x * 8;
			length = len * 8;
			count = 8;
			break;
		case _4x4:
			target = plVidMem + y * 4 * plScrLineBytes + x * 4;
			length = len * 4;
			count = 4;
			break;
	}
	for (i=0; i < count; i++)
	{
		memset (target, 0, length);
		target += plScrLineBytes;
	}
}

void swtext_displaystrattr_cp437(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len)
{
	switch (plCurrentFont)
	{
		case _8x16:
			swtext_displaystrattr_unifont_8x16 (y, x, buf, len, 0);
			break;
		case _8x8:
			swtext_displaystrattr_cpfont_8x8 (y, x, buf, len, 0);
			break;
		case _4x4:
			swtext_displaystrattr_cpfont_4x4 (y, x, buf, len, 0);
			break;
	}
}

void swtext_displaystrattr_iso8859latin1(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len)
{
	switch (plCurrentFont)
	{
		case _8x16:
			swtext_displaystrattr_unifont_8x16 (y, x, buf, len, latin1_to_unicode);
			break;
		case _8x8:
			swtext_displaystrattr_cpfont_8x8 (y, x, buf, len, latin1_table);
			break;
		case _4x4:
			swtext_displaystrattr_cpfont_4x4 (y, x, buf, len, latin1_table);
			break;
	}
}


void swtext_displaystr_cp437(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	switch (plCurrentFont)
	{
		case _8x16:
			swtext_displaystr_unifont_8x16 (y, x, attr, str, len, 0);
			break;
		case _8x8:
			swtext_displaystr_cpfont_8x8 (y, x, attr, str, len, 0);
			break;
		case _4x4:
			swtext_displaystr_cpfont_4x4 (y, x, attr, str, len, 0);
			break;
	}
}

void swtext_displaystr_iso8859latin1(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	switch (plCurrentFont)
	{
		case _8x16:
			swtext_displaystr_unifont_8x16 (y, x, attr, str, len, latin1_to_unicode);
			break;
		case _8x8:
			swtext_displaystr_cpfont_8x8 (y, x, attr, str, len, latin1_table);
			break;
		case _4x4:
			swtext_displaystr_cpfont_4x4 (y, x, attr, str, len, latin1_table);
			break;
	}
}

void swtext_drawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	int yh1, yh2, yh3;

	uint8_t *target, f, b;
	int i;
	int font_width;
	int font_height;

	if (hgt>((yh*(unsigned)16)-4))
		hgt=(yh*16)-4;

	yh1=(yh+2)/3;
	yh2=(yh+yh1+1)/2 - yh1;
	yh3=yh-yh1-yh2;

	switch (plCurrentFont)
	{
		default:
		case _8x16:
			font_width = 8;
			font_height = 16;
			break;
		case _8x8:
			font_width = 8;
			font_height = 8;
			hgt >>= 1;
			break;
		case _4x4:
			font_width = 4;
			font_height = 4;
			hgt >>= 2;
			break;
	}
	target = plVidMem + ((yb + 1) * font_height - 1) * plScrLineBytes + x * font_width;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh1 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target -= plScrLineBytes;
	}
	c>>=8;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh2 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target -= plScrLineBytes;
	}
	c>>=8;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh3 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target -= plScrLineBytes;
	}
}

void swtext_idrawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	int yh1, yh2, yh3;

	uint8_t *target, f, b;
	int i;
	int font_width;
	int font_height;

	if (hgt>((yh*(unsigned)16)-4))
		hgt=(yh*16)-4;

	yh1=(yh+2)/3;
	yh2=(yh+yh1+1)/2 - yh1;
	yh3=yh-yh1-yh2;

	switch (plCurrentFont)
	{
		default:
		case _8x16:
			font_width = 8;
			font_height = 16;
			break;
		case _8x8:
			font_width = 8;
			font_height = 8;
			hgt >>= 1;
			break;
		case _4x4:
			font_width = 4;
			font_height = 4;
			hgt >>= 2;
			break;
	}
	target = plVidMem + (yb - yh + 1) * font_height * plScrLineBytes + x * font_width;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh1 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target += plScrLineBytes;
	}
	c>>=8;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh2 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target += plScrLineBytes;
	}
	c>>=8;
	f = c & 0x0f;
	b = (c >> 4) & 0x0f;
	for (i = yh3 * font_height; i > 0; i--)
	{
		if (hgt > 0)
		{
			memset (target, f, font_width-1);
			target[font_width-1] = b;
			hgt--;
		} else {
			memset (target, b, font_width);
		}
		target += plScrLineBytes;
	}
}
