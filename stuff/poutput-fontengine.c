/* OpenCP Module Player
 * copyright (c) 2020-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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

static TTF_Font *unifont_bmp, *unifont_csur, *unifont_upper;

struct font_entry_8x8_t  cp437_8x8 [256];
struct font_entry_8x16_t cp437_8x16[256];

static struct font_entry_8x8_t  latin1_8x8 [sizeof(plFont_8x8_latin1_addons)  / sizeof (plFont_8x8_latin1_addons[0] )];
static struct font_entry_8x16_t latin1_8x16[sizeof(plFont_8x16_latin1_addons) / sizeof (plFont_8x16_latin1_addons[0])];

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

/* width will be set to 8 or 16, depending on the glyph */
uint8_t *fontengine_8x8(uint32_t codepoint, int *width)
{
	int i;
	TTF_Surface *text_surface = 0;
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

#warning TODO, we need to extract and scale these
#if 0
	       if (                            (codepoint <= 0x0d8ff)  ||
	            ((codepoint >= 0x0f900) && (codepoint <= 0x0ffff)) )
	{
		text_surface = unifont_bmp ? TTF_RenderGlyph32_Shaded (unifont_bmp, codepoint) : 0;
	} else if ( ((codepoint >= 0x0e000) && (codepoint <= 0x0f8ff)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
	} else if ( ((codepoint >= 0x10000) && (codepoint <= 0x1ffff)) ||
	            ((codepoint >= 0xe0000) && (codepoint <= 0xeffff)) )
	{
		text_surface = unifont_upper ? TTF_RenderGlyph32_Shaded (unifont_upper, codepoint) : 0;
	} else if ( ((codepoint >= 0xf0000) && (codepoint >= 0xffffd)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
	}
#else
	fprintf (stderr, "TODO scale glyph U+%X\n", codepoint);
#endif

	entry = malloc (sizeof (*entry));
	if (text_surface)
	{
		if ((text_surface->w != 8) && (text_surface->w != 16))
		{
			memset (entry->data, 0xaa, sizeof (entry->data));
			entry->width = 8;
			fprintf (stderr, "TTF + unifont + U+%X: gave invalid width: %d\n", (int)codepoint, (int)text_surface->w);
		} if (text_surface->h != 16)
		{
			memset (entry->data, 0x42, sizeof (entry->data));
			entry->width = 8;
			fprintf (stderr, "TTF + unifont + U+%X: gave invalid height: %d\n", (int)codepoint, (int)text_surface->h);
		} else {
			int x, y, o=0, i=0;
			entry->width = text_surface->w;
			for (y=0; y < text_surface->h; y++)
			{
				for (x=0; x < text_surface->w; x+=8)
				{
					entry->data[o] = 0;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x80;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x40;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x20;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x10;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x08;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x04;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x02;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x01;
					o++;
				}
				i -= text_surface->w;
				i += text_surface->pitch;
			}
		}
	} else {
		memset (entry->data, 0x18, sizeof (entry->data));
		entry->width = 8;
		fprintf (stderr, "TTF + unifont + U+%X: did not find a glyph\n", (int)codepoint);
	}
	entry->codepoint = codepoint;
	entry->score=0;
	fontengine_8x8_append(entry);

	*width = entry->width;
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

/* width will be set to 8 or 16, depending on the glyph */
uint8_t *fontengine_8x16(uint32_t codepoint, int *width)
{
	int i;
	TTF_Surface *text_surface = 0;
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

	       if (                            (codepoint <= 0x0d8ff)  ||
	            ((codepoint >= 0x0f900) && (codepoint <= 0x0ffff)) )
	{
		text_surface = unifont_bmp ? TTF_RenderGlyph32_Shaded (unifont_bmp, codepoint) : 0;
	} else if ( ((codepoint >= 0x0e000) && (codepoint <= 0x0f8ff)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
	} else if ( ((codepoint >= 0x10000) && (codepoint <= 0x1ffff)) ||
	            ((codepoint >= 0xe0000) && (codepoint <= 0xeffff)) )
	{
		text_surface = unifont_upper ? TTF_RenderGlyph32_Shaded (unifont_upper, codepoint) : 0;
	} else if ( ((codepoint >= 0xf0000) && (codepoint >= 0xffffd)) )
	{
		text_surface = unifont_csur ? TTF_RenderGlyph32_Shaded (unifont_csur, codepoint) : 0;
	}

	entry = malloc (sizeof (*entry));
	if (text_surface)
	{
		if ((text_surface->w != 8) && (text_surface->w != 16))
		{
			memset (entry->data, 0xaa, sizeof (entry->data));
			entry->width = 8;
			fprintf (stderr, "TTF + unifont + U+%X: gave invalid width: %d\n", (int)codepoint, (int)text_surface->w);
		} if (text_surface->h != 16)
		{
			memset (entry->data, 0x42, sizeof (entry->data));
			entry->width = 8;
			fprintf (stderr, "TTF + unifont + U+%X: gave invalid height: %d\n", (int)codepoint, (int)text_surface->h);
		} else {
			int x, y, o=0, i=0;
			entry->width = text_surface->w;
			for (y=0; y < text_surface->h; y++)
			{
				for (x=0; x < text_surface->w; x+=8)
				{
					entry->data[o] = 0;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x80;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x40;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x20;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x10;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x08;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x04;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x02;
					if (((uint8_t*)text_surface->pixels)[i++]) entry->data[o] |= 0x01;
					o++;
				}
				i -= text_surface->w;
				i += text_surface->pitch;
			}
		}
	} else {
		memset (entry->data, 0x18, sizeof (entry->data));
		entry->width = 8;
		fprintf (stderr, "TTF + unifont + U+%X: did not find a glyph\n", (int)codepoint);
	}
	entry->codepoint = codepoint;
	entry->score=0;
	fontengine_8x16_append(entry);

	*width = entry->width;
	return entry->data;
}

int fontengine_init (void)
{
	int i;
	if ( TTF_Init() < 0)
	{
		fprintf (stderr, "[TTF] Unable to init truetype-font library: %s\n", TTF_GetError());
		TTF_ClearError();
		/* this should never fail */
		return 1;
	}

	unifont_bmp = TTF_OpenFontFilename(UNIFONTDIR "/unifont.ttf", 16, 0, 0, 0);
	if (!unifont_bmp)
	{
		fprintf (stderr, "TTF_OpenFont(\"" UNIFONTDIR "/unifont.ttf\") failed: %s\n", TTF_GetError());
		TTF_ClearError();
	}
	unifont_csur = TTF_OpenFontFilename(UNIFONTDIR "/unifont_csur.ttf", 16, 0, 0, 0);
	if (!unifont_csur)
	{
		fprintf (stderr, "TTF_OpenFont(\"" UNIFONTDIR "/unifont_csur.ttf\") failed: %s\n", TTF_GetError());
		TTF_ClearError();
	}
	unifont_upper = TTF_OpenFontFilename(UNIFONTDIR "/unifont_upper.ttf", 16, 0, 0, 0);
	if (!unifont_upper)
	{
		fprintf (stderr, "TTF_OpenFont(\"" UNIFONTDIR "/unifont_upper.ttf\") failed: %s\n", TTF_GetError());
		TTF_ClearError();
	}

	for (i=0; i < 256; i++)
	{
		cp437_8x8[i].codepoint = ocp_cp437_to_unicode[i];
		cp437_8x8[i].width=8;
		memcpy (cp437_8x8[i].data, plFont88[i], 16);
		fontengine_8x8_append (cp437_8x8 + i);
		cp437_8x8[i].score = 255;
	}
	for (i=0; i < (sizeof(latin1_8x8)/sizeof(latin1_8x8[0])); i++)
	{
		int j;
		latin1_8x8[i].codepoint = plFont_8x8_latin1_addons[i].codepoint;
		latin1_8x8[i].width=8;
		memcpy (latin1_8x8[i].data, plFont_8x8_latin1_addons[i].data, 16);
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
		cp437_8x16[i].width=8;
		memcpy (cp437_8x16[i].data, plFont816[i], 16);
		fontengine_8x16_append (cp437_8x16 + i);
		cp437_8x16[i].score = 255;
	}
	for (i=0; i < (sizeof(latin1_8x16)/sizeof(latin1_8x16[0])); i++)
	{
		int j;
		latin1_8x16[i].codepoint = plFont_8x16_latin1_addons[i].codepoint;
		latin1_8x16[i].width=8;
		memcpy (latin1_8x16[i].data, plFont_8x16_latin1_addons[i].data, 16);
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

	if (unifont_bmp)
	{
		TTF_CloseFont(unifont_bmp);
		unifont_bmp = 0;
	}
	if (unifont_csur)
	{
		TTF_CloseFont(unifont_csur);
		unifont_csur = 0;
	}
	if (unifont_upper)
	{
		TTF_CloseFont(unifont_upper);
		unifont_upper = 0;
	}
	TTF_Quit();
}
