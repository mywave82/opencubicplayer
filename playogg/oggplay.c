/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OGGPlay - Player for Ogg Vorbis files
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
 *
 * revision history: (please note changes here)
 *  -nb040911   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss040916   Stian Skjelstad <stian@nixia.no>
 *    -fixed problem regarding random-sound in the first buffer-run
 *  -ss040916   Stian Skjelstad <stian@nixia.no>
 *    -fixed the signess problem around PANPROC
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "oggplay.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"

static int current_section;

static uint32_t oggRate; /* devp rate */

static uint32_t voll,volr;
static int vol;
static int bal;
static int pan;
static int srnd;

static int opt25_50;
static char opt25[26];
static char opt50[51];

static OggVorbis_File ov;
static int oggstereo;
static int oggrate;
static ogg_int64_t oggpos; /* absolute sample position in the source stream */
static ogg_int64_t ogglen; /* absolute length in samples positions of the source stream */
static int oggneedseek;

static int16_t *oggbuf=NULL;
static struct ringbuffer_t *oggbufpos = 0;
static uint_fast32_t oggbuffpos;
static uint_fast32_t oggbufrate;
static volatile int active;
static int ogg_looped;
static int donotloop;

static int ogg_inpause;

static volatile int clipbusy=0;

static struct ocpfilehandle_t *oggfile;

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

static void plrMono16ToStereo16(int16_t *buf, int len)
{ /* convert from end to start, so that we do not overwrite samples as data expands in size */
	int i;
	for (i = len; i >= 0; i--)
	{
		buf[i<<1] = buf[(i<<1)+1] = buf[i];
	}
}

static void oggIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (!active)
		return;

	while (1)
	{
		size_t read;
		long result = 0;
		int pos1, pos2;
		int length1, length2;

		if (oggneedseek)
		{
			oggneedseek=0;
#ifdef HAVE_OV_PCM_SEEK_LAP
			ov_pcm_seek_lap(&ov, oggpos);
#else
			ov_pcm_seek(&ov, oggpos);
#endif

		}
		if (ov_pcm_tell(&ov)!=oggpos)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[OGG] warning, frame position is broken in file (got=%" PRId64 ", expected= %" PRId64 ")\n", ov_pcm_tell(&ov), oggpos);
		}

		cpifaceSession->ringbufferAPI->get_head_samples (oggbufpos, &pos1, &length1, &pos2, &length2);

		if (!length1)
		{
			return;
		}
		read = length1;

		/* check if we are going to read until EOF, and if so, do we allow loop or not */
		if (oggpos+read>=ogglen)
		{
			read=(ogglen-oggpos);
		}

		if (read)
		{
#ifndef WORDS_BIGENDIAN
			result=ov_read(&ov, (char *)(oggbuf+(pos1<<1)), read<<(1+oggstereo), 0, 2, 1, &current_section);
#else
			result=ov_read(&ov, (char *)(oggbuf+(pos1<<1)), read<<(1+oggstereo), 1, 2, 1, &current_section);
#endif

			if (result<=0) /* broken data... we can survive */
			{
				bzero (oggbuf+(pos1<<1), read<<2); /* always clear 16bit, stereo, signed */
				cpifaceSession->cpiDebug (cpifaceSession, "[OGG] ov_read failed: %ld\n", result);
				result=read;
			} else {
				result>>=(1+oggstereo);
				if (!oggstereo)
				{
					plrMono16ToStereo16 (oggbuf+(pos1<<1), result);
				}
			}
			cpifaceSession->ringbufferAPI->head_add_samples (oggbufpos, result);
		} else {
			break;
		}

		if ((oggpos+result) >= ogglen)
		{
			if (donotloop)
			{
				ogg_looped |= 1;
				oggpos = ogglen;
				break;
			} else {
				ogg_looped &= ~1;
				oggpos = 0;
				oggneedseek = 1;
			}
		} else {
			oggpos += result;
		}
	}
}

void __attribute__ ((visibility ("internal"))) oggIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (ogg_inpause || (ogg_looped == 3))
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
			oggIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (oggbufpos, &pos1, &length1, &pos2, &length2);

			if (oggbufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2); // limiting targetlength here, saves us from doing this per sample later
					ogg_looped |= 2;
				} else {
					ogg_looped &= ~2;
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

						rs = oggbuf[pos1<<1];
						ls = oggbuf[(pos1<<1) + 1];

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
				ogg_looped &= ~2;

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
							ogg_looped |= 2;
							break;
						}
						/* will we overflow the oggbuf if we advance? */
						if ((length1+length2) < ((oggbufrate+oggbuffpos)>>16))
						{
							ogg_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)oggbuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)oggbuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)oggbuf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)oggbuf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)oggbuf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)oggbuf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)oggbuf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)oggbuf[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,oggbuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,oggbuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,oggbuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,oggbuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,oggbuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,oggbuffpos);
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

						oggbuffpos+=oggbufrate;
						progress = oggbuffpos>>16;
						oggbuffpos &= 0xffff;
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
			} /* if (oggbufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (oggbufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();

	clipbusy--;
}

static size_t read_func (void *ptr, size_t size, size_t nmemb, void *token)
{
	uint64_t retval;
	retval = oggfile->read (oggfile, ptr, size * nmemb);
	return retval / size;
}

static int seek_func (void *token, ogg_int64_t offset, int whence)
{
	switch (whence)
	{
		case SEEK_SET:
			if (oggfile->seek_set (oggfile, offset) < 0)
			{
				return -1;
			}
			break;
		case SEEK_END:
			if (oggfile->seek_end (oggfile, offset) < 0)
			{
				return -1;
			}
			break;
		case SEEK_CUR:
			if (oggfile->seek_cur (oggfile, offset) < 0)
			{
				return -1;
			}
			break;
		default:
			return -1;
	}
	return oggfile->getpos (oggfile);
}

static int close_func(void *token)
{
	return 0;
}

static long tell_func (void *token)
{
	return oggfile->getpos (oggfile);
}


struct ogg_comment_t __attribute__ ((visibility ("internal"))) **ogg_comments;
int                  __attribute__ ((visibility ("internal")))   ogg_comments_count;
struct ogg_picture_t __attribute__ ((visibility ("internal")))  *ogg_pictures;
int                  __attribute__ ((visibility ("internal")))   ogg_pictures_count;

static void add_comment2(const char *title, const char *value)
{
	int n = 0;
	for (n = 0; n < ogg_comments_count; n++)
	{
		int res = strcmp (ogg_comments[n]->title, title);
		if (res == 0)
		{
			// append to at this point
			ogg_comments[n] = realloc (ogg_comments[n], sizeof (*ogg_comments[n]) + sizeof (ogg_comments[n]->value[0]) * (ogg_comments[n]->value_count + 1));
			ogg_comments[n]->value[ogg_comments[n]->value_count++] = strdup(value);
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
	ogg_comments = realloc (ogg_comments, sizeof (ogg_comments[0]) * (ogg_comments_count+1));
	memmove (ogg_comments + n + 1, ogg_comments + n, (ogg_comments_count - n) * sizeof (ogg_comments[0]));
	ogg_comments[n] = malloc (sizeof (*ogg_comments[n]) + sizeof (ogg_comments[n]->value[0]));
	ogg_comments[n]->title = strdup (title);
	ogg_comments[n]->value_count = 1;
	ogg_comments[n]->value[0] = strdup (value);
	ogg_comments_count++;
}

static void add_picture(const uint16_t actual_width,
		        const uint16_t actual_height,
			uint8_t *data_bgra,
			const char *description,
			const uint32_t description_length,
			const uint32_t picture_type)
{
	ogg_pictures = realloc (ogg_pictures, sizeof (ogg_pictures[0]) * (ogg_pictures_count + 1));
	ogg_pictures[ogg_pictures_count].picture_type = picture_type;
	ogg_pictures[ogg_pictures_count].description = malloc (description_length + 1);
	memcpy (ogg_pictures[ogg_pictures_count].description, description, description_length);
	ogg_pictures[ogg_pictures_count].description[description_length] = 0;
	ogg_pictures[ogg_pictures_count].width = actual_width;
	ogg_pictures[ogg_pictures_count].height = actual_height;
	ogg_pictures[ogg_pictures_count].data_bgra = data_bgra;
	ogg_pictures[ogg_pictures_count].scaled_width = 0;
	ogg_pictures[ogg_pictures_count].scaled_height = 0;
	ogg_pictures[ogg_pictures_count].scaled_data_bgra = 0;

	ogg_pictures_count++;
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
static void add_picture_binary (struct cpifaceSessionAPI_t *cpifaceSession, const uint8_t *src, unsigned int srclen)
{
	uint32_t picture_type;
	uint32_t mime_length;
	const uint8_t *mime;
	uint32_t description_length;
	const uint8_t *description;
	uint32_t width;
	uint32_t height;
	uint32_t bpp;
	uint32_t colors; // if GIF color palette
	uint32_t data_length;
	const uint8_t *data;

	if (srclen < 4)
	{
		return;
	}
	picture_type = (src[0]<<24) | (src[1]<<16) | (src[2]<<8) | src[3];
	src += 4; srclen -= 4;

	if (srclen < 4)
	{
		return;
	}
	mime_length = (src[0]<<24) | (src[1]<<16) | (src[2]<<8) | src[3];
	src += 4; srclen -= 4;

	if (srclen < mime_length)
	{
		return;
	}
	mime = src;
	src += mime_length; srclen -= mime_length;

	if (srclen < 4)
	{
		return;
	}
	description_length = (src[0]<<24) | (src[1]<<16) | (src[2]<<8) | src[3];
	src += 4; srclen -= 4;

	if (srclen < description_length)
	{
		return;
	}
	description = src;
	src += description_length; srclen -= description_length;

	if (srclen < 4)
	{
		return;
	}
	width = (src[0]<<24) | (src[1]<<16) | (src[2]<<8) | src[3];
	src += 4; srclen -= 4;

	if (srclen < 4)
	{
		return;
	}
	height = (src[0]<<24) | (src[1]<<16) | (src[2]<<8) | src[3];
	src += 4; srclen -= 4;

	if (srclen < 4)
	{
		return;
	}
	bpp = (src[0]<<24) | (src[1]<<16) | (src[2]<<8) | src[3];
	src += 4; srclen -= 4;

	if (srclen < 4)
	{
		return;
	}
	colors = (src[0]<<24) | (src[1]<<16) | (src[2]<<8) | src[3];
	src += 4; srclen -= 4;

	if (srclen < 4)
	{
		return;
	}
	data_length = (src[0]<<24) | (src[1]<<16) | (src[2]<<8) | src[3];
	src += 4; srclen -= 4;

	if (srclen < data_length)
	{
		return;
	}
	data = src;
	src += data_length; srclen -= data_length;

#if 0
	if ((mime_length == 3) && (!strncasecmp (mime, "-->", 3)))
	{
		// TODO - URL
	}
#endif
#ifdef HAVE_LZW
	if ((mime_length == 9) && (!strncasecmp ((const char *)mime, "image/gif", 9)))
	{
		uint16_t actual_height, actual_width;
		uint8_t *data_bgra;
		if (!cpifaceSession->console->try_open_gif (&actual_width, &actual_height, &data_bgra, data, data_length))
		{
			add_picture (actual_width, actual_height, data_bgra, (const char *)description, description_length, picture_type);
		}
		return;
	}
#endif

	if ((mime_length == 9) && (!strncasecmp ((const char *)mime, "image/png", 9)))
	{
		uint16_t actual_height, actual_width;
		uint8_t *data_bgra;
		if (!cpifaceSession->console->try_open_png (&actual_width, &actual_height, &data_bgra, (uint8_t *)data, data_length))
		{
			add_picture (actual_width, actual_height, data_bgra, (const char *)description, description_length, picture_type);
		}
		return;
	}

	if (((mime_length == 9) && (!strncasecmp ((const char *)mime, "image/jpg", 9))) || ((mime_length == 10) && (!strncasecmp ((const char *)mime, "image/jpeg", 10))))
	{
		uint16_t actual_height, actual_width;
		uint8_t *data_bgra;
		if (!cpifaceSession->console->try_open_jpeg (&actual_width, &actual_height, &data_bgra, data, data_length))
		{
			add_picture (actual_width, actual_height, data_bgra, (const char *)description, description_length, picture_type);
		}
		return;
	}
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static uint8_t base64_index (const char src)
{
	if ((src >= 'A') && (src <= 'Z')) return src - 'A';
	if ((src >= 'a') && (src <= 'z')) return src - 'a' + 26;
	if ((src >= '0') && (src <= '9')) return src - '0' + 52;
	if (src == '+') return 62;
	if (src == '/') return 63;
	if (src == '=') return 64;
	return 65;
}

static void add_picture_base64 (struct cpifaceSessionAPI_t *cpifaceSession, const char *src)
{
	int srclen = strlen (src);
	uint8_t *dst;
	int dstlen = 0;

	if (srclen < 2)
	{
		return;
	}

	dst = malloc (srclen * 3 / 4); // this is a good estimate, but it might overshoot due to padding

	while (srclen >= 2)
	{
		uint8_t tmp1, tmp2, tmp3, tmp4;

		tmp1 = base64_index (*src);
		src++; srclen--;
		if (tmp1 >= 64)
		{
			break;
		}

		tmp2 = base64_index (*src);
		src++; srclen--;
		if (tmp2 >= 64)
		{/* this is invalid encoding */
			break;
		}

		*dst = (tmp1<<2) | (tmp2>>4); //6 + 2
		dst++; dstlen++;

		if (!srclen)
		{
			break;
		}
		tmp3 = base64_index (*src);
		src++; srclen--;
		if (tmp3 >= 64)
		{
			break;
		}

		*dst = (tmp2<<4) | (tmp3>>2); //4 + 4
		dst++; dstlen++;

		if (!srclen)
		{
			break;
		}
		tmp4 = base64_index (*src);
		src++; srclen--;
		if (tmp4 >= 64)
		{
			break;
		}

		*dst = (tmp3<<6) | tmp4; // 2 + 6
		dst++; dstlen++;
	}

	dst -= dstlen;

	add_picture_binary (cpifaceSession, dst, dstlen);

	free (dst);
}

static void add_comment (struct cpifaceSessionAPI_t *cpifaceSession, const char *src)
{
	char *equal, *tmp, *tmp2;
	if (!strncasecmp (src, "METADATA_BLOCK_PICTURE=", 23))
	{
		add_picture_base64 (cpifaceSession, src + 23);
		return;
	}
	equal = strchr (src, '=');

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

	add_comment2(tmp, src + (equal - src) + 1);

	free (tmp);
}

char __attribute__ ((visibility ("internal"))) oggLooped(void)
{
	return ogg_looped == 3;
}

void __attribute__ ((visibility ("internal"))) oggSetLoop(uint8_t s)
{
	donotloop=!s;
}

void __attribute__ ((visibility ("internal"))) oggPause(uint8_t p)
{
	ogg_inpause=p;
}

static void oggSetSpeed (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	oggbufrate=imuldiv(256*sp, oggrate, oggRate);
}

static void oggSetVolume (void)
{
	volr=voll=vol*4;
	if (bal<0)
		voll=(voll*(64+bal))>>6;
	else
		volr=(volr*(64-bal))>>6;
}

static void oggSet(int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			oggSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			oggSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			oggSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			oggSetVolume();
			break;
	}
}

static int oggGet(int ch, int opt)
{
	return 0;
}

ogg_int64_t __attribute__ ((visibility ("internal"))) oggGetPos (struct cpifaceSessionAPI_t *cpifaceSession)
{
	return (ogglen + ogglen + oggpos - cpifaceSession->ringbufferAPI->get_tail_available_samples(oggbufpos) - cpifaceSession->plrDevAPI->Idle())%ogglen;
}

void __attribute__ ((visibility ("internal"))) oggGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct ogginfo *info)
{
	static int lastsafe=0;
	info->pos = oggGetPos (cpifaceSession);
	info->len=ogglen;
	info->rate=oggrate;
	info->stereo=oggstereo;
	info->bit16=1;
	if ((info->bitrate=ov_bitrate_instant (&ov))<0)
		info->bitrate=lastsafe;
	else
		lastsafe=info->bitrate;
	if (!opt25_50)
	{
		vorbis_info *vi = ov_info (&ov, -1);
		if (vi)
		{
			snprintf (opt25, sizeof (opt25), "Ogg Vorbis version %d", vi->version);
			snprintf (opt50, sizeof (opt50), "Ogg Vorbis version %d, %d channels", vi->version, vi->channels);
			opt25_50 = 1;
		}
	}
	info->opt25=opt25;
	info->opt50=opt50;
}

void __attribute__ ((visibility ("internal"))) oggSetPos (struct cpifaceSessionAPI_t *cpifaceSession, ogg_int64_t pos)
{
	pos=(pos+ogglen)%ogglen;

	oggneedseek=1;
	oggpos=pos;
	cpifaceSession->ringbufferAPI->reset (oggbufpos);
}

static void oggFreeComments (void)
{
	int i, j;

	for (i=0; i < ogg_comments_count; i++)
	{
		for (j=0; j < ogg_comments[i]->value_count; j++)
		{
			free (ogg_comments[i]->value[j]);
		}
		free (ogg_comments[i]->title);
		free (ogg_comments[i]);
	}
	free (ogg_comments);
	ogg_comments = 0;
	ogg_comments_count = 0;

	for (i=0; i < ogg_pictures_count; i++)
	{
		free (ogg_pictures[i].data_bgra);
		free (ogg_pictures[i].scaled_data_bgra);
		free (ogg_pictures[i].description);
	}
	free (ogg_pictures);
	ogg_pictures = 0;
	ogg_pictures_count = 0;
}

static ov_callbacks callbacks =
{
	read_func,
	seek_func,
	close_func,
	tell_func
};
int __attribute__ ((visibility ("internal"))) oggOpenPlayer(struct ocpfilehandle_t *oggf, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;
	int result;
	int retval;
	struct vorbis_info *vi;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	oggfile = oggf;
	oggfile->seek_set (oggfile, 0);
	oggfile->ref (oggfile);
	if ((result = ov_open_callbacks(oggfile, &ov, NULL, 0, callbacks)))
	{
		switch (result)
		{
			case OV_EREAD:      cpifaceSession->cpiDebug (cpifaceSession, "[OGG] ov_open_callbacks(): A read from media returned an error.\n"); break;
			case OV_ENOTVORBIS: cpifaceSession->cpiDebug (cpifaceSession, "[OGG] ov_open_callbacks(): Bitstream does not contain any Vorbis data.\n"); break;
			case OV_EVERSION:   cpifaceSession->cpiDebug (cpifaceSession, "[OGG] ov_open_callbacks(): Vorbis version mismatch.\n"); break;
			case OV_EBADHEADER: cpifaceSession->cpiDebug (cpifaceSession, "[OGG] ov_open_callbacks(): Invalid Vorbis bitstream header.\n"); break;
			case OV_EFAULT:     cpifaceSession->cpiDebug (cpifaceSession, "[OGG] ov_open_callbacks(): Internal logic fault; indicates a bug or heap/stack corruption.\n"); break;
			default:            cpifaceSession->cpiDebug (cpifaceSession, "[OGG] ov_open_callbacks(): Unknown error %d\n", result); break;
		}
		goto error_out_file;
	}

	vi=ov_info(&ov,-1);
	oggstereo=(vi->channels>=2);
	oggrate=vi->rate;

	oggRate=oggrate;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&oggRate, &format, oggfile, cpifaceSession))
	{
		retval = errPlay;
		goto error_out_file;
	}

	oggbufrate=imuldiv(65536, oggrate, oggRate);

	oggpos=0;
	ogglen=ov_pcm_total(&ov, -1);
	if (!ogglen)
	{
		retval = errFormStruc;
		goto error_out_plrDevAPI_Play;
	}

	oggbuf=malloc(1024 * 128);
	if (!oggbuf)
	{
		retval = errAllocMem;
		goto error_out_plrDevAPI_Play;
	}
	oggbufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, 1024*32);
	if (!oggbufpos)
	{
		retval = errAllocMem;
		goto error_out_oggbuf;
	}
	oggbuffpos=0;
	current_section=0;
	oggneedseek=0;

	{
		int i;

		vorbis_comment *vf = 0;
		vf = ov_comment (&ov, -1);
		if (vf)
		{
			for (i=0; i < vf->comments; i++)
			{
				add_comment (cpifaceSession, vf->user_comments[i]);
			}
		}
	}

	ogg_inpause=0;
	ogg_looped=0;

	cpifaceSession->mcpSet = oggSet;
	cpifaceSession->mcpGet = oggGet;

	cpifaceSession->mcpAPI->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	active=1;
	opt25_50 = 0;
	opt25[0] = 0;
	opt50[0] = 0;

	return errOk;

	//cpifaceSession->ringbufferAPI->free (oggbufpos);
	//oggbufpos = 0;

error_out_oggbuf:
	free(oggbuf);
	oggbuf = 0;

error_out_plrDevAPI_Play:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);

error_out_file:
	ov_clear(&ov);

	oggFreeComments ();

	if (oggfile)
	{
		oggfile->unref (oggfile);
		oggfile = 0;
	}
	return retval;
}

void __attribute__ ((visibility ("internal"))) oggClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (active)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	}
	active=0;

	if (oggbufpos)
	{
		cpifaceSession->ringbufferAPI->free (oggbufpos);
		oggbufpos = 0;
	}

	free(oggbuf);
	oggbuf=NULL;

	ov_clear(&ov);

	oggFreeComments ();

	if (oggfile)
	{
		oggfile->unref (oggfile);
		oggfile = 0;
	}
}
