/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * The main integration against libopus and libogg
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

/* Initial code based on https://github.com/xiph/speex/blob/master/src/speexdec.c */

#include "config.h"
#include <math.h>
#include <ogg/ogg.h>
#include <opus/opus.h>
#include <opus/opus_multistream.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "opusplay.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

static struct ocpfilehandle_t *fh;

#define OCP_OPUS_MAX_CHANNELS 8

static ogg_sync_state oy;
static ogg_page       og;
static ogg_packet     op;
static ogg_stream_state os;
static uint8_t channels;
static uint16_t preskip;
static OpusMSDecoder *decoder;
static int opus_serialno;
static int packetconsumed;
static float *output;

static unsigned int opus_skip_samples;
/* static unsigned int opus_skip_packets; */
static uint64_t opus_filesize;
static uint64_t opus_headersize;
static uint64_t opus_current_sample;
static uint32_t opus_next_page;
static int16_t opus_current_packet;
static uint64_t opus_filepos_head; /* next page */
static uint32_t devpRate;
static uint32_t opusbufrate;
static int16_t *opusbuf;
static struct ringbuffer_t *opusbufpos;
static uint32_t opusbuffpos;
static int opus_looped;
static int donotloop;

static uint32_t voll,volr;
static int vol;
static int bal;
static int pan;
static int srnd;

static char opt25[26];
static char opt50[51];

struct opus_info_t
{
	uint64_t current_sample;
	uint32_t page;
	uint32_t bitrate;
	uint16_t packet;
	uint8_t in_opusbuf;
	uint8_t in_devp;
	const struct plrDevAPI_t *plrDevAPI;
};

static struct opus_info_t info_buffers[1024];
static unsigned int       info_lastused; // to speed up allocation
static struct opus_info_t info_last;
static int                info_purge_opusbuf;

static void info_apply_from_devp (void *arg, int samples_ago)
{
	struct opus_info_t *info = arg;
	info_last = *info;
	info->in_devp = 0;
}
static void transfer_info_from_opusbuf_to_devp (void *arg, int samples_ago)
{
	struct opus_info_t *info = (struct opus_info_t *)arg;
	info->in_opusbuf = 0;

	if (info_purge_opusbuf)
	{
		return;
	}

	int samples_until = samples_ago * opusbufrate / 65536;

	info->in_devp = 1;
	info->plrDevAPI->OnBufferCallback (-samples_until, info_apply_from_devp, info);
}

static struct opus_info_t *opus_info_allocate(void)
{
	int i;
	for (i=1; i < 1025; i++)
	{
		int j = (info_lastused + i) & 1023;
		if (info_buffers[j].in_opusbuf) continue;
		if (info_buffers[j].in_devp) continue;
		info_lastused = j;
		return &info_buffers[j];
	}
	return 0;
}

OCP_INTERNAL struct opus_comment_t **opus_comments;
OCP_INTERNAL int                     opus_comments_count;

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

struct opus_pages_t
{
	uint64_t offset;
	uint64_t granule_position;
	uint32_t size;
	uint16_t packets;
	unsigned char ismeta;
};
static struct opus_pages_t *opus_pages = 0;
static uint32_t opus_pages_scanned = 0;
static uint32_t opus_pages_size = 0;

static void opus_append_pagelist (uint32_t page, uint32_t length, uint64_t offset, int ismeta, uint16_t packets, uint64_t granule_position)
{
	if (page)
	{
		/* correct opus_current_sample, file might be corrupt.... */
		/* granule_position is what the new sample_offset will be AFTER this page is consumed */
		opus_current_sample = opus_pages[page-1].granule_position;
	}

	if (page < opus_pages_scanned)
	{
		return;
	}

	if (page && opus_pages[page-1].ismeta)
	{
		opus_headersize = offset;
	}

	if ((page + 2) >= opus_pages_size)
	{
		struct opus_pages_t *temp = realloc (opus_pages, sizeof (opus_pages[0]) * (opus_pages_size + 100));
		if (!temp)
		{
			return;
		}
		opus_pages = temp;
		opus_pages_size += 100;
	}
	opus_pages[page].offset = offset;
	opus_pages[page].ismeta = ismeta;
	opus_pages[page].packets = packets;
	opus_pages[page].size = length;
	opus_pages[page].granule_position = granule_position;
	opus_pages[page+1].offset = offset + length; /* so we can seek to the last known page */
	opus_pages_scanned = page + 1;
}

static void add_comment2(char *title, char *value)
{
	int n = 0;
	for (n = 0; n < opus_comments_count; n++)
	{
		int res = strcmp (opus_comments[n]->title, title);
		if (res == 0)
		{
			// append to at this point
			opus_comments[n] = realloc (opus_comments[n], sizeof (*opus_comments[n]) + sizeof (opus_comments[n]->value[0]) * (opus_comments[n]->value_count + 1));
			opus_comments[n]->value[opus_comments[n]->value_count++] = value;
			free (title);
			return;
		}
		if (res < 0)
		{
			continue;
		} else {
			// insert it at this point
			goto insert;
		}
	}

insert:
	struct opus_comment_t **temp;
	temp = realloc (opus_comments, sizeof (opus_comments[0]) * (opus_comments_count+1));
	if (!temp)
	{
		fprintf (stderr, "add_comment2(): realloc failed\n");
		free (title);
		free (value);
		return;
	}
	opus_comments = temp;
	memmove (opus_comments + n + 1, opus_comments + n, (opus_comments_count - n) * sizeof (opus_comments[0]));
	opus_comments[n] = malloc (sizeof (*opus_comments[n]) + sizeof (opus_comments[n]->value[0]));
	opus_comments[n]->title = title;
	opus_comments[n]->value_count = 1;
	opus_comments[n]->value[0] = value;
	opus_comments_count++;
}

static void add_comment (const char *src, uint32_t length)
{
	const char *equal;
	char *tmp, *iter, *tmp2;
	size_t l;
#if 0
	if (!strncasecmp (src, "METADATA_BLOCK_PICTURE=", 23))
	{
		add_picture_base64 (cpifaceSession, src + 23);
		return;
	}
#endif
	equal = memchr (src, '=', length);

	if (!equal)
	{
		return;
	}
	if (equal == src)
	{
		return;
	}

	l = equal - src;
	tmp = malloc (l + 1);
	if (!tmp)
	{
		return;
	}
	strncpy (tmp, src, l);
	tmp[l] = 0;

	if ((tmp[0] >= 'a') && (tmp[0] <= 'z')) tmp[0] -= 0x20;

	for (iter = tmp + 1; *iter; iter++)
	{
		if ((iter[0] >= 'A') && (iter[0] <= 'Z')) iter[0] += 0x20;
	}

	l = length - (equal - src) - 1;
	tmp2 = malloc (l + 1);
	if (!tmp2)
	{
		free (tmp);
		return;
	}
	strncpy (tmp2, equal + 1, l);
	tmp2[l] = 0;

	add_comment2(tmp, tmp2);
}

static void parse_comments(const uint8_t *d, unsigned int len)
{
	if ((len < 8) || (memcmp (d, "OpusTags", 8)))
	{
		return;
	}
	d += 8;
	len -= 8;

	/* the comment packet starts with a single length:data pair containing the name of the encoder used */
	{
		uint32_t encoder_len;
		if (len < 4)
		{
			return;
		}
		encoder_len = d[0] | (d[1]<<8) | (d[2]<<16) | (d[3]<<24);
		d += 4;
		len -= 4;

		if (len < encoder_len)
		{
			return;
		}
		if (len)
		{
#if 0 /* we currently ignore it */
			set_comment_encoder (d, encoder_len);
#endif
		}
		d += encoder_len;
		len -= encoder_len;
	} while (0);

	uint32_t items, i;
	if (len < 4)
	{
		return;
	}
	items = d[0] | (d[1]<<8) | (d[2]<<16) | (d[3]<<24);
	d += 4;
	len -= 4;

	for (i = 0; i < items; i++)
	{
		if (len < 4)
		{
			return;
		}
		uint32_t item_length = d[0] | (d[1]<<8) | (d[2]<<16) | (d[3]<<24);
		d += 4;
		len -= 4;
		if (len < item_length)
		{
			return;
		}
		add_comment ((const char *)d, item_length);
		d += item_length;
		len -= item_length;
	}
}

static void free_comments (void)
{
	int i, j;

	for (i=0; i < opus_comments_count; i++)
	{
		for (j=0; j < opus_comments[i]->value_count; j++)
		{
			free (opus_comments[i]->value[j]);
		}
		free (opus_comments[i]->title);
		free (opus_comments[i]);
	}
	free (opus_comments);
	opus_comments = 0;
	opus_comments_count = 0;
}

static void opusIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	while (1)
	{
		int pos1, pos2;
		int length1, length2;

		if (/*(!opus_skip_packets) &&*/ (!packetconsumed)) do
		{
			int ret;

			cpifaceSession->ringbufferAPI->get_head_samples (opusbufpos, &pos1, &length1, &pos2, &length2);

			if ((length1 + length2) < 5760)
			{
				return;
			}

			/*Decode frame*/

			packetconsumed = 1;
			ret = opus_multistream_decode_float (decoder, op.packet, op.bytes, output, 5760, 0);

			if (ret == 0)
			{
				continue; // void packet??
			}
			if (ret < 0)
			{
				switch (ret)
				{
					case OPUS_BAD_ARG:
						cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_float() failed, One or more invalid/out of range arguments\n");
						break;
					case OPUS_BUFFER_TOO_SMALL:
						cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_float() failed, Not enough bytes allocated in the buffer\n");
						break;
					case OPUS_INTERNAL_ERROR:
						cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_float() failed, An internal error was detected\n");
						break;
					case OPUS_INVALID_PACKET:
						cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_float() failed, The compressed data passed is corrupted\n");
						break;
					case OPUS_UNIMPLEMENTED:
						cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_float() failed, Invalid/unsupported request number\n");
						break;
					case OPUS_INVALID_STATE:
						cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_float() failed, An encoder or decoder structure is invalid or already freed\n");
						break;
					case OPUS_ALLOC_FAIL:
						cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_float() failed, Memory allocation has failed\n");
						break;
					default:
						cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_float() failed, Unknown error\n");
						break;
				}
				continue;
			}

			unsigned int bufferfill = ret;
			if (opus_skip_samples)
			{
				if (bufferfill <= opus_skip_samples)
				{
					opus_skip_samples -= bufferfill;
					opus_current_sample += ret;
					continue;
				}
				memmove (output, output + opus_skip_samples * channels, (bufferfill - opus_skip_samples) * channels);
				bufferfill -= opus_skip_samples;
				opus_skip_samples = 0;
			}

			int i;
			for (i = 0; i < bufferfill; i++)
			{
				float left, right;
				switch (channels)
				{ /* scale-factors found in opusfile/src/opusfile.c: OP_STEREO_DOWNMIX */
					default:
					case 1: /* mono */
						left = right = output[i];
						break;
					case 2: /* stereo */
						left  = output[ i<<1   ];
						right = output[(i<<1)+1];
						break;
					case 3: /* 3.0 */
						left  = output[  i * 3     ] * 0.5858F +
						        output[ (i * 3) + 1] * 0.4142F;
						right = output[ (i * 3) + 1] * 0.4142F +
						        output[ (i * 3) + 2] * 0.5858F;
						break;
					case 4: /* quadrophonic */
						left  = output[  i * 4     ] * 0.4226F +
						        output[ (i * 4) + 2] * 0.366F +
						        output[ (i * 4) + 3] * 0.2114F;
						right = output[ (i * 4) + 1] * 0.4226F +
						        output[ (i * 4) + 2] * 0.2114F +
						        output[ (i * 4) + 3] * 0.336F;
						break;
					case 5: /* 5.0 */
						left  = output[  i * 5     ] * 0.651F +
					                output[ (i * 5) + 1] * 0.46F +
						        output[ (i * 5) + 3] * 0.5636F +
						        output[ (i * 5) + 4] * 0.3254F;
						right = output[ (i * 5) + 1] * 0.46F +
						        output[ (i * 5) + 2] * 0.651F +
						        output[ (i * 5) + 3] * 0.3254F +
						        output[ (i * 5) + 4] * 0.5636F;
						break;
					case 6: /* 5.1 */
						left =  output[  i * 6     ] * 0.529F +
					                output[ (i * 6) + 1] * 0.3741F +
						        output[ (i * 6) + 3] * 0.4582F +
						        output[ (i * 6) + 4] * 0.2645F +
						        output[ (i * 6) + 5] * 0.3741F;
						right = output[ (i * 6) + 1] * 0.3741F +
						        output[ (i * 6) + 2] * 0.529F +
						        output[ (i * 6) + 3] * 0.2645F +
						        output[ (i * 6) + 4] * 0.4582F +
						        output[ (i * 6) + 5] * 0.3741F;
						break;
					case 7: /* 6.1 */
						left =  output[  i * 7     ] * 0.4553F +
						        output[ (i * 7) + 1] * 0.322F +
						        output[ (i * 7) + 3] * 0.3943F +
						        output[ (i * 7) + 4] * 0.2277F +
						        output[ (i * 7) + 5] * 0.2788F +
						        output[ (i * 7) + 6] * 0.322F;
						right = output[ (i * 7) + 1] * 0.322F +
						        output[ (i * 7) + 2] * 0.4553F +
						        output[ (i * 7) + 3] * 0.2277F +
						        output[ (i * 7) + 4] * 0.3943F +
						        output[ (i * 7) + 5] * 0.2788F +
						        output[ (i * 7) + 6] * 0.322F;
						break;
					case 8: /* 7.1 */
						left =  output[  i * 8     ] * 0.3886F +
						        output[ (i * 8) + 1] * 0.2748F +
							output[ (i * 8) + 3] * 0.3366F +
							output[ (i * 8) + 4] * 0.1943F +
							output[ (i * 8) + 5] * 0.3366F +
							output[ (i * 8) + 6] * 0.1943F +
						        output[ (i * 8) + 7] * 0.2748F;
						right = output[ (i * 8) + 1] * 0.2748F +
						        output[ (i * 8) + 2] * 0.3886F +
						        output[ (i * 8) + 3] * 0.1943F +
						        output[ (i * 8) + 4] * 0.3366F +
						        output[ (i * 8) + 5] * 0.1943F +
							output[ (i * 8) + 6] * 0.3366F +
						        output[ (i * 8) + 7] * 0.2748F;
						break;
				}

				/* convert float into int16_t */
				if (left >= 1.0)
				{
					opusbuf[ pos1<<1   ] = 32767;
				} else if (left <= -1.0)
				{
					opusbuf[ pos1<<1   ] = -32768;
				} else {
					opusbuf[ pos1<<1   ] = floor (left * 32768.0);
				}

				if (right >= 1.0)
				{
					opusbuf[(pos1<<1)+1] = 32767;
				} else if (right <= -1.0)
				{
					opusbuf[(pos1<<1)+1] = -32768;
				} else {
					opusbuf[(pos1<<1)+1] = floor (right * 32768.0);
				}

				pos1++;
				length1--;
				if (!length1)
				{
					pos1 = pos2;
					length1 = length2;
				}
			}

			struct opus_info_t *info;
			if ((info = opus_info_allocate()))
			{
				info->page = opus_next_page - 1;
				info->bitrate = (op.bytes << 3) * 48000 / ret;
				info->packet = opus_current_packet - 1;
				info->current_sample = opus_current_sample;
				info->in_opusbuf = 1;
				info->in_devp = 0;
				info->plrDevAPI = cpifaceSession->plrDevAPI;
				cpifaceSession->ringbufferAPI->add_tail_callback_samples (opusbufpos, 0, transfer_info_from_opusbuf_to_devp, info);
			}
			opus_current_sample += ret;
			cpifaceSession->ringbufferAPI->head_add_samples (opusbufpos, bufferfill);
		} while (0);

again:
		if (ogg_stream_packetout(&os, &op) != 1) /* get the next packet */
		{ /* no more packets, find the next page */

			uint32_t old_returned = oy.returned;
			switch (ogg_sync_pageout(&oy, &og))
			{
				case 0:
					char *data;
					uint64_t result;
					data = ogg_sync_buffer (&oy, 4096);

					result = fh->read (fh, data, 4096);
					if (!result)
					{
						if (!donotloop)
						{
							ogg_sync_reset (&oy);
							ogg_stream_reset (&os);
							uint32_t u;
							for (u=0; u < opus_pages_scanned; u++)
							{
								if (opus_pages[u].offset && !opus_pages[u].ismeta)
								{
									opus_next_page = u;
									opus_current_packet = 0;
									opus_filepos_head = opus_pages[u].offset;
									fh->seek_set (fh, opus_filepos_head);
									data = ogg_sync_buffer (&oy, 4096);
									result = fh->read (fh, data, 4096);
									if (result)
									{
										opus_skip_samples = preskip;
										/* opus_skip_packets = 0; */
										ogg_sync_wrote (&oy, result);
										goto again;
									}
								}
							}
						}
						opus_looped |= 1;
						return;
					}
					ogg_sync_wrote (&oy, result);
					goto again;

				default:
				case -1:
					opus_looped |= 1;
					return;

				case 1:
					opus_append_pagelist (opus_next_page, oy.returned - old_returned, opus_filepos_head, 0, ogg_page_packets(&og), ogg_page_granulepos(&og)); /* 0, 0 */
					opus_next_page++;
					opus_current_packet = 0;
					opus_filepos_head += (oy.returned - old_returned);

					break;
			}
			opus_looped &= ~1;
			ogg_stream_pagein (&os, &og); /* inspect page */
			goto again; /* retry to retrieve packets */
		} else {
			packetconsumed = 0;
		}
		opus_current_packet++;
		/*
		if (opus_skip_packets > 0)
		{
			opus_skip_packets--;
			continue;
		}
		*/
	}
}

static void opusSeekPage (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t page, unsigned int skipsamples)
{
	/*
	opus_skip_packets = frame;
	*/
	opus_next_page = page;
	opus_filepos_head = opus_pages[page].offset;
	opus_skip_samples = skipsamples;
	info_purge_opusbuf = 1;
	cpifaceSession->ringbufferAPI->reset (opusbufpos);
	info_purge_opusbuf = 0;

	ogg_sync_reset (&oy);
	ogg_stream_reset (&os);
	fh->seek_set (fh, opus_filepos_head);

	packetconsumed = 1;
}

OCP_INTERNAL void opusSeekHome (struct cpifaceSessionAPI_t *cpifaceSession)
{
	static uint32_t i;
	for (i=0; i < opus_pages_scanned; i++)
	{
		if (!opus_pages[i].ismeta)
		{
			opusSeekPage (cpifaceSession, i, preskip);
			return;
		}
	}
}

OCP_INTERNAL void opusSeekReverse (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int length)
{
	uint64_t oldpos = opus_current_sample;

	if (oldpos <= preskip)
	{ /* not even "HOME" yet */
		return;
	}
	if (oldpos <= length)
	{
		opusSeekHome (cpifaceSession);
		return;
	}

	uint64_t targetpos = oldpos - length;
	unsigned targetpage = opus_next_page - 1; /* current_page is actually next page */

	if (opus_pages[targetpage].ismeta) /* fail-safe... */
	{
		targetpage++;
	}


	while (1)
	{
		if (opus_pages[targetpage - 1].granule_position > targetpos)
		{
			targetpage--;
			if (!targetpage)
			{
				opusSeekHome (cpifaceSession);
				return;
			}
			continue;
		}
		if (opus_pages[targetpage].granule_position < targetpos)
		{ /* should not be possible */
			opusSeekPage (cpifaceSession, targetpage + 1, 0);
		} else {
			opusSeekPage (cpifaceSession, targetpage, opus_pages[targetpage].granule_position - targetpos); /* seek home */
		}
		return;
	}
}

OCP_INTERNAL void opusSeekForward (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int length)
{
	opus_skip_samples += length;
	info_purge_opusbuf = 1;
	cpifaceSession->ringbufferAPI->reset (opusbufpos);
	info_purge_opusbuf = 0;
}

OCP_INTERNAL void opusIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{

	if (cpifaceSession->InPause || (opus_looped == 3))
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
			opusIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (opusbufpos, &pos1, &length1, &pos2, &length2);

			if (opusbufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2); // limiting targetlength here, saves us from doing this per sample later
					opus_looped |= 2;
				} else {
					opus_looped &= ~2;
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

						rs = opusbuf[pos1<<1];
						ls = opusbuf[(pos1<<1) + 1];

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
				opus_looped &= ~2;

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
							opus_looped |= 2;
							break;
						}
						/* will we overflow the opusbuf if we advance? */
						if ((length1+length2) < ((opusbufrate+opusbuffpos)>>16))
						{
							opus_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)opusbuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)opusbuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)opusbuf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)opusbuf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)opusbuf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)opusbuf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)opusbuf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)opusbuf[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,opusbuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,opusbuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,opusbuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,opusbuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,opusbuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,opusbuffpos);
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

						opusbuffpos+=opusbufrate;
						progress = opusbuffpos>>16;
						opusbuffpos &= 0xffff;
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
			} /* if (opusbufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (opusbufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();
}

static int process_header(struct cpifaceSessionAPI_t *cpifaceSession, ogg_packet *op, uint8_t *channels, uint16_t *preskip, uint32_t *originalsamplerate, int16_t *outputgain, uint8_t *streams, uint8_t *coupled_streams, uint8_t mapping[])
{
	if (op->bytes < 19)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[opus] Magic header too small\n");
		return -1;
	}
	if (memcmp (op->packet, "OpusHead", 8))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[opus] Magic header has incorrect signature\n");
		return -1;
	}
	if (op->packet[8] != 1)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[opus] Header version != 1\n");
		return -1;
	}
	if ((op->packet[9] < 1) || (op->packet[9] > OCP_OPUS_MAX_CHANNELS))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[opus] Invalid number of channels (%d)\n", op->packet[9]);
		return -1;
	}
	*channels = op->packet[9];

	*preskip = (int16_t)(((uint16_t)(op->packet[10])) | (((uint16_t)(op->packet[11]))<<8));

	*originalsamplerate =   (uint32_t)(op->packet[12])      |
	                      (((uint32_t)(op->packet[13]))<< 8) |
	                      (((uint32_t)(op->packet[14]))<<16) |
	                      (((uint32_t)(op->packet[15]))<<24) ;

	*outputgain = ((uint16_t)(op->packet[16])) | (((uint16_t)(op->packet[17]))<<8);

	switch (op->packet[18])
	{
		case 0:
			if ((*channels) > 2)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] Invalid number of channels for channel mapping family 0 (%d)\n", op->packet[9]);
				return -1;
			}
			*streams = 1;
			*coupled_streams = ((*channels) >= 2);
			mapping[0] = 0;
			mapping[1] = 1;
			break;
		case 1:
			if (op->bytes < (19 + 2 + *channels))
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] Magic header too small to contain channel mapping (%d)\n", op->packet[9]);
				return -1;
			}
			*streams = op->packet[19];
			*coupled_streams = op->packet[20];
			if (*coupled_streams > *streams)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] Coupled streams > streams");
				return -1;
			}
			if (((int)*coupled_streams + *streams) > *channels)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] Coupled streams + streams > channels");
				return -1;
			}
			memcpy (mapping, op->packet + 21, *channels);
			break;
		default:
			cpifaceSession->cpiDebug (cpifaceSession, "[opus] Unknown channel mapping family (%d)", op->packet[18]);
			return -1;
	}

	return 0;
}

OCP_INTERNAL char opusLooped (void)
{
	return opus_looped == 3;
}

OCP_INTERNAL void opusSetLoop (uint8_t s)
{
	donotloop=!s;
}

static void opusSetSpeed (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	opusbufrate = imuldiv (256 * sp, 48000, devpRate);
}

static void opusSetVolume (void)
{
	volr = voll = vol * 4;
	if (bal < 0)
		voll = (voll * (64 + bal)) >> 6;
	else
		volr = (volr * (64 - bal)) >> 6;
}

static void opusSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			opusSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			opusSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			opusSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			opusSetVolume();
			break;
	}
}

static int opusGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

OCP_INTERNAL void opusGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct opusinfo *i)
{
	uint32_t page = info_last.page;
	i->filepos = page ?
		(opus_pages[page].offset +
		 opus_pages[page].size * info_last.packet / opus_pages[page].packets
		) - opus_headersize : 0;
	i->filelen = opus_filesize - opus_headersize;
	i->rate = opusbufrate;
	i->bitrate = info_last.bitrate;
	i->opt25 = opt25;
	i->opt50 = opt50;
}

OCP_INTERNAL int opusOpenPlayer (struct ocpfilehandle_t *_fh, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;
	int retval;

	int packet_count = 0;
	int stream_inited = 0;

	/* uint8_t channels;   global */
	uint8_t streams;
	uint8_t coupled_streams;
	uint8_t mapping[OCP_OPUS_MAX_CHANNELS];
	/* uint16_t preskip;   global */
	uint32_t OrigHz;
	int16_t outputgain;

	opus_next_page = 0;
	opus_filepos_head = 0;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	fh = _fh;
	fh->seek_set (fh, 0);
	fh->ref (fh);

	opus_serialno = -1;

	/*Init Ogg data struct*/
	ogg_sync_init(&oy);

	/*Spool Main decoding loop until we have Opus data */
	while (1)
	{
		char *data;
		uint64_t result;
		data = ogg_sync_buffer (&oy, 4096);

		result = fh->read (fh, data, 4096);
		if (!result)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[opus] didn't find any audio data\n");
			retval = errFormMiss;
			goto error_out;
		}
		ogg_sync_wrote (&oy, result);

		uint32_t old_returned = oy.returned;
		while (ogg_sync_pageout(&oy, &og) == 1)
		{
			opus_append_pagelist (opus_next_page, oy.returned - old_returned, opus_filepos_head, 1, ogg_page_packets(&og), ogg_page_granulepos(&og)); /* 0, 0 */
			opus_next_page++;
			opus_filepos_head += (oy.returned - old_returned);
			old_returned = oy.returned;

			if (!stream_inited)
			{
				ogg_stream_init (&os, ogg_page_serialno (&og));
				stream_inited = 1;
			} else {
				/* should not happen during initial packets */
				if (ogg_page_serialno (&og) != os.serialno)
				{
					ogg_stream_reset_serialno (&os, ogg_page_serialno (&og));
				}
			}

			ogg_stream_pagein (&os, &og);

			while (/*!eos &&*/ ogg_stream_packetout (&os, &op) == 1)
			{
				if ((op.bytes >= 19) && (!memcmp(op.packet, "OpusHead", 8)))
				{
					opus_serialno = os.serialno;
				}
				if (opus_serialno == -1 || os.serialno != opus_serialno)
				{
					cpifaceSession->cpiDebug (cpifaceSession, "[opus] serial-number changed before able to decode any audio data\n");
					retval = errFormSig;
					goto error_out;
				}
				/*If first packet, process as Speex header*/
				if (packet_count == 0)
				{
					if (process_header(cpifaceSession, &op, &channels, &preskip, &OrigHz, &outputgain, &streams, &coupled_streams, mapping))
					{
						cpifaceSession->cpiDebug (cpifaceSession, "[opus] decoding format header failed\n");
						retval = errFormSig;
						goto error_out;
					}
				} else if (packet_count==1)
				{
					parse_comments (op.packet, op.bytes);
					goto stream_ready;
				}
				packet_count++;
			}
		}
	}

stream_ready:
	int error = 0;
	packetconsumed = 1;
	decoder = opus_multistream_decoder_create (48000, channels, streams, coupled_streams, mapping, &error);
	if (!decoder)
	{
		retval = errFormStruc;
		switch (error)
		{
			case OPUS_BAD_ARG:
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_create() failed, One or more invalid/out of range arguments\n");
				goto error_out;
			case OPUS_BUFFER_TOO_SMALL:
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_create() failed, Not enough bytes allocated in the buffer\n");
				goto error_out;
			case OPUS_INTERNAL_ERROR:
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_create() failed, An internal error was detected\n");
				goto error_out;
			case OPUS_INVALID_PACKET:
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_create() failed, The compressed data passed is corrupted\n");
				goto error_out;
			case OPUS_UNIMPLEMENTED:
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_create() failed, Invalid/unsupported request number\n");
				goto error_out;
			case OPUS_INVALID_STATE:
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_create() failed, An encoder or decoder structure is invalid or already freed\n");
				goto error_out;
			case OPUS_ALLOC_FAIL:
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_create() failed, Memory allocation has failed\n");
				retval = errAllocMem;
				goto error_out;
			default:
				cpifaceSession->cpiDebug (cpifaceSession, "[opus] opus_multistream_decoder_create() failed, Unknown error\n");
				goto error_out;
		}
	}

	opus_multistream_decoder_ctl (decoder, OPUS_SET_GAIN (outputgain));

	/* frame_size can be up to 120ms, at 48000Hz is 5760 samples */
	output = malloc (5760 * sizeof (float) * channels);
	if (!output)
	{
		retval = errAllocMem;
		goto error_out;
	}

	devpRate = 48000;
	format = PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&devpRate, &format, fh, cpifaceSession))
	{
		retval = errPlay;
		goto error_out;
	}

	opusbufrate = imuldiv (65536, 48000, devpRate);

	uint32_t buffer_length = 48000 / 4; /* 250 ms, in addition to devp */
	opusbuf = malloc(buffer_length * sizeof (int16_t) * 2 /* stereo */);
	if (!opusbuf)
	{
		retval = errAllocMem;
		goto error_out_plrDevAPI_Play;
	}
	opusbufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, buffer_length);
	if (!opusbufpos)
	{
		retval = errAllocMem;
		goto error_out_opusbuf;
	}
	opusbuffpos=0;
	opus_looped=0;

	cpifaceSession->mcpSet = opusSet;
	cpifaceSession->mcpGet = opusGet;

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	char channelstring[32];
	switch (channels)
	{
		default: snprintf (channelstring, sizeof (channelstring), "%d channels", channels); break;
		case 1: strcpy (channelstring, "Mono"); break;
		case 2: strcpy (channelstring, "Stereo"); break;
		case 3: strcpy (channelstring, "3.0 (3 channels)"); break;
		case 4: strcpy (channelstring, "Quadrophonic"); break;
		case 5: strcpy (channelstring, "5.0 (5 channels)"); break;
		case 6: strcpy (channelstring, "5.1 (6 channels)"); break;
		case 7: strcpy (channelstring, "6.1 (7 channels)"); break;
		case 8: strcpy (channelstring, "7.1 (8 channels)"); break;
	}
	snprintf (opt25, sizeof (opt25), "%s, 48000Hz", channelstring);
	snprintf (opt50, sizeof (opt50), "%s, (%dHz=>)48000Hz=>%dHz", channelstring, (int)OrigHz, (int)devpRate);

	opus_filesize = fh->filesize (fh);
	opus_headersize = 0;
	opus_skip_samples = preskip;
	/*
	opus_skip_packets = 0;
	*/
	opus_current_sample = 0;
	opus_current_packet = 0;

	memset (info_buffers, 0, sizeof (info_buffers));
	info_lastused = 0;
	info_purge_opusbuf = 0;
	memset (&info_last, 0, sizeof (info_last));

	opusIdler (cpifaceSession); // ensure that we have the initial non-meta entry in speex_pages
	opusIdler (cpifaceSession); // ensure that we have the initial non-meta entry in speex_pages

	return errOk;

error_out_opusbuf:
	free(opusbuf);
	opusbuf = 0;

error_out_plrDevAPI_Play:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);

error_out:
	if (decoder)
	{
		opus_multistream_decoder_destroy (decoder);
		decoder = 0;
	}

	free (output);
	output = 0;

	ogg_stream_clear (&os);

	ogg_sync_clear (&oy);

	fh->unref (fh);
	fh = 0;

	free_comments ();

	return retval;
}

OCP_INTERNAL void opusClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);

	if (decoder)
	{
		opus_multistream_decoder_destroy (decoder);
		decoder = 0;
	}

	free(opusbuf);
	opusbuf = 0;

	free (output);
	output = 0;

	ogg_stream_clear (&os);

	ogg_sync_clear (&oy);

	fh->unref (fh);
	fh = 0;

	free_comments ();

	free (opus_pages);
	opus_pages = 0;
	opus_pages_size = 0;
	opus_pages_scanned = 0;
}
