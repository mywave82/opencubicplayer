/* OpenCP Module Player
 * copyright (c) 2007-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Small test application for testing the FLAC API.
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

#include <stdio.h>

#include <FLAC/all.h>

static FILE *myfile = NULL;

static FLAC__SeekableStreamDecoder *decoder = 0;
static void *myvar = 0;


/* FLAC decoder needs more data */
static FLAC__SeekableStreamDecoderReadStatus read_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	FLAC__byte buffer[],
	unsigned int *bytes,
	void *client_data)
{
/*
	returns possible:
		FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
		FLAC__STREAM_DECODER_READ_STATUS_ABORT
		FLAC__STREAM_DECODER_READ_STATUS_CONTINUE
	update *bytes with data actuelly read (up to *bytes long into buffer)
*/
	int retval;

	fprintf(stderr, "read_callback\n");

	retval = fread(buffer, 1, *bytes, myfile);
	if (retval<=0)
	{
		*bytes=0;
		if (feof(myfile))
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}
	*bytes = retval;
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderWriteStatus write_callback (
	const FLAC__SeekableStreamDecoder *decoder,
	const FLAC__Frame *frame,
	const FLAC__int32 * const buffer[],
	void *client_data)
{/*
	frame can probably be stored until we decode more data
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE   is nice
	size of buffer =  frame->header.channels * frame->header.blocksize * frame->header.bits_per_sample / 8
	buffer is a 2D array with [channel][sample] as index
*/
	fprintf(stderr, "write_callback TODO\n");
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_callback(const FLAC__SeekableStreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	if (metadata->type!=FLAC__METADATA_TYPE_STREAMINFO)
	{
		fprintf(stderr, "FLAC__METADATA_TYPE_STREAMINFO is not the first header\n");
		return;
	}

	fprintf(stderr, "metadata.min_blocksize: %d\n", metadata->data.stream_info.min_blocksize);
	fprintf(stderr, "metadata.max_blocksize: %d\n", metadata->data.stream_info.max_blocksize);
	fprintf(stderr, "metadata.min_framesize: %d\n", metadata->data.stream_info.min_framesize);
	fprintf(stderr, "metadata.max_framesize: %d\n", metadata->data.stream_info.max_framesize);
	fprintf(stderr, "metadata.sample_rate: %d\n", metadata->data.stream_info.sample_rate);
	fprintf(stderr, "metadata.channels: %d\n", metadata->data.stream_info.channels);
	fprintf(stderr, "metadata.bits_per_sample: %d\n", metadata->data.stream_info.bits_per_sample);
	fprintf(stderr, "metadata.total_samples: %lld\n", metadata->data.stream_info.total_samples);

	fprintf(stderr, "metadata_callback TODO\n");
	return;
}

static FLAC__SeekableStreamDecoderSeekStatus seek_callback(const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	fprintf(stderr, "seek_callback\n");

	if (!fseek(myfile, absolute_byte_offset, SEEK_SET))
		return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
	else
		return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
}

static FLAC__SeekableStreamDecoderTellStatus tell_callback(const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	fprintf(stderr, "tell_callback\n");

	if ((*absolute_byte_offset=ftell(myfile))<0)
		return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR;
	else
		return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__SeekableStreamDecoderLengthStatus length_callback(const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	long temp = ftell(myfile);

	fprintf(stderr, "length_callback\n");

	if (temp<0)
		return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;
	if (fseek(myfile, 0, SEEK_END))
		return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;
	*stream_length = ftell(myfile);
	fseek(myfile, temp, SEEK_SET);
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool eof_callback(const FLAC__SeekableStreamDecoder *decoder, void *client_data)
{
	fprintf(stderr, "eof_callback\n");

	return feof(myfile);
}

static void error_callback(const FLAC__SeekableStreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	fprintf(stderr, "ERROR\n");
	fprintf(stderr, "%s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

void READMORE(void)
{
	if (!FLAC__seekable_stream_decoder_process_single(decoder))
	{
		fprintf(stderr, "FLAC__seekable_stream_decoder_process_single() failed?\n");
		return;
	}

	if (FLAC__seekable_stream_decoder_get_state(decoder)==FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM)
	{
		fprintf(stderr, "EOF\n");
		return;
	}
}

void DOSEEK(int SAMPLEPOS)
{
	/* REWIND = SAMPLEPOS of 0 */
	FLAC__seekable_stream_decoder_seek_absolute(decoder, SAMPLEPOS);
}

int main(int argc, char *argv[])
{
	if (argc!=2)
	{
		fprintf(stderr, "usage: %s flacfile\n", argv[0]);
		return -1;
	}
	myfile = fopen(argv[1], "ro");
	if (!myfile)
	{
		perror("fopen()");
		return -1;
	}

	decoder = FLAC__seekable_stream_decoder_new();
	if (!decoder)
	{
		fprintf(stderr, "FLAC__seekable_stream_decoder_new() failed, out of memory?\n");
		return 1;
	}

	FLAC__seekable_stream_decoder_set_md5_checking(decoder, 0);

	FLAC__seekable_stream_decoder_set_read_callback(decoder, read_callback);
	FLAC__seekable_stream_decoder_set_write_callback(decoder, write_callback);
	FLAC__seekable_stream_decoder_set_metadata_callback(decoder, metadata_callback);
	FLAC__seekable_stream_decoder_set_seek_callback(decoder, seek_callback);
	FLAC__seekable_stream_decoder_set_tell_callback(decoder, tell_callback);
	FLAC__seekable_stream_decoder_set_length_callback(decoder, length_callback);
	FLAC__seekable_stream_decoder_set_eof_callback(decoder, eof_callback);

	FLAC__seekable_stream_decoder_set_client_data(decoder, myvar);

	FLAC__seekable_stream_decoder_set_error_callback(decoder, error_callback);

	switch (FLAC__seekable_stream_decoder_init(decoder))
	{
		case FLAC__SEEKABLE_STREAM_DECODER_OK:
			fprintf(stderr, "init ok\n");
			break;
		case FLAC__SEEKABLE_STREAM_DECODER_SEEKING:
			fprintf(stderr, "seeking\n");
			break;
		case FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM:
			fprintf(stderr, "eof\n");
			break;
		case FLAC__SEEKABLE_STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
			fprintf(stderr, "malloc failed\n");
			break;
		case FLAC__SEEKABLE_STREAM_DECODER_STREAM_DECODER_ERROR:
			fprintf(stderr, "decoder error\n");
			break;
		case FLAC__SEEKABLE_STREAM_DECODER_READ_ERROR:
			fprintf(stderr, "read error\n");
			break;
		case FLAC__SEEKABLE_STREAM_DECODER_SEEK_ERROR:
			fprintf(stderr, "seek error\n");
			break;
		case FLAC__SEEKABLE_STREAM_DECODER_ALREADY_INITIALIZED:
			fprintf(stderr, "already init'ed\n");
			break;
		case FLAC__SEEKABLE_STREAM_DECODER_INVALID_CALLBACK:
			fprintf(stderr, "invalid callback\n");
			break;
		case FLAC__SEEKABLE_STREAM_DECODER_UNINITIALIZED:
			fprintf(stderr, "decoder uninted\n");
			break;
		default:
			fprintf(stderr, "init not ok\n");
			break;
	}

	FLAC__seekable_stream_decoder_process_until_end_of_metadata(decoder);

	FLAC__seekable_stream_decoder_process_single(decoder);

	fprintf(stderr, "channels=%d\n", FLAC__seekable_stream_decoder_get_channels(decoder));
	fprintf(stderr, "bits per sample=%d\n", FLAC__seekable_stream_decoder_get_bits_per_sample(decoder));

/*
	fprintf(stderr, "position=%d\n", FLAC__seekable_stream_decoder_get_decode_position(decoder));
*/


	FLAC__seekable_stream_decoder_finish(decoder);
	FLAC__seekable_stream_decoder_delete(decoder);

	fclose(myfile);

	return 0;
}
