/* OpenCP Module Player
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * QOAPlay - Player for Quiet OK Audio files
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

#define QOA_MALLOC(sz) malloc(sz)
#define QOA_FREE(p) free(p)
#define QOA_NO_STDIO
#define QOA_IMPLEMENTATION

#include "config.h"
#include <assert.h>
#include <fcntl.h>
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
#include "qoaplay.h"
#include "qoa-git/qoa.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#ifdef PLAYQOA_DEBUG
#define debug_printf_load(...) cpifaceSession->cpiDebug (cpifaceSession, __VA_ARGS__)
#define debug_printf_stream(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf_load(format,args...) ((void)0)
#define debug_printf_stream(format,args...) ((void)0)
#endif

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

static int qoa_looped;

static uint32_t voll,volr;
static int vol;
static int bal;
static int pan;
static int srnd;

static int donotloop;

static char opt25[26];
static char opt50[51];

static qoa_desc qoa;
static unsigned char qoa_file_data[65536];
static unsigned int  qoa_file_data_fill;      /* how much data is qoa_filedata */
static unsigned int  qoa_file_data_offset;    /* how far into the qoa_file_data are we */
static unsigned int  qoa_file_data_remaining; /* how far into the qoa_file_data are we */
static unsigned int  qoa_frame_size;
static uint32_t      qoa_samplepos;

static int16_t *qoa_audio_ring_buffer;
static struct ringbuffer_t *qoa_audio_ring_position;
static int16_t *qoa_audio_frame_buffer;

static uint32_t qoaRate; /* devp output rate */
static uint32_t qoabufrate; /* relation between file and the devp, fixed 16.16 format */
static uint32_t qoabuffpos; /* fine position */

struct ocpfilehandle_t *qoafile;

static int qoa_needseek;
static uint32_t qoa_seekto; /* sample position */

static void qoaIdler(struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (!qoafile)
		return;

	if (qoa_needseek && qoa_frame_size)
	{
		/* round down to nearest frame */
		uint32_t qoa_framenumber = (qoa_seekto + QOA_FRAME_LEN - 1) / QOA_FRAME_LEN;

		/* calculate file position, and perform seek */
		qoa_file_data_offset = 8 + qoa_framenumber * qoa_frame_size;
		qoafile->seek_set (qoafile, qoa_file_data_offset);

		/* calculate sample position */
		qoa_samplepos = qoa_framenumber * QOA_FRAME_LEN;

		/* reset all buffets */
		qoa_file_data_fill = 0;
		qoa_file_data_remaining = 0;
		qoa_needseek = 0;
		cpifaceSession->ringbufferAPI->reset (qoa_audio_ring_position);
	}

	/* fill up the file I/O buffer */
	if (qoa_file_data_remaining < qoa_frame_size)
	{
		uint32_t result;
		memmove (qoa_file_data, qoa_file_data + qoa_file_data_offset, qoa_file_data_remaining);
		qoa_file_data_fill = qoa_file_data_remaining;
		qoa_file_data_offset = 0;

		result = qoafile->read (qoafile, qoa_file_data + qoa_file_data_fill, sizeof (qoa_file_data) - qoa_file_data_fill);
		if (result)
		{
			qoa_file_data_fill += result;
			qoa_file_data_remaining += result;
		} else if (!donotloop)
		{
			qoafile->seek_set (qoafile, 8); /* 8 is the size of the file_header */
			result = qoafile->read (qoafile, qoa_file_data + qoa_file_data_fill, sizeof (qoa_file_data) - qoa_file_data_fill);
			if (result)
			{
				qoa_file_data_fill += result;
				qoa_file_data_remaining += result;
			}
		}
	}

	if (!qoa_file_data_remaining)
	{
		return;
	}

	/* fill up the ring-buffer */
	do
	{
		unsigned int used, samplefill = 0;
		int pos1, pos2;
		int length1, length2;

		cpifaceSession->ringbufferAPI->get_head_samples (qoa_audio_ring_position, &pos1, &length1, &pos2, &length2);

		if ((length1 + length2) < QOA_FRAME_LEN) break;

		used = qoa_decode_frame (qoa_file_data + qoa_file_data_offset, qoa_file_data_remaining, &qoa, (short *)qoa_audio_frame_buffer, &samplefill);
		if (!used)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "qoa_decode_frame() failed to decode more data\n");
			qoa_looped |= 1;
			break;
		}
		qoa_file_data_offset += used;
		qoa_file_data_remaining -= used;

		if (length1 > samplefill)
		{
			length1 = samplefill;
		}
		length2 = samplefill - length1;

		switch (qoa.channels)
		{
			case 0: /* not reachable */ break;
			case 1:
			{
				int16_t *src = qoa_audio_frame_buffer;
				int16_t *dst = qoa_audio_ring_buffer + (pos1 << 1); /* we only need << 1, since qoa_audio_ring_buffer is int16_t already */
				while (length1)
				{
					*(dst++) = *src;
					*(dst++) = *(src++);
					length1--;
				}
				dst = qoa_audio_ring_buffer;
				while (length2)
				{
					*(dst++) = *src;
					*(dst++) = *(src++);
					length2--;
				}
				break;
			}
			case 2:
				memcpy (qoa_audio_ring_buffer + (pos1 << 1), qoa_audio_frame_buffer, length1 << 2); /* we only need << 1, since qoa_audio_ring_buffer is int16_t already */ 
				if (length2)
				{
					memcpy (qoa_audio_ring_buffer, qoa_audio_frame_buffer + (length1 << 1), length2 << 2); /* we only need << 1, since qoa_audio_frame_buffer is int16_t already */
				}
				break;
			default:
			{
				int16_t *src = qoa_audio_frame_buffer;
				int16_t *dst = qoa_audio_ring_buffer + (pos1 << 1); /* we only need << 1, since qoa_audio_ring_buffer is int16_t already */ 
				int extra = qoa.channels - 2;
				while (length1)
				{
					*(dst++) = *src;
					*(dst++) = *(src++);
					length1--;
					dst += extra;
				}
				dst = qoa_audio_ring_buffer;
				while (length2)
				{
					*(dst++) = *src;
					*(dst++) = *(src++);
					length2--;
					dst += extra;
				}
				break;
			}
		}
		cpifaceSession->ringbufferAPI->head_add_samples (qoa_audio_ring_position, samplefill);
		qoa_samplepos += samplefill;
		if ((qoa_samplepos) >= qoa.samples)
		{
			if (donotloop)
			{
				qoa_looped |= 1;
				qoa_samplepos = qoa.samples;
				break;
			} else {
				qoa_looped &= ~1;
				qoa_samplepos = 0;
				qoa_needseek = 1;
				break;
			}
		}
	} while (qoa_file_data_remaining >= qoa_frame_size);
}

OCP_INTERNAL void qoaIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->InPause || (qoa_looped == 3))
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
			qoaIdler(cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (qoa_audio_ring_position, &pos1, &length1, &pos2, &length2);

			if (qoabufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2);
					qoa_looped |= 2;
				} else {
					qoa_looped &= ~2;
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

						rs = qoa_audio_ring_buffer[(pos1<<1) + 0]; /* we only need << 1, since qoa_audio_ring_buffer is int16_t already */ 
						ls = qoa_audio_ring_buffer[(pos1<<1) + 1]; /* we only need << 1, since qoa_audio_ring_buffer is int16_t already */ 

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
				qoa_looped &= ~2;

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
							qoa_looped |= 2;
							break;
						}
						/* will we overflow the wavebuf if we advance? */
						if ((length1+length2) < ((qoabufrate+qoabuffpos)>>16))
						{
							qoa_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						lvm1 = (uint16_t)qoa_audio_ring_buffer[wpm1*2+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						rvm1 = (uint16_t)qoa_audio_ring_buffer[wpm1*2+1]^0x8000;
						 lc0 = (uint16_t)qoa_audio_ring_buffer[wp0*2+0]^0x8000;
						 rc0 = (uint16_t)qoa_audio_ring_buffer[wp0*2+1]^0x8000;
						 lv1 = (uint16_t)qoa_audio_ring_buffer[wp1*2+0]^0x8000;
						 rv1 = (uint16_t)qoa_audio_ring_buffer[wp1*2+1]^0x8000;
						 lv2 = (uint16_t)qoa_audio_ring_buffer[wp2*2+0]^0x8000;
						 rv2 = (uint16_t)qoa_audio_ring_buffer[wp2*2+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,qoabuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,qoabuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,qoabuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,qoabuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,qoabuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,qoabuffpos);
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

						qoabuffpos+=qoabufrate;
						progress = qoabuffpos>>16;
						qoabuffpos &= 0xffff;
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
			} /* if (qoabufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (qoa_audio_ring_position, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();
}


OCP_INTERNAL void qoaGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct qoainfo *i)
{
	i->pos = qoa_samplepos;
	i->len = qoa.samples;
	i->rate = qoa.samplerate;
	i->channels = qoa.channels;
	i->bitrate = (uint64_t)((qoa_frame_size << 3)) * qoa.samplerate / QOA_FRAME_LEN;
	i->opt25 = opt25;
	i->opt50 = opt50;
}

OCP_INTERNAL void qoaSetPos (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t pos)
{
	if (pos >= qoa.samples)
	{
		return;
	}
	qoa_needseek = 1;
	qoa_seekto = pos;
}


OCP_INTERNAL char qoaIsLooped (void)
{
	return qoa_looped == 3;
}

OCP_INTERNAL void qoaSetLoop (uint8_t s)
{
	donotloop=!s;
}

static void qoaSetSpeed (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	qoabufrate=imuldiv(256 * sp, qoa.samplerate, qoaRate);
}

static void qoaSetVolume (void)
{
	volr = voll = vol * 4;
	if (bal < 0)
		voll = (voll * (64 + bal)) >> 6;
	else
		volr = (volr * (64 - bal)) >> 6;
}

static void qoaSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			qoaSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			qoaSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			qoaSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			qoaSetVolume();
			break;
	}
}

static int qoaGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

OCP_INTERNAL int qoaOpenPlayer (struct ocpfilehandle_t *fp, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int retval;
	uint_fast32_t ring_buffer_length;
	enum plrRequestFormat format;

	debug_printf_load ("qoaOpenPlayer (%p)\n", fp);

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	qoa_file_data_fill = fp->read (fp, qoa_file_data, sizeof (qoa_file_data));
	qoa_file_data_remaining = qoa_file_data_fill;
	if (qoa_file_data_fill < QOA_MIN_FILESIZE)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "QOA: File too small\n");
		return errFormStruc;
	}

	qoa_file_data_offset = qoa_decode_header (qoa_file_data, qoa_file_data_remaining, &qoa);
	qoa_file_data_remaining -= qoa_file_data_offset;
	if (qoa_file_data_offset == 0)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "QOA: header decoder failed\n");
		return errFormStruc;
	}
	if (qoa.channels > 16)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "QOA: qoa.channels=%d is too high for the default file-buffer size\n");
		return errFormStruc;
	}
	if (!qoa.samples)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "QOA: qoa.samples count is zero/stream file: not gueranteed to have constant frame sizes, seeking might cause random noise\n");
	}

	qoa_frame_size = QOA_FRAME_SIZE (qoa.channels, QOA_SLICES_PER_FRAME);

	ring_buffer_length = (qoa.samplerate + 1) >> 1; /* 0.5 seconds */
	if (ring_buffer_length < (QOA_FRAME_LEN * 2))
	{
		ring_buffer_length = QOA_FRAME_LEN * 2;
	}

	qoa_audio_ring_buffer = malloc (ring_buffer_length * sizeof (uint16_t) * 2);
	if (!qoa_audio_ring_buffer)
	{
		retval = errAllocMem;
		goto error_out;
	}

	qoa_audio_frame_buffer = malloc (QOA_FRAME_LEN * sizeof (uint16_t) * qoa.channels);
	if (!qoa_audio_ring_buffer)
	{
		free (qoa_audio_ring_buffer);
		qoa_audio_frame_buffer = 0;
		return errAllocMem;
	}

	qoa_audio_ring_position = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, ring_buffer_length);
	if (!qoa_audio_ring_position)
	{
		free (qoa_audio_ring_buffer);
		free (qoa_audio_frame_buffer);
		qoa_audio_ring_buffer = 0;
		qoa_audio_frame_buffer = 0;
		return errAllocMem;
	}

	format = PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&qoaRate, &format, fp, cpifaceSession))
	{
		retval = errPlay;
		goto error_out;
	}
	qoabufrate = imuldiv(0x10000, qoa.samplerate, qoaRate);

	cpifaceSession->mcpSet = qoaSet;
	cpifaceSession->mcpGet = qoaGet;

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	switch (qoa.channels)
	{
		case 1:
			snprintf (opt25, sizeof(opt25), "16bit, mono, %uHz", qoa.samplerate);
			snprintf (opt50, sizeof(opt50), "QOA 16bit, mono, %uHz", qoa.samplerate);
			break;
		case 2:
			snprintf (opt25, sizeof(opt25), "16bit, stereo, %uHz", qoa.samplerate);
			snprintf (opt50, sizeof(opt50), "QOA 16bit, stereo, %uHz", qoa.samplerate);
			break;
		default:
			snprintf (opt25, sizeof(opt25), "16bit, %u ch, %uHz", qoa.channels, qoa.samplerate);
			snprintf (opt50, sizeof(opt50), "QOA 16bit, %u ch (using first two channels), %uHz", qoa.channels, qoa.samplerate);
			break;
	}


	opt25[0] = 0;
	opt50[0] = 0;

	qoafile = fp;
	qoafile->ref (qoafile);

	qoa_needseek = 0;
	qoa_seekto = 0;
	qoa_samplepos = 0;

	qoa_looped = 0;

	return errOk;

error_out:
	if (qoa_audio_ring_position)
	{
		cpifaceSession->ringbufferAPI->free (qoa_audio_ring_position);
		qoa_audio_ring_position = 0;
	}
	free (qoa_audio_ring_buffer);
	free (qoa_audio_frame_buffer);
	qoa_audio_ring_buffer = 0;
	qoa_audio_frame_buffer = 0;
	return retval;
}

OCP_INTERNAL void qoaClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	debug_printf_stream ("qoaClosePlayer\n");

	if (cpifaceSession->plrDevAPI)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	}

	if (qoa_audio_ring_position)
	{
		cpifaceSession->ringbufferAPI->free (qoa_audio_ring_position);
		qoa_audio_ring_position = 0;
	}
	free (qoa_audio_ring_buffer);
	free (qoa_audio_frame_buffer);
	qoa_audio_ring_buffer = 0;
	qoa_audio_frame_buffer = 0;

	if (qoafile)
	{
		qoafile->unref (qoafile);
		qoafile = 0;
	}
}
