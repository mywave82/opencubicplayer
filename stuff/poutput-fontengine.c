/* OpenCP Module Player
 * copyright (c) 2020-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Basic glue for the different console implementations for unix
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
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "boot/psetting.h"
#include "cp437.h"
#include "pfonts.h"
#include "poutput-fontengine.h"
#include "ttf.h"
#include "utf-8.h"

/* TODO: unicode points to skip: in UTF-8
 *
 * UNICODE_BOM_NATIVE  0xFEFF
 * UNICODE_BOM_SWAPPED 0xFFFE
 */

static struct font_entry_8x8_t **font_entries_8x8;
static int font_entries_8x8_fill;
static int font_entries_8x8_allocated;

static struct font_entry_8x16_t **font_entries_8x16;
static int font_entries_8x16_fill;
static int font_entries_8x16_allocated;

static struct font_entry_16x32_t **font_entries_16x32;
static int font_entries_16x32_fill;
static int font_entries_16x32_allocated;

static TTF_Font *unifont_bmp;
#if defined(UNIFONT_CSUR_TTF) || defined(UNIFONT_CSUR_OTF) || defined (UNIFONT_RELATIVE)
static TTF_Font *unifont_csur;
#endif
static TTF_Font *unifont_upper;

struct font_entry_8x8_t   cp437_8x8  [256];
struct font_entry_8x16_t  cp437_8x16 [256];
struct font_entry_16x32_t cp437_16x32[256];

static struct font_entry_8x8_t  latin1_8x8 [sizeof(plFont_8x8_latin1_addons)  / sizeof (plFont_8x8_latin1_addons[0] )];
static struct font_entry_8x16_t latin1_8x16[sizeof(plFont_8x16_latin1_addons) / sizeof (plFont_8x16_latin1_addons[0])];
static struct font_entry_16x32_t latin1_16x32[sizeof(plFont_16x32_latin1_addons) / sizeof (plFont_16x32_latin1_addons[0])];

/*
 BMP   = Basic Multilingual Plane            https://en.wikipedia.org/wiki/Plane_(Unicode)#Basic_Multilingual_Plane
 SMP   = Supplementary Multilingual Plane    https://en.wikipedia.org/wiki/Plane_(Unicode)#Supplementary_Multilingual_Plane
 SIP   = Supplementary Ideographic Plane     https://en.wikipedia.org/wiki/Plane_(Unicode)#Supplementary_Ideographic_Plane
 TIP   = Tertiary Ideographic Plane          https://en.wikipedia.org/wiki/Plane_(Unicode)#Tertiary_Ideographic_Plane
 SSP   = Supplementary Special-purpose Plane https://en.wikipedia.org/wiki/Plane_(Unicode)#Supplementary_Special-purpose_Plane

 CSUR  = ConScript Unicode Registry          https://en.wikipedia.org/wiki/ConScript_Unicode_Registry
 UCSUR = Under-ConScript Unicode Registry    https://en.wikipedia.org/wiki/ConScript_Unicode_Registry

RANGE                DATAFILE  NAME
U+000000 - U+00D7FF  BMP
U+00E000 - U+00F8FF  CSUR      (CSUR/UCSUR)
U+00F900 - U+00FFFF  BMP
U+010000 - U+01FFFF  UPPER     (SMP)
U+020000 - U+02FFFF  -----     (SIP)
U+030000 - U+03FFFF  -----     (TIP)
..
U+0E0000 - U+0EFFFF  UPPER     (SPP)
U+0F0000 - U+0FFFFD  CSUR      (CSUR/UCSUR)
*/

/* returns the new index */
static int fontengine_8x8_scoreup (int index)
{
	if (font_entries_8x8[index]->score >= 254)
	{
		return index;
	}
	font_entries_8x8[index]->score++;
	while (1)
	{
		struct font_entry_8x8_t *temp;
		if (!index)
		{
			return index;
		}
		if (font_entries_8x8[index-1]->score >= font_entries_8x8[index]->score)
		{
			return index;
		}
		temp = font_entries_8x8[index-1];
		font_entries_8x8[index-1] = font_entries_8x8[index];
		font_entries_8x8[index] = temp;
		index--;
	}
	return index;
}

/* returns the new index */
static int fontengine_8x16_scoreup (int index)
{
	if (font_entries_8x16[index]->score >= 254)
	{
		return index;
	}
	font_entries_8x16[index]->score++;
	while (1)
	{
		struct font_entry_8x16_t *temp;
		if (!index)
		{
			return index;
		}
		if (font_entries_8x16[index-1]->score >= font_entries_8x16[index]->score)
		{
			return index;
		}
		temp = font_entries_8x16[index-1];
		font_entries_8x16[index-1] = font_entries_8x16[index];
		font_entries_8x16[index] = temp;
		index--;
	}
	return index;
}

/* returns the new index */
static int fontengine_16x32_scoreup (int index)
{
	if (font_entries_16x32[index]->score >= 254)
	{
		return index;
	}
	font_entries_16x32[index]->score++;
	while (1)
	{
		struct font_entry_16x32_t *temp;
		if (!index)
		{
			return index;
		}
		if (font_entries_16x32[index-1]->score >= font_entries_16x32[index]->score)
		{
			return index;
		}
		temp = font_entries_16x32[index-1];
		font_entries_16x32[index-1] = font_entries_16x32[index];
		font_entries_16x32[index] = temp;
		index--;
	}
	return index;
}

static void fontengine_8x8_append (struct font_entry_8x8_t *entry)
{
	if (font_entries_8x8_fill >= font_entries_8x8_allocated)
	{
		int newallocated = font_entries_8x8_allocated += 64;
		struct font_entry_8x8_t **newentries = realloc (font_entries_8x8, font_entries_8x8_allocated * sizeof (font_entries_8x8[0]));

		if (!newentries)
		{
			fprintf (stderr, "fontengine_8x8_append: malloc() failure....\n");
			return;
		}

		font_entries_8x8 = newentries;
		font_entries_8x8_allocated = newallocated;
	}
	font_entries_8x8[font_entries_8x8_fill] = entry;
	font_entries_8x8_fill++;
	/* initial score is 5 */
	fontengine_8x8_scoreup (font_entries_8x8_fill-1);
	fontengine_8x8_scoreup (font_entries_8x8_fill-1);
	fontengine_8x8_scoreup (font_entries_8x8_fill-1);
	fontengine_8x8_scoreup (font_entries_8x8_fill-1);
	fontengine_8x8_scoreup (font_entries_8x8_fill-1);
}

static void fontengine_8x16_append (struct font_entry_8x16_t *entry)
{
	if (font_entries_8x16_fill >= font_entries_8x16_allocated)
	{
		int newallocated = font_entries_8x16_allocated += 64;
		struct font_entry_8x16_t **newentries = realloc (font_entries_8x16, font_entries_8x16_allocated * sizeof (font_entries_8x16[0]));

		if (!newentries)
		{
			fprintf (stderr, "fontengine_8x16_append: malloc() failure....\n");
			return;
		}

		font_entries_8x16 = newentries;
		font_entries_8x16_allocated = newallocated;
	}
	font_entries_8x16[font_entries_8x16_fill] = entry;
	font_entries_8x16_fill++;
	/* initial score is 5 */
	fontengine_8x16_scoreup (font_entries_8x16_fill-1);
	fontengine_8x16_scoreup (font_entries_8x16_fill-1);
	fontengine_8x16_scoreup (font_entries_8x16_fill-1);
	fontengine_8x16_scoreup (font_entries_8x16_fill-1);
	fontengine_8x16_scoreup (font_entries_8x16_fill-1);
}

static void fontengine_16x32_append (struct font_entry_16x32_t *entry)
{
	if (font_entries_16x32_fill >= font_entries_16x32_allocated)
	{
		int newallocated = font_entries_16x32_allocated += 64;
		struct font_entry_16x32_t **newentries = realloc (font_entries_16x32, font_entries_16x32_allocated * sizeof (font_entries_16x32[0]));

		if (!newentries)
		{
			fprintf (stderr, "fontengine_16x32_append: malloc() failure....\n");
			return;
		}

		font_entries_16x32 = newentries;
		font_entries_16x32_allocated = newallocated;
	}
	font_entries_16x32[font_entries_16x32_fill] = entry;
	font_entries_16x32_fill++;
	/* initial score is 5 */
	fontengine_16x32_scoreup (font_entries_16x32_fill-1);
	fontengine_16x32_scoreup (font_entries_16x32_fill-1);
	fontengine_16x32_scoreup (font_entries_16x32_fill-1);
	fontengine_16x32_scoreup (font_entries_16x32_fill-1);
	fontengine_16x32_scoreup (font_entries_16x32_fill-1);
}

void fontengine_8x8_iterate (void)
{
	int i;

	for (i=font_entries_8x8_fill-1; i >= 0; i--)
	{
		if (font_entries_8x8[i]->score == 255)
		{
			continue;
		}
		font_entries_8x8[i]->score--;
		if (font_entries_8x8[i]->score)
		{
			continue;
		}
		/* if a score reaches zero, we should be at the end of the list..... since we sort the list */
		free (font_entries_8x8[i]);
		font_entries_8x8[i] = 0;
		font_entries_8x8_fill--;
		assert (font_entries_8x8_fill == i);
	}
}

int fontengine_8x8_forceunifont (uint32_t codepoint, int *width, uint8_t data[16])
{
	TTF_Surface *text_surface = 0;

	if (codepoint == 0)
	{
		codepoint = ' ';
	}

	       if (                            (codepoint <= 0x0d8ff)  ||
	            ((codepoint >= 0x0f900) && (codepoint <= 0x0ffff)) )
	{
		text_surface = unifont_bmp ? TTF_RenderGlyph32_Shaded (unifont_bmp, codepoint) : 0;
#if defined(UNIFONT_CSUR_TTF) || defined(UNIFONT_CSUR_OTF)
	} else if ( ((codepoint >= 0x0e000) && (codepoint <= 0x0f8ff)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
#endif
	} else if ( ((codepoint >= 0x10000) && (codepoint <= 0x1ffff)) ||
	            ((codepoint >= 0xe0000) && (codepoint <= 0xeffff)) )
	{
		text_surface = unifont_upper ? TTF_RenderGlyph32_Shaded (unifont_upper, codepoint) : 0;
#if defined(UNIFONT_CSUR_TTF) || defined(UNIFONT_CSUR_OTF)
	} else if ( ((codepoint >= 0xf0000) && (codepoint >= 0xffffd)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
#endif
	}

	if (text_surface && ((text_surface->w == 8) || (text_surface->w == 16)) && (text_surface->h == 16))
	{
		int x, y, o=0, i=0;
		uint8_t data8[32];
		*width = text_surface->w;

		for (y=0; y < text_surface->h; y++)
		{
			for (x=0; x < text_surface->w; x+=8)
			{
				data8[o] = 0;
				if (((uint8_t*)text_surface->pixels)[i++]) data8[o] |= 0x80;
				if (((uint8_t*)text_surface->pixels)[i++]) data8[o] |= 0x40;
				if (((uint8_t*)text_surface->pixels)[i++]) data8[o] |= 0x20;
				if (((uint8_t*)text_surface->pixels)[i++]) data8[o] |= 0x10;
				if (((uint8_t*)text_surface->pixels)[i++]) data8[o] |= 0x08;
				if (((uint8_t*)text_surface->pixels)[i++]) data8[o] |= 0x04;
				if (((uint8_t*)text_surface->pixels)[i++]) data8[o] |= 0x02;
				if (((uint8_t*)text_surface->pixels)[i++]) data8[o] |= 0x01;
				o++;
			}
			i -= text_surface->w;
			i += text_surface->pitch;
		}

		/* SCALE */
		if (*width == 16)
		{
			int iter = 0;
			int height = 16;
			uint16_t data16[16];
			memcpy (data16, data8, 32);
			while (height > 8)
			{
				int i;
				iter=!iter;
				if (data16[0] == 0) /* first is blank */
				{
					memmove (data16, data16 + 1, (height - 1)<<1);
					height--;
					if (height == 8) break;
				}

				if ((data16[height-2] == 0) && (data16[height-1])) /* two last are blank */
				{
					height--;
					continue;
				}

				if (iter)
				{
					for (i=0; i < (height - 1); i++) /* find duplicates */
					{
						if (data16[i] == data16[i+1])
						{
							memmove (data16 + i, data16 + i + 1, (height - i - 1)<<1);
							height--;
							i = 20;
							break;
						}
					}
				} else {
					for (i=height - 2; i >= 0; i--) /* find duplicates */
					{
						if (data16[i] == data16[i+1])
						{
							memmove (data16 + i, data16 + i + 1, (height - i - 1)<<1);
							height--;
							i = 20;
							break;
						}
					}
				}
				if (i == 20) continue;

				if ((data16[height-1])) /* last last is blank */
				{
					height--;
					continue;
				}

				for (i=0;;i++) /* merge lines */
				{
					data16[height - i - 1] |= data16[height - i];
					memmove (data16 + height - i - 1, data16 + height - i, (i)<<1);
					height--;
					if (height <= 8) break;
				}
			}
			memcpy (data, data16, 16);
		} else {
			int iter = 0;
			int height = 16;
			while (height > 8)
			{
				int i;
				iter=!iter;
				if (data8[0] == 0) /* first is blank */
				{
					memmove (data8, data8 + 1, height - 1);
					height--;
					if (height == 8) break;
				}

				if ((data8[height-2] == 0) && (data8[height-1])) /* two last are blank */
				{
					height--;
					continue;
				}

				if (iter)
				{
					for (i=0; i < (height - 1); i++) /* find duplicates */
					{
						if (data8[i] == data8[i+1])
						{
							memmove (data8 + i, data8 + i + 1, height - i - 1);
							height--;
							i = 20;
							break;
						}
					}
				} else {
					for (i=height - 2; i >= 0; i--) /* find duplicates */
					{
						if (data8[i] == data8[i+1])
						{
							memmove (data8 + i, data8 + i + 1, height - i - 1);
							height--;
							i = 20;
							break;
						}
					}
				}
				if (i == 20) continue;

				if ((data8[height-1])) /* last last is blank */
				{
					height--;
					continue;
				}

				for (i=0;;i++) /* merge lines */
				{
					data8[height - i - 1] |= data8[height - i];
					memmove (data8 + height - i - 1, data8 + height - i, i);
					height--;
					if (height <= 8) break;
				}
			}
			memcpy (data, data8, 8);
		}
		return 0;
	} else {
		*width = 8;
		memset (data, 0, 16);
		return 1;
	}
}

/* width will be set to 8 or 16, depending on the glyph */
uint8_t *fontengine_8x8(uint32_t codepoint, int *width)
{
	int i;
	struct font_entry_8x8_t *entry = 0;

	if (codepoint == 0)
	{
		codepoint = ' ';
	}

	for (i=0; i < font_entries_8x8_fill; i++)
	{
		if (font_entries_8x8[i]->codepoint == codepoint)
		{
			i = fontengine_8x8_scoreup (i);
			i = fontengine_8x8_scoreup (i);
			*width = font_entries_8x8[i]->width;
			return font_entries_8x8[i]->data;
		}
	}

	entry = malloc (sizeof (*entry));
	fontengine_8x8_forceunifont (codepoint, width, entry->data);

	entry->width = *width;
	entry->codepoint = codepoint;
	entry->score=0;
	fontengine_8x8_append(entry);

	return entry->data;
}

void fontengine_8x16_iterate (void)
{
	int i;

	for (i=font_entries_8x16_fill-1; i >= 0; i--)
	{
		if (font_entries_8x16[i]->score == 255)
		{
			continue;
		}
		font_entries_8x16[i]->score--;
		if (font_entries_8x16[i]->score)
		{
			continue;
		}
		/* if a score reaches zero, we should be at the end of the list..... since we sort the list */
		free (font_entries_8x16[i]);
		font_entries_8x16[i] = 0;
		font_entries_8x16_fill--;
		assert (font_entries_8x16_fill == i);
	}
}

void fontengine_16x32_iterate (void)
{
	int i;

	for (i=font_entries_16x32_fill-1; i >= 0; i--)
	{
		if (font_entries_16x32[i]->score == 255)
		{
			continue;
		}
		font_entries_16x32[i]->score--;
		if (font_entries_16x32[i]->score)
		{
			continue;
		}
		/* if a score reaches zero, we should be at the end of the list..... since we sort the list */
		free (font_entries_16x32[i]);
		font_entries_16x32[i] = 0;
		font_entries_16x32_fill--;
		assert (font_entries_16x32_fill == i);
	}
}

int fontengine_8x16_forceunifont (uint32_t codepoint, int *width, uint8_t data[32])
{
	TTF_Surface *text_surface = 0;

	if (codepoint == 0)
	{
		codepoint = ' ';
	}

	       if (                            (codepoint <= 0x0d8ff)  ||
	            ((codepoint >= 0x0f900) && (codepoint <= 0x0ffff)) )
	{
		text_surface = unifont_bmp ? TTF_RenderGlyph32_Shaded (unifont_bmp, codepoint) : 0;
#if defined(UNIFONT_CSUR_TTF) || defined(UNIFONT_CSUR_OTF)
	} else if ( ((codepoint >= 0x0e000) && (codepoint <= 0x0f8ff)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
#endif
	} else if ( ((codepoint >= 0x10000) && (codepoint <= 0x1ffff)) ||
	            ((codepoint >= 0xe0000) && (codepoint <= 0xeffff)) )
	{
		text_surface = unifont_upper ? TTF_RenderGlyph32_Shaded (unifont_upper, codepoint) : 0;
#if defined(UNIFONT_CSUR_TTF) || defined(UNIFONT_CSUR_OTF)
	} else if ( ((codepoint >= 0xf0000) && (codepoint >= 0xffffd)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
#endif
	}

	if (text_surface && ((text_surface->w == 8) || (text_surface->w == 16)) && (text_surface->h == 16))
	{
		int x, y, o=0, i=0;
		*width = text_surface->w;
		for (y=0; y < text_surface->h; y++)
		{
			for (x=0; x < text_surface->w; x+=8)
			{
				data[o] = 0;
				if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= 0x80;
				if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= 0x40;
				if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= 0x20;
				if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= 0x10;
				if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= 0x08;
				if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= 0x04;
				if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= 0x02;
				if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= 0x01;
				o++;
			}
			i -= text_surface->w;
			i += text_surface->pitch;
		}
		return 0;
	} else {
		*width = 8;
		memset (data, 0, 32);
		return 1;
	}
}

int fontengine_16x32_forceunifont (uint32_t codepoint, int *width, uint8_t data[128])
{ /* contains simple 2:1 scaling */
	TTF_Surface *text_surface = 0;

	if (codepoint == 0)
	{
		codepoint = ' ';
	}

	       if (                            (codepoint <= 0x0d8ff)  ||
	            ((codepoint >= 0x0f900) && (codepoint <= 0x0ffff)) )
	{
		text_surface = unifont_bmp ? TTF_RenderGlyph32_Shaded (unifont_bmp, codepoint) : 0;
#if defined(UNIFONT_CSUR_TTF) || defined(UNIFONT_CSUR_OTF)
	} else if ( ((codepoint >= 0x0e000) && (codepoint <= 0x0f8ff)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
#endif
	} else if ( ((codepoint >= 0x10000) && (codepoint <= 0x1ffff)) ||
	            ((codepoint >= 0xe0000) && (codepoint <= 0xeffff)) )
	{
		text_surface = unifont_upper ? TTF_RenderGlyph32_Shaded (unifont_upper, codepoint) : 0;
#if defined(UNIFONT_CSUR_TTF) || defined(UNIFONT_CSUR_OTF)
	} else if ( ((codepoint >= 0xf0000) && (codepoint >= 0xffffd)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
#endif
	}

	if (text_surface && ((text_surface->w == 8) || (text_surface->w == 16)) && (text_surface->h == 16))
	{
		int x, y, o=0, i=0;
		if (text_surface->w == 8)
		{
			*width = 16;
			for (y=0; y < text_surface->h; y++)
			{
				for (x=0; x < text_surface->w; x+=8)
				{
					data[o] = 0;
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x80 | 0x40);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x20 | 0x10);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x08 | 0x04);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x02 | 0x01);
					o++;

					data[o] = 0;
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x80 | 0x40);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x20 | 0x10);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x08 | 0x04);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x02 | 0x01);
					o++;

					data[o] = data[o-2];
					o++;

					data[o] = data[o-2];
					o++;
				}
				i -= text_surface->w;
				i += text_surface->pitch;
			}
		} else {
			*width = 32;
			for (y=0; y < text_surface->h; y++)
			{
				for (x=0; x < text_surface->w; x+=16)
				{
					data[o] = 0;
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x80 | 0x40);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x20 | 0x10);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x08 | 0x04);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x02 | 0x01);
					o++;

					data[o] = 0;
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x80 | 0x40);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x20 | 0x10);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x08 | 0x04);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x02 | 0x01);
					o++;

					data[o] = 0;
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x80 | 0x40);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x20 | 0x10);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x08 | 0x04);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x02 | 0x01);
					o++;

					data[o] = 0;
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x80 | 0x40);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x20 | 0x10);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x08 | 0x04);
					if (((uint8_t*)text_surface->pixels)[i++]) data[o] |= (0x02 | 0x01);
					o++;

					data[o] = data[o-4];
					o++;

					data[o] = data[o-4];
					o++;

					data[o] = data[o-4];
					o++;

					data[o] = data[o-4];
					o++;
				}
				i -= text_surface->w;
				i += text_surface->pitch;
			}
		}
		return 0;
	} else {
		*width = 16;
		memset (data, 0, 128);
		return 1;
	}
}

/* width will be set to 8 or 16, depending on the glyph */
uint8_t *fontengine_8x16(uint32_t codepoint, int *width)
{
	int i;
//	TTF_Surface *text_surface = 0;
	struct font_entry_8x16_t *entry = 0;

	if (codepoint == 0)
	{
		codepoint = ' ';
	}

	for (i=0; i < font_entries_8x16_fill; i++)
	{
		if (font_entries_8x16[i]->codepoint == codepoint)
		{
			i = fontengine_8x16_scoreup (i);
			i = fontengine_8x16_scoreup (i);
			*width = font_entries_8x16[i]->width;
			return font_entries_8x16[i]->data;
		}
	}

	entry = malloc (sizeof (*entry));
	fontengine_8x16_forceunifont (codepoint, width, entry->data);

	entry->width = *width;
	entry->codepoint = codepoint;
	entry->score=0;
	fontengine_8x16_append(entry);

	return entry->data;
}

/* width will be set to 16 or 32, depending on the glyph */
uint8_t *fontengine_16x32(uint32_t codepoint, int *width)
{
	int i;
//	TTF_Surface *text_surface = 0;
	struct font_entry_16x32_t *entry = 0;

	if (codepoint == 0)
	{
		codepoint = ' ';
	}

	for (i=0; i < font_entries_16x32_fill; i++)
	{
		if (font_entries_16x32[i]->codepoint == codepoint)
		{
			i = fontengine_16x32_scoreup (i);
			i = fontengine_16x32_scoreup (i);
			*width = font_entries_16x32[i]->width;
			return font_entries_16x32[i]->data;
		}
	}

	entry = malloc (sizeof (*entry));
	fontengine_16x32_forceunifont (codepoint, width, entry->data);

	entry->width = *width;
	entry->codepoint = codepoint;
	entry->score=0;
	fontengine_16x32_append(entry);

	return entry->data;
}

int fontengine_init (void)
{
	int i;
#ifdef UNIFONT_RELATIVE
# ifdef UNIFONT_TTF
#  undef UNIFONT_TTF
# endif
# ifdef UNIFONT_CSUR_TTF
#  undef UNIFONT_CSUR_TTF
# endif
# ifdef UNIFONT_UPPER_TTF
#  undef UNIFONT_UPPER_TTF
# endif
# ifdef UNIFONT_OTF
#  undef UNIFONT_OTF
# endif
# ifdef UNIFONT_CSUR_OTF
#  undef UNIFONT_CSUR_OTF
# endif
# ifdef UNIFONT_UPPER_OTF
#  undef UNIFONT_UPPER_OTF
# endif
	char *UNIFONT_TTF;
	char *UNIFONT_CSUR_TTF;
	char *UNIFONT_UPPER_TTF;
	char *UNIFONT_OTF;
	char *UNIFONT_CSUR_OTF;
	char *UNIFONT_UPPER_OTF;
#endif
	char error_ttf[256];
	char error_otf[256];

	if ( TTF_Init() < 0)
	{
		fprintf (stderr, "[TTF] Unable to init truetype-font library: %s\n", TTF_GetError());
		TTF_ClearError();
		/* this should never fail */
		return 1;
	}

#ifdef UNIFONT_RELATIVE
	UNIFONT_TTF       = malloc (strlen (configAPI.DataPath) + strlen ("unifont.ttf"      ) + 1);
	UNIFONT_CSUR_TTF  = malloc (strlen (configAPI.DataPath) + strlen ("unifont_csur.ttf" ) + 1);
	UNIFONT_UPPER_TTF = malloc (strlen (configAPI.DataPath) + strlen ("unifont_upper.ttf") + 1);
	UNIFONT_OTF       = malloc (strlen (configAPI.DataPath) + strlen ("unifont.otf"      ) + 1);
	UNIFONT_CSUR_OTF  = malloc (strlen (configAPI.DataPath) + strlen ("unifont_csur.otf" ) + 1);
	UNIFONT_UPPER_OTF = malloc (strlen (configAPI.DataPath) + strlen ("unifont_upper.otf") + 1);
	sprintf (UNIFONT_TTF,       "%s%s", configAPI.DataPath, "unifont.ttf"      );
	sprintf (UNIFONT_CSUR_TTF,  "%s%s", configAPI.DataPath, "unifont_csur.ttf" );
	sprintf (UNIFONT_UPPER_TTF, "%s%s", configAPI.DataPath, "unifont_upper.ttf");
	sprintf (UNIFONT_OTF,       "%s%s", configAPI.DataPath, "unifont.otf"      );
	sprintf (UNIFONT_CSUR_OTF,  "%s%s", configAPI.DataPath, "unifont_csur.otf" );
	sprintf (UNIFONT_UPPER_OTF, "%s%s", configAPI.DataPath, "unifont_upper.otf");
#endif

	unifont_bmp = TTF_OpenFontFilename(UNIFONT_OTF, 16, 0, 0, 0);
	if (!unifont_bmp)
	{
		snprintf (error_otf, sizeof (error_otf), "TTF_OpenFont(\"%s\") failed: %s\n", UNIFONT_OTF, TTF_GetError());
		TTF_ClearError();

		unifont_bmp = TTF_OpenFontFilename(UNIFONT_TTF, 16, 0, 0, 0);
		if (!unifont_bmp)
		{
			snprintf (error_ttf, sizeof (error_ttf), "TTF_OpenFont(\"%s\") failed: %s\n", UNIFONT_TTF, TTF_GetError());
			TTF_ClearError();

			fputs (error_otf, stderr);
			fputs (error_ttf, stderr);
		}
	}

#if defined(UNIFONT_CSUR_OTF) || defined (UNIFONT_RELATIVE)
	unifont_csur = TTF_OpenFontFilename(UNIFONT_CSUR_OTF, 16, 0, 0, 0);
	if (!unifont_csur)
	{
		snprintf (error_otf, sizeof (error_otf), "TTF_OpenFont(\"%s\") failed: %s\n", UNIFONT_CSUR_OTF, TTF_GetError());
		TTF_ClearError();
	}
#endif
#if defined(UNIFONT_CSUR_TTF) || defined (UNIFONT_RELATIVE)
	if (!unifont_csur)
	{
		unifont_csur = TTF_OpenFontFilename(UNIFONT_CSUR_TTF, 16, 0, 0, 0);
		if (!unifont_csur)
		{
			snprintf (error_ttf, sizeof (error_ttf), "TTF_OpenFont(\"%s\") failed: %s\n", UNIFONT_CSUR_TTF, TTF_GetError());
			TTF_ClearError();
		}
	}
#endif

# if defined(UNIFONT_CSUR_TTF) || defined(UNIFONT_CSUR_OTF) || defined (UNIFONT_RELATIVE)
	if (!unifont_csur)
	{
# if defined(UNIFONT_CSUR_OTF) || defined (UNIFONT_RELATIVE)
		fputs (error_otf, stderr);
# endif
# if defined(UNIFONT_CSUR_TTF) || defined (UNIFONT_RELATIVE)
		fputs (error_ttf, stderr);
# endif
	}
#endif

	unifont_upper = TTF_OpenFontFilename(UNIFONT_UPPER_OTF , 16, 0, 0, 0);
	if (!unifont_upper)
	{
		snprintf (error_otf, sizeof (error_ttf), "TTF_OpenFont(\"%s\") failed: %s\n", UNIFONT_UPPER_OTF, TTF_GetError());
		TTF_ClearError();

		unifont_upper = TTF_OpenFontFilename(UNIFONT_UPPER_TTF , 16, 0, 0, 0);
		if (!unifont_upper)
		{
			snprintf (error_ttf, sizeof (error_otf), "TTF_OpenFont(\"%s\") failed: %s\n", UNIFONT_UPPER_TTF, TTF_GetError());
			TTF_ClearError();

			fputs (error_otf, stderr);
			fputs (error_ttf, stderr);
		}
	}

	for (i=0; i < 256; i++)
	{
		cp437_8x8[i].codepoint = ocp_cp437_to_unicode[i];
		cp437_8x8[i].width = 8;
		memcpy (cp437_8x8[i].data, plFont88[i], 16);
		fontengine_8x8_append (cp437_8x8 + i);
		cp437_8x8[i].score = 255;
	}
	for (i=0; i < (sizeof(latin1_8x8)/sizeof(latin1_8x8[0])); i++)
	{
		int j;
		latin1_8x8[i].codepoint = plFont_8x8_latin1_addons[i].codepoint;
		latin1_8x8[i].width = 8;
		memcpy (latin1_8x8[i].data, plFont_8x8_latin1_addons[i].data, sizeof (plFont_8x8_latin1_addons[i].data));
		for (j=0; j < font_entries_8x8_fill; j++)
		{
			if (font_entries_8x8[j]->codepoint == latin1_8x8[i].codepoint)
			{
				fprintf (stderr, "[FontEngine] Codepoint from latin1 already added via cp437: codepoint=U+0%04X\n", latin1_8x8[i].codepoint);
				goto do_not_add;
			}
		}
		fontengine_8x8_append (latin1_8x8 + i);
do_not_add:
		latin1_8x8[i].score = 255;
	}

	for (i=0; i < 256; i++)
	{
		cp437_8x16[i].codepoint = ocp_cp437_to_unicode[i];
		cp437_8x16[i].width = 8;
		memcpy (cp437_8x16[i].data, plFont816[i], 16);
		fontengine_8x16_append (cp437_8x16 + i);
		cp437_8x16[i].score = 255;
	}
	for (i=0; i < (sizeof(latin1_8x16)/sizeof(latin1_8x16[0])); i++)
	{
		int j;
		latin1_8x16[i].codepoint = plFont_8x16_latin1_addons[i].codepoint;
		latin1_8x16[i].width = 8;
		memcpy (latin1_8x16[i].data, plFont_8x16_latin1_addons[i].data, sizeof (plFont_8x16_latin1_addons[i].data));
		for (j=0; j < font_entries_8x16_fill; j++)
		{
			if (font_entries_8x16[j]->codepoint == latin1_8x16[i].codepoint)
			{
				fprintf (stderr, "[FontEngine] Codepoint from latin1 already added via cp437: codepoint=U+0%04X\n", latin1_8x16[i].codepoint);
				goto do_not_add2;
			}
		}
		fontengine_8x16_append (latin1_8x16 + i);
do_not_add2:
		latin1_8x16[i].score = 255;
	}

	for (i=0; i < 256; i++)
	{
		cp437_16x32[i].codepoint = ocp_cp437_to_unicode[i];
		cp437_16x32[i].width = 16;
		memcpy (cp437_16x32[i].data, plFont1632[i], 128);
		fontengine_16x32_append (cp437_16x32 + i);
		cp437_16x32[i].score = 255;
	}
	for (i=0; i < (sizeof(latin1_16x32)/sizeof(latin1_16x32[0])); i++)
	{
		int j;
		latin1_16x32[i].codepoint = plFont_16x32_latin1_addons[i].codepoint;
		latin1_16x32[i].width = 16;
		memcpy (latin1_16x32[i].data, plFont_16x32_latin1_addons[i].data, sizeof (plFont_16x32_latin1_addons[i].data));
		for (j=0; j < font_entries_16x32_fill; j++)
		{
			if (font_entries_16x32[j]->codepoint == latin1_16x32[i].codepoint)
			{
				fprintf (stderr, "[FontEngine] Codepoint from latin1 already added via cp437: codepoint=U+0%04X\n", latin1_16x32[i].codepoint);
				goto do_not_add3;
			}
		}
		fontengine_16x32_append (latin1_16x32 + i);
do_not_add3:
		latin1_16x32[i].score = 255;
	}


#ifdef UNIFONT_RELATIVE
	free (UNIFONT_TTF);
	free (UNIFONT_CSUR_TTF);
	free (UNIFONT_UPPER_TTF);
	free (UNIFONT_OTF);
	free (UNIFONT_CSUR_OTF);
	free (UNIFONT_UPPER_OTF);
#endif

	return 0;
}

void fontengine_done (void)
{
	int i;

	for (i=0; i < font_entries_8x8_fill; i++)
	{
		if (font_entries_8x8[i]->score != 255) // do not try to free static entries
		{
			free (font_entries_8x8[i]);
		}
	}
	free (font_entries_8x8);
	font_entries_8x8 = 0;
	font_entries_8x8_fill = 0;
	font_entries_8x8_allocated = 0;

	for (i=0; i < font_entries_8x16_fill; i++)
	{
		if (font_entries_8x16[i]->score != 255) // do not try to free static entries
		{
			free (font_entries_8x16[i]);
		}
	}
	free (font_entries_8x16);
	font_entries_8x16 = 0;
	font_entries_8x16_fill = 0;
	font_entries_8x16_allocated = 0;

	for (i=0; i < font_entries_16x32_fill; i++)
	{
		if (font_entries_16x32[i]->score != 255) // do not try to free static entries
		{
			free (font_entries_16x32[i]);
		}
	}
	free (font_entries_16x32);
	font_entries_16x32 = 0;
	font_entries_16x32_fill = 0;
	font_entries_16x32_allocated = 0;

	if (unifont_bmp)
	{
		TTF_CloseFont(unifont_bmp);
		unifont_bmp = 0;
	}
#if defined(UNIFONT_CSUR_TTF) || defined(UNIFONT_CSUR_OTF)
	if (unifont_csur)
	{
		TTF_CloseFont(unifont_csur);
		unifont_csur = 0;
	}
#endif
	if (unifont_upper)
	{
		TTF_CloseFont(unifont_upper);
		unifont_upper = 0;
	}
	TTF_Quit();
}
