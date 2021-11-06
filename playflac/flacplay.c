/* OpenCP Module Player
 * copyright (c) '08-'10 Stian Skjelstad <stian@nixia.no>
 *
 * FLACPlay - Player for FLAC
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
#include <errno.h>
#include <fcntl.h>
#include <FLAC/all.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "cpiface/gif.h"
#include "cpiface/jpeg.h"
#include "cpiface/png.h"
#include "dev/deviplay.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "flacplay.h"
#include "stuff/imsrtns.h"
#include "stuff/poll.h"
#include "stuff/timer.h"

static volatile int clipbusy=0;

/* options */
static int inpause;

static unsigned long amplify; /* TODO */
static unsigned long voll,volr;
static int pan;
static int srnd;

static struct ocpfilehandle_t *flacfile = NULL;
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__SeekableStreamDecoder *decoder = 0;
#else
FLAC__StreamDecoder *decoder = 0;
#endif
static int eof_flacfile = 0;
static int eof_buffer = 0;
static uint64_t samples;

/* flacIdler dumping locations */
static uint16_t *flacbuf;    /* in 16bit samples */
static uint32_t  flacbufrate; /* This is the mix rate in 16:16 fixed format */
static struct ringbuffer_t *flacbufpos;
static uint32_t  flacbuffpos; /* read fine-pos.. when flacbufrate has a fraction */
/* source info from stream */
static unsigned int flacrate; /* this is the source rate */
static int flacstereo; /* this we can ignore I think */
static unsigned int flac_max_blocksize;
static int flacbits=0;

static uint64_t  flaclastpos;

/* devp pre-buffer zone */
static uint16_t *buf16; /* here we dump data, with correct rate, ready for converting, sample size, layout, etc */

/* devp buffer zone */
static uint32_t bufpos; /* devp write head location */
static uint32_t buflen; /* devp buffer-size in samples */
static void *plrbuf; /* the devp buffer */
static int stereo; /* boolean */
static int bit16; /* boolean */
static int signedout; /* boolean */
static int reversestereo; /* boolean */
static int donotloop=1;

static int flacPendingSeek = 0;
static uint64_t flacPendingSeekPos;

struct flac_comment_t __attribute__ ((visibility ("internal"))) **flac_comments;
int                   __attribute__ ((visibility ("internal")))   flac_comments_count;
struct flac_picture_t __attribute__ ((visibility ("internal")))  *flac_pictures;
int                   __attribute__ ((visibility ("internal")))   flac_pictures_count;

static void add_comment2(const char *title, const char *value, const uint32_t valuelen)
{
	int n = 0;
	for (n = 0; n < flac_comments_count; n++)
	{
		int res = strcmp (flac_comments[n]->title, title);
		if (res == 0)
		{
			// append to at this point
			flac_comments[n] = realloc (flac_comments[n], sizeof (flac_comments[n]) + sizeof (flac_comments[n]->value[0]) * (flac_comments[n]->value_count + 1));
			flac_comments[n]->value[flac_comments[n]->value_count++] = malloc (valuelen + 1);
			memcpy (flac_comments[n]->value[flac_comments[n]->value_count++], value, valuelen);
			flac_comments[n]->value[flac_comments[n]->value_count++][valuelen] = 0;
			return;
		}
		if (res < 0)
		{
			continue;
		} else {
			// insert it at this point
			break;
		}
	}

	flac_comments = realloc (flac_comments, sizeof (flac_comments[0]) * (flac_comments_count+1));
	memmove (flac_comments + n + 1, flac_comments + n, (flac_comments_count - n) * sizeof (flac_comments[0]));
	flac_comments[n] = malloc (sizeof (*flac_comments[n]) + sizeof (flac_comments[n]->value[0]));
	flac_comments[n]->title = strdup (title);
	flac_comments[n]->value_count = 1;
	flac_comments[n]->value[0] = strdup (value);
	flac_comments_count++;
}

static void add_comment(const char *src, const uint32_t srclen)
{
	char *equal, *tmp, *tmp2;
#if 0
	if (!strncasecmp (src, "METADATA_BLOCK_PICTURE=", 23))
	{
		add_picture_base64(src + 23);
		return;
	}
#endif
	equal = memchr (src, '=', srclen);

	if (!equal)
	{
		return;
	}
	if (equal == src)
	{
		return;
	}

	tmp = malloc (equal - src + 1);
	strncpy (tmp, src, equal - src);
	tmp[equal-src] = 0;

	if ((tmp[0] >= 'a') && (tmp[0] <= 'z')) tmp[0] -= 0x20;

	for (tmp2 = tmp + 1; *tmp2; tmp2++)
	{
		if ((tmp2[0] >= 'A') && (tmp2[0] <= 'Z')) tmp2[0] += 0x20;
	}

	add_comment2(tmp, src + (equal - src) + 1, srclen - (equal + 1 - src));

	free (tmp);
}

static void add_picture(const uint16_t actual_width,
		        const uint16_t actual_height,
			uint8_t *data_bgra,
			const char *description,
			const uint32_t picture_type)
{
	flac_pictures = realloc (flac_pictures, sizeof (flac_pictures[0]) * (flac_pictures_count + 1));
	flac_pictures[flac_pictures_count].picture_type = picture_type;
	flac_pictures[flac_pictures_count].description = strdup (description);
	flac_pictures[flac_pictures_count].width = actual_width;
	flac_pictures[flac_pictures_count].height = actual_height;
	flac_pictures[flac_pictures_count].data_bgra = data_bgra;
	flac_pictures[flac_pictures_count].scaled_width = 0;
	flac_pictures[flac_pictures_count].scaled_height = 0;
	flac_pictures[flac_pictures_count].scaled_data_bgra = 0;

	flac_pictures_count++;
}

#define PANPROC \
do { \
	float _rs = rs, _ls = ls; \
	if(pan==-64) \
	{ \
		float t=_ls; \
		_ls = _rs; \
		_rs = t; \
	} else if(pan==64) \
	{ \
	} else if(pan==0) \
		_rs=_ls=(_rs+_ls) / 2.0; \
	else if(pan<0) \
	{ \
		_ls = _ls / (-pan/-64.0+2.0) + _rs*(64.0+pan)/128.0; \
		_rs = _rs / (-pan/-64.0+2.0) + _ls*(64.0+pan)/128.0; \
	} else if(pan<64) \
	{ \
		_ls = _ls / (pan/-64.0+2.0) + _rs*(64.0-pan)/128.0; \
		_rs = _rs / (pan/-64.0+2.0) + _ls*(64.0-pan)/128.0; \
	} \
	rs = _rs * volr / 256.0; \
	ls = _ls * voll / 256.0; \
} while(0)

/* FLAC decoder needs more data */
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__SeekableStreamDecoderReadStatus read_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__byte buffer[],
	unsigned int *bytes,
	void *client_data)
#else
static FLAC__StreamDecoderReadStatus read_callback (
	const FLAC__StreamDecoder *decoder,
	FLAC__byte buffer[],
	size_t *bytes,
	void *client_data)
#endif
{
	int retval;

	retval = flacfile->read (flacfile, buffer, *bytes);
	if (retval<=0)
	{
		*bytes=0;
		if (flacfile->eof (flacfile))
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}
	*bytes = retval;
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static inline int16_t make_16bit(const FLAC__int32 src, int bps)
{
	if (bps==16)
		return src;
	else if (bps>16)
		return src>>(bps-16);
	else
		return src<<(16-bps);
}
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__StreamDecoderWriteStatus write_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	const FLAC__Frame *frame,
	const FLAC__int32 * const buffer[],
	void *client_data)
#else
static FLAC__StreamDecoderWriteStatus write_callback (
	const FLAC__StreamDecoder *decoder,
	const FLAC__Frame *frame,
	const FLAC__int32 * const buffer[],
	void *client_data)
#endif
{/*
	frame can probably be stored until we decode more data
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE   is nice
	size of buffer =  frame->header.channels * frame->header.blocksize * frame->header.bits_per_sample / 8
	buffer is a 2D array with [channel][sample] as index
*/
	unsigned int i;
	unsigned short xormask=0x0000;

	int pos1, length1, pos2, length2;

	if (frame->header.number_type==FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER)
		flaclastpos=(uint64_t)(frame->header.number.frame_number) * frame->header.blocksize;
	else
		flaclastpos=frame->header.number.sample_number;

	if (srnd)
		xormask=0xffff;

	ringbuffer_get_head_samples (flacbufpos, &pos1, &length1, &pos2, &length2);

	if (frame->header.blocksize > (length1+length2))
	{
		fprintf (stderr, "playflac: ERROR: frame->header.blocksize %d >= available space in ring-buffer %d + %d\n", frame->header.blocksize, length1, length2);
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	for (i=0;i<frame->header.blocksize;i++)
	{
		int ls, rs;

		ls  = make_16bit(buffer[0][i], frame->header.bits_per_sample);
		rs = make_16bit(buffer[1][i], frame->header.bits_per_sample);
		/* append to buffer */
		PANPROC;
		flacbuf[pos1*2]=(int16_t)ls^xormask;
		flacbuf[pos1*2+1]=rs;
		pos1++;
		if (!--length1)
		{
			pos1 = pos2;
			length1 = length2;
			pos2 = 0;
			length2 = 0;
		}
	}

	ringbuffer_head_add_samples (flacbufpos, frame->header.blocksize);

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static void metadata_callback(
	const FLAC__SeekableStreamDecoder *decoder,
	const FLAC__StreamMetadata *metadata,
	void *client_data)
#else
static void metadata_callback(
	const FLAC__StreamDecoder *decoder,
	const FLAC__StreamMetadata *metadata,
	void *client_data)
#endif
{
	switch (metadata->type)
	{
		case FLAC__METADATA_TYPE_STREAMINFO:
		{
			flacrate           = metadata->data.stream_info.sample_rate;
			flacstereo         = metadata->data.stream_info.channels > 1;
			flacbits           = metadata->data.stream_info.bits_per_sample;
			flac_max_blocksize = metadata->data.stream_info.max_blocksize;
			samples            = metadata->data.stream_info.total_samples;
#if 0
			fprintf(stderr, "METADATA_TYPE_STREAMINFO\n");
			fprintf(stderr, "streaminfo.min_blocksize: %d\n", metadata->data.stream_info.min_blocksize);
			fprintf(stderr, "streaminfo.max_blocksize: %d\n", metadata->data.stream_info.max_blocksize);
			fprintf(stderr, "streaminfo.min_framesize: %d\n", metadata->data.stream_info.min_framesize);
			fprintf(stderr, "streaminfo.max_framesize: %d\n", metadata->data.stream_info.max_framesize);
			fprintf(stderr, "streaminfo.sample_rate: %d\n", metadata->data.stream_info.sample_rate);
			fprintf(stderr, "streaminfo.channels: %d\n", metadata->data.stream_info.channels);
			fprintf(stderr, "streaminfo.bits_per_sample: %d\n", metadata->data.stream_info.bits_per_sample);
			fprintf(stderr, "streaminfo.total_samples: %"PRIu64"\n", metadata->data.stream_info.total_samples);
			fprintf(stderr, "\n");
#endif
			break;
		}
#if 0
		case FLAC__METADATA_TYPE_PADDING:
		{
			fprintf (stderr, "METADATA_TYPE_PADDING\n\n");
			break;
		}
		case FLAC__METADATA_TYPE_APPLICATION:
		{
			fprintf (stderr, "METADATA_TYPE_APPLICATION\n");
			fprintf (stderr, "id: \"%c%c%c%c\"\n\n", metadata->data.application.id[0], metadata->data.application.id[1], metadata->data.application.id[2], metadata->data.application.id[3]);
			break;
		}
		case FLAC__METADATA_TYPE_SEEKTABLE:
		{
			fprintf (stderr, "METADATA_TYPE_SEEKTABLE\n");
			fprintf (stderr, "num_points: %"PRIu32"\n\n", metadata->data.seek_table.num_points);
			break;
		}
#endif
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		{
#if 0
			uint32_t i;
#endif
			uint32_t j;

			for (j=0; j < metadata->data.vorbis_comment.num_comments; j++)
			{
				add_comment ((char *)metadata->data.vorbis_comment.comments[j].entry, metadata->data.vorbis_comment.comments[j].length);
			}
#if 0
			fprintf (stderr, "METADATA_TYPE_VORBIS_COMMENT\n");
			fprintf (stderr, "vendorstring: \"");
			for (i=0; i < metadata->data.vorbis_comment.vendor_string.length; i++)
			{
				fputc(metadata->data.vorbis_comment.vendor_string.entry[i], stderr);
			}
			fprintf (stderr, "\"\n");
			fprintf (stderr, "num_comments: %"PRIu32"\n", metadata->data.vorbis_comment.num_comments);
			for (j=0; j < metadata->data.vorbis_comment.num_comments; j++)
			{
				fprintf (stderr, "[%"PRIu32"]: \"", j);
				for (i=0; i < metadata->data.vorbis_comment.comments[j].length; i++)
				{
					fputc(metadata->data.vorbis_comment.comments[j].entry[i], stderr);
				}
				fprintf (stderr, "\"\n");
			}
			fprintf (stderr, "\n");
#endif
			break;
		}
#if 0
		case FLAC__METADATA_TYPE_CUESHEET:
		{
			uint32_t i;
			fprintf (stderr, "METADATA_TYPE_CUESHEET\n");
			fprintf (stderr, "media_catalog_number: \"%s\"\n", metadata->data.cue_sheet.media_catalog_number);
			fprintf (stderr, "lead_in: %"PRIu64"\n", metadata->data.cue_sheet.lead_in);
			fprintf (stderr, "is_cd: %d\n", !!metadata->data.cue_sheet.is_cd);
			fprintf (stderr, "num_tracks: %"PRIu32"\n", metadata->data.cue_sheet.num_tracks);
			for (i = 0; i < metadata->data.cue_sheet.num_tracks; i++)
			{
				//
			}

			break;
		}
#endif
		case FLAC__METADATA_TYPE_PICTURE:
		{
#if 0
			fprintf (stderr, "METADATA_TYPE_PICTURE\n");
			fprintf (stderr, "type: %d\n", metadata->data.picture.type);
			fprintf (stderr, "mime_type: \"%s\"\n", metadata->data.picture.mime_type);
			fprintf (stderr, "description: \"%s\"\n", metadata->data.picture.description);
			fprintf (stderr, "width: %"PRIu32"\n", metadata->data.picture.width);
			fprintf (stderr, "height: %"PRIu32"\n", metadata->data.picture.height);
			fprintf (stderr, "depth: %"PRIu32"\n", metadata->data.picture.depth);
			fprintf (stderr, "colors: %"PRIu32"\n", metadata->data.picture.colors);
			fprintf (stderr, "data_length: %"PRIu32"\n", metadata->data.picture.data_length);
			// data
			fprintf (stderr, "\n");
#endif

#ifdef HAVE_LZW
			if (!strcasecmp (metadata->data.picture.mime_type, "image/gif"))
			{
				uint16_t actual_height, actual_width;
				uint8_t *data_bgra;
				if (!GIF87_try_open_bgra (&actual_width, &actual_height, &data_bgra, metadata->data.picture.data, metadata->data.picture.data_length))
				{
					add_picture (actual_width, actual_height, data_bgra, (const char *)metadata->data.picture.description, metadata->data.picture.type);
				}
				break;
			}
#endif

			if (!strcasecmp (metadata->data.picture.mime_type, "image/png"))
			{
				uint16_t actual_height, actual_width;
				uint8_t *data_bgra;
				if (!try_open_png (&actual_width, &actual_height, &data_bgra, metadata->data.picture.data, metadata->data.picture.data_length))
				{
					add_picture (actual_width, actual_height, data_bgra, (const char *)metadata->data.picture.description, metadata->data.picture.type);
				}
				break;
			}
			if ((!strcasecmp (metadata->data.picture.mime_type, "image/jpg")) || (!strcasecmp (metadata->data.picture.mime_type, "image/jpeg")))
			{
				uint16_t actual_height, actual_width;
				uint8_t *data_bgra;
				if (!try_open_jpeg (&actual_width, &actual_height, &data_bgra, metadata->data.picture.data, metadata->data.picture.data_length))
				{
					add_picture (actual_width, actual_height, data_bgra, (const char *)metadata->data.picture.description, metadata->data.picture.type);
				}
				break;
			}

			break;
		}
		default:
			break;
	}
	return;
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__SeekableStreamDecoderSeekStatus seek_callback(
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__uint64 absolute_byte_offset,
	void *client_data)
#else
static FLAC__StreamDecoderSeekStatus seek_callback(
	const FLAC__StreamDecoder *decoder,
	FLAC__uint64 absolute_byte_offset,
	void *client_data)
#endif
{
	if (flacfile->seek_set (flacfile, absolute_byte_offset) == 0)
	{
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
		return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
#else
		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
#endif
	} else {
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
		return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
#else
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
#endif
	}
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__SeekableStreamDecoderTellStatus tell_callback(
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__uint64 *absolute_byte_offset,
	void *client_data)
#else
static FLAC__StreamDecoderTellStatus tell_callback(
	const FLAC__StreamDecoder *decoder,
	FLAC__uint64 *absolute_byte_offset,
	void *client_data)
#endif
{
	*absolute_byte_offset = flacfile->getpos (flacfile);
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
#else
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
#endif
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__SeekableStreamDecoderLengthStatus length_callback(
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__uint64 *stream_length,
	void *client_data)
#else
static FLAC__StreamDecoderLengthStatus length_callback(
	const FLAC__StreamDecoder *decoder,
	FLAC__uint64 *stream_length,
	void *client_data)
#endif
{
	uint64_t temp;

	temp = flacfile->filesize (flacfile);
	if (temp == FILESIZE_STREAM)
	{
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
		return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;
#else
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
#endif

	}
	if (temp == FILESIZE_ERROR)
	{
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
		return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;
#else
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
#endif
	}

	*stream_length = temp;
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
#else
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
#endif
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__bool eof_callback(
	const FLAC__SeekableStreamDecoder *decoder,
	void *client_data)
#else
static FLAC__bool eof_callback(
	const FLAC__StreamDecoder *decoder,
	void *client_data)
#endif
{
	return flacfile->eof (flacfile);
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static void error_callback(
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__StreamDecoderErrorStatus status,
	void *client_data)
#else
static void error_callback(
	const FLAC__StreamDecoder *decoder,
	FLAC__StreamDecoderErrorStatus status,
	void *client_data)
#endif
{
	fprintf(stderr, "playflac: ERROR libflac: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

int __attribute__ ((visibility ("internal"))) flacOpenPlayer(struct ocpfilehandle_t *file)
{
	int temp;
	uint32_t flacbuflen;

	if (flacfile)
	{
		flacfile->unref (flacfile);
		flacfile = 0;
	}
	flacfile = file;
	flacfile->ref (flacfile);

	inpause=0;
	voll=256;
	volr=256;
	pan=64;
	srnd=0;
	eof_flacfile=0;
	eof_buffer=0;
	flacSetAmplify(65536);

	buf16=0;
	flacbuf=0;
	flacbufpos = 0;
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	decoder = FLAC__seekable_stream_decoder_new();
#else
	decoder = FLAC__stream_decoder_new();
#endif
	if (!decoder)
	{
		fprintf(stderr, "playflac: FLAC__seekable_stream_decoder_new() failed, out of memory?\n");
		return 0;
	}

	FLAC__stream_decoder_set_metadata_respond_all (decoder);

	flac_max_blocksize=0;
	flacrate=0;
	flacstereo=1;

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	FLAC__seekable_stream_decoder_set_md5_checking(decoder, 0);

	FLAC__seekable_stream_decoder_set_read_callback(decoder, read_callback);
	FLAC__seekable_stream_decoder_set_write_callback(decoder, write_callback);
	FLAC__seekable_stream_decoder_set_metadata_callback(decoder, metadata_callback);
	FLAC__seekable_stream_decoder_set_seek_callback(decoder, seek_callback);
	FLAC__seekable_stream_decoder_set_tell_callback(decoder, tell_callback);
	FLAC__seekable_stream_decoder_set_length_callback(decoder, length_callback);
	FLAC__seekable_stream_decoder_set_eof_callback(decoder, eof_callback);
	FLAC__seekable_stream_decoder_set_client_data(decoder, 0);
	FLAC__seekable_stream_decoder_set_error_callback(decoder, error_callback);
	if ((temp=FLAC__seekable_stream_decoder_init(decoder))!=FLAC__SEEKABLE_STREAM_DECODER_OK)
	{
		fprintf(stderr, "playflac: FLAC__seekable_stream_decoder_init() failed, %s\n", FLAC__SeekableStreamDecoderStateString[temp]);
		FLAC__seekable_stream_decoder_delete(decoder);
		decoder = NULL;
		goto error_out;
	}
	if (!FLAC__seekable_stream_decoder_process_until_end_of_metadata(decoder))
	{
		fprintf(stderr, "playflac: FLAC__seekable_stream_decoder_process_until_end_of_metadata() failed\n");
		goto error_out;
	}
#else
	FLAC__stream_decoder_set_md5_checking(decoder, true);
	if((temp=FLAC__stream_decoder_init_stream(
	   decoder,
	   read_callback,
	   seek_callback,
	   tell_callback,
	   length_callback,
	   eof_callback,
	   write_callback,
	   metadata_callback,
	   error_callback,
	   0 /*my_client_data*/
	)) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		fprintf(stderr, "playflac: FLAC__stream_decoder_init_stream() failed, %s\n", FLAC__StreamDecoderStateString[temp]);
		FLAC__stream_decoder_delete(decoder);
		decoder = NULL;
		goto error_out;
	}
	if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder))
	{
		fprintf(stderr, "playflac: FLAC__seekable_stream_decoder_process_until_end_of_metadata() failed\n");
		goto error_out;
	}
#endif



	if (flac_max_blocksize<=0)
	{
		fprintf(stderr, "playflac: max blocksize not set\n");
		goto error_out;
	}


	plrSetOptions(flacrate, (PLR_SIGNEDOUT|PLR_16BIT)|PLR_STEREO);

	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);

	flacbufrate=imuldiv(65536, flacrate, plrRate);

	flacbuflen = flac_max_blocksize * 2 /* we need to be able to fit two buffers in here */ + 64 /* slack */;
	if (flacbuflen<8192)
		flacbuflen=8192;
	if (!(flacbuf=malloc(flacbuflen*sizeof(uint16_t)*2/*stereo*/)))
	{
		fprintf(stderr, "playflac: malloc() failed\n");
		goto error_out;
	}

	flacbufpos = ringbuffer_new_samples (RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_STEREO, flacbuflen);
	flacbuffpos=0;

	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize * plrRate / 1000, file))
	{
		fprintf(stderr, "playflac: plrOpenPlayer() failed\n");
		goto error_out;
	}

	if (!(buf16=malloc(sizeof(uint16_t) * buflen * 2 /* stereo */)))
	{
		fprintf(stderr, "playflac: malloc() failed\n");
		goto error_out;
	}
	bufpos=0;

	if (!pollInit(flacIdle))
	{
		fprintf(stderr, "playflac: pollInit failed\n");
		goto error_out;
	}

	return 1;

error_out:
	plrClosePlayer();
	return 0;
}

void __attribute__ ((visibility ("internal"))) flacClosePlayer(void)
{
	int i, j;

	pollClose();
	plrClosePlayer();

	if (flacbuf)
	{
		free(flacbuf);
		flacbuf=0;
	}
	if (flacbufpos)
	{
		ringbuffer_free (flacbufpos);
		flacbufpos = 0;
	}
	if (buf16)
	{
		free(buf16);
		buf16=0;
	}

	if (flacfile)
	{
		flacfile->unref (flacfile);
		flacfile = 0;
	}
	if (!decoder)
		return;
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	FLAC__seekable_stream_decoder_finish(decoder);
	FLAC__seekable_stream_decoder_delete(decoder);
#else
	FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);
#endif
	decoder = NULL;

	for (i=0; i < flac_comments_count; i++)
	{
		for (j=0; j < flac_comments[i]->value_count; j++)
		{
			free (flac_comments[i]->value[j]);
		}
		free (flac_comments[i]->title);
		free (flac_comments[i]);
	}
	free (flac_comments);
	flac_comments = 0;
	flac_comments_count = 0;

	for (i=0; i < flac_pictures_count; i++)
	{
		free (flac_pictures[i].data_bgra);
		free (flac_pictures[i].scaled_data_bgra);
		free (flac_pictures[i].description);
	}
	free (flac_pictures);
	flac_pictures = 0;
	flac_pictures_count = 0;
}

static void flacIdler(void)
{
	while (ringbuffer_get_head_available_samples (flacbufpos) >= flac_max_blocksize)
	{
		/* Seek, causes a decoding to happen */
		if (flacPendingSeek)
		{
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
			if (!FLAC__seekable_stream_decoder_seek_absolute(decoder, flacPendingSeekPos))
#else
			if (!FLAC__stream_decoder_seek_absolute(decoder, flacPendingSeekPos))
#endif
			{
				fprintf (stderr, "playflac: ERROR: Seek failed\n");
				eof_flacfile = 1;
			}
			flacPendingSeek = 0;
			continue;
		}

		/* fprintf(stderr, "we can fit more data\n"); */
		if (eof_flacfile)
		{
			/* fprintf(stderr, "eof reached\n"); */
			break;
		}
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
		if ((FLAC__seekable_stream_decoder_get_state(decoder)==FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM)||(!FLAC__seekable_stream_decoder_process_single(decoder)))
#else
		if ((FLAC__stream_decoder_get_state(decoder)==FLAC__STREAM_DECODER_END_OF_STREAM)||(!FLAC__stream_decoder_process_single(decoder)))
#endif
		{
			if (donotloop)
			{
				eof_flacfile=1;
				break;
			} else {
				flacPendingSeek = 1;
				flacPendingSeekPos = 0;
			}
		}
	}
}

void __attribute__ ((visibility ("internal"))) flacMetaDataLock(void)
{
	clipbusy++;
}

void __attribute__ ((visibility ("internal"))) flacMetaDataUnlock(void)
{
	clipbusy--;
}

void __attribute__ ((visibility ("internal"))) flacIdle(void)
{
	uint32_t bufplayed;
	uint32_t bufdelta;
	uint32_t pass2;

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	/* Where is our devp reading head? */
	bufplayed=plrGetBufPos()>>(stereo+bit16);
	bufdelta=(buflen+bufplayed-bufpos)%buflen; /* all these are in samples */

	/* No delta on the devp? */
	if (!bufdelta)
	{
		clipbusy--;
		if (plrIdle)
			plrIdle();
		return;
	}

	/* fill up our buffers */
	flacIdler();

	if (inpause)
	{
		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		if (bit16)
		{
			plrClearBuf((uint16_t *)plrbuf+(bufpos<<stereo), (bufdelta-pass2)<<stereo, !signedout);
			if (pass2)
				plrClearBuf((uint16_t *)plrbuf, pass2<<stereo, !signedout);
		} else {
			plrClearBuf(buf16, bufdelta<<stereo, !signedout);
			plr16to8((uint8_t *)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((uint8_t *)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
		}
		bufpos+=bufdelta;
		if (bufpos>=buflen)
			bufpos-=buflen;
	} else {
		int pos1, length1, pos2, length2;
		int i;
		unsigned int buf16_filled=0;

		/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
		ringbuffer_get_tail_samples (flacbufpos, &pos1, &length1, &pos2, &length2);

		if (flacbufrate==0x10000) /* 1.0... just copy into buf16 direct until we run out of target buffer or source buffer */
		{
			if (bufdelta>(length1+length2))
			{
				bufdelta=(length1+length2);
				eof_buffer=1;
			} else {
				eof_buffer=0;
			}

			while (buf16_filled<bufdelta)
			{
				uint32_t w=bufdelta-buf16_filled;

				if (!length1)
				{
					pos1 = pos2;
					length1 = length2;
					pos2 = 0;
					length2 = 0;
				}

				if (!length1)
				{
					fprintf (stderr, "playflac: ERROR, length1 == 0, in flacIdle\n");
					_exit(1);
				}

				if (w>length1)
				{
					w=length1;
				}
				memcpy(buf16+2/*stereo*/*buf16_filled, flacbuf+pos1*2/*stereo*/, w*sizeof(uint16_t)*2/*stereo*/);
				buf16_filled+=w;

				length1-=w;
				pos1+=w;

				ringbuffer_tail_consume_samples (flacbufpos, w);
			}
		} else { /* re-sample intil we don't have more target-buffer or source-buffer */
			unsigned int accumulated_progress = 0;

			eof_buffer=0;

			for (buf16_filled=0; buf16_filled<bufdelta; buf16_filled++)
			{
				int32_t c0, c1, c2, c3, ls, rs, vm1, v1, v2;
				uint32_t wpm1, wp0, wp1, wp2;
				unsigned int progress;

				/* will the interpolation overflow? */
				if ((length1+length2) <= 3)
				{
					eof_buffer=1;
					break;
				}
				/* will we overflow the flacbuf if we advance? */
				if ((length1+length2) < ((flacbufrate+flacbuffpos)>>16))
				{
					eof_buffer=1;
					break;
				}

				switch (length1)
				{
					case 1:
						wpm1 = pos1;
						wp0  = pos2;
						wp1  = pos2+1;
						wp2  = pos2+2;
						break;
					case 2:
						wpm1 = pos1;
						wp0  = pos1+1;
						wp1  = pos2;
						wp2  = pos2+1;
						break;
					case 3:
						wpm1 = pos1;
						wp0  = pos1+1;
						wp1  = pos1+2;
						wp2  = pos2;
						break;
					default:
						wpm1 = pos1;
						wp0  = pos1+1;
						wp1  = pos1+2;
						wp2  = pos1+3;
						break;
				}

				vm1= (unsigned)(flacbuf[wpm1*2])^0x8000;
				c0 = (unsigned)(flacbuf[wp0*2])^0x8000;
				v1 = (unsigned)(flacbuf[wp1*2])^0x8000;
				v2 = (unsigned)(flacbuf[wp2*2])^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3,flacbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3,flacbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3,flacbuffpos);
				ls = c3+c0;
				if (ls>65535)
					ls=65535;
				else if (ls<0)
					ls=0;

				vm1= (unsigned)(flacbuf[wpm1*2+1])^0x8000;
				c0 = (unsigned)(flacbuf[wp0*2+1])^0x8000;
				v1 = (unsigned)(flacbuf[wp1*2+1])^0x8000;
				v2 = (unsigned)(flacbuf[wp2*2+1])^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3,flacbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3,flacbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3,flacbuffpos);
				rs = c3+c0;
				if (rs>65535)
					rs=65535;
				else if(rs<0)
					rs=0;
				buf16[buf16_filled*2+0]=(uint16_t)ls^0x8000;
				buf16[buf16_filled*2+1]=(uint16_t)rs^0x8000;

				flacbuffpos+=flacbufrate;
				progress = flacbuffpos>>16;
				flacbuffpos&=0xFFFF;

				accumulated_progress += progress;

				/* did we wrap? if so, progress up to the wrapping point */
				if (progress >= length1)
				{
					progress -= length1;
					pos1 = pos2;
					length1 = length2;
					pos2 = 0;
					length2 = 0;
				}
				if (progress)
				{
					pos1 += progress;
					length1 -= progress;
				}
			}
			ringbuffer_tail_consume_samples (flacbufpos, accumulated_progress);
		}

		bufdelta=buf16_filled;

		/* when we copy out of buf16, pass the buffer-len that wraps around end-of-buffer till pass2 */
		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		bufdelta-=pass2;

		if (bit16)
		{
			if (stereo)
			{
				if (reversestereo)
				{
					int16_t *p=(int16_t *)plrbuf+2*bufpos;
					int16_t *b=(int16_t *)buf16;
					if (signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1];
							p[1]=b[0];
							p+=2;
							b+=2;
						}
						p=(int16_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1];
							p[1]=b[0];
							p+=2;
							b+=2;
						}
					} else {
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1]^0x8000;
							p[1]=b[0]^0x8000;
							p+=2;
							b+=2;
						}
						p=(int16_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1]^0x8000;
							p[1]=b[0]^0x8000;
							p+=2;
							b+=2;
						}
					}
				} else {
					int16_t *p=(int16_t *)plrbuf+2*bufpos;
					int16_t *b=(int16_t *)buf16;
					if (signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[0];
							p[1]=b[1];
							p+=2;
							b+=2;
						}
						p=(int16_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[0];
							p[1]=b[1];
							p+=2;
							b+=2;
						}
					} else {
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[0]^0x8000;
							p[1]=b[1]^0x8000;
							p+=2;
							b+=2;
						}
						p=(int16_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[0]^0x8000;
							p[1]=b[1]^0x8000;
							p+=2;
							b+=2;
						}
					}
				}
			} else {
				int16_t *p=(int16_t *)plrbuf+bufpos;
				int16_t *b=(int16_t *)buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0];
						p++;
						b++;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0];
						p++;
						b++;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0]^0x8000;
						p++;
						b++;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0]^0x8000;
						p++;
						b++;
					}
				}
			}
		} else {
			if (stereo)
			{
				if (reversestereo)
				{
					uint8_t *p=(uint8_t *)plrbuf+2*bufpos;
					uint8_t *b=(uint8_t *)buf16;
					if (signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[3];
							p[1]=b[1];
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[3];
							p[1]=b[1];
							p+=2;
							b+=4;
						}
					} else {
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[3]^0x80;
							p[1]=b[1]^0x80;
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[3]^0x80;
							p[1]=b[1]^0x80;
							p+=2;
							b+=4;
						}
					}
				} else {
					uint8_t *p=(uint8_t *)plrbuf+2*bufpos;
					uint8_t *b=(uint8_t *)buf16;
					if (signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1];
							p[1]=b[3];
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1];
							p[1]=b[3];
							p+=2;
							b+=4;
						}
					} else {
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1]^0x80;
							p[1]=b[3]^0x80;
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1]^0x80;
							p[1]=b[3]^0x80;
							p+=2;
							b+=4;
						}
					}
				}
			} else {
				uint8_t *p=(uint8_t *)plrbuf+bufpos;
				uint8_t *b=(uint8_t *)buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[1];
						p++;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[1];
						p++;
						b+=2;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[1]^0x80;
						p++;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[1]^0x80;
						p++;
						b+=2;
					}
				}
			}
		}

		bufpos+=buf16_filled;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	plrAdvanceTo(bufpos<<(stereo+bit16));

	if (plrIdle)
		plrIdle();

	clipbusy--;
}

void __attribute__ ((visibility ("internal"))) flacSetLoop(uint8_t s)
{
	donotloop=!s;
}
int __attribute__ ((visibility ("internal"))) flacIsLooped(void)
{
	return eof_buffer&&eof_flacfile;
}
void __attribute__ ((visibility ("internal"))) flacPause(int p)
{
	inpause=p;
}
void __attribute__ ((visibility ("internal"))) flacSetAmplify(uint32_t amp)
{
	//fprintf(stderr, "flacSetAmplify TODO\n");
	amplify=amp;
}
void __attribute__ ((visibility ("internal"))) flacSetSpeed(uint16_t sp)
{
	if (sp<32)
		sp=32;
	flacbufrate=imuldiv(256*sp, flacrate, plrRate);
	/*
	fprintf(stderr, "flacbufrate=0x%08x\n", flacbufrate);
	*/
}
void __attribute__ ((visibility ("internal"))) flacSetVolume(uint8_t vol_, int8_t bal_, int8_t pan_, uint8_t opt)
{
	pan=pan_;
	volr=voll=vol_*4;
	if (bal_<0)
		volr=(volr*(64+bal_))>>6;
	else
		voll=(voll*(64-bal_))>>6;
	srnd=opt;
}
void __attribute__ ((visibility ("internal"))) flacGetInfo(struct flacinfo *info)
{
	info->pos=flaclastpos;
	info->len=samples;
	info->rate=flacrate;
	info->timelen=samples/flacrate;
	info->stereo=flacstereo;
	info->bits=flacbits;
}
uint64_t __attribute__ ((visibility ("internal"))) flacGetPos(void)
{
	return (flaclastpos + samples - ringbuffer_get_tail_available_samples (flacbufpos)) % samples;
}
void __attribute__ ((visibility ("internal"))) flacSetPos(uint64_t pos)
{
	if (pos>=samples)
	{
		if (donotloop)
			pos=samples-1;
		else
			pos%=samples;
	}

	/* Seek, causes a decoding to happen, so we just flag it as pending, and let Idle perform it when buffer has space */
	flacPendingSeek = 1;
	flacPendingSeekPos = pos;
}
