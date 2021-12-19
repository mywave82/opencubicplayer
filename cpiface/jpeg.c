#include "config.h"
#include <jpeglib.h>
#include <setjmp.h>
#include <stdlib.h>
#include "types.h"
#include "jpeg.h"

#if BITS_IN_JSAMPLE != 8
# error BITS_IN_JSAMPLE != 8
#endif

#ifdef JPEG_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format,args...) ((void)0)
#endif

struct jpeg_error_mgr_ocp
{
	struct jpeg_error_mgr parent;
	jmp_buf setjmp_buffer;
};

static char jpegLastErrorMsg[JMSG_LENGTH_MAX];
static void jpegErrorExit (j_common_ptr cinfo)
{
	/* cinfo->err actually points to a jpegErrorManager struct */
	struct jpeg_error_mgr_ocp* myerr = (struct jpeg_error_mgr_ocp*) cinfo->err;

	/* Create the message */
	( *(cinfo->err->format_message) ) (cinfo, jpegLastErrorMsg);

	/* Jump to the setjmp point */
	longjmp(myerr->setjmp_buffer, 1);
}

int try_open_jpeg (uint16_t *width, uint16_t *height, uint8_t **data_bgra, const uint8_t *src, uint_fast32_t srclen)
{
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr_ocp jerr;
#ifndef JCS_EXT_BGRA
	uint8_t *data_rgb = 0;
#endif

	*data_bgra = 0;
	*width = *height = 0;

        cinfo.err = jpeg_std_error(&jerr.parent);
	cinfo.err->error_exit = jpegErrorExit;
	jpeg_create_decompress(&cinfo);

	if (setjmp(jerr.setjmp_buffer))
	{
		fprintf (stderr, "[CPIFACE/JPEG] libjpeg fatal error: %s\n", jpegLastErrorMsg);
		jpeg_destroy_decompress(&cinfo);
		free (*data_bgra);
#ifndef JCS_EXT_BGRA
		free (data_rgb);
		data_rgb = 0;
#endif
		*data_bgra = 0;
		*width = *height = 0;
		return -1;
	}

	jpeg_mem_src(&cinfo, src, srclen);
	if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
	{
		snprintf (jpegLastErrorMsg, sizeof (jpegLastErrorMsg), "%s", "jpeg_read_header() failed");
		longjmp (jerr.setjmp_buffer, 1);
	}

	debug_printf ("[CPIFACE/JPEG] width=%u height=%u components=%d\n", (unsigned int)cinfo.image_width, (unsigned int)cinfo.image_height, cinfo.num_components);

	if ((cinfo.image_width > 1920) || (cinfo.image_height > 1080))
	{
		snprintf (jpegLastErrorMsg, sizeof (jpegLastErrorMsg), "resolution too big: %ux%x", (unsigned int)cinfo.image_width, (unsigned int)cinfo.image_height);

		longjmp (jerr.setjmp_buffer, 1);
	}

#ifndef JCS_EXT_BGRA
	cinfo.out_color_space = JCS_RGB;
	data_rgb = malloc (cinfo.image_width * cinfo.image_height * 3);
#else
	cinfo.out_color_space = JCS_EXT_BGRA;
#endif
	cinfo.dct_method = JDCT_ISLOW;
	*data_bgra = malloc (cinfo.image_width * cinfo.image_height * 4);

	if (!jpeg_start_decompress (&cinfo))
	{
		snprintf (jpegLastErrorMsg, sizeof (jpegLastErrorMsg), "jpeg_start_decompress() failed");
		longjmp (jerr.setjmp_buffer, 1);
	}

	*width = cinfo.image_width;
	*height = cinfo.image_height;

	while (cinfo.output_scanline < cinfo.output_height)
	{
#ifndef JCS_EXT_BGRA
		JSAMPROW rows[1] = {data_rgb + (cinfo.output_scanline * 3) * cinfo.image_width};
#else
		JSAMPROW rows[1] = {*data_bgra + (cinfo.output_scanline << 2) * cinfo.image_width};
#endif
		jpeg_read_scanlines(&cinfo, rows, 1);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

#ifndef JCS_EXT_BGRA
	{
		uint8_t *src = data_rgb;
		uint8_t *dst = *data_bgra;
		int i;
		for (i=0; i < cinfo.image_width * cinfo.image_height; i++)
		{
			dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
			dst[3] = 255;
			dst+=4;
			src+=3;
		}
	}
#endif

	return 0;
}
