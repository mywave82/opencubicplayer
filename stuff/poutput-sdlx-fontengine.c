#include "ttf.h"
#include "utf-8.h"

/* TODO: unicode points to skip: in UTF-8
 *
 * UNICODE_BOM_NATIVE  0xFEFF
 * UNICODE_BOM_SWAPPED 0xFFFE
 */

struct font_entry_t
{
	uint32_t codepoint;
	//char code[6+1];
	unsigned char width; /* 8 or 16 */
	/* for 16 lines font, we have have 1 bit per pixel */
	unsigned char data[16*2]; /* we fit upto 16 by 16 pixels */
	uint8_t score;
};

static struct font_entry_t **font_entries;
static int font_entries_fill;
static int font_entries_allocated;

static TTF_Font *unifont_bmp, *unifont_csur, *unifont_upper;

static struct font_entry_t cp437_8x16[256];

static struct font_entry_t latin1_8x16[sizeof(plFont_8x16_latin1_addons) / sizeof (plFont_8x16_latin1_addons[0])];

/*
 BMP   = Basic Multilingual Plane            https://en.wikipedia.org/wiki/Plane_(Unicode)#Basic_Multilingual_Plane
 SMP   = Supplementary Multilingual Plane    https://en.wikipedia.org/wiki/Plane_(Unicode)#Supplementary_Multilingual_Plane
 SIP   = Supplementary Ideographic Plane     https://en.wikipedia.org/wiki/Plane_(Unicode)#Supplementary_Ideographic_Plane
 TIP   = Tertiary Ideographic Plane          https://en.wikipedia.org/wiki/Plane_(Unicode)#Tertiary_Ideographic_Plane
 SSP   = Supplementary Special-purpose Plane https://en.wikipedia.org/wiki/Plane_(Unicode)#Supplementary_Special-purpose_Plane

 CSUR  = ConScript Unicode Registry          https://en.wikipedia.org/wiki/ConScript_Unicode_Registry
 UCSUR = Under-ConScript Unicode Registry    https://en.wikipedia.org/wiki/ConScript_Unicode_Registry

U+000000 - U+00D7FF  BMP
U+00E000 - U+00F8FF  CSUR  (CSUR/UCSUR)
U+00F900 - U+00FFFF  BMP
U+010000 - U+01FFFF  UPPER (SMP)
U+020000 - U+02FFFF  ----- (SIP)
U+030000 - U+03FFFF  ----- (TIP)
..
U+0E0000 - U+0EFFFF  UPPER (SPP)
U+0F0000 - U+0FFFFD  CSUR  (CSUR/UCSUR)
*/

/* returns the new index */
static int fontengine_scoreup (int index)
{
	if (font_entries[index]->score >= 254)
	{
		return index;
	}
	font_entries[index]->score++;
	while (1)
	{
		struct font_entry_t *temp;
		if (!index)
		{
			return index;
		}
		if (font_entries[index-1]->score >= font_entries[index]->score)
		{
			return index;
		}
		temp = font_entries[index-1];
		font_entries[index-1] = font_entries[index];
		font_entries[index] = temp;
		index--;
	}
	return index;
}

static void fontengine_append (struct font_entry_t *entry)
{
	if (font_entries_fill >= font_entries_allocated)
	{
		int newallocated = font_entries_allocated += 64;
		struct font_entry_t **newentries = realloc (font_entries, font_entries_allocated * sizeof (font_entries[0]));

		if (!newentries)
		{
			fprintf (stderr, "fontengine_append: malloc() failure....\n");
			return;
		}
		font_entries = newentries;
		font_entries_allocated = newallocated;
	}
	font_entries[font_entries_fill] = entry;
	font_entries_fill++;
	/* initial score is 5 */
	fontengine_scoreup (font_entries_fill-1);
	fontengine_scoreup (font_entries_fill-1);
	fontengine_scoreup (font_entries_fill-1);
	fontengine_scoreup (font_entries_fill-1);
	fontengine_scoreup (font_entries_fill-1);
}

static void fontengine_iterate (void)
{
	int i;

	for (i=font_entries_fill-1; i >= 0; i--)
	{
		if (font_entries[i]->score == 255)
		{
			continue;
		}
		font_entries[i]->score--;
		if (font_entries[i]->score)
		{
			continue;
		}
		/* if a score reaches zero, we should be at the end of the list..... since we sort the list */
		free (font_entries[i]);
		font_entries[i] = 0;
		font_entries_fill--;
		assert (font_entries_fill == i);
	}
}

/* width will be set to 8 or 16, depending on the glyph */
uint8_t *fontengine_8x16(uint32_t codepoint, int *width)
{
	int i;
	TTF_Surface *text_surface = 0;
	struct font_entry_t *entry = 0;

	if (codepoint == 0)
	{
		codepoint = ' ';
	}

	for (i=0; i < font_entries_fill; i++)
	{
		if (font_entries[i]->codepoint == codepoint)
		{
			i = fontengine_scoreup (i);
			*width = font_entries[i]->width;
			return font_entries[i]->data;
		}
	}

	       if (                            (codepoint <= 0x0d8ff)  ||
	            ((codepoint >= 0x0f900) && (codepoint <= 0x0ffff)) )
	{
		text_surface = TTF_RenderGlyph32_Shaded (unifont_bmp, codepoint);
	} else if ( ((codepoint >= 0x0e000) && (codepoint <= 0x0f8ff)) )
	{
		text_surface = TTF_RenderGlyph32_Shaded (unifont_csur, codepoint);
	} else if ( ((codepoint >= 0x10000) && (codepoint <= 0x1ffff)) ||
	            ((codepoint >= 0xe0000) && (codepoint <= 0xeffff)) )
	{
		text_surface = TTF_RenderGlyph32_Shaded (unifont_upper, codepoint);
	} else if ( ((codepoint >= 0xf0000) && (codepoint >= 0xffffd)) )
	{
		text_surface = TTF_RenderGlyph32_Shaded (unifont_csur, codepoint);
	}

	if (text_surface)
	{
		if ((text_surface->w != 8) && (text_surface->w != 16))
		{
			fprintf (stderr, "TTF + unifont + U+%X: gave invalid width: %d\n", (int)codepoint, (int)text_surface->w);
		} if (text_surface->h != 16)
		{
			fprintf (stderr, "TTF + unifont + U+%X: gave invalid height: %d\n", (int)codepoint, (int)text_surface->h);
		} else {
			int x, y, o=0, i=0;
			entry = malloc (sizeof (*entry));
			entry->codepoint = codepoint;
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
			entry->score=0;
			fontengine_append(entry);
		}
	}

	if (entry)
	{
		*width = entry->width;
		return entry->data;
	}

#warning FALLBACK to original font

	*width = 0;
	return 0;
}

static int fontengine_init (void)
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
		cp437_8x16[i].codepoint = ocp_cp437_to_unicode[i];
		cp437_8x16[i].width=8;
		memcpy (cp437_8x16[i].data, plFont816[i], 16);
		fontengine_append (cp437_8x16 + i);
		cp437_8x16[i].score = 255;
	}
	for (i=0; i < (sizeof(latin1_8x16)/sizeof(latin1_8x16[0])); i++)
	{
		int j;
		latin1_8x16[i].codepoint = plFont_8x16_latin1_addons[i].codepoint;
		latin1_8x16[i].width=8;
		memcpy (latin1_8x16[i].data, plFont_8x16_latin1_addons[i].data, 16);
		for (j=0; j < font_entries_fill; j++)
		{
			if (font_entries[j]->codepoint == latin1_8x16[i].codepoint)
			{
				fprintf (stderr, "[FontEngine] Codepoint from latin1 already added via cp437: codepoint=U+0%04X\n", latin1_8x16[i].codepoint);
				goto do_not_add;
			}
		}
		fontengine_append (latin1_8x16 + i);
do_not_add:
		latin1_8x16[i].score = 255;
	}
	return 0;
}

static void fontengine_done (void)
{
#warning free memory here..... except the score 255 ones... + index
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
