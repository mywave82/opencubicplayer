/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * WavPackPlay - Player for WavPack files
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wavpack/wavpack.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"
#include "wavpackplay.h"

static WavpackContext *wpc;
static WavpackStreamReader64 wpfile = {0};
static struct ocpfilehandle_t *mainfile, *controlfile;
static uint64_t TotalSamples;
static int BytesPerSample;
static uint32_t SampleRate;
static int RealNumberOfChannels;
static int NumberOfChannels;
static int Mode; // MODE_FLOAT is important here

static uint32_t devpRate; /* this is the target rate (devp) */

static int32_t *wavpackdumpbuf;
static int16_t *wavpackbuf;
static struct ringbuffer_t *wavpackbufpos;
static uint_fast32_t wavpackbuffpos;
static uint_fast32_t wavpackbufrate;

static int eof_buffer;
static int eof_wavpackfile;

static int donotloop;
static char opt25[25];
static char opt50[50];

static uint32_t voll,volr;
static int vol;
static int bal;
static int pan;
static int srnd;

OCP_INTERNAL struct wavpack_comment_t *wavpack_comments;
OCP_INTERNAL int                       wavpack_comments_count;
OCP_INTERNAL struct wavpack_picture_t *wavpack_pictures;
OCP_INTERNAL int                       wavpack_pictures_count;

static void add_comment (int index, const char *value)
{
	char **data = realloc (wavpack_comments[index].value, sizeof (wavpack_comments[index].value[0]) * (wavpack_comments[index].value_count + 1));
	if (!data)
	{
		return;
	}
	wavpack_comments[index].value = data;
	wavpack_comments[index].value[wavpack_comments[index].value_count] = strdup (value);
	if (wavpack_comments[index].value[wavpack_comments[index].value_count])
	{
		wavpack_comments[index].value_count++;
	}
}

static void add_comment_steal_strings (char *title, char *_value)
{
	if (!title)
	{
		goto error_out;
	}
	if (!_value)
	{
		goto error_out;
	}

	struct wavpack_comment_t *data = realloc (wavpack_comments, sizeof (wavpack_comments[0]) * (wavpack_comments_count + 1));
	if (!data)
	{
		goto error_out;
	}
	wavpack_comments = data;
	wavpack_comments[wavpack_comments_count].title = title;
	wavpack_comments[wavpack_comments_count].value_count = 0;
	wavpack_comments[wavpack_comments_count].value = 0;
	title=0;
	wavpack_comments_count++;

	char *value = _value;
	while (*value)
	{
		char *nl = strchr (value, 0x0a);
		char *cr = strchr (value, 0x0d);
		if (!nl) nl = value + strlen (value);
		if (!cr) cr = value + strlen (value);
		if (nl < cr)
		{
			*nl = 0;
			add_comment (wavpack_comments_count - 1, value);
			value = nl + 1;
			if ((*value) && (value == cr))
			{
				value++;
			}
		} else if (cr < nl)
		{
			*cr = 0;
			add_comment (wavpack_comments_count - 1, value);
			value = cr + 1;
			if ((*value) && (value == nl))
			{
				value++;
			}
		} else {
			add_comment (wavpack_comments_count - 1, value);
			//value += strlen (value);
			break;
		}
	}
error_out:
	free (title);
	free (_value);
}

static void add_picture (char *title,
                         char *filename,
                         const uint16_t width,
		         const uint16_t height,
			 const uint8_t *data_bgra)
{
	struct wavpack_picture_t *data = realloc (wavpack_pictures, sizeof (wavpack_pictures[0]) * (wavpack_pictures_count + 1));
	if (!data)
	{
		goto error_out;
	}
	wavpack_pictures = data;

	wavpack_pictures[wavpack_pictures_count].title = title;
	wavpack_pictures[wavpack_pictures_count].filename = filename;
	wavpack_pictures[wavpack_pictures_count].width = width;
	wavpack_pictures[wavpack_pictures_count].height = height;
	wavpack_pictures[wavpack_pictures_count].data_bgra = data_bgra;
	wavpack_pictures[wavpack_pictures_count].scaled_width = 0;
	wavpack_pictures[wavpack_pictures_count].scaled_height = 0;
	wavpack_pictures[wavpack_pictures_count].scaled_data_bgra = 0;

	wavpack_pictures_count++;
	return;

error_out:
	free (title);
	free (filename); /* also frees data_bgra */
}


static void add_picture_steal_strings (struct cpifaceSessionAPI_t *cpifaceSession, char *title, char *data, int data_length)
{
	const char *end_of_filename;
	const uint8_t *filedata;
	int filedata_length;

	if ((!title) || (!data) || (!data_length))
	{
		goto error_out;
	}

	if (!(end_of_filename = memchr (data, 0, data_length)))
	{
		goto error_out;
	}
	if (((end_of_filename - data) + 1) >= data_length)
	{
		goto error_out;
	}
	filedata = (const uint8_t *)end_of_filename + 1;
	filedata_length = data_length - (filedata - (const uint8_t *)data);

	if (filedata_length >= 107)
	{
		if ((filedata[0] == 0xff) && (filedata[1] == 0xd8) && (filedata[2] == 0xff))
		{
			uint16_t actual_height, actual_width;
			uint8_t *data_bgra;
			if (!cpifaceSession->console->try_open_jpeg (&actual_width, &actual_height, &data_bgra, (uint8_t *)filedata, filedata_length))
			{
				add_picture (title, data /* filename */, actual_width, actual_height, data_bgra);
				return;
			}
		}
	}
	if (filedata_length >= 67)
	{
		if ((filedata[0] == 137) &&
		    (filedata[1] == 80) &&
		    (filedata[2] == 78) &&
		    (filedata[3] == 71) &&
		    (filedata[4] == 13) &&
		    (filedata[5] == 10) &&
		    (filedata[6] == 26) &&
		    (filedata[7] == 10))
		{
			uint16_t actual_height, actual_width;
			uint8_t *data_bgra;
			if (!cpifaceSession->console->try_open_png (&actual_width, &actual_height, &data_bgra, (uint8_t *)filedata, filedata_length))
			{
				add_picture (title, data /* filename */ , actual_width, actual_height, data_bgra);
				return;
			}
		}
	}

error_out:
	free (title);
	free (data);
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

static void wavpackSetSpeed (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	wavpackbufrate=imuldiv(256*sp, SampleRate, devpRate);
}

static void wavpackSetVolume (void)
{
	volr = voll = vol * 4;
	if (bal < 0)
		voll = (voll * (64 + bal)) >> 6;
	else
		volr = (volr * (64 - bal)) >> 6;
}

static void wavpackSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			wavpackSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			wavpackSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			wavpackSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			wavpackSetVolume();
			break;
	}
}

static int wavpackGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

static void wavpackIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (!wpc)
		return;

	while (1)
	{
		int pos1, pos2;
		int length1, length2;
		cpifaceSession->ringbufferAPI->get_head_samples (wavpackbufpos, &pos1, &length1, &pos2, &length2);

		if (!length1)
		{
			return;
		}

		/* wavpackdumpbuf is the same length and wavpackbuf, so need to limit read size due to that buffer */

		uint32_t result = WavpackUnpackSamples (wpc, wavpackdumpbuf, length1);
		if (result == 0)
		{
			if (donotloop)
			{
				eof_wavpackfile = 1;
			} else {
				WavpackSeekSample64 (wpc, 0);
			}
			break;
		}
		eof_wavpackfile = 0;

		int i;
		if (Mode & MODE_FLOAT)
		{
			float *f = (float *)wavpackdumpbuf;
			int16_t *t = wavpackbuf + (pos1 << 1);
			int r = result;
			if (NumberOfChannels == 2)
			{
				r *= 2; // stereo, two actual samples, per sample row
				for (i=0; i < r; i++)
				{
					if (*f >= 1.0)
					{
						*t++ = 32767;
					} else if (*f <= -1.0)
					{
						*t++ = -32768;
					} else {
						*t++ = floor ((*f) * 32768.0);
					}
					f++;
				}
			} else {
				// mono, single sample per sample row
				for (i=0; i < r; i++)
				{
					if (*f >= 1.0)
					{
						t[0] = t[1] = 32767;
					} else if (*f <= -1.0)
					{
						t[0] = t[1] = -32768;
					} else {
						t[0] = t[1] = floor ((*f) * 32768.0);
					}
					f++;
					t += 2;
				}
			}
		} else {
			int32_t *d = wavpackdumpbuf;
			int16_t *t = wavpackbuf + (pos1 << 1);
			int r = result;
			if (NumberOfChannels == 2)
			{
				r *= 2; // stereo, two actual samples, per sample row
				switch (BytesPerSample)
				{
					case 4:
						for (i=0; i < r; i++)
						{
							*t++ = (*d++) >> 16;
						}
						break;

					case 3:
						for (i=0; i < r; i++)
						{
							*t++ = (*d++) >> 8;
						}
						break;

					case 2:
						for (i=0; i < r; i++)
						{
							*t++ = *d++;
						}
						break;

					default:
					case 1:
						for (i=0; i < r; i++)
						{
							uint16_t v = (uint8_t)*d++;
							v |= (v<<8);
							*t++ = (int16_t)v;

						}
						break;
				}
			} else {
				// mono, single sample per sample row
				switch (BytesPerSample)
				{
					case 4:
						for (i=0; i < r; i++)
						{
							t[0] = t[1] = (*d++) >> 16;
							t += 2;
						}
						break;

					case 3:
						for (i=0; i < r; i++)
						{
							t[0] = t[1] = (*d++) >> 8;
							t += 2;
						}
						break;

					case 2:
						for (i=0; i < r; i++)
						{
							t[0] = t[1] = *d++;
							t += 2;
						}
						break;

					default:
					case 1:
						for (i=0; i < r; i++)
						{
							uint16_t v = (uint8_t)*d++;
							v |= (v<<8);
							t[0] = t[1] = (int16_t)v;
							t += 2;
						}
						break;
				}
			}
		}

		cpifaceSession->ringbufferAPI->head_add_samples (wavpackbufpos, result);
	}
}

OCP_INTERNAL void wavpackIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->InPause || (eof_buffer && eof_wavpackfile))
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
			wavpackIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (wavpackbufpos, &pos1, &length1, &pos2, &length2);
			eof_buffer = !length1;

			if (wavpackbufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2); // limiting targetlength here, saves us from doing this per sample later
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

						rs = wavpackbuf[pos1<<1];
						ls = wavpackbuf[(pos1<<1) + 1];

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
							break;
						}
						/* will we overflow the wavpackbuf if we advance? */
						if ((length1+length2) < ((wavpackbufrate + wavpackbuffpos)>>16))
						{
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)wavpackbuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)wavpackbuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)wavpackbuf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)wavpackbuf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)wavpackbuf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)wavpackbuf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)wavpackbuf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)wavpackbuf[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,wavpackbuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,wavpackbuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,wavpackbuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,wavpackbuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,wavpackbuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,wavpackbuffpos);
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

						wavpackbuffpos+=wavpackbufrate;
						progress = wavpackbuffpos>>16;
						wavpackbuffpos &= 0xffff;
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
			} /* if (wavpackbufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (wavpackbufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();
}

static int32_t wpfile_read_bytes (void *id, void *data, int32_t bcount)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;
	return f->read (f, data, bcount);
}

static int32_t wpfile_write_bytes (void *id, void *data, int32_t bcount)
{
	return 0;
}

static int64_t wpfile_get_pos (void *id)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;
	return f->getpos (f);
}

static int wpfile_set_pos_abs (void *id, int64_t pos)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;
	return f->seek_set (f, pos);
}

static int wpfile_set_pos_rel (void *id, int64_t newpos, int mode)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;

	switch (mode)
	{
		case SEEK_SET: return f->seek_set (f, newpos);
		case SEEK_CUR:
		{
			uint64_t len = f->filesize (f);
			uint64_t curpos = f->getpos (f);
			if (newpos >= 0)
			{
				if ((curpos + newpos) > len) return -1;
				return f->seek_set (f, curpos + newpos);
			}
			if (-newpos > curpos)
			{
				return -1;
			}
			return f->seek_set (f, curpos + newpos);
		}
		case SEEK_END:
		{
			uint64_t len = f->filesize (f);
			if (newpos > 0)
			{
				return -1;
			}
			if (-newpos > len)
			{
				return -1;
			}
			return f->seek_set (f, len + newpos);
		}
	}
	return -1;
}

static int wpfile_push_back_byte (void *id, int c)
{
	return wpfile_set_pos_rel (id, -1, SEEK_CUR);
}

static int64_t wpfile_get_length (void *id)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;
	return f->filesize (f);
}

static int wpfile_can_seek (void *id)
{
	return 1;
}

static int wpfile_truncate_here (void *id)
{
	return -1;
}

static int wpfile_close (void *id)
{
	return 0;
}

OCP_INTERNAL int wavpackOpenPlayer (struct ocpfilehandle_t *fh, struct ocpfilehandle_t *fh_wvc, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int retval;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	wpfile.read_bytes     = wpfile_read_bytes;
	wpfile.write_bytes    = wpfile_write_bytes;
	wpfile.get_pos        = wpfile_get_pos;
	wpfile.set_pos_abs    = wpfile_set_pos_abs;
	wpfile.set_pos_rel    = wpfile_set_pos_rel;
	wpfile.push_back_byte = wpfile_push_back_byte;
	wpfile.get_length     = wpfile_get_length;
	wpfile.can_seek       = wpfile_can_seek;
	wpfile.truncate_here  = wpfile_truncate_here;
	wpfile.close          = wpfile_close;

	char error[128];
	wpc = WavpackOpenFileInputEx64 (&wpfile, fh, fh_wvc, error, ((fh->origin->compression < COMPRESSION_SOLID) ? OPEN_TAGS : 0) | (fh_wvc ? OPEN_WVC : 0) | OPEN_TAGS | OPEN_2CH_MAX | OPEN_NORMALIZE, 0);
	if (!wpc)
	{
		cpifaceSession->cpiDebug(cpifaceSession, "WavpackOpenFileInputEx64() failed: %s\n", error);
		return errFormStruc;
	}

	mainfile = fh;
	controlfile = fh_wvc;
	if (fh) fh->ref (fh);
	if (fh_wvc) fh_wvc->ref (fh_wvc);

	TotalSamples         = WavpackGetNumSamples64 (wpc);
	BytesPerSample       = WavpackGetBytesPerSample (wpc);
	SampleRate           = WavpackGetSampleRate (wpc);
	RealNumberOfChannels = WavpackGetNumChannels (wpc);
	NumberOfChannels     = WavpackGetReducedChannels (wpc);
	Mode                 = WavpackGetMode (wpc);

	devpRate = SampleRate;
	enum plrRequestFormat format = PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&devpRate, &format, fh, cpifaceSession))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[WAVPACK] plrOpenPlayer() failed\n");
		retval = errPlay;
		goto error_out;
	}

	uint32_t buffer_length = SampleRate / 4; /* 250 ms, in addition to devp */
	if (buffer_length < 8192)
	{
		buffer_length = 8192;
	}
	wavpackbuf = malloc (buffer_length * sizeof (int16_t) * 2 /* stereo */);
	if (!wavpackbuf)
	{
		retval = errAllocMem;
		goto error_out_devp;
	}
	wavpackdumpbuf = malloc (buffer_length * sizeof (int32_t) * 2 /* stereo */);
	if (!wavpackdumpbuf)
	{
		retval = errAllocMem;
		goto error_out_devp;
	}
	wavpackbufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_STEREO, buffer_length);
	if (!wavpackbufpos)
	{
		retval = errAllocMem;
		goto error_out_devp;

	}
	wavpackbuffpos = 0;
	wavpackbufrate = imuldiv(65536, SampleRate, devpRate);

	eof_buffer = 0;
	eof_wavpackfile = 0;

	snprintf (opt25, sizeof (opt25), "%s, %dch",
		(Mode&MODE_FLOAT)?"flt":(BytesPerSample==1)?"8bit":(BytesPerSample==2)?"16bit":(BytesPerSample==3)?"24bit":"32bit",
		RealNumberOfChannels);
	snprintf (opt50, sizeof (opt50), "%s, %d channels",
		(Mode&MODE_FLOAT)?"float":(BytesPerSample==1)?"8-bit":(BytesPerSample==2)?"16-bit":(BytesPerSample==3)?"24-bit":"32-bit",
		RealNumberOfChannels);

	cpifaceSession->mcpSet = wavpackSet;
	cpifaceSession->mcpGet = wavpackGet;

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	{
		int t, b, i;
		char id[64];
		t = WavpackGetNumTagItems (wpc);
		for (i=0; i < t; i++)
		{
			if (WavpackGetTagItemIndexed (wpc, i, id, sizeof (id)) < sizeof (id))
			{
				int size = WavpackGetTagItem (wpc, id, 0, 0);
				if (size > 0)
				{
					char *temp = malloc (size + 1);
					if (temp)
					{
						WavpackGetTagItem (wpc, id, temp, size + 1);
						add_comment_steal_strings (strdup (id), temp);
					}
				}
			}
		}
		b = WavpackGetNumBinaryTagItems (wpc);
		for (i=0; i < b; i++)
		{
			if (WavpackGetBinaryTagItemIndexed (wpc, i, id, sizeof (id)) < sizeof (id))
			{
				int size = WavpackGetBinaryTagItem (wpc, id, 0, 0);
				if (size > 0)
				{
					char *temp = malloc (size);
					if (temp)
					{
						WavpackGetBinaryTagItem (wpc, id, temp, size);
						add_picture_steal_strings (cpifaceSession, strdup (id), temp, size);
					}
				}
			}
		}

	}

	return errOk;

error_out_devp:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);

error_out:
	if (fh_wvc)
	{
		WavpackCloseFile (wpc);
		wpc = 0;
	}

	if (mainfile)
	{
		mainfile->unref (mainfile);
		mainfile = 0;
	}

	if (controlfile)
	{
		controlfile->unref (controlfile);
		controlfile = 0;
	}

	if (wavpackbufpos)
	{
		cpifaceSession->ringbufferAPI->free (wavpackbufpos);
		wavpackbufpos = 0;
	}

	free (wavpackbuf);
	wavpackbuf = 0;

	free (wavpackdumpbuf);
	wavpackdumpbuf = 0;

	return retval;
}

OCP_INTERNAL void wavpackClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i, j;

	if (cpifaceSession->plrDevAPI)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	}

	if (wpc)
	{
		WavpackCloseFile (wpc);
		wpc = 0;
	}

	if (mainfile)
	{
		mainfile->unref (mainfile);
		mainfile = 0;
	}

	if (controlfile)
	{
		controlfile->unref (controlfile);
		controlfile = 0;
	}

	if (wavpackbufpos)
	{
		cpifaceSession->ringbufferAPI->free (wavpackbufpos);
		wavpackbufpos = 0;
	}

	free (wavpackbuf);
	wavpackbuf = 0;

	free (wavpackdumpbuf);
	wavpackdumpbuf = 0;

	for (i=0; i < wavpack_comments_count; i++)
	{
		for (j=0; j < wavpack_comments[i].value_count; j++)
		{
			free (wavpack_comments[i].value[j]);
		}
		free (wavpack_comments[i].title);
		free (wavpack_comments[i].value);
	}
	free (wavpack_comments);
	wavpack_comments = 0;
	wavpack_comments_count = 0;

	for (i=0; i < wavpack_pictures_count; i++)
	{
		free (wavpack_pictures[i].title);
		free (wavpack_pictures[i].filename); /* also frees data_bgra */
		//free (wavpack_pictures[i].data_bgra);
		free (wavpack_pictures[i].scaled_data_bgra);
	}
	free (wavpack_pictures);
	wavpack_pictures = 0;
	wavpack_pictures_count = 0;

}

OCP_INTERNAL void wavpackSetPos (struct cpifaceSessionAPI_t *cpifaceSession, uint64_t newpos)
{
	if (wpc)
	{
		WavpackSeekSample64 (wpc, newpos);
	}
}

OCP_INTERNAL int wavpackLooped (void)
{
	return eof_buffer && eof_wavpackfile;
}

OCP_INTERNAL void wavpackGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct wavpackinfo *pi)
{
	pi->pos = WavpackGetSampleIndex64 (wpc);
	pi->len = TotalSamples;
	pi->rate = SampleRate;
	pi->stereo = RealNumberOfChannels > 1;
	pi->bit16 = BytesPerSample >= 2;
	pi->bitrate = (int)WavpackGetInstantBitrate (wpc);
	pi->opt25 = opt25;
	pi->opt50 = opt50;
}

OCP_INTERNAL void wavpackSetLoop (int s)
{
	donotloop=!s;
}
