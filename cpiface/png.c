#include "config.h"
#include <png.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "types.h"
#include "png.h"

#ifdef PNG_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format,args...) ((void)0)
#endif

struct png_virt_io
{
	uint8_t *src;
	uint_fast32_t len;
	uint_fast32_t pos;
};

static void png_read_ocp(png_structp png_ptr, png_bytep data, png_size_t length)
{
	struct png_virt_io *io = png_get_io_ptr (png_ptr);

	if ((io->pos + length) > io->len)
	{
		debug_printf("[CPIFACE/PNG] png_read_ocp(): ran out of data - EOF\n");
		longjmp( png_jmpbuf( png_ptr), 1);
	}
	memcpy (data, io->src + io->pos, length);
	io->pos += length;
}

int try_open_png (uint16_t *width, uint16_t *height, uint8_t **data_bgra, uint8_t *src, uint_fast32_t srclen)
{
	png_structp png_ptr      = 0;
	png_infop   info_ptr     = 0;
	png_infop   end_info     = 0;
	struct png_virt_io io    = {src, srclen, 0};
	png_bytep  *row_pointers = 0;

	*data_bgra = 0;
	*width = *height = 0;

	if (srclen < 8)
	{
		debug_printf("[CPIFACE/PNG] try_open_png(): srclen < 8, unable to check header\n");
		return -1;
	}
	if (png_sig_cmp(src, 0, 8))
	{
		debug_printf("[CPIFACE/PNG] try_open_png(): header is not a valid PNG file\n");
		return -1;
	}

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
                                          NULL /*(png_voidp)user_error_ptr*/,
                                          NULL /*user_error_fn*/,
                                          NULL /*user_warning_fn*/);
	if (!png_ptr)
	{
		debug_printf("[CPIFACE/PNG] png_create_read_struct() failed\n");
		return -1;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr)
	{
		debug_printf ("[CPIFACE/PNG] png_create_info_struct() failed #1\n");
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return -1;
	}

	end_info = png_create_info_struct( png_ptr);
	if (!end_info)
	{
		debug_printf ("[CPIFACE/PNG] png_create_info_struct() failed #2\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		return -1;
	}

	if (setjmp (png_jmpbuf( png_ptr)))
	{
		debug_printf ("[CPIFACE/PNG] loading PNG, fatal error\n");
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
		free (row_pointers);
		free (*data_bgra); *data_bgra = 0;
		*width = *height = 0;
		return -1;
	}

	png_set_read_fn (png_ptr, &io, png_read_ocp);

	png_set_user_limits (png_ptr, 1920, 1080);

#if 1
	png_read_info (png_ptr, info_ptr);

	{
		png_uint_32 w, h;
		int bit_depth, color_type, interlace_type, compression_type, filter_method;
		int i;
		int number_of_passes = 1;

		png_get_IHDR(png_ptr, info_ptr, &w, &h,
		             &bit_depth, &color_type, &interlace_type,
		             &compression_type, &filter_method);

		debug_printf ("[CPIFACE/PNG] png_get_IHDR: width=%"PRIu32" height=%"PRIu32" bit_depth=%d color_type=0x%x, interlace_type=0x%x, compression_type=0x%x filter_method=0x%x\n", w, h, bit_depth, color_type, interlace_type, compression_type, filter_method);

		switch (color_type)
		{
			case PNG_COLOR_TYPE_GRAY:
				/* (bit depths 1, 2, 4, 8, 16) */
				if (bit_depth == 16)
				{
					png_set_strip_16 (png_ptr);
				} else if (bit_depth < 8)
				{
					png_set_packing (png_ptr);
				}
				png_set_expand (png_ptr);
				png_set_add_alpha (png_ptr, 0xff, PNG_FILLER_AFTER);
				break;
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				/* (bit depths 8, 16) */
				if (bit_depth == 16)
				{
					png_set_strip_16 (png_ptr);
				}
				png_set_expand (png_ptr);
				break;
			case PNG_COLOR_TYPE_PALETTE:
				/* bit depths 1, 2, 4, 8) */
				png_set_palette_to_rgb (png_ptr);
				png_set_bgr (png_ptr);
				png_set_add_alpha (png_ptr, 0xff, PNG_FILLER_AFTER);
				break;
			case PNG_COLOR_TYPE_RGB:
				/* (bit_depths 8, 16) */
				if (bit_depth == 16)
				{
					png_set_strip_16 (png_ptr);
				}
				png_set_bgr (png_ptr);
				png_set_add_alpha (png_ptr, 0xff, PNG_FILLER_AFTER);
				break;
			case PNG_COLOR_TYPE_RGB_ALPHA:
				/* (bit_depths 8, 16) */
				png_set_bgr (png_ptr);
				break;
			default:
				debug_printf ("[CPIFACE/PNG] unknown color_type\n");
				longjmp( png_jmpbuf( png_ptr), 1);
		}

		if (interlace_type == PNG_INTERLACE_ADAM7)
		{
			number_of_passes = png_set_interlace_handling(png_ptr);
		}

		png_read_update_info(png_ptr, info_ptr);

		*width = w;
		*height = h;

		*data_bgra = malloc (w * h * 4);

		row_pointers = malloc (h * sizeof (row_pointers[0]));
		for (i = 0; i < h; i++)
		{
			row_pointers[i] = *data_bgra + w * i * 4;
		}
		for (i=0; i < number_of_passes; i++)
		{
			png_read_image (png_ptr, row_pointers);
		}
	}

#else
	png_read_png (png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_BGR | PNG_TRANSFORM_GRAY_TO_RGB, NULL)
#endif

	png_read_end(png_ptr, end_info);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	png_free_data (png_ptr, info_ptr, PNG_FREE_ALL, -1);

	free (row_pointers); row_pointers = 0;

	return 0;
}
