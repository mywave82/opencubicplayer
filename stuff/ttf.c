/* OpenCP Module Player
 * copyright (c) 2001-'20 Sam Lantinga <slouken@libsdl.org>
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * TrueType (tm) support - using FreeType library.
 *
 * This code is based on code from the SDL_ttf library and is released
 * as-is. Not much is left of the original code, and it can probably be
 * cleaned up further if needed. The current goal is only to load the
 * GNU Unifont ttf files.
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

#define USE_FREETYPE_ERRORS 1

#include "config.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_STROKER_H
#include FT_GLYPH_H
#include FT_TRUETYPE_IDS_H

#include "types.h"

#include "ttf.h"
#include "utf-8.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))


/* Round glyph width to 8 bytes */
#define HAVE_BLIT_GLYPH_64

/* Android armeabi-v7a doesn't like int64 (Maybe all other __ARM_ARCH < 7 ?),
 * un-activate it, especially if NEON isn't detected */
#if defined(__ARM_ARCH)
#  if __ARM_ARCH < 8
#    if defined(HAVE_BLIT_GLYPH_64)
#      undef HAVE_BLIT_GLYPH_64
#    endif
#  endif
#endif

/* Default: round glyph width to 4 bytes to copy them faster */
#define HAVE_BLIT_GLYPH_32

/* FIXME: Right now we assume the gray-scale renderer Freetype is using
   supports 256 shades of gray, but we should instead key off of num_grays
   in the result FT_Bitmap after the FT_Render_Glyph() call. */
#define NUM_GRAYS       256

/* Handy routines for converting from fixed point 26.6 */
#define FT_FLOOR(X) (((X) & -64) / 64)
#define FT_CEIL(X)  FT_FLOOR((X) + 63)

/* Handy routine for converting to fixed point 26.6 */
#define F26Dot6(X)  ((X) << 6)

typedef struct {
    unsigned char *buffer; /* aligned */
    int            left;
    int            top;
    int            width;
    int            rows;
    int            pitch;
} TTF_Image;

/* Cached glyph information */
typedef struct cached_glyph {
    TTF_Image pixmap;
    int sz_left;
    int sz_top;
    int sz_width;
    int sz_rows;
    int advance;
    int pos_buf_y;
} c_glyph;

/* The structure used to hold internal font information */
struct _TTF_Font {
    /* Freetype2 maintains all sorts of useful info itself */
    FT_Face face;

    /* We'll cache these ourselves */
    int height;
    int ascent;
    //int descent;

    /* Whether kerning is desired */
    int use_kerning;

    /* We are responsible for closing the font stream */
    FILE *src;
    FT_Open_Args args;

	TTF_Surface *last_surface;
	int last_surface_size;
};


//#ifdef _WIN32
//typedef TTF_Font::PosBuf PosBuf_t;
//#else
typedef void PosBuf_t;
//#endif

/* The FreeType font engine/library */
static FT_Library library;
static int TTF_initialized = 0;

#define TTF_CHECK_INITIALIZED(errval)                   \
    if (!TTF_initialized) {                             \
        TTF_SetError("Library not initialized");        \
        return errval;                                  \
    }

#define TTF_CHECK_POINTER(p, errval)                    \
    if (!(p)) {                                         \
        TTF_SetError("Passed a NULL pointer");          \
        return errval;                                  \
    }

/* Don't use Duff's device to unroll loops */
#define DUFFS_LOOP(pixel_copy_increment, width)                         \
{ int n;                                                                \
    for ( n=width; n > 0; --n ) {                                       \
        pixel_copy_increment;                                           \
    }                                                                   \
}

#define DUFFS_LOOP4(pixel_copy_increment, width)                        \
    DUFFS_LOOP(pixel_copy_increment, width)

static inline void BG(const TTF_Image *image, uint8_t *destination, int32_t srcskip, uint32_t dstskip)
{
    const uint8_t *src    = image->buffer;
    uint8_t       *dst    = destination;
    uint32_t       width  = image->width;
    uint32_t       height = image->rows;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            *dst++ |= *src++;
        , width);
        /* *INDENT-ON* */
        src += srcskip;
        dst += dstskip;
    }
}

#if defined(HAVE_BLIT_GLYPH_64)
static inline void BG_64(const TTF_Image *image, uint8_t *destination, int32_t srcskip, uint32_t dstskip)
{
    const uint64_t *src    = (uint64_t *)image->buffer;
    uint64_t       *dst    = (uint64_t *)destination;
    uint32_t        width  = image->width / 8;
    uint32_t        height = image->rows;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            *dst++ |= *src++;
        , width);
        /* *INDENT-ON* */
        src = (const uint64_t *)((const uint8_t *)src + srcskip);
        dst = (uint64_t *)((uint8_t *)dst + dstskip);
    }
}
#elif defined(HAVE_BLIT_GLYPH_32)
static inline void BG_32(const TTF_Image *image, uint8_t *destination, int32_t srcskip, uint32_t dstskip)
{
    const uint32_t *src    = (uint32_t *)image->buffer;
    uint32_t       *dst    = (uint32_t *)destination;
    uint32_t        width  = image->width / 4;
    uint32_t        height = image->rows;

    while (height--) {
        /* *INDENT-OFF* */
        DUFFS_LOOP4(
            *dst++ |= *src++;
        , width);
        /* *INDENT-ON* */
        src = (const uint32_t *)((const uint8_t *)src + srcskip);
        dst = (uint32_t *)((uint8_t *)dst + dstskip);
    }
}
#endif

static void clip_glyph(int *_x, int *_y, TTF_Image *image, const TTF_Surface *textbuf)
{
    int above_w;
    int above_h;
    int x = *_x;
    int y = *_y;

    /* Don't go below x=0 */
    if (x < 0) {
        int tmp = -x;
        x = 0;
        image->width  -= tmp;
        image->buffer += tmp;
    }
    /* Don't go above textbuf->w */
    above_w = x + image->width - textbuf->w;
    if (above_w > 0) {
        image->width -= above_w;
    }
    /* Don't go below y=0 */
    if (y < 0) {
        int tmp = -y;
        y = 0;
        image->rows   -= tmp;
        image->buffer += tmp * image->pitch;
    }
    /* Don't go above textbuf->h */
    above_h = y + image->rows - textbuf->h;
    if (above_h > 0) {
        image->rows -= above_h;
    }
    /* Could be negative if (x > textbuf->w), or if (x + width < 0) */
    image->width = MAX(0, image->width);
    image->rows  = MAX(0, image->rows);

    /* After 'image->width' clipping:
     * Make sure 'rows' is also 0, so it doesn't break USE_DUFFS_LOOP */
    if (image->width == 0) {
        image->rows = 0;
    }

    *_x = x;
    *_y = y;
}

/* Glyph width is rounded, dst addresses are aligned, src addresses are not aligned */
static int Get_Alignement()
{
#if defined(HAVE_BLIT_GLYPH_64)
	return 8;
#elif defined(HAVE_BLIT_GLYPH_32)
	return 4;
#else
	return 1;
#endif
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#endif
#define BUILD_RENDER_LINE(NAME, BLIT_GLYPH_OPTIM) \
                                                                                                                        \
static inline                                                                                                           \
void Render_Glyph_##NAME(TTF_Font *font, c_glyph *glyph, TTF_Surface *textbuf, int xstart, int ystart)                  \
{                                                                                                                       \
    const int alignment = Get_Alignement() - 1;                                                                         \
    {                                                                                                                   \
        int x       = 0;                                                                                                \
        int y       = glyph->pos_buf_y;                                                                                 \
                                                                                                                        \
            TTF_Image *image = &glyph->pixmap;                                                                          \
            int above_w, above_h;                                                                                       \
            uint32_t dstskip;                                                                                             \
            int32_t srcskip; /* Can be negative */                                                                       \
            uint8_t *dst;                                                                                                 \
            int rest;                                                                                                   \
            uint8_t *saved_buffer = image->buffer;                                                                        \
            int saved_width = image->width;                                                                             \
            image->buffer += alignment;                                                                                 \
            /* Position updated after glyph rendering */                                                                \
            x = xstart + FT_FLOOR(x) + image->left;                                                                     \
            y = ystart + FT_FLOOR(y) - image->top;                                                                      \
                                                                                                                        \
            /* Make sure glyph is inside textbuf */                                                                     \
            above_w = x + image->width - textbuf->w;                                                                    \
            above_h = y + image->rows  - textbuf->h;                                                                    \
                                                                                                                        \
            if (x >= 0 && y >= 0 && above_w <= 0 && above_h <= 0) {                                                     \
                /* Most often, glyph is inside textbuf */                                                               \
                /* Compute dst */                                                                                       \
                dst  = (uint8_t *)textbuf->pixels + y * textbuf->pitch + x;                                               \
                /* Align dst, get rest, shift & align glyph width */                                                    \
                rest = ((size_t)dst & alignment);                                                                       \
                dst  = (uint8_t *)((size_t)dst & ~alignment);                                                             \
                image->buffer -= rest;                                                                                  \
                image->width   = (image->width + rest + alignment) & ~alignment;                                        \
                /* Compute srcskip, dstskip */                                                                          \
                srcskip = image->pitch - image->width;                                                                  \
                dstskip = textbuf->pitch - image->width;                                                                \
                /* Render glyph at (x, y) with optimized copy functions */                                              \
                BLIT_GLYPH_OPTIM(image, dst, srcskip, dstskip);                                                         \
                /* restore modification */                                                                              \
                image->width = saved_width;                                                                             \
            } else {                                                                                                    \
                /* Modify a copy, and clip it */                                                                        \
                TTF_Image image_clipped = *image;                                                                       \
                /* Intersect image glyph at (x,y) with textbuf */                                                       \
                clip_glyph(&x, &y, &image_clipped, textbuf);                                                            \
                /* Compute dst */                                                                                       \
                dst = (uint8_t *)textbuf->pixels + y * textbuf->pitch + x;                                                \
                /* Compute srcskip, dstskip */                                                                          \
                srcskip = image_clipped.pitch - image_clipped.width;                                                    \
                dstskip = textbuf->pitch - image_clipped.width;                                                         \
                /* Render glyph at (x, y) */                                                                            \
                BG(&image_clipped, dst, srcskip, dstskip);                                                              \
            }                                                                                                           \
            image->buffer = saved_buffer;                                                                               \
    }                                                                                                                   \
}

/* BUILD_RENDER_LINE(NAME, BLIT_GLYPH_OPTIM) */
#if defined(HAVE_BLIT_GLYPH_64)
BUILD_RENDER_LINE(64_Shaded  , BG_64  )
#elif defined(HAVE_BLIT_GLYPH_32)
BUILD_RENDER_LINE(32_Shaded  , BG_32  )
#else
BUILD_RENDER_LINE(8_Shaded   , BG     )
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static inline void Render_Glyph(TTF_Font *font, c_glyph *glyph, TTF_Surface *textbuf, int xstart, int ystart)
{
    /* Render line (pos_buf) to textbuf at (xstart, ystart) */
#if defined(HAVE_BLIT_GLYPH_64)
	Render_Glyph_64_Shaded(font, glyph, textbuf, xstart, ystart);
#elif defined(HAVE_BLIT_GLYPH_32)
	Render_Glyph_32_Shaded(font, glyph, textbuf, xstart, ystart);
#else
	Render_Glyph_8_Shaded(font, glyph, textbuf, xstart, ystart);
#endif
}

static TTF_Surface* Create_Surface_Shaded(TTF_Font *font, int width, int height)
{
	const int alignment = Get_Alignement() - 1;
	int64_t size;
	int pitch = width + alignment;

	pitch += alignment;
	pitch &= ~alignment;

	size = height * pitch + sizeof (TTF_Surface);

	if (size > font->last_surface_size)
	{
		void *temp = realloc (font->last_surface, size);
		if (!temp)
		{
			return NULL;
		}
		font->last_surface = temp;
		font->last_surface_size = size;
	}

	font->last_surface->w = width;
	font->last_surface->h = height;
	font->last_surface->pitch = pitch;

	memset (font->last_surface->pixels, 0, height * pitch);

	return font->last_surface;
}

static char TTF_ErrorBuffer[128];
static void TTF_SetError (const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf (TTF_ErrorBuffer, sizeof (TTF_ErrorBuffer), fmt, ap);
	va_end(ap);
}

const char *TTF_GetError (void)
{
	return TTF_ErrorBuffer;
}

void TTF_ClearError (void)
{
	TTF_ErrorBuffer[0] = 0;
}


#if defined(USE_FREETYPE_ERRORS)
static void TTF_SetFTError(const char *msg, FT_Error error)
{
#undef FTERRORS_H_
#define FT_ERRORDEF(e, v, s)    { e, s },
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { 0, NULL } };
	const struct
	{
		int          err_code;
		const char  *err_msg;
	} ft_errors[] =
#include FT_ERRORS_H

	unsigned int i;
	const char *err_msg = NULL;

	for (i = 0; i < sizeof (ft_errors) / sizeof (ft_errors[0]); ++i)
	{
		if (error == ft_errors[i].err_code)
		{
			err_msg = ft_errors[i].err_msg;
			break;
		}
	}
	if (!err_msg)
	{
		err_msg = "unknown FreeType error";
	}
	TTF_SetError("%s: %s", msg, err_msg);
}
#else
#define TTF_SetFTError(msg, error)    TTF_SetError(msg)
#endif /* USE_FREETYPE_ERRORS */

int TTF_Init(void)
{
	int status = 0;

	if (!TTF_initialized)
	{
		FT_Error error = FT_Init_FreeType(&library);
		if (error)
		{
			TTF_SetFTError("Couldn't init FreeType engine", error);
			status = -1;
		}
	}
	if (status == 0)
	{
		TTF_initialized++;
	}
	return status;
}

static unsigned long RWread (FT_Stream stream,
                             unsigned long offset,
                             unsigned char *buffer,
                             unsigned long count)
{
	FILE *src = (FILE *)stream->descriptor.pointer;
	fseek(src, offset, SEEK_SET);
	if (count == 0)
	{
		return 0;
	}
	return fread(buffer, count, 1, src) ? count : 0;
}

TTF_Font* TTF_OpenFontFILE(FILE *src, int ptsize, long index, unsigned int hdpi, unsigned int vdpi)
{
	TTF_Font *font;
	FT_Error error;
	FT_Face face;
	FT_Stream stream;
	FT_CharMap found;
	int64_t position;
	int i;

	if (!TTF_initialized)
	{
		TTF_SetError("Library not initialized");
		return NULL;
	}

	if (!src)
	{
		TTF_SetError("Passed a NULL font source");
		return NULL;
	}

	/* Check to make sure we can seek in this stream */
	position = fseek (src, 0, SEEK_SET);
	if (position < 0)
	{
		TTF_SetError("Can't seek in stream");
		fclose (src);
		return NULL;
	}

	font = (TTF_Font *)calloc(sizeof (*font), 1);
	if (font == NULL)
	{
		TTF_SetError("Out of memory");
		fclose (src);
		return NULL;
	}
	font->src = src;

	stream = (FT_Stream)calloc(sizeof (*stream), 1);
	if (stream == NULL)
	{
		TTF_SetError("Out of memory");
		TTF_CloseFont(font);
		return NULL;
	}

	stream->read = RWread;
	stream->descriptor.pointer = src;
	stream->pos = (unsigned long)position;
	fseek (src, 0, SEEK_END);
	stream->size = (unsigned long)(ftell(src) - position);

	font->args.flags = FT_OPEN_STREAM;
	font->args.stream = stream;

	error = FT_Open_Face(library, &font->args, index, &font->face);
	if (error || font->face == NULL)
	{
		TTF_SetFTError("Couldn't load font file", error);
		TTF_CloseFont(font);
		return NULL;
	}
	face = font->face;

	/* Set charmap for loaded font */
	found = 0;
	for (i = 0; i < face->num_charmaps; i++)
	{
		FT_CharMap charmap = face->charmaps[i];
		if (charmap->platform_id == 3 && charmap->encoding_id == 10)
		{ /* UCS-4 Unicode */
			found = charmap;
			break;
		}
	}
	if (!found)
	{
		for (i = 0; i < face->num_charmaps; i++)
		{
			FT_CharMap charmap = face->charmaps[i];
			if ((charmap->platform_id == 3 && charmap->encoding_id == 1) /* Windows Unicode */
			 || (charmap->platform_id == 3 && charmap->encoding_id == 0) /* Windows Symbol */
			 || (charmap->platform_id == 2 && charmap->encoding_id == 1) /* ISO Unicode */
		         || (charmap->platform_id == 0)) /* Apple Unicode */
			{
				found = charmap;
				break;
			}
		}
	}
	if (found)
	{
		/* If this fails, continue using the default charmap */
		FT_Set_Charmap(face, found);
	}

	/* Set the default font style */
	font->use_kerning = FT_HAS_KERNING(font->face);

	if (TTF_SetFontSizeDPI(font, ptsize, hdpi, vdpi) < 0)
	{
		TTF_SetFTError("Couldn't set font size", error);
		TTF_CloseFont(font);
		return NULL;
	}
	return font;
}

/* Update font parameter depending on a style change */
static int TTF_initFontMetrics(TTF_Font *font)
{
	FT_Face face = font->face;

	/* Make sure that our font face is scalable (global metrics) */
	if (FT_IS_SCALABLE(face))
	{
		/* Get the scalable font metrics for this font */
		FT_Fixed scale       = face->size->metrics.y_scale;
		font->ascent         = FT_CEIL(FT_MulFix(face->ascender, scale));
		//font->descent        = FT_CEIL(FT_MulFix(face->descender, scale));
		font->height         = FT_CEIL(FT_MulFix(face->ascender - face->descender, scale));
	} else {
		/* Get the font metrics for this font, for the selected size */
		font->ascent         = FT_CEIL(face->size->metrics.ascender);
		//font->descent        = FT_CEIL(face->size->metrics.descender);
		font->height         = FT_CEIL(face->size->metrics.height);
	}

	return 0;
}

int TTF_SetFontSizeDPI(TTF_Font *font, int ptsize, unsigned int hdpi, unsigned int vdpi)
{
	FT_Face face = font->face;
	FT_Error error;

	/* Make sure that our font face is scalable (global metrics) */
	if (FT_IS_SCALABLE(face))
	{
		/* Set the character size using the provided DPI.  If a zero DPI
		 * is provided, then the other DPI setting will be used.  If both
		 * are zero, then Freetype's default 72 DPI will be used.  */
		error = FT_Set_Char_Size(face, 0, ptsize * 64, hdpi, vdpi);
		if (error)
		{
			TTF_SetFTError("Couldn't set font size", error);
			return -1;
		}
	} else {
		/* Non-scalable font case.  ptsize determines which family
		 * or series of fonts to grab from the non-scalable format.
		 * It is not the point size of the font.  */
		if (face->num_fixed_sizes <= 0)
		{
			TTF_SetError("Couldn't select size : no num_fixed_sizes");
			return -1;
		}

		/* within [0; num_fixed_sizes - 1] */
		ptsize = MAX(ptsize, 0);
		ptsize = MIN(ptsize, face->num_fixed_sizes - 1);

		error = FT_Select_Size(face, ptsize);
		if (error)
		{
			TTF_SetFTError("Couldn't select size", error);
			return -1;
		}
	}

	if (TTF_initFontMetrics(font) < 0)
	{
		TTF_SetError("Cannot initialize metrics");
		return -1;
	}

	return 0;
}

TTF_Font* TTF_OpenFontFilename(const char *filename, int ptsize, long index, unsigned int hdpi, unsigned int vdpi)
{
	FILE *file = fopen (filename, "rb");
	if ( file == NULL )
	{
		return NULL;
	}
	return TTF_OpenFontFILE(file, ptsize, index, hdpi, vdpi);
}

static void Flush_Glyph(c_glyph *glyph)
{
	if (glyph->pixmap.buffer)
	{
		free(glyph->pixmap.buffer);
		glyph->pixmap.buffer = NULL;
	}
}

static FT_Error Load_Glyph(TTF_Font *font, FT_ULong char_code, c_glyph *cached)
{
	const int alignment = Get_Alignement() - 1;
	FT_GlyphSlot slot;
	FT_Error error;
	TTF_Image *dst   = &cached->pixmap;
	FT_Glyph   glyph = NULL;
	FT_Bitmap *src;

	error = FT_Load_Glyph(font->face, char_code, FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL);
	if (error)
	{
		TTF_SetFTError("Couldn't find glyph", error);
		return error;
	}

	/* Get our glyph shortcut */
	slot = font->face->glyph;

	/* Get the glyph metrics, always needed */
	cached->sz_left  = slot->bitmap_left;
	cached->sz_top   = slot->bitmap_top;
	cached->sz_rows  = slot->bitmap.rows;
	cached->sz_width = slot->bitmap.width;

	/* Current version of freetype is 2.9.1, but on older freetype (2.8.1) this can be 0.
	 * Try to get them from 'FT_Glyph_Metrics' */
	if (cached->sz_left == 0 && cached->sz_top == 0 && cached->sz_rows == 0 && cached->sz_width == 0)
	{
		FT_Glyph_Metrics *metrics = &slot->metrics;
		if (metrics)
		{
			int minx = FT_FLOOR(metrics->horiBearingX);
			int maxx = FT_CEIL(metrics->horiBearingX + metrics->width);
			int maxy = FT_FLOOR(metrics->horiBearingY);
			int miny = maxy - FT_CEIL(metrics->height);

			cached->sz_left  = minx;
			cached->sz_top   = maxy;
			cached->sz_rows  = maxy - miny;
			cached->sz_width = maxx - minx;
		}
	}

	/* All FP 26.6 are 'long' but 'int' should be engouh */
	cached->advance  = (int)slot->metrics.horiAdvance; /* FP 26.6 */

	/* Render the glyph */
	error = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
	if (error)
	{
		TTF_SetFTError("Couldn't render glyph", error);
		return error;
	}

	/* Access bitmap from slot */
	src         = &slot->bitmap;

	/* Get new metrics, from slot */
	dst->left   = slot->bitmap_left;
	dst->top    = slot->bitmap_top;

	/* Common metrics */
	dst->width  = src->width;
	dst->rows   = src->rows;
	dst->buffer = NULL;

	/* FT can make small size glyph of 'width == 0', and 'rows != 0'.
	 * Make sure 'rows' is also 0, so it doesn't break USE_DUFFS_LOOP */
	if (dst->width == 0)
	{
		dst->rows = 0;
	}

	/* Compute pitch: glyph is padded right to be able to read an 'aligned' size expanding on the right */
	dst->pitch = dst->width + alignment;

	if (dst->rows != 0)
	{
		unsigned int i;

		/* Glyph buffer is NOT aligned,
		 * Extra width so it can read an 'aligned' size expanding on the left */
		dst->buffer = (unsigned char *)calloc(alignment + dst->pitch * dst->rows, 1);

		if (!dst->buffer)
		{
			return FT_Err_Out_Of_Memory;
		}

		/* Shift, so that the glyph is decoded centered */
		dst->buffer += alignment;

		/* FT_Render_Glyph() and .fon fonts always generate a two-color (black and white)
		 * glyphslot surface, even when rendered in FT_RENDER_MODE_NORMAL. */
		/* FT_IS_SCALABLE() means that the face contains outline glyphs, but does not imply
		 * that outline is rendered as 8-bit grayscale, because embedded bitmap/graymap is
		 * preferred (see FT_LOAD_DEFAULT section of FreeType2 API Reference).
		 * FT_Render_Glyph() canreturn two-color bitmap or 4/16/256 color graymap
		 * according to the format of embedded bitmap/graymap. */
		for (i = 0; i < (unsigned int)src->rows; i++)
		{
			unsigned char *srcp = src->buffer + i * src->pitch;
			unsigned char *dstp = dst->buffer + i * dst->pitch;
			unsigned int k, quotient, remainder;

			/* Decode exactly the needed size from src->width */
			if (src->pixel_mode == FT_PIXEL_MODE_MONO)
			{
				quotient  = src->width / 8;
				remainder = src->width & 0x7;
			} else if (src->pixel_mode == FT_PIXEL_MODE_GRAY2)
			{
				quotient  = src->width / 4;
				remainder = src->width & 0x3;
			} else if (src->pixel_mode == FT_PIXEL_MODE_GRAY4)
			{
				quotient  = src->width / 2;
				remainder = src->width & 0x1;
			} else {
				quotient  = src->width;
				remainder = 0;
			}

/* FT_RENDER_MODE_MONO and src->pixel_mode MONO */
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(push, 1)
#pragma warning(disable:4127)
#endif

/* FT_RENDER_MODE_NORMAL and src->pixel_mode MONO */
#define NORMAL_MONO(K_MAX)                                                  \
                if ((K_MAX)) {                                              \
                    unsigned char c = *srcp++;                              \
                    for (k = 0; k < (K_MAX); ++k) {                         \
                        if ((c&0x80) >> 7) {                                \
                            *dstp++ = NUM_GRAYS - 1;                        \
                        } else {                                            \
                            *dstp++ = 0x00;                                 \
                        }                                                   \
                        c <<= 1;                                            \
                    }                                                       \
                }

/* FT_RENDER_MODE_NORMAL and src->pixel_mode GRAY2 */
#define NORMAL_GRAY2(K_MAX)                                                 \
                if ((K_MAX)) {                                              \
                    unsigned char c = *srcp++;                              \
                    for (k = 0; k < (K_MAX); ++k) {                         \
                        if ((c&0xA0) >> 6) {                                \
                            *dstp++ = NUM_GRAYS * ((c&0xA0) >> 6) / 3 - 1;  \
                        } else {                                            \
                            *dstp++ = 0x00;                                 \
                        }                                                   \
                        c <<= 2;                                            \
                    }                                                       \
                }

/* FT_RENDER_MODE_NORMAL and src->pixel_mode GRAY4 */
#define NORMAL_GRAY4(K_MAX)                                                 \
                if ((K_MAX)) {                                              \
                    unsigned char c = *srcp++;                              \
                    for (k = 0; k < (K_MAX); ++k) {                         \
                        if ((c&0xF0) >> 4) {                                \
                            *dstp++ = NUM_GRAYS * ((c&0xF0) >> 4) / 15 - 1; \
                        } else {                                            \
                            *dstp++ = 0x00;                                 \
                        }                                                   \
                        c <<= 4;                                            \
                    }                                                       \
                }

			if (src->pixel_mode == FT_PIXEL_MODE_MONO)
			{
				/* This special case wouldn't be here if the FT_Render_Glyph()
				 * function wasn't buggy when it tried to render a .fon font with 256
				 * shades of gray.  Instead, it returns a black and white surface
				 * and we have to translate it back to a 256 gray shaded surface. */
				while (quotient--)
				{
					NORMAL_MONO(8);
				}
				NORMAL_MONO(remainder);
			} else if (src->pixel_mode == FT_PIXEL_MODE_GRAY2)
			{
				while (quotient--)
				{
					NORMAL_GRAY2(4);
				}
				NORMAL_GRAY2(remainder);
			} else if (src->pixel_mode == FT_PIXEL_MODE_GRAY4)
			{
				while (quotient--)
				{
					NORMAL_GRAY4(2);
				}
				NORMAL_GRAY4(remainder);
			} else {
				memcpy(dstp, srcp, src->width);
			}
		}
	}
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(pop)
#endif

	/* Shift back */
	if (dst->buffer)
	{
		dst->buffer -= alignment;
	}

	/* Free outlined glyph */
	if (glyph)
	{
		FT_Done_Glyph(glyph);
	}

	/* We're done, this glyph is cached since 'stored' is not 0 */
	return 0;
}

void TTF_CloseFont(TTF_Font *font)
{
	if (font)
	{
		void *args_stream = font->args.stream;
		if (font->last_surface)
		{
			free (font->last_surface);
			font->last_surface = 0;
			font->last_surface_size = 0;
		}
		if (font->face)
		{
			FT_Done_Face(font->face);
		}
		free (args_stream);
		fclose (font->src);
		free(font);
	}
}

static int TTF_Size_Internal_CodePoint (TTF_Font *font, c_glyph *glyph,
                                        int *w, int *h, int *xstart, int *ystart)
{
	int pos_y;
	int minx, maxx;
	int miny, maxy;

	TTF_CHECK_INITIALIZED(-1);
	TTF_CHECK_POINTER(font, -1);

	/* Compute positions where to copy the glyph bitmap */
	pos_y = F26Dot6(font->ascent);

	glyph->pos_buf_y     = pos_y;

	/* Compute previsionnal global bounding box */
	pos_y = FT_FLOOR(pos_y) - glyph->sz_top;

	minx = MIN(0, glyph->sz_left);
	maxx = MAX(FT_FLOOR(glyph->advance), glyph->sz_left + glyph->sz_width);
	miny = MIN(0, pos_y);
	maxy = MAX(font->height, pos_y + glyph->sz_rows);

	/* Initial x start position: often 0, except when a glyph would be written at
	 * a negative position. In this case an offset is needed for the whole line. */
	*xstart = (minx < 0)? -minx : 0;

	/* Initial y start: compensation for a negative y offset */
	*ystart = (miny < 0)? -miny : 0;

	/* Fill the bounds rectangle */
	*w = (maxx - minx);
	*h = (maxy - miny);

	return 0;
}

TTF_Surface *TTF_RenderGlyph32_Shaded (TTF_Font *font, uint32_t ch)
{
	int xstart, ystart, width, height;
	TTF_Surface *textbuf = NULL;
	c_glyph glyph;

	TTF_CHECK_INITIALIZED (NULL);
	TTF_CHECK_POINTER (font, NULL);

	FT_UInt idx = FT_Get_Char_Index (font->face, ch);
	if (Load_Glyph (font, idx, &glyph))
	{
		return 0;
	}

	/* Get the dimensions of the text surface */
	if ((TTF_Size_Internal_CodePoint (font, &glyph, &width, &height, &xstart, &ystart) < 0) || !width)
	{
		TTF_SetError("Text has zero width");
		goto failure;
	}

	/* Create surface for rendering */
	textbuf = Create_Surface_Shaded (font, width, height);
	if (textbuf == NULL)
	{
		TTF_SetError("Memory allocation error");
		goto failure;
	}

	/* Render one text line to textbuf at (xstart, ystart) */
	Render_Glyph(font, &glyph, textbuf, xstart, ystart);

	Flush_Glyph(&glyph);

	return textbuf;

failure:
	Flush_Glyph(&glyph);

	return NULL;
}

void TTF_Quit(void)
{
	if (TTF_initialized)
	{
		if (--TTF_initialized == 0)
		{
			FT_Done_FreeType(library);
		}
	}
}
