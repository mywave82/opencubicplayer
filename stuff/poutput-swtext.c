/* OpenCP Module Player
 * copyright (c) 2020-'22 Stian Skjelstad <stian.skjelestad@gmail.com>
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
#include "framelock.h"
#include "latin1.h"
#include "pfonts.h"
#include "poutput.h"
#include "poutput-fontengine.h"
#include "poutput-swtext.h"
#include "utf-8.h"

/* GNU unifont "poutput-fontengine" supports 8x16 (some glyphs are 16x16) */
/* OpenCubicPlayer built-in font supports (8x16) 8x8 in CP437 only */

static void swtext_displaycharattr_double8x8(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t f, b;

	target = plVidMem + y * 8 * plScrLineBytes + x * 8;

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

static void swtext_displaycharattr_doublefirsthalf8x8(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t f, b;

	target = plVidMem + y * 8 * plScrLineBytes + x * 8;

	f = attr & 0x0f;
	b = attr >> 4;

	for (i=0; i < 8; i++)
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

static void swtext_displaycharattr_single8x8(uint16_t y, uint16_t x, uint8_t *cp, uint8_t attr)
{
	uint8_t *target;
	int i, j;
	uint8_t f, b;

	target = plVidMem + y * 8 * plScrLineBytes + x * 8;

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

static void swtext_displaystrattr_unifont_8x8(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len, const uint16_t *codepage)
{
	if (codepage)
	{
		while (len)
		{
			uint8_t *cp;
			int fontwidth;

			if (x >= plScrWidth) return;

			cp = fontengine_8x8 (codepage[(*buf)&0x0ff], &fontwidth);
			/* all these codepoints should always use only one CELL */
			swtext_displaycharattr_single8x8 (y, x, cp, plpalette[((*buf)>>8)]);
			x += 1;
			len -= 1;
			buf++;
		}
	} else { /* codepage == NULL => optimization, ocp-cp437 is in the start of the unifont cache */
		while (len)
		{
			if (x >= plScrWidth) return;
			/* all these codepoints should always use only one CELL */
			swtext_displaycharattr_single8x8 (y, x, cp437_8x8[(*buf)&0x0ff].data, plpalette[((*buf)>>8)]);
			x += 1;
			len -= 1;
			buf++;
		}
	}
}

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

static void swtext_displaystr_unifont_8x8(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len, const uint16_t *codepage)
{
	if (codepage)
	{
		while (len)
		{
			uint8_t *cp;
			int fontwidth;

			if (x >= plScrWidth) return;

			/* all these codepoints should always use only one CELL */
			cp = fontengine_8x8 (codepage[*(uint8_t *)str], &fontwidth);
			swtext_displaycharattr_single8x8 (y, x, cp, attr);
			x += 1;
			len -= 1;
			if (*str) str++;
		}
	} else { /* codepage == NULL => optimization, ocp-cp437 is in the start of the unifont cache */
		while (len)
		{

			if (x >= plScrWidth) return;

			/* all these codepoints should always use only one CELL */
			swtext_displaycharattr_single8x8 (y, x, cp437_8x8[*(uint8_t *)str].data, attr);
			x += 1;
			len -= 1;
			if (*str) str++;
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

static void swtext_displaystr_unifont_utf8_8x8(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	int _strlen = strlen (str);
	while (len)
	{
		int cp, inc;
		uint8_t *data;
		int fontwidth;

		if (x >= plScrWidth) return;

		cp = utf8_decode (str, _strlen, &inc);
		str += inc;
		_strlen -= inc;

		/* all these codepoints should always use only one CELL */
		data = fontengine_8x8 (cp, &fontwidth);

		if (fontwidth == 16)
		{
			if (len > 1)
			{
				swtext_displaycharattr_double8x8 (y, x, data, attr);
				x += 2;
				len -= 2;
			} else {
				swtext_displaycharattr_doublefirsthalf8x8 (y, x, data, attr);
				x += 1;
				len -= 1;
			}
		} else {
			swtext_displaycharattr_single8x8 (y, x, data, attr);
			x += 1;
			len -= 1;
		}
	}
}

static void swtext_displaystr_unifont_utf8_8x16(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	int _strlen = strlen (str);
	while (len)
	{
		int cp, inc;
		uint8_t *data;
		int fontwidth;

		if (x >= plScrWidth) return;

		cp = utf8_decode (str, _strlen, &inc);
		str += inc;
		_strlen -= inc;

		/* all these codepoints should always use only one CELL */
		data = fontengine_8x16 (cp, &fontwidth);

		if (fontwidth == 16)
		{
			if (len > 1)
			{
				swtext_displaycharattr_double8x16 (y, x, data, attr);
				x += 2;
				len -= 2;
			} else {
				swtext_displaycharattr_doublefirsthalf8x16 (y, x, data, attr);
				x += 1;
				len -= 1;
			}
		} else {
			swtext_displaycharattr_single8x16 (y, x, data, attr);
			x += 1;
			len -= 1;
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
			swtext_displaystrattr_unifont_8x8  (y, x, buf, len, 0);
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
			swtext_displaystr_unifont_8x8  (y, x, attr, str, len, 0);
			break;
	}
}

void swtext_displaystr_utf8(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	switch (plCurrentFont)
	{
		case _8x16:
			swtext_displaystr_unifont_utf8_8x16 (y, x, attr, str, len);
			return;
		case _8x8:
			swtext_displaystr_unifont_utf8_8x8  (y, x, attr, str, len);
			return;
	}
}

int swtext_measurestr_utf8 (const char *src, int srclen)
{
	int retval = 0;
	while (srclen > 0)
	{
		int cp, inc;
		int fontwidth;

		cp = utf8_decode (src, srclen, &inc);
		src += inc;
		srclen -= inc;

		fontengine_8x16 (cp, &fontwidth);

		if (fontwidth == 16)
		{
			retval += 2;
		} else if (fontwidth == 8)
		{
			retval++;
		}
	}
	return retval;
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

static unsigned int swtext_curshape=0, swtext_curposx=0, swtext_curposy=0;
static uint8_t swtext_cursor_buffer[16*8];
static int swtext_shapetimer=0;
static int swtext_shapetoggler=0;
static int swtext_shapestatus = 0;

void swtext_setcur(uint16_t y, uint16_t x)
{
	swtext_curposx=x;
	swtext_curposy=y;
}

void swtext_setcurshape(uint16_t shape)
{
	swtext_curshape=shape;
}

void swtext_cursor_inject (void)
{
	swtext_shapestatus = 0;

	/* if we have an active cursor, iterate the blink-timer */
	if (swtext_curshape)
	{
		swtext_shapetimer++;
		if (swtext_shapetimer >= ((fsFPS<=3)?1:(fsFPS / 3)))
		{
			swtext_shapetoggler^=1;
			swtext_shapetimer=0;
		}
		if (swtext_shapetoggler)
		{
			swtext_shapestatus=swtext_curshape;
		}
	}

#warning we need a swtext_curshapeattr API, instead of guessing the colors (we no longer have vgamem)
	if (swtext_shapestatus == 1)
	{ /* save original buffer, and add a color 15 _ marker */
		switch (plCurrentFont)
		{
			case _8x16:
				memcpy (swtext_cursor_buffer + 0, plVidMem + swtext_curposx * 8 + (swtext_curposy * 16 + 13) * plScrLineBytes, 8);
				memcpy (swtext_cursor_buffer + 8, plVidMem + swtext_curposx * 8 + (swtext_curposy * 16 + 14) * plScrLineBytes, 8);
				memset (plVidMem + swtext_curposx * 8 + (swtext_curposy * 16 + 13) * plScrLineBytes, 15, 8);
				memset (plVidMem + swtext_curposx * 8 + (swtext_curposy * 16 + 14) * plScrLineBytes, 14, 8);
				break;
			case _8x8:
				memcpy (swtext_cursor_buffer, plVidMem + swtext_curposx * 8 + (swtext_curposy * 8 + 7) * plScrLineBytes, 8);
				memset (plVidMem + swtext_curposx * 8 + (swtext_curposy * 8 + 7) * plScrLineBytes, 15, 8);
				break;
		}
	} else if (swtext_shapestatus == 2)
	{ /* backup original memory, and rewrite with \xdb snoop background color, and use fixed white foreground */
		int i;
		uint8_t c = 0x0f;
		switch (plCurrentFont)
		{
			case _8x16:
				c |= plVidMem[swtext_curposx * 8 + 7 + swtext_curposy * 16 * plScrLineBytes] << 4;
				for (i=0;i<16;i++)
				{
					memcpy (swtext_cursor_buffer + i * 8, plVidMem + swtext_curposx * 8 + (swtext_curposy * 16 + i) * plScrLineBytes, 8);
				}
				break;
			case _8x8:
				c |= plVidMem[swtext_curposx * 8 + 7 + swtext_curposy * 8 * plScrLineBytes] << 4;
				for (i=0;i<8;i++)
				{
					memcpy (swtext_cursor_buffer + i * 8, plVidMem + swtext_curposx * 8 + (swtext_curposy * 8 + i) * plScrLineBytes, 8);
				}
				break;
		}
		swtext_displaystr_cp437 (swtext_curposy, swtext_curposx, c, "\xdb", 1);
	}
}

void swtext_cursor_eject (void)
{
	/* restore original buffer */
	if (swtext_shapestatus == 1)
	{
		switch (plCurrentFont)
		{
			case _8x16:
				memcpy (plVidMem + swtext_curposx * 8 + (swtext_curposy * 16 + 13) * plScrLineBytes, swtext_cursor_buffer + 0, 8);
				memcpy (plVidMem + swtext_curposx * 8 + (swtext_curposy * 16 + 14) * plScrLineBytes, swtext_cursor_buffer + 8, 8);
				break;
			case _8x8:
				memcpy (plVidMem + swtext_curposx * 8 + (swtext_curposy * 8 + 7) * plScrLineBytes, swtext_cursor_buffer, 8);
				break;
		}
	} else if (swtext_shapestatus == 2)
	{ /* backup original memory, and rewrite with \xdb snoop background color, and use fixed white foreground */
		int i;
		switch (plCurrentFont)
		{
			case _8x16:
				for (i=0;i<16;i++)
				{
					memcpy (plVidMem + swtext_curposx * 8 + (swtext_curposy * 16 + i) * plScrLineBytes, swtext_cursor_buffer + i * 8, 8);
				}
				break;
			case _8x8:
				for (i=0;i<8;i++)
				{
					memcpy (plVidMem + swtext_curposx * 8 + (swtext_curposy * 8 + i) * plScrLineBytes, swtext_cursor_buffer + i * 8, 8);
				}
				break;
		}
	}
}
