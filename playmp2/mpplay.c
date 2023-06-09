/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * MPPlay - Player for MPEG Audio Layer 1/2/3 files
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
#include <assert.h>
#include <fcntl.h>
#include <mad.h>
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
#include "id3.h"
#include "mpplay.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#ifdef PLAYMP2_DEBUG
#define debug_printf_load(...) cpifaceSession->cpiDebug (cpifaceSession, __VA_ARGS__)
#define debug_printf_stream(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf_load(format,args...) ((void)0)
#define debug_printf_stream(format,args...) ((void)0)
#endif

/* options */
static int mpeg_looped;

static unsigned int voll,volr;
static int vol;
static int bal;
static int pan;
static int srnd;

/* the synth */
static struct mad_stream stream;
static struct mad_frame frame;
static struct mad_synth synth;
static unsigned int data_in_synth;
OCP_INTERNAL int mpeg_Bitrate; /* bitrate of the last decoded frame */
static int mpegrate; /* samp-rate in the mpeg */
static int mpegstereo;
static unsigned char *GuardPtr;

static uint32_t mpegRate; /* devp rate */
/* Are resourses in-use (needs to be freed at Close) ?*/
static int active=0;

/* file-buffer */
#define MPEG_BUFSZ    40000   /* 2.5 s at 128 kbps; 1 s at 320 kbps */
static unsigned char data[MPEG_BUFSZ];
static int data_length;
static int mpeg_eof;
static int opt25_50;
static char opt25[26];
static char opt50[51];

/* stats about the file-buffer */
static struct ocpfilehandle_t *file;
static uint64_t ofs;
static uint64_t fl;
static uint64_t datapos;
static uint64_t newpos;

static int donotloop=1;

/* mpegIdler dumping locations */
static int16_t *mpegbuf = 0;     /* the buffer */
static struct ringbuffer_t *mpegbufpos = 0;
static uint32_t mpegbuffpos; /* read fine-pos.. when mpegbufrate has a fraction */
static uint32_t mpegbufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */

/* clipper threadlock since we use a timer-signal */
static volatile int clipbusy;
#warning Remove mpeg_inSIGINT .... ??
static volatile int mpeg_inSIGINT;
 /* if (mpeg_inSIGINT), then data is updated into HoldingTag only
  *
  * When we try to fetch, we increase clipbusy, if mpeg_inSIGINT is not set, we transfer from HoldingTag into CurrentTag
  *
  * Then we use data from CurrentTag
  */
static struct ID3_t CurrentTag;
static struct ID3_t HoldingTag;
static int newHoldingTag;

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


static inline mad_fixed_t clip(mad_fixed_t sample)
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

static inline signed long audio_linear_round(unsigned int bits, mad_fixed_t sample)
{
	/* round */
	sample += (1L << (MAD_F_FRACBITS - bits));

	/* clip */
	sample=clip(sample);

	/* quantize and scale */
	return sample >> (MAD_F_FRACBITS + 1 - bits);
}

static void audio_pcm_s16(int16_t *data, unsigned int nsamples, mad_fixed_t const *left, mad_fixed_t const *right)
{
	unsigned int len;
	int ls, rs;

	len = nsamples;

	if (right) /* stereo */
		while (len--)
		{
			rs = audio_linear_round(16, *left++);
			ls = audio_linear_round(16, *right++);
			data[0] = rs;
			data[1] = ls;
			data += 2;
		}
	else /* mono */
		while (len--)
		{
			rs = ls = audio_linear_round(16, *left++);
			data[0] = rs;
			data[1] = ls;
			data += 2;

		}
}

OCP_INTERNAL void mpegGetID3 (struct ID3_t **ID3)
{
	clipbusy++;

	if (!mpeg_inSIGINT) // This should never happen when clipbusy is increased
	{
		if (newHoldingTag)
		{
			ID3_clear(&CurrentTag);
			CurrentTag = HoldingTag;
			memset (&HoldingTag, 0, sizeof (HoldingTag));
			newHoldingTag = 0;
		}
	}

	*ID3 = &CurrentTag;

	clipbusy--;
}


static inline uint_fast32_t id3_tag_query(const unsigned char *data, uint_fast32_t length)
{
	uint_fast32_t i;

	if (length>=3)
	{
		if ((data[0]=='T')&&
		    (data[1]=='A')&&
		    (data[2]=='G'))
		{
			debug_printf_stream ("[MPx] we probably have a TAG\n");
			return 128;
		}
		if ((data[0]=='E')&&
		    (data[1]=='X')&&
		    (data[2]=='T'))
		{
			debug_printf_stream ("[MPx] we probably have a EXT\n");
			return 128;
		}
	}
	if (length>=10)
	{
		if ((data[0]=='I')&&
		    (data[1]=='D')&&
		    (data[2]=='3')&&
		    (data[3]!=0xff)&&
		    (data[4]!=0xff))
		{
			uint_fast32_t size = (data[6]<<21)|(data[7]<<14)|(data[8]<<7)|(data[9]);
			debug_printf_stream ("[MPx] we probably have a ID3\n");
			return size+10; /* size of header */
		}
		if ((data[0]=='3')&&
		    (data[1]=='D')&&
		    (data[2]=='I')&&
		    (data[3]!=0xff)&&
		    (data[4]!=0xff))
		{
			debug_printf_stream ("[MPx] we probably have a FOOTER\n");
			return 10;
		}
	}

	for (i=0; (data[i] == 0x00) && (i < length); i++); /* all zero bytes we can skip, they are probably padding */

	debug_printf_stream ("[MPx] We have %d bytes of zero (%02x len=%d)\n", (int)i, data[0], (int)length);

	return i; /* if no zero bytes were found, we can not tell */
}

static uint8_t *id3_tag_buffer;
static uint32_t id3_tag_target;
static uint32_t id3_tag_position;

static void id3_tag_init (uint32_t len)
{
	free (id3_tag_buffer); id3_tag_buffer = 0;
	id3_tag_target = 0;
	id3_tag_position = 0;
	if (len < 32*1024*1024)
	{
		id3_tag_buffer = malloc (len);
		if (id3_tag_buffer)
		{
			id3_tag_target = len;
		}
	}
}

static void apply_id3 (struct ID3_t *ID3)
{
	debug_printf_stream ("[MPx] Got a tag to apply\n");
	if (mpeg_inSIGINT)
	{
		ID3_clear (&HoldingTag);
		HoldingTag = *ID3;
		newHoldingTag = 0;
	} else {
		ID3_clear (&CurrentTag);
		CurrentTag = *ID3;
	}
}

static void got_id3v1x(uint8_t *buffer)
{
	struct ID3v1data_t data;
	struct ID3_t ID3 = {0};

	debug_printf_stream ("[MPx] Trying to parse ID3v1.x\n");

	if (parse_ID3v1x(&data, buffer, 128)) return;
	if (finalize_ID3v1(&ID3, &data)) return;

	debug_printf_stream ("[MPx] Parsing successfull\n");

	apply_id3 (&ID3);
}

static void got_id3v12(uint8_t *buffer)
{
	struct ID3v1data_t data;
	struct ID3_t ID3 = {0};

	debug_printf_stream ("[MPx] Trying to parse ID3v1.x + ID3v1.2\n");

	if (parse_ID3v1x(&data, buffer+128, 128)) return;
	if (parse_ID3v12(&data, buffer, 128)) return;
	if (finalize_ID3v1(&ID3, &data)) return;

	debug_printf_stream ("[MPx] Parsing successfull\n");

	apply_id3 (&ID3);
}

static void got_id3v2(uint8_t *buffer, uint32_t length)
{
	struct ID3_t ID3 = {0};

	debug_printf_stream ("[MPx] Trying to parse ID3v2.x\n");

	if (parse_ID3v2x (&ID3, buffer, length)) return;

	debug_printf_stream ("[MPx] Parsing successfull\n");

	apply_id3 (&ID3);
}

static void id3_parse (void)
{
	if ((id3_tag_target == 128) &&
	    (id3_tag_buffer[0] == 'T') &&
	    (id3_tag_buffer[1] == 'A') &&
	    (id3_tag_buffer[2] == 'G'))
	{
		got_id3v1x(id3_tag_buffer);
	} else if ((id3_tag_buffer[0] == 'I') &&
		   (id3_tag_buffer[1] == 'D') &&
		   (id3_tag_buffer[2] == '3'))
	{
		got_id3v2(id3_tag_buffer, id3_tag_target);
	}

	free (id3_tag_buffer); id3_tag_buffer = 0;
	id3_tag_target = 0;
	id3_tag_position = 0;
}

static void id3_feed_null (void)
{
	if (id3_tag_target)
	{
		memset (id3_tag_buffer + id3_tag_position, 0, id3_tag_target - id3_tag_position);
		id3_parse();
	}
}

static void id3_feed (const uint8_t *data, uint32_t len)
{
	debug_printf_stream ("[MPx] id3_feed id3_tag_target=%"PRId32" len=%"PRId32"\n", id3_tag_target, len);
	if (id3_tag_target)
	{
		debug_printf_stream ("[MPx] id3_tag_target=%d id3_tag_position=%d len=%d\n", id3_tag_target, id3_tag_position, len);
		if (len > (id3_tag_target - id3_tag_position))
		{
			len = id3_tag_target - id3_tag_position;
		}
		debug_printf_stream ("[MPx]   memcpy len=%d\n", len);
		memcpy (id3_tag_buffer + id3_tag_position, data, len);
		id3_tag_position += len;
		if (id3_tag_position == id3_tag_target)
		{
			id3_parse();
		}
	}
}

static int stream_for_frame(void)
{
	if (data_in_synth)
		return 1;
	if (datapos!=newpos) /* force buffer flush */
	{
		debug_printf_stream ("[MPx] forcing buffer flush\n");
		datapos=newpos;
		file->seek_set (file, datapos + ofs);
		data_length=0;
		stream.next_frame=0;
		stream.error=MAD_ERROR_BUFLEN;
		GuardPtr=0;
	}

	if (GuardPtr&&(stream.this_frame==GuardPtr)) /* last frame is incomplete */
	{
		debug_printf_stream ("[MPx] EOF-KNOCKED\n");
		if (donotloop)
		{
			return 0;
		}
	}
	while(1)
	{
		if(stream.buffer==NULL || stream.error==MAD_ERROR_BUFLEN)
		{
			uint32_t len, target;

			debug_printf_stream ("[MPx] buffer==NULL || stream.error==MAD_ERROR_BUFLEN  (%p==NULL || %d==%d)\n", stream.buffer, stream.error, MAD_ERROR_BUFLEN);

			if (stream.next_frame)
			{
				debug_printf_stream ("[MPx] stream.next_frame!=NULL (%p, remove %ld of bytes from buffer and GuardPtr)\n", stream.next_frame, stream.next_frame - data);

				if (GuardPtr)
				{
					GuardPtr-=stream.next_frame - data;
				}
				memmove(data, stream.next_frame, data_length = ((data + data_length) - stream.next_frame));
				stream.next_frame=0;
				debug_printf_stream ("[MPx]   keeped some data   data=%p datalen=0x%08x (%p) GuardPtr=%p 0x%08"PRIx64"/0x%08"PRIx64"\n", data, data_length, data + data_length, GuardPtr, datapos, fl);
			}
			target = MPEG_BUFSZ - data_length;
			if (target>65536)
				target=65536;
			if (target>(fl-datapos))
				target=fl-datapos;
			if (target)
			{
				len = file->read (file, data + data_length, target);
			} else
				len = 0;
			debug_printf_stream ("[MPx] wanted to read %"PRIu32" bytes, and got %"PRIu32" bytes\n", target, len);

			if (!len)
			{
				len=0;
				if ( file->eof(file) ||!target) /* !target is the estimated EOF, feof(file) in-case file has shrunk... */
				{
					if (donotloop||target) /* if target was set, we must have had an error */
					{
						debug_printf_stream ("[MPx] eof found, and we are not looping\n");
						mpeg_eof=1;
						GuardPtr=data + data_length;
						assert(MPEG_BUFSZ - data_length >= MAD_BUFFER_GUARD);
						while (len < MAD_BUFFER_GUARD)
						{
							debug_printf_stream ("[MPx]    adding NIL byte, len < MAD_BUFFER_GUARD\n");
							data[data_length + len++] = 0;
						}

						debug_printf_stream ("[MPx]   this is the new data   data=%p datalen=0x%08x (%p) GuardPtr=%p 0x%08"PRIx64"/0x%08"PRIx64" (ofs=%"PRIu64" len=%d target=%ld)\n", data, data_length, data + data_length, GuardPtr, datapos, fl, ofs, len, (long int)target);
					} else {
						mpeg_eof=0;
						datapos = newpos = 0;
						file->seek_set (file, ofs);
						if (stream.skiplen)
						{
							id3_feed_null();
							stream.skiplen = 0;
						}

						debug_printf_stream ("[MPx] eof found, and we are looping   data=%p datalen=0x%08x (%p) GuardPtr=%p 0x%08"PRIx64"/0x%08"PRIx64"\n", data, data_length, data + data_length, GuardPtr, datapos, fl);
						return 0;
					}
				}
			} else {
				GuardPtr=0;
				datapos += len;
				newpos += len;

				debug_printf_stream ("[MPx]   some data is prepared   data=%p datalen=0x%08x (%p) GuardPtr=%p 0x%08"PRIx64"/0x%08"PRIx64"\n", data, data_length, data + data_length, GuardPtr, datapos, fl);
			}
			if (len)
			{
				mad_stream_buffer(&stream, data, data_length += len);
				debug_printf_stream ("[MPx] POST mad_stream_buffer   data=%p datalen=0x%08x (%p) GuardPtr=%p 0x%08"PRIx64"/0x%08"PRIx64"\n", data, data_length, data + data_length, GuardPtr, datapos, fl);
			}
		}
		stream.error=0;

		if (stream.skiplen)
		{
			unsigned long skiplen = stream.skiplen;
			if (skiplen > (stream.bufend - stream.this_frame))
			{
				skiplen = stream.bufend - stream.this_frame;
			}
			id3_feed (stream.buffer, skiplen);
			stream.skiplen    -= skiplen;
			stream.buffer     += skiplen;
			stream.this_frame += skiplen;
			stream.next_frame = stream.this_frame;
			stream.error       = MAD_ERROR_BUFLEN;
			continue;
		}

		/* decode data */
		{
			int tagsize = id3_tag_query(stream.this_frame, stream.bufend - stream.this_frame);
			if (tagsize>0)
			{
				id3_tag_init (tagsize);
				mad_stream_skip(&stream, tagsize);
				continue;
			}
		}
		debug_printf_stream ("[MPx] about to call mad_header_decode()\n");
		if (mad_header_decode(&frame.header, &stream) == -1)
		{
			debug_printf_stream ("[MPx] mad_header_decode() failed: %s\n", mad_stream_errorstr(&stream));
			goto error;
		}
		debug_printf_stream ("[MPx] header samplerate=%d bitrate=%ld\n", frame.header.samplerate, frame.header.bitrate);
		debug_printf_stream ("[MPx] about to call mad_frame_decode()\n");

		if (mad_frame_decode(&frame, &stream) == -1)
		{
			debug_printf_stream ("[MPx] mad_frame_decode() failed: %s\n", mad_stream_errorstr(&stream));
error:
			if (stream.error==MAD_ERROR_BUFLEN)
			{
				if (mpeg_eof)
					return 0;
				else
					continue;
			}
			if (!MAD_RECOVERABLE(stream.error))
			{
				return 0;
			}
			if (stream.error==MAD_ERROR_BADCRC)
			{
				mad_frame_mute(&frame);
				continue;
			} else if (stream.error==MAD_ERROR_LOSTSYNC)
			{
				int tagsize;
				debug_printf_stream ("[MPx] this_frame=%p buffer=%p bufend=%p, GuardPtr=%p\n", stream.this_frame, stream.buffer, stream.bufend, GuardPtr);
				if (stream.this_frame==GuardPtr)
					return 0;
				tagsize = id3_tag_query(stream.this_frame, stream.bufend - stream.this_frame);
				if (tagsize>0)
				{
					id3_tag_init (tagsize);
					mad_stream_skip(&stream, tagsize);
					continue;
				} else if ((stream.bufend - stream.this_frame) > 10) /* close to the end of the file */
				{
					mad_stream_skip(&stream, tagsize);
					continue;
				} else if (mpeg_eof)
				{
					return 0;
				} else {
					mad_stream_skip(&stream, 1); /* skip atleast one byte */
					continue;
				}
			}

			continue;
		}
		if (!opt25_50)
		{
			debug_printf_stream ("[MPx] MPEG 2 layer %s, %s%s\n",
				(frame.header.layer==MAD_LAYER_I)?"I":(frame.header.layer==MAD_LAYER_I)?"II":"III",
				(frame.header.mode==MAD_MODE_SINGLE_CHANNEL)?"mono":(frame.header.mode==MAD_MODE_DUAL_CHANNEL)?"Dual Channel":(frame.header.mode==MAD_MODE_JOINT_STEREO)?"Joint Stereo":"Stereo",
				(frame.header.emphasis==MAD_EMPHASIS_NONE)?"":(frame.header.emphasis==MAD_EMPHASIS_50_15_US)?", 50/15us emphasis":(frame.header.emphasis==MAD_EMPHASIS_CCITT_J_17)?", CCITT J.17 emph":", unknown emphasis");
			opt25_50=1;
			snprintf (opt25, sizeof (opt25), "MPEG 2 layer %s, %s",
				(frame.header.layer==MAD_LAYER_I)?"I":(frame.header.layer==MAD_LAYER_I)?"II":"III",
				(frame.header.mode==MAD_MODE_SINGLE_CHANNEL)?"mono":"stereo");

			snprintf (opt50, sizeof (opt50), "MPEG 2 layer %s, %s%s",
				(frame.header.layer==MAD_LAYER_I)?"I":(frame.header.layer==MAD_LAYER_I)?"II":"III",
				(frame.header.mode==MAD_MODE_SINGLE_CHANNEL)?"mono":(frame.header.mode==MAD_MODE_DUAL_CHANNEL)?"Dual Channel":(frame.header.mode==MAD_MODE_JOINT_STEREO)?"Joint Stereo":"Stereo",
				(frame.header.emphasis==MAD_EMPHASIS_NONE)?"":(frame.header.emphasis==MAD_EMPHASIS_50_15_US)?", 50/15us emphasis":(frame.header.emphasis==MAD_EMPHASIS_CCITT_J_17)?", CCITT J.17 emph":", unknown emphasis");
		}
		mad_synth_frame(&synth, &frame);
		debug_printf_stream ("[MPx] synth pcm.length=%d pcm.samplerate=%d pcm.channels=%d\n", synth.pcm.length, synth.pcm.samplerate, synth.pcm.channels);
		data_in_synth=synth.pcm.length;
		mpeg_Bitrate=frame.header.bitrate;
		mpegstereo=synth.pcm.channels==2;
		return 1;
	}
}

static void mpegIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (!active)
		return;

	while (1)
	{
		int pos1, pos2;
		int length1, length2;

		size_t read;

		cpifaceSession->ringbufferAPI->get_head_samples (mpegbufpos, &pos1, &length1, &pos2, &length2);

		if (!length1)
		{
			return;
		}
		read = length1;

		mpeg_looped |= 1;

		if (!data_in_synth)
		{
			if (!stream_for_frame())
			{
				break;
			}
		}

		if (!mpeg_eof)
		{
			mpeg_looped &= ~1;
		}

		if (read>data_in_synth)
		{
			read=data_in_synth;
		}

		if (synth.pcm.channels==1) /* use channel 0 twice */
			audio_pcm_s16(mpegbuf+(pos1<<1), read, synth.pcm.samples[0]+synth.pcm.length-data_in_synth, synth.pcm.samples[0]+synth.pcm.length-data_in_synth);
		else /* use channel 0 and 1, even if we maybe have surround */
			audio_pcm_s16(mpegbuf+(pos1<<1), read, synth.pcm.samples[0]+synth.pcm.length-data_in_synth, synth.pcm.samples[1]+synth.pcm.length-data_in_synth);
		cpifaceSession->ringbufferAPI->head_add_samples (mpegbufpos, read);
		data_in_synth-=read;
	}
}

OCP_INTERNAL void mpegIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (cpifaceSession->InPause || (mpeg_looped == 3))
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
			mpeg_inSIGINT++;
			mpegIdler (cpifaceSession);
			mpeg_inSIGINT--;

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (mpegbufpos, &pos1, &length1, &pos2, &length2);

			if (mpegbufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2); // limiting targetlength here, saves us from doing this per sample later
					mpeg_looped |= 2;
				} else {
					mpeg_looped &= ~2;
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

						rs = mpegbuf[(pos1<<1) + 0];
						ls = mpegbuf[(pos1<<1) + 1];

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
				mpeg_looped &= ~2;

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
							mpeg_looped |= 2;
							break;
						}
						/* will we overflow the mpegbuf if we advance? */
						if ((length1+length2) < ((mpegbufrate+mpegbuffpos)>>16))
						{
							mpeg_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)mpegbuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)mpegbuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)mpegbuf[(wp0<<1)+0]^0x8000;
						 lc0 = (uint16_t)mpegbuf[(wp0<<1)+1]^0x8000;
						 rv1 = (uint16_t)mpegbuf[(wp1<<1)+0]^0x8000;
						 lv1 = (uint16_t)mpegbuf[(wp1<<1)+1]^0x8000;
						 rv2 = (uint16_t)mpegbuf[(wp2<<1)+0]^0x8000;
						 lv2 = (uint16_t)mpegbuf[(wp2<<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,mpegbuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,mpegbuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,mpegbuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,mpegbuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,mpegbuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,mpegbuffpos);
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
						mpegbuffpos += mpegbufrate;
						progress = mpegbuffpos>>16;
						mpegbuffpos &= 0xffff;
						accumulated_source+=progress;
						pos1+=progress;
						length1-=progress;
						targetlength--;

						if (length1 < 0)
						{
							length2 += length1;
							length1 = 0;
						}

						accumulated_target++;
					} /* while (targetlength && length1) */
					length1 = length2;
					length2 = 0;
					pos1 = pos2;
					pos2 = 0;
				} /* while (targetlength && length1) */
			} /* if (mpegbufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (mpegbufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();

	clipbusy--;
}

OCP_INTERNAL void mpegSetLoop (uint8_t s)
{
	donotloop=!s;
}
OCP_INTERNAL char mpegIsLooped (void)
{
	return mpeg_looped == 3;
}
static void mpegSetSpeed (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	mpegbufrate=imuldiv(256*sp, mpegrate, mpegRate);
}
static void mpegSetVolume (void)
{
	volr=voll=vol*4;
	if (bal<0)
		voll=(voll*(64+bal))>>6;
	else
		volr=(volr*(64-bal))>>6;
}
static void mpegSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			mpegSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			mpegSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			mpegSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			mpegSetVolume();
			break;
	}
}

static int mpegGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

OCP_INTERNAL void mpegGetInfo (struct mpeginfo *info)
{
	info->pos=datapos;
	info->len=fl;
	info->rate=mpeg_Bitrate;
	info->stereo=mpegstereo;
	info->bit16=1;
	info->opt25=opt25;
	info->opt50=opt50;
}
OCP_INTERNAL uint32_t mpegGetPos (void)
{
	return datapos;
}
OCP_INTERNAL void mpegSetPos (uint32_t pos)
{
	/*if (pos<0)
		pos=0;*/
	if (pos>fl)
		pos=fl;
	newpos=pos;
}

static int mpegOpenPlayer_FindRangeAndTags (struct ocpfilehandle_t *mpegfile)
{
	if (mpegfile->seek_set (mpegfile, 0) >= 0)
	{
		unsigned char sig[4];
		if (mpegfile->read (mpegfile, sig, 4) != 4)
		{
			return 0;
		}
		mpegfile->seek_set (mpegfile, 0);
		if (!memcmp(sig, "RIFF", 4))
		{
			debug_printf_stream ("[mppplay.c]: container RIFF (mpeg3, layer 2 probably AKA mp2)\n");

			mpegfile->seek_set (mpegfile, 12);
			fl=0;
			while (1)
			{
				uint32_t t;
				if (mpegfile->read (mpegfile, sig, 4) != 4)
				{
					return 0;
				}
				debug_printf_stream ("[mppplay.c]: chunk: %c%c%c%c\n", sig[0], sig[1], sig[2], sig[3]);
				if (ocpfilehandle_read_uint32_le (mpegfile, &t))
				{
					return 0;
				}
				fl = t;
				debug_printf_stream ("[mppplay.c]: length: %d\n", (int)fl);
				if (!memcmp(sig, "data", 4))
				{
					ofs = mpegfile->getpos (mpegfile);
					break;
				}
				mpegfile->seek_cur (mpegfile, fl);
			}
		} else {
			fl = mpegfile->filesize (mpegfile);
		}

		while (1)
		{
			if (fl >= 256)
			{
				uint8_t buffer[256];

				mpegfile->seek_set (mpegfile, ofs + fl - 256);
				if (mpegfile->read (mpegfile, buffer, 256) != 256)
				{
					return 0;
				}
				if ((buffer[128] == 'T') &&
				    (buffer[129] == 'A') &&
				    (buffer[130] == 'G'))
				{
					if ((buffer[0] == 'E') &&
					    (buffer[1] == 'X') &&
					    (buffer[2] == 'T'))
					{
						debug_printf_stream ("[MPx] got ID3v1.x and ID3v1.2 at the file-end\n");
						got_id3v12(buffer);
						fl -= 256;
						continue;
					} else {
						debug_printf_stream ("[MPx] got ID3v1.x at the file-end\n");
						got_id3v1x(buffer+128);
						fl -= 128;
						continue;
					}
				}
			}
			if (fl >= 25)
			{
				uint8_t sbuffer[10];
				mpegfile->seek_set (mpegfile, ofs + fl - 10);
				if (mpegfile->read (mpegfile, sbuffer, 10) != 10)
				{
					return 0;
				}
				if ((sbuffer[0] == '3') &&
				    (sbuffer[1] == 'D') &&
				    (sbuffer[2] == 'I') &&
				    (sbuffer[3] == 4) &&
				    (sbuffer[4] == 0) &&
				  (!(sbuffer[6] & 0x80)) &&
				  (!(sbuffer[7] & 0x80)) &&
				  (!(sbuffer[8] & 0x80)) &&
				  (!(sbuffer[9] & 0x80)))
				{
					uint32_t size = (sbuffer[6] << 21) |
					                (sbuffer[7] << 14) |
					                (sbuffer[8] <<  7) |
					                 sbuffer[9];
					debug_printf_stream ("[MPx] got ID3V2.4 footer (size=%"PRIu32")\n", size);
					if ((size + 20) <= fl)
					{
						if (size < 32*1024*1024)
						{
							uint8_t *buffer = malloc (size + 10);
							mpegfile->seek_set (mpegfile, ofs + fl - 20 - size);
							if (mpegfile->read (mpegfile, buffer, size + 10) != (size + 10))
							{
								return 0;
							}
							if ((buffer[0] == 'I') &&
							    (buffer[1] == 'D') &&
							    (buffer[2] == '3'))
							{
								debug_printf_stream ("[MPx] got ID3v2.x by using footer\n");
								got_id3v2(buffer, size + 10);
							}
							free (buffer);
						}
						fl -= size + 20;
						continue;
					}
				}
			}
			if (fl >= 25)
			{
				int32_t s;
				uint32_t fetch_size = (fl > 512 * 1024)?512*1024:fl;
				uint8_t *bbuffer = malloc (fetch_size);
				debug_printf_stream("[MPx] seek to %d\n", (int)(ofs+fl - fetch_size));
				mpegfile->seek_set (mpegfile, ofs + fl - fetch_size);
				debug_printf_stream("[MPx] read %d bytes\n", fetch_size);
				if (mpegfile->read (mpegfile, bbuffer, fetch_size) != fetch_size)
				{
					free (bbuffer);
					return 0;
				}
				debug_printf_stream ("[MPx] search for ID3v.2x\n");
				for (s = fetch_size - 15; s >= 0; s--)
				{
					if ((bbuffer[s+0] == 'I') &&
					    (bbuffer[s+1] == 'D') &&
					    (bbuffer[s+2] == '3') &&
					   ((bbuffer[s+3] == 2) || (bbuffer[s+3] == 3) || (bbuffer[s+3] == 4)) &&
					    (bbuffer[s+4] == 0) &&
					    (bbuffer[s+5] != 255) &&
					  (!(bbuffer[s+6] & 0x80)) &&
					  (!(bbuffer[s+7] & 0x80)) &&
					  (!(bbuffer[s+8] & 0x80)) &&
					  (!(bbuffer[s+9] & 0x80)))
					{
						uint32_t size = (bbuffer[s+6] << 21) |
						                (bbuffer[s+7] << 14) |
						                (bbuffer[s+8] <<  7) |
						                 bbuffer[s+9];
						if (size >= (fetch_size - s))
						{
							debug_printf_stream ("[MPx] got ID3v2.x by searching backwards: size=%d\n", (int)(fetch_size - s));
							got_id3v2(bbuffer + s, (fetch_size - s));
							fl -= (fetch_size - s);
							continue;
						}
					}
				}
				free (bbuffer);
			}
			break;
		}
	} else {
		fl=0xffffffff; /* stream */
	}

	return 1;
}

OCP_INTERNAL int mpegOpenPlayer (struct ocpfilehandle_t *mpegfile, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int retval;
	enum plrRequestFormat format;
	ofs=0;

	debug_printf_load ("mpegOpenPlayer (%p)\n", mpegfile);

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	if (!mpegOpenPlayer_FindRangeAndTags (mpegfile))
	{
		return errFormStruc;
	}

	file = mpegfile;
	file->ref (file);

	debug_printf_load ("  mpegOpenPlayer file=%p\n", file);
	debug_printf_load ("  mpegOpenPlayer ofs=0x%08"PRIx64" fl=0x%08"PRIx64"\n", ofs, fl);

	newpos=datapos=0;
	data_length=0;
	data_in_synth=0;
	mpeg_eof=0;
	mpeg_looped=0;

	mad_stream_init(&stream);
	mad_frame_init(&frame);
	mad_synth_init(&synth);
	mad_stream_options(&stream, MAD_OPTION_IGNORECRC);

	GuardPtr=NULL;
	stream.error=0;
	stream.buffer=0;
	stream.this_frame=0;
	file->seek_set (file, ofs);

	if (!stream_for_frame())
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[MPx] stream_for_frame() failed\n");
		retval = errFormStruc;
		goto error_out;
	}
	mpegrate=frame.header.samplerate;

	mpegRate=mpegrate;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&mpegRate, &format, file, cpifaceSession))
	{
		retval = errPlay;
		goto error_out;
	}
	mpegbufrate=imuldiv(65536, mpegrate, mpegRate);

	if (!(mpegbuf=malloc(32768)))
	{
		retval = errAllocMem;
		goto error_out_plrDevAPI_Play;
	}
	mpegbufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, 8192);
	if (!mpegbufpos)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[MPx] ringbuffer_new_samples() failed\n");
		retval = errAllocMem;
		goto error_out_plrDevAPI_Play;
	}
	mpegbuffpos=0;
	GuardPtr=0;

	cpifaceSession->mcpSet = mpegSet;
	cpifaceSession->mcpGet = mpegGet;

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	active = 1;
	opt25_50 = 0;
	opt25[0] = 0;
	opt50[0] = 0;
	return errOk;

error_out_plrDevAPI_Play:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);
error_out:
	if (file)
	{
		file->unref (file);
		file = 0;
	}
	if (mpegbufpos)
	{
		cpifaceSession->ringbufferAPI->free (mpegbufpos);
		mpegbufpos = 0;
	}
	free(mpegbuf); mpegbuf=0;

	mad_synth_finish(&synth);
	mad_frame_finish(&frame);
	mad_stream_finish(&stream);
	return retval;
}

OCP_INTERNAL void mpegClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	free (id3_tag_buffer); id3_tag_buffer = 0;
	id3_tag_target = 0;
	id3_tag_position = 0;

	if (active)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);

		mad_synth_finish(&synth);
		mad_frame_finish(&frame);
		mad_stream_finish(&stream);

		active=0;
	}
	if (mpegbufpos)
	{
		cpifaceSession->ringbufferAPI->free (mpegbufpos);
		mpegbufpos = 0;
	}
	free(mpegbuf); mpegbuf=0;

	ID3_clear(&CurrentTag);
	ID3_clear(&HoldingTag);
	newHoldingTag = 0;

	if (file)
	{
		file->unref (file);
		file = 0;
	}
}
