#include <SDL_ttf.h>
#include "utf-8.h"

struct font_entry_t
{
	uint32_t codepoint;
	char code[6+1];
	unsigned char width; /* 8 or 16 */
	/* for 16 lines font, we have have 1 bit per pixel */
	unsigned char data[16*2]; /* we fit upto 16 by 16 pixels */
	uint8_t score;
};

static struct font_entry_t **font_entries;
static int font_entries_fill;
static int font_entries_allocated;

static TTF_Font *unifont_bmp, *unifont_csur, *unifont_upper;

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
	if (font_entries[index]->score == 255)
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
	const SDL_Color color1={  0,  0,  0};
	const SDL_Color color2={255,255,255};
	SDL_Surface *text_surface;
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
		text_surface = TTF_RenderGlyph_Shaded (unifont_bmp, codepoint, color1, color2);
	} else if ( ((codepoint >= 0x0e000) && (codepoint <= 0x0f8ff)) )
	{
		text_surface = TTF_RenderGlyph_Shaded (unifont_csur, codepoint, color1, color2);
	} else if ( ((codepoint >= 0x10000) && (codepoint <= 0x1ffff)) ||
	            ((codepoint >= 0xe0000) && (codepoint <= 0xeffff)) )
	{
		char data[6+1];
		utf8_encode (data, codepoint);
		/* TTF_RenderGlyph_Shaded only supports 16bit codepoints */
		text_surface = TTF_RenderUTF8_Shaded (unifont_upper, data, color1, color2);
	} else if ( ((codepoint >= 0xf0000) && (codepoint >= 0xffffd)) )
	{
		char data[6+1];
		utf8_encode (data, codepoint);
		/* TTF_RenderGlyph_Shaded only supports 16bit codepoints */
		text_surface = TTF_RenderUTF8_Shaded (unifont_csur, data, color1, color2);
	}

	if (text_surface)
	{
		if ((text_surface->w != 8) && (text_surface->w != 16))
		{
			fprintf (stderr, "SDL_TTF + unifont + U+%X: gave invalid width: %d\n", (int)codepoint, (int)text_surface->w);
		} if (text_surface->h != 16)
		{
			fprintf (stderr, "SDL_TTF + unifont + U+%X: gave invalid height: %d\n", (int)codepoint, (int)text_surface->h);
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
			}
			entry->score=0;
			fontengine_append(entry);
		}
		SDL_FreeSurface(text_surface);
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
	if ( TTF_Init() < 0)
	{
		fprintf (stderr, "[SDL2 ttf] Unable to init truetype-font library: %s\n", TTF_GetError());
		SDL_ClearError();
		/* this should never fail */
		return 1;
	}

	unifont_bmp = TTF_OpenFont(UNIFONTDIR "/unifont.ttf", 16);
	if (!unifont_bmp)
	{
		fprintf (stderr, "TTF_OpenFont(\""UNIFONTDIR "\"/unifont.ttf\") failed: %s\n", TTF_GetError());
		SDL_ClearError();
	}
	unifont_csur = TTF_OpenFont(UNIFONTDIR "/unifont_csur.ttf", 16);
	if (!unifont_csur)
	{
		fprintf (stderr, "TTF_OpenFont(\""UNIFONTDIR "\"/unifont_csur.ttf\") failed: %s\n", TTF_GetError());
		SDL_ClearError();
	}
	unifont_upper = TTF_OpenFont(UNIFONTDIR "/unifont_upper.ttf", 16);
	if (!unifont_upper)
	{
		fprintf (stderr, "TTF_OpenFont(\""UNIFONTDIR "\"/unifont_upper.ttf\") failed: %s\n", TTF_GetError());
		SDL_ClearError();
	}

	return 0;
}

static void fontengine_done (void)
{
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
