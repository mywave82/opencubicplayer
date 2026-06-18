/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * FLAC file support for CDFS images
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

#ifndef HAVE_FLAC
static struct cdfs_datasource_handle_t *spawn_audiofile_handle_flac (struct ocpfilehandle_t *fh)
{
	fprintf (stderr, "[CUE/TOC]: Warning: FLAC support not enabled, adding audio file source failed\n");
	return 0;
}
#else

#include <FLAC/all.h>

struct cdfs_datasource_handle_flac_t
{
	struct cdfs_datasource_handle_t h;
	int refcount;

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	FLAC__SeekableStreamDecoder *decoder;
#else
	FLAC__StreamDecoder *decoder;
#endif

	struct ocpfilehandle_t *fh;
	uint64_t samples;
	unsigned int rate; /* this is the source rate */
	int channels; /* this we can ignore I think */
	unsigned int max_blocksize;
	int bits;

	uint64_t lastpos_fromlib;
	uint64_t lastfill_fromlib;
	uint64_t buffersize; /* given in stereo-samples */
	int16_t *buffer;
};

static void audiofile_handle_flac_ref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_flac_t *ah = (struct cdfs_datasource_handle_flac_t *)_ah;
	ah->refcount++;
}

static void audiofile_handle_flac_unref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_flac_t *ah = (struct cdfs_datasource_handle_flac_t *)_ah;
	ah->refcount--;
	if (ah->refcount)
	{
		return;
	}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	FLAC__seekable_stream_decoder_finish (ah->decoder);
	FLAC__seekable_stream_decoder_delete (ah->decoder);
#else
	FLAC__stream_decoder_finish (ah->decoder);
	FLAC__stream_decoder_delete (ah->decoder);
#endif
	free (ah->buffer);
	ah->fh->unref (ah->fh);
	free (ah);
}

/* FLAC decoder needs more data */
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__SeekableStreamDecoderReadStatus flac_read_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__byte buffer[],
	unsigned int *bytes,
	void *client_data)
#else
static FLAC__StreamDecoderReadStatus flac_read_callback (
	const FLAC__StreamDecoder *decoder,
	FLAC__byte buffer[],
	size_t *bytes,
	void *client_data)
#endif
{
	struct cdfs_datasource_handle_flac_t *h = (struct cdfs_datasource_handle_flac_t *)client_data;
	int retval = h->fh->read (h->fh, buffer, *bytes);
	if (retval<=0)
	{
		*bytes=0;
		if (h->fh->eof (h->fh))
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}
	*bytes = retval;
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static unsigned int audiofile_handle_flac_read (struct cdfs_datasource_handle_t *_ah, uint64_t offset, uint8_t *target, unsigned int len)
{
	struct cdfs_datasource_handle_flac_t *ah = (struct cdfs_datasource_handle_flac_t *)_ah;
	uint64_t retval = 0;

	debug_printf ("audiofile_handle_flac_read(offset %"PRIu64", len %u)\n", offset, len);

	if ((len & 3) || (offset & 3))
	{
		memset (target, 0, len);
		return 0;
	}
	offset >>= 2; /* stereo, 16-bit */
	len >>= 2; /* stereo, 16-bit */

	if ((!ah->lastfill_fromlib) ||
	    (offset < ah->lastpos_fromlib) ||
	    (offset > ah->lastpos_fromlib + ah->lastfill_fromlib))
	{
		debug_printf(" flac_seek (%"PRIu64")\n", offset);

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
		if (!FLAC__seekable_stream_decoder_seek_absolute (ah->decoder, offset))
#else
		if (!FLAC__stream_decoder_seek_absolute (ah->decoder, offset))
#endif
		{
			fprintf (stderr, "audiofile_handle_flac_read(): seek failed\n");
			memset (target, 0, len << 2);
			return 0;
		}
	}
	while (len)
	{
		if ((!ah->lastfill_fromlib) ||
		    (offset == ah->lastpos_fromlib + ah->lastfill_fromlib))
		{

			debug_printf(" flac_decode_single lastfill=%"PRIu64" offset=%"PRIu64",lastpos=%"PRIu64"+lastfill=%"PRIu64",sum=%"PRIu64"\n", ah->lastfill_fromlib, offset, ah->lastpos_fromlib, ah->lastfill_fromlib, ah->lastpos_fromlib + ah->lastfill_fromlib);

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
			if (!FLAC__seekable_stream_decoder_process_single (ah->decoder))
#else
			if (!FLAC__stream_decoder_process_single (ah->decoder))
#endif
			{
				fprintf (stderr, "audiofile_handle_flac_read(): decode failed\n");
				memset (target, 0, len << 2);
				return retval;
			}
		}

		if ((!ah->lastfill_fromlib) ||
		    (offset < ah->lastpos_fromlib) ||
		    (offset > ah->lastpos_fromlib + ah->lastfill_fromlib))
		{
			fprintf (stderr, "audiofile_handle_flac_read(): assertion lastpos_fromlib and lastfill_fromlib\n");
			memset (target, 0, len << 2);
			return retval;
		}
		int get = len;
		if ((offset + get) > (ah->lastpos_fromlib + ah->lastfill_fromlib))
		{
			get = (ah->lastpos_fromlib + ah->lastfill_fromlib) - offset;
		}
		memcpy (target, ah->buffer + ((offset - ah->lastpos_fromlib) << 1), get << 2);
		target += get << 2;
		retval += get << 2;
		len -= get;
		offset += get;
	}

	return retval;
}


#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static void flac_metadata_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	const FLAC__StreamMetadata *metadata,
	void *client_data)
#else
static void flac_metadata_callback (
	const FLAC__StreamDecoder *decoder,
	const FLAC__StreamMetadata *metadata,
	void *client_data)
#endif
{
	debug_printf(" METADATA type=%d\n", (int)metadata->type);

	struct cdfs_datasource_handle_flac_t *h = (struct cdfs_datasource_handle_flac_t *)client_data;

	switch (metadata->type)
	{
		case FLAC__METADATA_TYPE_STREAMINFO:
		{
			h->rate           = metadata->data.stream_info.sample_rate;
			h->channels       = metadata->data.stream_info.channels;
			h->bits           = metadata->data.stream_info.bits_per_sample;
			h->max_blocksize  = metadata->data.stream_info.max_blocksize;
			h->samples        = metadata->data.stream_info.total_samples;
			break;
		}

		default:
			break;
	}
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__SeekableStreamDecoderSeekStatus flac_seek_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__uint64 absolute_byte_offset,
	void *client_data)
#else
static FLAC__StreamDecoderSeekStatus flac_seek_callback (
	const FLAC__StreamDecoder *decoder,
	FLAC__uint64 absolute_byte_offset,
	void *client_data)
#endif
{
	struct cdfs_datasource_handle_flac_t *h = (struct cdfs_datasource_handle_flac_t *)client_data;

	debug_printf (" SEEK offset=%"PRIu64"\n", absolute_byte_offset);

	if (h->fh->seek_set (h->fh, absolute_byte_offset) == 0)
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
static FLAC__SeekableStreamDecoderTellStatus flac_tell_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__uint64 *absolute_byte_offset,
	void *client_data)
#else
static FLAC__StreamDecoderTellStatus flac_tell_callback (
	const FLAC__StreamDecoder *decoder,
	FLAC__uint64 *absolute_byte_offset,
	void *client_data)
#endif
{
	struct cdfs_datasource_handle_flac_t *h = (struct cdfs_datasource_handle_flac_t *)client_data;

	*absolute_byte_offset = h->fh->getpos (h->fh);

	debug_printf (" TELL =>> %"PRIu64"\n", *absolute_byte_offset);
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
#else
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
#endif
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__SeekableStreamDecoderLengthStatus flac_length_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__uint64 *stream_length,
	void *client_data)
#else
static FLAC__StreamDecoderLengthStatus flac_length_callback (
	const FLAC__StreamDecoder *decoder,
	FLAC__uint64 *stream_length,
	void *client_data)
#endif
{
	struct cdfs_datasource_handle_flac_t *h = (struct cdfs_datasource_handle_flac_t *)client_data;
	uint64_t temp;

	temp = h->fh->filesize (h->fh);
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

	debug_printf(" LENGTH => %"PRIu64"\n", temp);

	*stream_length = temp;
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
#else
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
#endif
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__bool flac_eof_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	void *client_data)
#else
static FLAC__bool flac_eof_callback (
	const FLAC__StreamDecoder *decoder,
	void *client_data)
#endif
{
	struct cdfs_datasource_handle_flac_t *h = (struct cdfs_datasource_handle_flac_t *)client_data;

	debug_printf(" EOF => %d\n", (int)h->fh->eof (h->fh));

	return h->fh->eof (h->fh);
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static void flac_error_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__StreamDecoderErrorStatus status,
	void *client_data)
#else
static void flac_error_callback (
	const FLAC__StreamDecoder *decoder,
	FLAC__StreamDecoderErrorStatus status,
	void *client_data)
#endif
{
	fprintf (stderr, "cdfs: ERROR libflac: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

static inline int16_t make_16bit (const FLAC__int32 src, int bps)
{
	if (bps==16)
		return src;
	else if (bps>16)
		return src>>(bps-16);
	else
		return src<<(16-bps);
}

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
static FLAC__StreamDecoderWriteStatus flac_write_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	const FLAC__Frame *frame,
	const FLAC__int32 * const buffer[],
	void *client_data)
#else
static FLAC__StreamDecoderWriteStatus flac_write_callback (
	const FLAC__StreamDecoder *decoder,
	const FLAC__Frame *frame,
	const FLAC__int32 * const buffer[],
	void *client_data)
#endif
{
	debug_printf(" WRITE blocksize=%u sample_number=%"PRIu64"\n", (unsigned)frame->header.blocksize, (uint64_t)frame->header.number.sample_number);

	struct cdfs_datasource_handle_flac_t *h = (struct cdfs_datasource_handle_flac_t *)client_data;

	unsigned int i;

	if (frame->header.number_type==FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER)
		h->lastpos_fromlib = (uint64_t)(frame->header.number.frame_number) * frame->header.blocksize;
	else
		h->lastpos_fromlib = frame->header.number.sample_number;


	if (!frame->header.blocksize)
	{
		fprintf (stderr, "flac_write_callback(): blocksize=0\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}

	if (h->buffersize < frame->header.blocksize)
	{
		fprintf (stderr, "flac_write_callback(): blocksize > max_blocksize\n");
		int16_t *temp = realloc (h->buffer, frame->header.blocksize * 4 /* stereo + 16bit */);
		if (!temp)
		{
			fprintf (stderr, "flac_write_callback(): realloc() failed\n");
			h->lastfill_fromlib = h->buffersize;
			memset (h->buffer, 0, h->buffersize << 2);
			return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
		}
		h->buffer = temp;
		h->buffersize = frame->header.blocksize;
	}

	for (i=0;i<frame->header.blocksize;i++)
	{
		h->buffer[(i<<1) + 0] = make_16bit (buffer[0][i], frame->header.bits_per_sample);
		h->buffer[(i<<1) + 1] = make_16bit (buffer[1][i], frame->header.bits_per_sample);
	}

	h->lastfill_fromlib = frame->header.blocksize;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static struct cdfs_datasource_handle_t *spawn_audiofile_handle_flac (struct ocpfilehandle_t *fh)
{
	struct cdfs_datasource_handle_flac_t *retval;
	int temp;

	debug_printf ("spawn_audiofile_handle_flac\n");

	fh->seek_set (fh, 0);

	retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): calloc() failed\n");
		return 0;
	}
	retval->fh = fh;
	fh->ref (fh);

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	retval->decoder = FLAC__seekable_stream_decoder_new ();
	if (!retval->decoder)
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): FLAC__seekable_stream_decoder_new() failed\n");
		goto error_out_file;
	}
#else
	retval->decoder = FLAC__stream_decoder_new ();
	if (!retval->decoder)
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): FLAC__stream_decoder_new() failed\n");
		goto error_out_file;
	}
#endif

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	FLAC__seekable_stream_decoder_set_md5_checking (retval->decoder, 0);

	FLAC__seekable_stream_decoder_set_read_callback (retval->decoder, flac_read_callback);
	FLAC__seekable_stream_decoder_set_write_callback (retval->decoder, flac_write_callback);
	FLAC__seekable_stream_decoder_set_metadata_callback (retval->decoder, flac_metadata_callback);
	FLAC__seekable_stream_decoder_set_seek_callback (retval->decoder, flac_seek_callback);
	FLAC__seekable_stream_decoder_set_tell_callback (retval->decoder, flac_tell_callback);
	FLAC__seekable_stream_decoder_set_length_callback (retval->decoder, flac_length_callback);
	FLAC__seekable_stream_decoder_set_eof_callback (retval->decoder, flac_eof_callback);
	FLAC__seekable_stream_decoder_set_client_data (retval->decoder, retval);
	FLAC__seekable_stream_decoder_set_error_callback (retval->decoder, error_callback);
	if ((temp=FLAC__seekable_stream_decoder_init (retval->decoder))!=FLAC__SEEKABLE_STREAM_DECODER_OK)
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): FLAC__seekable_stream_decoder_init() failed: %s\n", FLAC__SeekableStreamDecoderStateString[temp]);

		goto error_out_decoder;
	}
	if (!FLAC__seekable_stream_decoder_process_until_end_of_metadata(decoder))
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): FLAC__seekable_stream_decoder_process_until_end_of_metadata() failed\n");
		goto error_out_decoder;
	}
#else
	FLAC__stream_decoder_set_md5_checking (retval->decoder, true);
	if ((temp=FLAC__stream_decoder_init_stream (
	   retval->decoder,
	   flac_read_callback,
	   flac_seek_callback,
	   flac_tell_callback,
	   flac_length_callback,
	   flac_eof_callback,
	   flac_write_callback,
	   flac_metadata_callback,
	   flac_error_callback,
           retval
	)) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): FLAC__stream_decoder_init_stream() failed: %s\n", FLAC__StreamDecoderStateString[temp]);
		goto error_out_decoder;
	}
	if (!FLAC__stream_decoder_process_until_end_of_metadata (retval->decoder))
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): FLAC__stream_decoder_process_until_end_of_metadata() failed\n");
		goto error_out_decoder;
	}
#endif

	if (retval->max_blocksize<=0)
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): max blocksize not set\n");
		goto error_out_decoder;
	}

	if (retval->channels != 2)
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): channels != 2\n");
		goto error_out_decoder;
	}

	if (retval->rate != 44100)
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): rate != 44100\n");
		goto error_out_decoder;
	}

	retval->buffersize = retval->max_blocksize;
	retval->buffer = malloc (retval->max_blocksize << (1 /* stereo */ + 1 /* 16-bit */));
	if (!retval->buffer)
	{
		fprintf (stderr, "spawn_audiofile_handle_flac(): malloc() dumping buffer failed\n");
		goto error_out_decoder;
	}

	retval->h.ref = audiofile_handle_flac_ref;
	retval->h.unref = audiofile_handle_flac_unref;
	retval->h.dirdb_ref = fh->dirdb_ref;
	retval->h.length = retval->samples << (1 /* stereo */ + 1 /* 16-bit */);
	retval->h.read = audiofile_handle_flac_read;

	retval->refcount = 1;

	return &retval->h;

error_out_decoder:
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	FLAC__seekable_stream_decoder_finish (retval->decoder);
	FLAC__seekable_stream_decoder_delete (retval->decoder);
#else
	FLAC__stream_decoder_finish (retval->decoder);
	FLAC__stream_decoder_delete (retval->decoder);
#endif
error_out_file:
	retval->fh->unref (retval->fh);
	free (retval);

	return 0;
}
#endif
