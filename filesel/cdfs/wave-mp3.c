/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OGG file support for CDFS images
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

#ifndef HAVE_MAD
static struct cdfs_datasource_handle_t *spawn_audiofile_handle_mp3 (struct ocpfilehandle_t *fh)
{
	fprintf (stderr, "[CUE/TOC]: Warning: MP3 support not enabled, adding audio file source failed\n");
	return 0;
}
#else

#include <mad.h>

struct cdfs_datasource_handle_mp3_t
{
	struct cdfs_datasource_handle_t h;
	int refcount;

	/* total file length */
	uint64_t total_file_length;
	uint64_t ID3_head_length;
	uint64_t frame0_length; /* if frame0 is a Xing or Info frame, it does not contain audio */
//	uint64_t ID3_foot_length; not important ALL in ALL

/* this is for seeking .... */
	uint64_t frames; // 0 = unknown Xing or Info frame will fill in if available
	uint64_t frames_scanned; /* filled */
	uint64_t frames_size; /* how large is the array, we realloc() in chunks */
	uint64_t *frameoffsets;

	struct ocpfilehandle_t *fh;

	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;

	uint64_t current_frame_in_stream;
	uint64_t current_frame_in_synth;
	uint8_t iobuffer[1045 + MAD_BUFFER_GUARD /* includes the minimum 4 bytes needed to next SYNC/LENGTH */]; /* largest 320kbps frame @ 44100Hz with padding and next sync */
	uint64_t iobuffer_offset;
	uint64_t iobuffer_fill;
};

/* Since we always will be MPEG 1.0, Layer III, SampleRate 44100, the frame length will always be 1152 */
#define MP3_FRAME_LENGTH 1152

/* MAIN_DATA stream can be reference upto 9 packets */
#define MAX_NUMBER_PACKETS_MAIN_DATA 9

static int audiofile_handle_mp3_store_framelength (struct cdfs_datasource_handle_mp3_t *ah, unsigned int framenumber, unsigned int framesize)
{
	if ((framenumber + 1) < ah->frames_scanned)
	{
		return 0;
	}

	/* we are going to know the location of the NEXT frame */
	if (ah->frames_size <= (ah->frames_scanned))
	{
		uint64_t *temp = realloc (ah->frameoffsets, sizeof (uint64_t) * (ah->frames_size + 1000));
		if (!temp)
		{
			fprintf (stderr, "audiofile_handle_mp3_store_framelength(): realloc frameoffsets failed\n");
			return -1;
		}
		ah->frameoffsets = temp;
		ah->frames_size += 1000;
	}

	ah->frameoffsets[framenumber + 1] = ah->frameoffsets[framenumber] + framesize;
	ah->frames_scanned++;
	debug_printf ("audiofile_handle_mp3_store_framelength frameoffset[%u]=%"PRIu64" \n", framenumber + 1, ah->frameoffsets[framenumber + 1]);
	return 0;
}

static inline mad_fixed_t mp3_clip(mad_fixed_t sample)
{
	enum {
		MIN = -MAD_F_ONE,
		MAX =  MAD_F_ONE - 1
	};

	if (sample > MAX)
		sample = MAX;
	else if (sample < MIN)
		sample = MIN;
	return sample;
}

static inline signed long mp3_audio_linear_round(unsigned int bits, mad_fixed_t sample)
{
	/* round */
	sample += (1L << (MAD_F_FRACBITS - bits));

	/* clip */
	sample = mp3_clip (sample);

	/* quantize and scale */
	return sample >> (MAD_F_FRACBITS + 1 - bits);
}

static void mp3_audio_pcm_s16(int16_t *data, unsigned int nsamples, mad_fixed_t const *left, mad_fixed_t const *right)
{
	unsigned int len;
	int ls, rs;

	len = nsamples;

	while (len--)
	{
		rs = mp3_audio_linear_round(16, *left++);
		ls = mp3_audio_linear_round(16, *right++);
		data[0] = rs;
		data[1] = ls;
		data += 2;
	}
}

static uint_fast16_t _mp3_get_framelength (const uint8_t *src)
{
	if (src[0] != 0xff) return 0;
	if ((src[1] & 0xe0) != 0xe0) return 0;
	unsigned int bitrateindex = src[2] >> 4;
	unsigned int padding = (src[2] >> 1) & 1;
	switch (bitrateindex)
	{
		default:
		case  0: return 0;
		case  1: return   32000 * 144 / 44100 + padding;
		case  2: return   40000 * 144 / 44100 + padding;
		case  3: return   48000 * 144 / 44100 + padding;
		case  4: return   56000 * 144 / 44100 + padding;
		case  5: return   64000 * 144 / 44100 + padding;
		case  6: return   80000 * 144 / 44100 + padding;
		case  7: return   96000 * 144 / 44100 + padding;
		case  8: return  112000 * 144 / 44100 + padding;
		case  9: return  128000 * 144 / 44100 + padding;
		case 10: return  160000 * 144 / 44100 + padding;
		case 11: return  192000 * 144 / 44100 + padding;
		case 12: return  224000 * 144 / 44100 + padding;
		case 13: return  256000 * 144 / 44100 + padding;
		case 14: return  320000 * 144 / 44100 + padding;
		case 15: return 0;
	}
}

static uint_fast16_t mp3_get_framelength (struct cdfs_datasource_handle_mp3_t *ah, uint64_t framenumber, const uint8_t *src)
{
	if (ah->iobuffer_offset != ah->frameoffsets[framenumber])
	{ /* this should hit during "fast-crawl" */
		debug_printf ("GET_FRAMELENGTH, TOP-UP, PROBABLY FAST CRAWL\n");

		ah->iobuffer_offset = ah->frameoffsets[framenumber];
		ah->fh->seek_set (ah->fh, ah->iobuffer_offset);
		ah->iobuffer_fill = ah->fh->read (ah->fh, ah->iobuffer, 4);
	} else if (ah->iobuffer_fill < 4)
	{ /* this might hit during "slow-crawl" */
		debug_printf ("GET_FRAMELENGTH, TOP-UP, PROBABLY DECODING\n");

		ah->iobuffer_fill += ah->fh->read (ah->fh, ah->iobuffer + ah->iobuffer_fill, sizeof (ah->iobuffer) - ah->iobuffer_fill);
	}

	if (ah->iobuffer_fill < 4)
	{
		fprintf (stderr, "mp3_get_framelength frame=%"PRIu64"): ran out of data\n", framenumber);
		return 0;
	}

	uint_fast16_t length = _mp3_get_framelength (src);
	if (!length)
	{
		fprintf (stderr, "mp3_get_framelength(frame=%"PRIu64"): FRAME SYNC not valid\n", framenumber);
		return 0;
	}
	if (audiofile_handle_mp3_store_framelength (ah, framenumber, length))
	{
		return 0;
	}
	return length;
}

static int audiofile_handle_mp3_read_fetch_frame_and_decode (struct cdfs_datasource_handle_mp3_t *ah, uint64_t currentframe, unsigned int framelength)
{
	int retval = -1;
	if (ah->iobuffer_fill < (framelength + MAD_BUFFER_GUARD))
	{
		ah->fh->seek_set (ah->fh, ah->iobuffer_offset + ah->iobuffer_fill);
		ah->iobuffer_fill += ah->fh->read (ah->fh, ah->iobuffer + ah->iobuffer_fill, sizeof (ah->iobuffer) - ah->iobuffer_fill);
	}
	if (ah->iobuffer_fill < framelength)
	{ /* we hit EOF, frame incomplete */
		fprintf (stderr, "audiofile_handle_mp3_read_fetch_frame_and_decode(frame=%"PRIu64"): hit EOF\n", currentframe);
		return retval;
	}

	if (ah->iobuffer_fill < (framelength + MAD_BUFFER_GUARD))
	{ /* when we hit the real EOF; but we need MAD_BUFFER_GUARD extra of data, provide dummy data */
		memset (ah->iobuffer + ah->iobuffer_fill, 0, (framelength + MAD_BUFFER_GUARD) - ah->iobuffer_fill);
		mad_stream_buffer (&ah->stream, ah->iobuffer, (framelength + MAD_BUFFER_GUARD));
	} else { /* we have the entire frame and MAD_BUFFER_GUARD */
		mad_stream_buffer (&ah->stream, ah->iobuffer, (framelength + MAD_BUFFER_GUARD));
	}

	if (mad_header_decode(&ah->frame.header, &ah->stream) == -1)
	{
		fprintf (stderr, "audiofile_handle_mp3_read_fetch_frame_and_decode(frame=%"PRIu64"): mad_header_decode() failed: %s\n", currentframe, mad_stream_errorstr (&ah->stream));
		goto error_out;
	}

	debug_printf ("     mad_frame_decode(frame=%"PRIu64")\n", currentframe);
	ah->current_frame_in_stream = currentframe;
	if (mad_frame_decode(&ah->frame, &ah->stream) == -1)
	{
		fprintf (stderr, "audiofile_handle_mp3_read_fetch_frame_and_decode(%"PRIu64"): mad_frame_decode() failed: %s\n", currentframe, mad_stream_errorstr (&ah->stream));
		retval = 1;
		goto error_out;
	}

	retval = 0;
error_out:
	memmove (ah->iobuffer, ah->iobuffer + framelength, ah->iobuffer_fill - framelength);
	ah->iobuffer_fill -= framelength;
	ah->iobuffer_offset += framelength;
	return retval;
}


static unsigned int audiofile_handle_mp3_read (struct cdfs_datasource_handle_t *_ah, uint64_t offset, uint8_t *target, unsigned int len)
{
	struct cdfs_datasource_handle_mp3_t *ah = (struct cdfs_datasource_handle_mp3_t *)_ah;
	unsigned int retval = 0;

	if ((len & 3) || (offset & 3))
	{
		memset (target, 0, len);
		return 0;
	}
	offset >>= 2; /* stereo, 16-bit */
	len >>= 2; /* stereo, 16-bit */

	uint64_t sourceframe = offset / MP3_FRAME_LENGTH;
	unsigned int sourceoffset = offset % MP3_FRAME_LENGTH;

	debug_printf("audiofile_handle_mp3_read(offset %"PRIu64", len %u)     sourceframe:%"PRIu64"   offset:%u\n", offset, len, sourceframe, sourceoffset);

	if (ah->frames) /* check for overrun */
	{
		if (sourceframe > ah->frames)
		{
			memset (target, 0, len<<2);
			return 0;
		}
	}

	unsigned int framelength;

	uint64_t currentframe; /* for crawling */
	uint64_t slow_crawl_from; /* if currentframe >= this, it must be feed into mad_frame_decode; since when we reach our target sourceframe, it might refer to "main data" upto MAX_NUMBER_PACKETS_MAIN_DATA back in time */

	if (ah->current_frame_in_synth == sourceframe)
	{ /* bypass all IF statements below... "cache" */
		debug_printf ("BYPASS, ALREADY IN SYNTH\n");
	} else if ((ah->current_frame_in_synth + 1) == sourceframe)
	{
		debug_printf ("NEXT NATURAL FRAME IN STREAM\n");

		/* please load in the next frame, normal play */
		currentframe = sourceframe;

		if (!(framelength = mp3_get_framelength (ah, currentframe, ah->iobuffer))) goto error_out;
		if (!framelength)
		{
			goto error_out;
		}

		if (audiofile_handle_mp3_read_fetch_frame_and_decode (ah, currentframe, framelength))
		{
			goto error_out;
		}

		debug_printf (" mad_synth_frame(frame=%"PRIu64")\n", sourceframe);
		mad_synth_frame(&ah->synth, &ah->frame);
		ah->current_frame_in_synth = sourceframe;
	} else if (sourceframe >= ah->frames_scanned) /* we have to crawl, learn the offsets and sizes */
	{
		debug_printf ("NEED TO CRAWL, NOT SCANNED YET\n");

		currentframe = ah->frames_scanned - 1;
/* we could evaluate the current numbers of frame in the ah->stream and the current frame in the ah->stream, and potentially gain small boost in speed if jumping only short forward, but easiest to always assume long jump */

		/* calculate from which point while crawling we have to include decoding due to "main_data".
		 *
		 * Corner-cases:
		 *   if target is just after the known sizes, we have to reverse slightly
		 *
		 *   if target is close the start of the file, we have to start at 0
		 */
		if ((sourceframe - currentframe) < MAX_NUMBER_PACKETS_MAIN_DATA)
		{ /* we need to reverse; possible from the start */
			if (sourceframe >= MAX_NUMBER_PACKETS_MAIN_DATA)
			{
				currentframe = 0;
			} else {
				currentframe = sourceframe - MAX_NUMBER_PACKETS_MAIN_DATA;
			}
			slow_crawl_from = currentframe;
		} else {
			if (sourceframe <= MAX_NUMBER_PACKETS_MAIN_DATA)
			{
				currentframe = 0;
				slow_crawl_from = 0;
			} else {
				/* no corner-case, the most normal execution path */
				slow_crawl_from = sourceframe - MAX_NUMBER_PACKETS_MAIN_DATA;
			}
		}

perform_crawl:
		debug_printf ("ENTER CRAWL LOOP: currentframe=%"PRIu64" slow_from=%"PRIu64" sourceframe=%"PRIu64"\n", currentframe, slow_crawl_from, sourceframe);

		while (currentframe <= sourceframe)
		{
			framelength = mp3_get_framelength (ah, currentframe, ah->iobuffer);
			if (!framelength)
			{
				goto error_out;
			}
			if (currentframe >= slow_crawl_from)
			{
				if (audiofile_handle_mp3_read_fetch_frame_and_decode (ah, currentframe, framelength) < 0)
				{
					goto error_out;
				}
			}
			currentframe++;
		}

		debug_printf (" mad_synth_frame(frame=%"PRIu64")\n", sourceframe);
		mad_synth_frame(&ah->synth, &ah->frame);
		ah->current_frame_in_synth = sourceframe;

	} else if (sourceframe != ah->current_frame_in_synth)
	{ /* if() check above ensures that the offset is known, if we reach here, the framesizes until this point is known */
		debug_printf("JUMP TO CORRECT FRAME\n");
		/* we have to decode MAX_NUMBER_PACKETS_MAIN_DATA before our target due to main_data stream */
		currentframe = sourceframe;
		if (currentframe <= MAX_NUMBER_PACKETS_MAIN_DATA)
		{
			currentframe = 0;
		} else {
			currentframe = sourceframe - MAX_NUMBER_PACKETS_MAIN_DATA;
		}
		slow_crawl_from = currentframe; // all frames are slow in this code-path
		goto perform_crawl;
	}

	unsigned int read = len;
	if (read > (MP3_FRAME_LENGTH - sourceoffset))
	{
		read = MP3_FRAME_LENGTH - sourceoffset;
	}
	debug_printf ("READ=%u ", read);
	mp3_audio_pcm_s16 ((void *)target, read, ah->synth.pcm.samples[0] + sourceoffset, ah->synth.pcm.samples[1] + sourceoffset);
	retval += read << 2;
	len -= read;
	debug_printf ("=> result=%u\n", len);
	if (len)
	{
		retval += audiofile_handle_mp3_read (_ah, (offset + read) << 2, target + (read << 2), len << 2);
	}
	debug_printf (" return %u\n", retval);
	return retval;

error_out:
	ah->current_frame_in_synth = ~2;
	memset (target, 0, len << 2);
	return 0;
}

static void audiofile_handle_mp3_ref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_mp3_t *ah = (struct cdfs_datasource_handle_mp3_t *)_ah;
	ah->refcount++;
}

static void audiofile_handle_mp3_unref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_mp3_t *ah = (struct cdfs_datasource_handle_mp3_t *)_ah;
	ah->refcount--;
	if (ah->refcount)
	{
		return;
	}

	mad_synth_finish (&ah->synth);
	mad_frame_finish (&ah->frame);
	mad_stream_finish (&ah->stream);

	free (ah->frameoffsets);

	ah->fh->unref (ah->fh);
	free (ah);
}

static struct cdfs_datasource_handle_t *spawn_audiofile_handle_mp3 (struct ocpfilehandle_t *fh)
{
	struct cdfs_datasource_handle_mp3_t *retval;

	uint8_t buffer[64];

	debug_printf ("spawn_audiofile_handle_mp3\n");

	fh->seek_set (fh, 0);

	retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "spawn_audiofile_handle_mp3(): calloc() failed\n");
		return 0;
	}
	retval->fh = fh;
	fh->ref (fh);

	if (fh->read (fh, buffer, sizeof(buffer)) != sizeof (buffer))
	{
		fprintf (stderr, "spawn_audiofile_handle_mp3(): read initial bytes failed\n");
		goto error_out_file;
	}
	if (!memcmp (buffer, "ID3", 3))
	{
		if ((buffer[6] & 0x80) ||
		    (buffer[7] & 0x80) ||
		    (buffer[8] & 0x80) ||
		    (buffer[9] & 0x80))
		{
			fprintf (stderr, "spawn_audiofile_handle_mp3(): ID3 tag stored MSB in size set\n");
			goto error_out_file;
		}
		retval->ID3_head_length = ((buffer[6] << 21) |
		                           (buffer[7] << 14) |
		                           (buffer[8] <<  7) |
		                            buffer[9]        ) + 10;

		fh->seek_set (fh, retval->ID3_head_length);
		if (fh->read (fh, buffer, sizeof(buffer)) != sizeof (buffer))
		{
			fprintf (stderr, "spawn_audiofile_handle_mp3(): read data after ID3v2 tag failed\n");
			goto error_out_file;
		}
	}

	int crc;
	int padding;
	if ((buffer[0] != 0xff) ||
	   ((buffer[1] & 0xe0) != 0xe0))
	{
		fprintf (stderr, "spawn_audiofile_handle_mp3(): MPEG frame sync not present\n");
		goto error_out_file;
	}
	if (((buffer[1] >> 3) & 3) != 3)
	{
		fprintf (stderr, "spawn_audiofile_handle_mp3(): MPEG not version 1\n");
		goto error_out_file;
	}
	if (((buffer[1] >> 1) & 3) != 1)
	{
		fprintf (stderr, "spawn_audiofile_handle_mp3(): MPEG not layer III\n");
		goto error_out_file;
	}
	crc = !(buffer[1] & 0x01);
	if (((buffer[2] >> 2) & 3) != 0)
	{
		fprintf (stderr, "spawn_audiofile_handle_mp3(): MPEG not 44.1kHz sample rate\n");
		goto error_out_file;
	}
	padding = (buffer[2] >> 1) & 1;
	switch (buffer[3] >> 6)
	{
		case 0: /* stereo */
		case 1: /* joint-stereo */
			break;
		case 2: /* dual mono */
			fprintf (stderr, "spawn_audiofile_handle_mp3(): file is dual mono\n");
			goto error_out_file;
		case 3: /* mono */
			fprintf (stderr, "spawn_audiofile_handle_mp3(): file is mono\n");
			goto error_out_file;
	}
	int bitrateindex = buffer[2] >> 4;
	if ((bitrateindex == 0) || (bitrateindex == 15))
	{
		fprintf (stderr, "spawn_audiofile_handle_mp3(): non-standard bitrate\n");
		goto error_out_file;
	}
	const unsigned bitrates[15] = { 0,  32000,  40000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000 };
	unsigned int bitrate = bitrates[bitrateindex];

	int i;
	for ( i = (crc ? 6 : 4); i < 36 /* location if MPEG 1.0, non-mono */; i++)
	{
		if (buffer[i] != 0) break;
	}
	if (i == 36)
	{
		if ((!memcmp (buffer + 36, "Xing", 4)) ||
		    (!memcmp (buffer + 36, "Info", 4)))
		{
			retval->frame0_length = 144 * bitrate / 44100 + padding;

			if (buffer[36+4+3] & 0x01)
			{
				retval->frames =
				(uint32_t)((((uint32_t)buffer[36+8+0])<<24)|
				           (((uint32_t)buffer[36+8+1])<<16)|
				           (((uint32_t)buffer[36+8+2])<<8)|
				           (((uint32_t)buffer[36+8+3])));
			}

			fh->seek_set (fh, retval->ID3_head_length + retval->frame0_length);
			if (fh->read (fh, buffer, sizeof(buffer)) != sizeof (buffer))
			{
				fprintf (stderr, "spawn_audiofile_handle_mp3(): read data after information frame 0 failed\n");
				goto error_out_file;
			}

			if ((buffer[0] != 0xff) ||
			   ((buffer[1] & 0xe0) != 0xe0) )
			{
				fprintf (stderr, "spawn_audiofile_handle_mp3(): frame 1, no SYNC FRAME\n");
				goto error_out_file;
			}
		}
	}

	if (retval->frames > 4000000) /* override possible broken frame0 information */
	{
		retval->frames = 0;
	}
	if (retval->frames)
	{
		retval->frames--; /* we are going to ignore frame 0 */
	}

	retval->frames_size = retval->frames ? retval->frames + 1 : 6500;
	retval->frameoffsets = malloc (sizeof (retval->frameoffsets[0]) * retval->frames_size);
	if (!retval->frameoffsets)
	{
		fprintf (stderr, "spawn_audiofile_handle_mp3(): malloc(frames) failed\n");
		goto error_out_file;
	}

	retval->frameoffsets[0] = retval->ID3_head_length + retval->frame0_length;
	retval->frames_scanned = 1;

	retval->current_frame_in_synth = ~0;
	mad_stream_init (&retval->stream);
	mad_frame_init (&retval->frame);
	mad_synth_init (&retval->synth);
	mad_stream_options (&retval->stream, MAD_OPTION_IGNORECRC);

	retval->h.ref = audiofile_handle_mp3_ref;
	retval->h.unref = audiofile_handle_mp3_unref;
	retval->h.dirdb_ref = fh->dirdb_ref;
	retval->h.length = retval->frames ? 4UL * (retval->frames * MP3_FRAME_LENGTH) : 4000000000UL;
	retval->h.read = audiofile_handle_mp3_read;

	retval->refcount = 1;

	debug_printf ("frames=%"PRIu64"\n", retval->frames);
	debug_printf ("ID3_head_length=%"PRIu64"\n", retval->ID3_head_length);
	debug_printf ("frame0_length=%"PRIu64"\n", retval->frame0_length);

	return &retval->h;

error_out_file:
	free (retval->frameoffsets);

	retval->fh->unref (retval->fh);
	free (retval);

	return 0;
}

#endif
