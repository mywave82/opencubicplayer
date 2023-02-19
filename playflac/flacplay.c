/* OpenCP Module Player
 * copyright (c) 2008-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <FLAC/all.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "flacplay.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"

/* options */
static int flac_inpause;

static uint32_t voll,volr;
static int vol;
static int bal;
static int pan;
static int srnd;

static struct ocpfilehandle_t *flacfile = NULL;
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__SeekableStreamDecoder *decoder = 0;
#else
static FLAC__StreamDecoder *decoder = 0;
#endif
static int eof_flacfile = 0;
static int eof_buffer = 0;
static uint64_t samples;

static int samples_for_bitrate;
static int samplerate_for_bitrate;
static int bitrate;

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
static uint32_t flacRate; /* this is the target rate (devp) */

static uint64_t  flaclastpos;

static int donotloop=1;

static volatile int clipbusy=0;

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
	if (srnd) \
	{ \
		ls ^= 0xffff; \
	} \
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
	struct cpifaceSessionAPI_t *cpifaceSession = client_data;

	unsigned int i;

	int pos1, length1, pos2, length2;

	if (frame->header.number_type==FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER)
		flaclastpos=(uint64_t)(frame->header.number.frame_number) * frame->header.blocksize;
	else
		flaclastpos=frame->header.number.sample_number;

	cpifaceSession->ringbufferAPI->get_head_samples (flacbufpos, &pos1, &length1, &pos2, &length2);

	if (frame->header.blocksize > (length1+length2))
	{
		fprintf (stderr, "playflac: ERROR: frame->header.blocksize %d >= available space in ring-buffer %d + %d\n", frame->header.blocksize, length1, length2);
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	for (i=0;i<frame->header.blocksize;i++)
	{
		/* append to buffer */
		flacbuf[pos1*2+0] = make_16bit(buffer[0][i], frame->header.bits_per_sample);
		flacbuf[pos1*2+1] = make_16bit(buffer[1][i], frame->header.bits_per_sample);
		pos1++;
		if (!--length1)
		{
			pos1 = pos2;
			length1 = length2;
			pos2 = 0;
			length2 = 0;
		}
	}

	cpifaceSession->ringbufferAPI->head_add_samples (flacbufpos, frame->header.blocksize);

	samples_for_bitrate += frame->header.blocksize;
	samplerate_for_bitrate = frame->header.sample_rate;

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
	struct cpifaceSessionAPI_t *cpifaceSession = client_data;

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
				if (!cpifaceSession->console->try_open_gif (&actual_width, &actual_height, &data_bgra, metadata->data.picture.data, metadata->data.picture.data_length))
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
				if (!cpifaceSession->console->try_open_png (&actual_width, &actual_height, &data_bgra, metadata->data.picture.data, metadata->data.picture.data_length))
				{
					add_picture (actual_width, actual_height, data_bgra, (const char *)metadata->data.picture.description, metadata->data.picture.type);
				}
				break;
			}
			if ((!strcasecmp (metadata->data.picture.mime_type, "image/jpg")) || (!strcasecmp (metadata->data.picture.mime_type, "image/jpeg")))
			{
				uint16_t actual_height, actual_width;
				uint8_t *data_bgra;
				if (!cpifaceSession->console->try_open_jpeg (&actual_width, &actual_height, &data_bgra, metadata->data.picture.data, metadata->data.picture.data_length))
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

static void flacIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	while (cpifaceSession->ringbufferAPI->get_head_available_samples (flacbufpos) >= flac_max_blocksize)
	{
		uint64_t prePOS;
		uint64_t postPOS;

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
		samples_for_bitrate = 0;
		prePOS = flacfile->getpos (flacfile);
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
		postPOS = flacfile->getpos (flacfile);
		/* Due to logic above extra check is necessary on samples_for_bitrate */
		bitrate = samples_for_bitrate != 0 ? (postPOS - prePOS) * 8 * samplerate_for_bitrate / samples_for_bitrate : 0;
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

void __attribute__ ((visibility ("internal"))) flacIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (flac_inpause || (eof_buffer && eof_flacfile))
	{
		cpifaceSession->plrDevAPI->Pause (1);
	} else {
		void *targetbuf;
		unsigned int targetlength; /* in samples */

		cpifaceSession->plrDevAPI->Pause (0);

		cpifaceSession->plrDevAPI->GetBuffer (&targetbuf, &targetlength);

		if (targetlength)
		{
			int16_t *t = targetbuf;
			unsigned int accumulated_target = 0;
			unsigned int accumulated_source = 0;
			int pos1, length1, pos2, length2;

			/* fill up our buffers */
			flacIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (flacbufpos, &pos1, &length1, &pos2, &length2);

			if (flacbufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2);
					eof_buffer=1;
				} else {
					eof_buffer=0;
				}

				// limit source to not overrun target buffer
				if (length1 > targetlength)
				{
					length1 = targetlength;
					length2 = 0;
				} else if ((length1 + length2) > targetlength)
				{
					length2 = targetlength - length1;
				}

				accumulated_source = accumulated_target = length1 + length2;

				while (length1)
				{
					while (length1)
					{
						int16_t rs, ls;

						rs = flacbuf[(pos1<<1) + 0];
						ls = flacbuf[(pos1<<1) + 1];

						PANPROC;

						*(t++) = rs;
						*(t++) = ls;

						pos1++;
						length1--;

						//accumulated_target++;
					}
					length1 = length2;
					length2 = 0;
					pos1 = pos2;
					pos2 = 0;
				}
				//accumulated_source = accumulated_target;
			} else {
				/* We are going to perform cubic interpolation of rate conversion... this bit is tricky */
				eof_buffer=0;

				while (targetlength && length1)
				{
					while (targetlength && length1)
					{
						uint32_t wpm1, wp0, wp1, wp2;
						int32_t rc0, rc1, rc2, rc3, rvm1,rv1,rv2;
						int32_t lc0, lc1, lc2, lc3, lvm1,lv1,lv2;
						unsigned int progress;
						int16_t rs, ls;

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

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)flacbuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)flacbuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)flacbuf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)flacbuf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)flacbuf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)flacbuf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)flacbuf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)flacbuf[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,flacbuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,flacbuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,flacbuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,flacbuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,flacbuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,flacbuffpos);
						lc3 += lc0;
						if (lc3<0)
							lc3=0;
						if (lc3>65535)
							lc3=65535;

						rs = rc3 ^ 0x8000;
						ls = lc3 ^ 0x8000;

						PANPROC;

						*(t++) = rs;
						*(t++) = ls;

						flacbuffpos+=flacbufrate;
						progress = flacbuffpos>>16;
						flacbuffpos&=0xFFFF;
						accumulated_source+=progress;
						pos1+=progress;
						length1-=progress;
						targetlength--;

						accumulated_target++;
					} /* while (targetlength && length1) */
					length1 = length2;
					length2 = 0;
					pos1 = pos2;
					pos2 = 0;
				} /* while (targetlength && length1) */
			} /* if (flacbufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (flacbufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();

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
	flac_inpause=p;
}

static void flacSetSpeed(uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	flacbufrate=imuldiv(256*sp, flacrate, flacRate);
}

static void flacSetVolume(void)
{
	volr=voll=vol*4;
	if (bal<0)
		volr=(volr*(64+bal))>>6;
	else
		voll=(voll*(64-bal))>>6;
}

static void flacSet (int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			flacSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			flacSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			flacSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			flacSetVolume();
			break;
	}
}

static int flacGet (int ch, int opt)
{
	return 0;
}

void __attribute__ ((visibility ("internal"))) flacGetInfo(struct flacinfo *info)
{
	info->pos=flaclastpos;
	info->len=samples;
	info->rate=flacrate;
	info->timelen=samples/flacrate;
	info->stereo=flacstereo;
	info->bits=flacbits;
	snprintf (info->opt25, sizeof (info->opt25), "%s - %s", FLAC__VERSION_STRING, FLAC__VENDOR_STRING);
	snprintf (info->opt50, sizeof (info->opt50), "%s - %s", FLAC__VERSION_STRING, FLAC__VENDOR_STRING);
	info->bitrate=bitrate;
}
uint64_t __attribute__ ((visibility ("internal"))) flacGetPos (struct cpifaceSessionAPI_t *cpifaceSession)
{
	return (flaclastpos + samples - cpifaceSession->ringbufferAPI->get_tail_available_samples (flacbufpos)) % samples;
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

static void flacFreeComments (void)
{
	int i, j;

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

int __attribute__ ((visibility ("internal"))) flacOpenPlayer(struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;
	int temp;
	uint32_t flacbuflen;
	int retval;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	flacfile = file;
	flacfile->ref (flacfile);

	flac_inpause=0;
	voll=256;
	volr=256;
	bal=0;
	vol=64;
	pan=64;
	srnd=0;
	eof_flacfile=0;
	eof_buffer=0;

	flacbuf=0;
	flacbufpos = 0;
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	decoder = FLAC__seekable_stream_decoder_new();
#else
	decoder = FLAC__stream_decoder_new();
#endif
	if (!decoder)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] FLAC__seekable_stream_decoder_new() failed, out of memory?\n");
		retval = errFileRead;
		goto error_out_flacfile;
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
	FLAC__seekable_stream_decoder_set_client_data(decoder, cpifaceSession);
	FLAC__seekable_stream_decoder_set_error_callback(decoder, error_callback);
	if ((temp=FLAC__seekable_stream_decoder_init(decoder))!=FLAC__SEEKABLE_STREAM_DECODER_OK)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] FLAC__seekable_stream_decoder_init() failed, %s\n", FLAC__SeekableStreamDecoderStateString[temp]);
		retval = errFormStruc;
		goto error_out_decoder;
	}
	if (!FLAC__seekable_stream_decoder_process_until_end_of_metadata(decoder))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] FLAC__seekable_stream_decoder_process_until_end_of_metadata() failed\n");
		retval = errFormStruc;
		goto error_out_decoder;
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
	   cpifaceSession /*my_client_data*/
	)) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] FLAC__stream_decoder_init_stream() failed, %s\n", FLAC__StreamDecoderStateString[temp]);
		retval = errFormStruc;
		goto error_out_decoder;
	}
	if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] FLAC__seekable_stream_decoder_process_until_end_of_metadata() failed\n");
		retval = errFormStruc;
		goto error_out_decoder;
	}
#endif

	if (flac_max_blocksize<=0)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] max blocksize not set\n");
		retval = errFormStruc;
		goto error_out_decoder;
	}

	flacRate=flacrate;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&flacRate, &format, file, cpifaceSession))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] plrOpenPlayer() failed\n");
		retval = errPlay;
		goto error_out_decoder;
	}

	flacbufrate=imuldiv(65536, flacrate, flacRate);
	flacbuflen = flac_max_blocksize * 2 /* we need to be able to fit two buffers in here */ + 64 /* slack */;
	if (flacbuflen<8192)
		flacbuflen=8192;
	if (!(flacbuf=malloc(flacbuflen*sizeof(uint16_t)*2/*stereo*/)))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] malloc() failed\n");
		retval = errAllocMem;
		goto error_out_plrDevAPI_Start;
	}

	flacbufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_STEREO, flacbuflen);
	if (!flacbufpos)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[FLAC] ringbuffer_new_samples() failed\n");
		retval = errAllocMem;
		goto error_out_flacbuf;
	}
	flacbuffpos=0;

	cpifaceSession->mcpSet=flacSet;
	cpifaceSession->mcpGet=flacGet;

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	return errOk;

	//cpifaceSession->ringbufferAPI->free (flacbufpos);
	//flacbufpos = 0;
error_out_flacbuf:
	free (flacbuf);
	flacbuf = 0;
error_out_plrDevAPI_Start:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);
error_out_decoder:
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	FLAC__seekable_stream_decoder_finish(decoder);
	FLAC__seekable_stream_decoder_delete(decoder);
#else
	FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);
#endif
	decoder = NULL;
error_out_flacfile:
	flacfile->unref (flacfile);
	flacfile = 0;

	flacFreeComments ();

	return retval;
}

void __attribute__ ((visibility ("internal"))) flacClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->plrDevAPI)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	}

	if (flacbuf)
	{
		free(flacbuf);
		flacbuf=0;
	}
	if (flacbufpos)
	{
		cpifaceSession->ringbufferAPI->free (flacbufpos);
		flacbufpos = 0;
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

	flacFreeComments ();
}
