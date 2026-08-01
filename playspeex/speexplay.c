/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * The main integration against libspeex and libogg
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

/* File-structure:
 *
 * Speex is based on frames and packets that could be transmitted over IP;
 * usefull for VOIP and similiar applications. A frame is a fixed length of
 * audio, and multiple frames can be stored in a single packet. For a
 * stream/session, the frames per packet is fixed and can typically be 1 to 1.
 * However, the packets can be stored in a file with .SPX extension using an
 * OGG container which is what this plugin expects.
 *
 * An OGG contained is split into pages (which provides syncronization if
 * file is corrupt).
 * An OGG page contains (among others):
 *   * Capture Pattern/syncronization
 *   * Granule Position (how many audio samples will the stream have decoded
 *     in total including this page)
 *   * Page Sequency Number
 *   * Laces (which decodes into packets)
 *
 * The laces is a list of up to 255 entries, each spanning up to 255 bytes.
 *   * If an entry is between 1 and 254, it designates a packet of that size.
 *   * If an entry is 255, is designates that 255 bytes is to be put into a
 *     buffer, and more data is be followed in the next entry. The following
 *     entries needs to be concatenated until you receive and entry of 0-254
 *     bytes long and you have a complete packet.
 *   * If a packet can not fit into a single page, it can span multiple pages.
 *     If the last entry in the lacing list is 255, the data continues into the
 *     next page. In the following page, a flag that tells if the first lacing
 *     entry is a continuation of the previous page.
 *
 * The very first page should contain a single packet that contains the speex
 * format header.
 *
 * The second page should contain a single packet with the meta-header stored
 * as "Vorbis comment" format. If the packet is large, it will span multiple
 * pages.
 *
 * To be future proof, the speex format header might tell us that there are
 * additional extra headers, so some extra pages should be skipped.
 */

/* Initial code based on https://github.com/xiph/speex/blob/master/src/speexdec.c */

#include "config.h"
#include <ogg/ogg.h>
#include <speex/speex.h>
#include <speex/speex_callbacks.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "speexplay.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

static struct ocpfilehandle_t *fh;

static ogg_sync_state oy;
static ogg_page       og;
static ogg_packet     op;
static ogg_stream_state os;
static const int enh_enabled = 1;
static SpeexBits bits;
static int channels;/* -1: follow original(but ends up as forced stereo), 1:mono (but code later force stereo blindly) 2:stereo     all values will end up stereo, but forced values might have side-effects */
static SpeexStereoState stereo;
static spx_int32_t speex_rate;
static int speex_serialno;
static ogg_int64_t page_granule;
static int frame_size;          // real frame size
static int nframes;
static int next_frame;
static int next_packet;
static void *st;
static int lookahead;
static int16_t *output;

static unsigned int speex_skip_samples;
static unsigned int speex_skip_frames;
static unsigned int speex_skip_packets;
static uint64_t speex_filesize;
static uint64_t speex_headersize;
static uint32_t speex_next_page;
static int16_t speex_current_frame;
static uint64_t speex_filepos_head; /* next page */
static uint32_t devpRate;
static uint32_t speexbufrate;
static int16_t *speexbuf;
static struct ringbuffer_t *speexbufpos;
static uint32_t speexbuffpos;
static int speex_looped;
static int donotloop;

static uint32_t voll,volr;
static int vol;
static int bal;
static int pan;
static int srnd;

static char opt25[26];
static char opt50[51];

struct speex_info_t
{
	uint32_t page;
	uint32_t bitrate;
	uint16_t frame;
	uint8_t in_speexbuf;
	uint8_t in_devp;
	const struct plrDevAPI_t *plrDevAPI;
};

static struct speex_info_t info_buffers[256];
static unsigned int        info_lastused; // to speed up allocation
static struct speex_info_t info_last;
static int                 info_purge_speexbuf;

static void info_apply_from_devp (void *arg, int samples_ago)
{
	struct speex_info_t *info = arg;
	info_last = *info;
	info->in_devp = 0;
}
static void transfer_info_from_speexbuf_to_devp (void *arg, int samples_ago)
{
	struct speex_info_t *info = (struct speex_info_t *)arg;
	info->in_speexbuf = 0;

	if (info_purge_speexbuf)
	{
		return;
	}		

	int samples_until = samples_ago * speexbufrate / 65536;

	info->in_devp = 1;
	info->plrDevAPI->OnBufferCallback (-samples_until, info_apply_from_devp, info);
}

static struct speex_info_t *speex_info_allocate(void)
{
	int i;
	for (i=1; i < 257; i++)
	{
		int j = (info_lastused + i) & 255;
		if (info_buffers[j].in_speexbuf) continue;
		if (info_buffers[j].in_devp) continue;
		info_lastused = j;
		return &info_buffers[j];
	}
	return 0;
}

OCP_INTERNAL struct speex_comment_t **speex_comments;
OCP_INTERNAL int                      speex_comments_count;

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

struct speex_pages_t
{
	uint64_t offset;
	uint32_t size;
	uint16_t packets;
	unsigned char ismeta;
};
static struct speex_pages_t *speex_pages = 0;
static uint32_t speex_pages_scanned = 0;
static uint32_t speex_pages_size = 0;

static void speex_append_pagelist (uint32_t page, uint32_t length, uint64_t offset, int ismeta, uint16_t packets)
{
	if (page < speex_pages_scanned)
	{
		return;
	}

	if (page && speex_pages[page-1].ismeta)
	{
		speex_headersize = offset;
	}

	if ((page + 2) >= speex_pages_size)
	{
		struct speex_pages_t *temp = realloc (speex_pages, sizeof (speex_pages[0]) * (speex_pages_size + 100));
		if (!temp)
		{
			fprintf (stderr, "speex_append_pagelist(): realloc failed\n");
			return;
		}
		speex_pages = temp;
		speex_pages_size += 100;
	}
	speex_pages[page].offset = offset;
	speex_pages[page].ismeta = ismeta;
	speex_pages[page].packets = packets;
	speex_pages[page].size = length;
	speex_pages[page+1].offset = offset + length; /* so we can seek to the last known page */
	speex_pages_scanned = page + 1;
}

static void add_comment2(char *title, char *value)
{
	int n = 0;
	for (n = 0; n < speex_comments_count; n++)
	{
		int res = strcmp (speex_comments[n]->title, title);
		if (res == 0)
		{
			// append to at this point
			speex_comments[n] = realloc (speex_comments[n], sizeof (*speex_comments[n]) + sizeof (speex_comments[n]->value[0]) * (speex_comments[n]->value_count + 1));
			speex_comments[n]->value[speex_comments[n]->value_count++] = value;
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
	struct speex_comment_t **temp;
	temp = realloc (speex_comments, sizeof (speex_comments[0]) * (speex_comments_count+1));
	if (!temp)
	{
		fprintf (stderr, "add_comment2(): realloc failed\n");
		free (title);
		free (value);
		return;
	}
	speex_comments = temp;
	memmove (speex_comments + n + 1, speex_comments + n, (speex_comments_count - n) * sizeof (speex_comments[0]));
	speex_comments[n] = malloc (sizeof (*speex_comments[n]) + sizeof (speex_comments[n]->value[0]));
	speex_comments[n]->title = title;
	speex_comments[n]->value_count = 1;
	speex_comments[n]->value[0] = value;
	speex_comments_count++;
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

	for (i=0; i < speex_comments_count; i++)
	{
		for (j=0; j < speex_comments[i]->value_count; j++)
		{
			free (speex_comments[i]->value[j]);
		}
		free (speex_comments[i]->title);
		free (speex_comments[i]);
	}
	free (speex_comments);
	speex_comments = 0;
	speex_comments_count = 0;
}

static void speexIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	while (1)
	{
		int pos1, pos2;
		int length1, length2;

		for (;next_frame < nframes; next_frame++)
		{
			int ret;

			cpifaceSession->ringbufferAPI->get_head_samples (speexbufpos, &pos1, &length1, &pos2, &length2);
			if ((length1 + length2) < frame_size)
			{
				return;
			}

			/*Decode frame*/
			ret = speex_decode_int(st, &bits, output);

			speex_current_frame++;

			if (speex_skip_packets)
			{
				continue; /* we are flushing */
			}

			if (speex_skip_frames)
			{
				speex_skip_frames--;
				continue;
			}

			if (ret==-1)
			{

				continue;
			}
			if (ret==-2)
			{
				fprintf (stderr, "Decoding error: corrupted stream?\n");
				break;
			}
			if (speex_bits_remaining(&bits)<0)
			{
				fprintf (stderr, "Decoding overflow: corrupted stream?\n");
				break;
			}

			if (channels==2)
			{
				speex_decode_stereo_int(output, frame_size, &stereo);
			}

			/* The first packet can instruct us to skip lookahead samples, these are used for priming the synthesizer */
			unsigned int bufferfill = frame_size;
			if (speex_skip_samples)
			{
				if (bufferfill <= speex_skip_samples)
				{
					speex_skip_samples -= bufferfill;
					continue;
				}
				if (channels == 2)
				{
					memmove (output, output + (speex_skip_samples<<1), (bufferfill - speex_skip_samples)<<2);
				} else {
					memmove (output, output + speex_skip_samples, (bufferfill - speex_skip_samples)<<1);
				}
				bufferfill -= speex_skip_samples;
				speex_skip_samples = 0;
			}

			if (channels == 2)
			{
				if (length1 >= bufferfill)
				{
					memcpy (speexbuf + (pos1<<1), output,                   bufferfill << 2);
				} else {
					memcpy (speexbuf + (pos1<<1), output,                  (length1 << 2));
					memcpy (speexbuf            , output + (length1 << 1), (bufferfill - length1) << 2);
				}
			} else {
				if (length1 >= bufferfill)
				{
					length1 = bufferfill;
					length2 = 0;
				} else {
					length2 = bufferfill - length1;
				}
				int pos = 0;
				while (length1)
				{
					speexbuf[pos1<<1] = speexbuf[(pos1<<1)+1] = output[pos];
					pos1++;
					pos++;
					length1--;
				}
				while (length2)
				{
					speexbuf[pos2<<1] = speexbuf[(pos2<<1)+1] = output[pos];
					pos2++;
					pos++;
					length2--;
				}
			}

			struct speex_info_t *info;
			if ((info = speex_info_allocate()))
			{
				spx_int32_t bitrate;
				speex_decoder_ctl(st, SPEEX_GET_BITRATE, &bitrate);

				info->page = speex_next_page - 1;
				info->bitrate = bitrate;
				info->frame = speex_current_frame - 1;
				info->in_speexbuf = 1;
				info->in_devp = 0;
				info->plrDevAPI = cpifaceSession->plrDevAPI;
				cpifaceSession->ringbufferAPI->add_tail_callback_samples (speexbufpos, 0, transfer_info_from_speexbuf_to_devp, info);
			}

			cpifaceSession->ringbufferAPI->head_add_samples (speexbufpos, bufferfill);
		}

		if (next_frame < nframes)
		{
			continue;
		}
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
							for (u=0; u < speex_pages_scanned; u++)
							{
								if (speex_pages[u].offset && !speex_pages[u].ismeta)
								{
									speex_next_page = u;
									speex_filepos_head = speex_pages[u].offset;
									fh->seek_set (fh, speex_filepos_head);
									data = ogg_sync_buffer (&oy, 4096);
									result = fh->read (fh, data, 4096);
									if (result)
									{
										speex_skip_samples = lookahead;
										ogg_sync_wrote (&oy, result);
										goto again;
									}
								}
							}
						}
						speex_looped |= 1;
						return;
					}
					ogg_sync_wrote (&oy, result);
					goto again;

				default:
				case -1:
					speex_looped |= 1;
					return;

				case 1:
					speex_append_pagelist (speex_next_page, oy.returned - old_returned, speex_filepos_head, 0, ogg_page_packets(&og)); /* 0, 0 */
					speex_next_page++;
					speex_current_frame = 0;
					speex_filepos_head += (oy.returned - old_returned);

					break;
			}
			speex_looped &= ~1;
			ogg_stream_pagein (&os, &og); /* inspect page */
			page_granule = ogg_page_granulepos(&og);
			goto again; /* retry to retrieve packets */
		}
		if (speex_skip_packets >= 1)
		{
			speex_skip_packets--;
			continue;
		}

		next_frame = 0;
		next_packet++;
		speex_bits_read_from(&bits, (char*)op.packet, op.bytes);
	}
}

static void speexSeekPage (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t page, unsigned int frame)
{
	speex_skip_packets = frame;
	speex_skip_frames = (page || frame); /* seeking home does not need to skip frames */
	speex_next_page = page;
	speex_filepos_head = speex_pages[page].offset;
	speex_skip_samples = lookahead;
	info_purge_speexbuf = 1;
	cpifaceSession->ringbufferAPI->reset (speexbufpos);
	info_purge_speexbuf = 0;

	ogg_sync_reset (&oy);
	ogg_stream_reset (&os);
	fh->seek_set (fh, speex_filepos_head);
}

OCP_INTERNAL void speexSeekHome (struct cpifaceSessionAPI_t *cpifaceSession)
{
	static uint32_t i;
	for (i=0; i < speex_pages_scanned; i++)
	{
		if (!speex_pages[i].ismeta)
		{
			speexSeekPage (cpifaceSession, i, 0);
		}
	}
}

OCP_INTERNAL void speexSeekReverse (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int length)
{
	length -= (length >= speex_current_frame) ? speex_current_frame  : length;

	unsigned int packets = length / nframes;
	unsigned targetpage = speex_next_page - 1; /* current_page is actually next page */

	if (speex_pages[targetpage].ismeta)
	{
		targetpage++;
	}

	while (1)
	{
		if (speex_pages[targetpage].packets <= packets)
		{
			if (speex_pages[targetpage-1].ismeta)
			{
				speexSeekPage (cpifaceSession, targetpage, 0); /* seek home */
				return;
			}
			packets -= speex_pages[targetpage].packets;
			targetpage--;
		} else {
			speexSeekPage (cpifaceSession, targetpage, speex_pages[targetpage].packets - packets + 1);
			return;
		}
	}
}

OCP_INTERNAL void speexSeekForward (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int length)
{
	speex_skip_packets += length;
	if (!speex_skip_frames)
	{
		speex_skip_frames = 1;
		if (speex_skip_packets)
		{
			speex_skip_packets--;
		}
	}
	info_purge_speexbuf = 1;
	cpifaceSession->ringbufferAPI->reset (speexbufpos);
	info_purge_speexbuf = 0;
	speex_skip_samples = lookahead;
}

OCP_INTERNAL void speexIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{

	if (cpifaceSession->InPause || (speex_looped == 3))
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
			speexIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (speexbufpos, &pos1, &length1, &pos2, &length2);

			if (speexbufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2); // limiting targetlength here, saves us from doing this per sample later
					speex_looped |= 2;
				} else {
					speex_looped &= ~2;
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

						rs = speexbuf[pos1<<1];
						ls = speexbuf[(pos1<<1) + 1];

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
				speex_looped &= ~2;

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
							speex_looped |= 2;
							break;
						}
						/* will we overflow the speexbuf if we advance? */
						if ((length1+length2) < ((speexbufrate+speexbuffpos)>>16))
						{
							speex_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)speexbuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)speexbuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)speexbuf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)speexbuf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)speexbuf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)speexbuf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)speexbuf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)speexbuf[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,speexbuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,speexbuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,speexbuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,speexbuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,speexbuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,speexbuffpos);
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

						speexbuffpos+=speexbufrate;
						progress = speexbuffpos>>16;
						speexbuffpos &= 0xffff;
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
			} /* if (speexbufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (speexbufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();
}

static void *process_header(struct cpifaceSessionAPI_t *cpifaceSession, ogg_packet *op, spx_int32_t enh_enabled, spx_int32_t *frame_size, spx_int32_t *rate, int *nframes, int *channels, SpeexStereoState *stereo, int *extra_headers, int32_t *bitrate, int *modeID)
{
	void *st;
	const SpeexMode *mode;
	SpeexHeader *header;
	SpeexCallback callback;

	header = speex_packet_to_header((char*)op->packet, op->bytes);
	if (!header)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "Cannot read header\n");
		return NULL;
	}
	if (header->mode >= SPEEX_NB_MODES || header->mode<0)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "Mode number %d does not (yet/any longer) exist in this version\n", header->mode);
		free(header);
		return NULL;
	}

	*modeID = header->mode;
	mode = speex_lib_get_mode (*modeID);

	if (header->speex_version_id > 1)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "This file was encoded with Speex bit-stream version %d, which I don't know how to decode\n", header->speex_version_id);
		free(header);
		return NULL;
	}

	if (mode->bitstream_version < header->mode_bitstream_version)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "The file was encoded with a newer version of Speex. You need to upgrade in order to play it.\n");
		free(header);
		return NULL;
	}
	if (mode->bitstream_version > header->mode_bitstream_version)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "The file was encoded with an older version of Speex. You would need to downgrade the version in order to play it.\n");
		free(header);
		return NULL;
	}

	st = speex_decoder_init (mode);
	if (!st)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "Decoder initialization failed.\n");
		free(header);
		return NULL;
	}
	speex_decoder_ctl (st, SPEEX_SET_ENH, &enh_enabled);
	speex_decoder_ctl (st, SPEEX_GET_FRAME_SIZE, frame_size);

	*rate = header->rate;

	speex_decoder_ctl (st, SPEEX_SET_SAMPLING_RATE, rate);

	*nframes = header->frames_per_packet;
	*channels = header->nb_channels;

	if ( (*channels) !=1 )
	{ /* if signal is not MONO, invoke the STEREO filtering (by panning) */
		*channels = 2;
		callback.callback_id = SPEEX_INBAND_STEREO;
		callback.func = speex_std_stereo_request_handler;
		callback.data = stereo;
		speex_decoder_ctl(st, SPEEX_SET_HANDLER, &callback);
	}

	if (header->vbr)
	{
		*bitrate = -1;
	} else {
		if (header->bitrate > 0)
		{
			*bitrate = header->bitrate;
		} else {
			*bitrate = 0;
		}
	}

	*extra_headers = header->extra_headers;

	free(header);
	return st;
}

OCP_INTERNAL char speexLooped (void)
{
	return speex_looped == 3;
}

OCP_INTERNAL void speexSetLoop (uint8_t s)
{
	donotloop=!s;
}

static void speexSetSpeed (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	speexbufrate = imuldiv (256 * sp, speex_rate, devpRate);
}

static void speexSetVolume (void)
{
	volr = voll = vol * 4;
	if (bal < 0)
		voll = (voll * (64 + bal)) >> 6;
	else
		volr = (volr * (64 - bal)) >> 6;
}

static void speexSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			speexSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			speexSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			speexSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			speexSetVolume();
			break;
	}
}

static int speexGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

OCP_INTERNAL void speexGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct speexinfo *i)
{
	uint32_t page = info_last.page;
	i->filepos = page ? 
		(speex_pages[page].offset +
		 speex_pages[page].size * info_last.frame / (speex_pages[page].packets * nframes)
		) - speex_headersize : 0;
	i->filelen = speex_filesize - speex_headersize;
	i->rate = speexbufrate;
	i->bitrate = info_last.bitrate;
	i->opt25 = opt25;
	i->opt50 = opt50;
}

OCP_INTERNAL int speexOpenPlayer (struct ocpfilehandle_t *_fh, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;
	int retval;
	int packet_count = 0;
	int extra_headers = 0;
	int stream_inited = 0;
	int modeID;
	int32_t bitrate;

	speex_next_page = 0;
	speex_filepos_head = 0;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	fh = _fh;
	fh->seek_set (fh, 0);
	fh->ref (fh);

	frame_size = 0;
	nframes = -1;
	speex_serialno = -1;

	/*Init Ogg data struct*/
	ogg_sync_init(&oy);

	speex_bits_init(&bits);

	/*Spool Main decoding loop until we have Speex data */
	while (1)
	{
		char *data;
		uint64_t result;
		data = ogg_sync_buffer (&oy, 4096);

		result = fh->read (fh, data, 4096);
		if (!result)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[speex] didn't find any audio data\n");
			retval = errFormMiss;
			goto error_out;
		}
		ogg_sync_wrote (&oy, result);

		uint32_t old_returned = oy.returned;
		while (ogg_sync_pageout(&oy, &og) == 1)
		{
			speex_append_pagelist (speex_next_page, oy.returned - old_returned, speex_filepos_head, 1, ogg_page_packets(&og)); /* 0, 0 */
			speex_next_page++;
			speex_filepos_head += (oy.returned - old_returned);
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
			page_granule = ogg_page_granulepos(&og);

			while (/*!eos &&*/ ogg_stream_packetout (&os, &op) == 1)
			{
				if ((op.bytes >= 5) && (!memcmp(op.packet, "Speex", 5)))
				{
					speex_serialno = os.serialno;
				}
				if (speex_serialno == -1 || os.serialno != speex_serialno)
				{
					cpifaceSession->cpiDebug (cpifaceSession, "[speex] serial-number changed before able to decode any audio data\n");
					retval = errFormSig;
					goto error_out;
				}
				/*If first packet, process as Speex header*/
				if (packet_count == 0)
				{
					speex_stereo_state_reset (&stereo);

					if (!(st = process_header(cpifaceSession, &op, enh_enabled, &frame_size, &speex_rate, &nframes, &channels, &stereo, &extra_headers, &bitrate, &modeID)))
					{
						cpifaceSession->cpiDebug (cpifaceSession, "[speex] decoding speex format header failed\n");
						retval = errFormSig;
						goto error_out;
					}
					speex_decoder_ctl(st, SPEEX_GET_LOOKAHEAD, &lookahead);
					if (!nframes)
					{
						nframes=1;
					}
				} else if (packet_count==1)
				{
					parse_comments (op.packet, op.bytes);
				}
				packet_count++;
				if (packet_count == (extra_headers + 2))
				{
					goto stream_ready;
				}
			}
		}
	}

stream_ready:
	output = malloc (frame_size * sizeof (int16_t) * channels);
	if (!output)
	{
		retval = errAllocMem;
		goto error_out;
	}

	devpRate = speex_rate;
	format=PLR_STEREO_16BIT_SIGNED;
	next_frame = nframes; /* pretend the current packet is just finished, so that the idler will fetch the next frame */
	if (!cpifaceSession->plrDevAPI->Play (&devpRate, &format, fh, cpifaceSession))
	{
		retval = errPlay;
		goto error_out;
	}

	speexbufrate = imuldiv (65536, speex_rate, devpRate);

	uint32_t buffer_length = speex_rate / 4; /* 250 ms, in addition to devp */
	if (buffer_length < 8192)
	{
		buffer_length = 8192;
	}
	speexbuf = malloc(buffer_length * sizeof (int16_t) * 2 /* stereo */);
	if (!speexbuf)
	{
		retval = errAllocMem;
		goto error_out_plrDevAPI_Play;
	}

	speexbufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, buffer_length);
	if (!speexbufpos)
	{
		retval = errAllocMem;
		goto error_out_speexbuf;
	}
	speexbuffpos=0;
	speex_looped=0;

	cpifaceSession->mcpSet = speexSet;
	cpifaceSession->mcpGet = speexGet;

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	opt25[0] = 0;
	opt50[0] = 0;

	speex_filesize = fh->filesize (fh);
	speex_headersize = 0;
	speex_skip_samples = lookahead;
	speex_skip_packets = 0;
	speex_skip_frames = 0;
	speex_current_frame = 0;

	memset (info_buffers, 0, sizeof (info_buffers));
	info_lastused = 0;
	info_purge_speexbuf = 0;
	memset (&info_last, 0, sizeof (info_last));


	char BR[16];
	if (bitrate < 0)
	{
		snprintf (BR, sizeof (BR), ", VBR");
	} else if (bitrate > 0)
	{
		snprintf (BR, sizeof (BR), ", %"PRId32"bps", bitrate);
	} else {
		/* quality encoded, parameter is not stored in the header */
		BR[0] = 0;
	}
	snprintf (opt25, sizeof (opt25), "%s, %"PRIu32"Hz, %s",
		(channels==2) ? "Stereo" : "Mono",
		speex_rate,
		(modeID==0) ? "NB" : (modeID==1) ? "WB" : (modeID==2) ? "UW" : "?B");
	snprintf (opt50, sizeof (opt50), "%s, %"PRIu32"Hz%s, %s",
		(channels==2) ? "Stereo" : "Mono",
		speex_rate,
		BR,
		(modeID==0) ? "NarrowBand" : (modeID==1) ? "WideBand" : (modeID==2) ? "UltraWideBand" : "Unknown mode");

	speexIdler (cpifaceSession); // ensure that we have the initial non-meta entry in speex_pages

	return errOk;

error_out_speexbuf:
	free(speexbuf);
	speexbuf = 0;

error_out_plrDevAPI_Play:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);

error_out:
	free (output);
	output = 0;

	ogg_stream_clear (&os);

	ogg_sync_clear (&oy);

	speex_bits_destroy (&bits);

	fh->unref (fh);
	fh = 0;

	free_comments ();

	return retval;
}

OCP_INTERNAL void speexClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);

	free(speexbuf);
	speexbuf = 0;

	free (output);
	output = 0;

	ogg_stream_clear (&os);

	ogg_sync_clear (&oy);

	speex_bits_destroy (&bits);

	fh->unref (fh);
	fh = 0;

	free_comments ();

	free (speex_pages);
	speex_pages = 0;
	speex_pages_size = 0;
	speex_pages_scanned = 0;
}
