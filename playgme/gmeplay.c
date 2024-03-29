/* OpenCP Module Player
 * copyright (c) 2024 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Game Music Emulator Play playback routines
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
#include <gme/gme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#include "gmeplay.h"

static Music_Emu *gmesession;

static int gmelooped;
static int doloop;

static unsigned long voll,volr;
static int bal;
static int vol;
static int pan;
static int srnd;

static int16_t *gmebuf;     /* the buffer */
static struct ringbuffer_t *gmebufpos = 0;
static uint32_t gmebuffpos; /* read fine-pos.. */
static uint32_t gmebufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */
static int gmeactivetrack;
static struct gme_info_t *gmetrackinfo;

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

static void gmeIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int pos1, pos2;
	int length1, length2;

	cpifaceSession->ringbufferAPI->get_head_samples (gmebufpos, &pos1, &length1, &pos2, &length2);

	while (length1)
	{
		const char *result;
		if (gme_track_ended (gmesession))
		{
			if (gmeactivetrack + 1 >= gme_track_count (gmesession))
			{
				if (doloop)
				{
					gme_start_track (gmesession, gmeactivetrack = 0);
				} else {
					gmelooped |= 1;
				}
				return;
			} else {
				if ((result = gme_start_track (gmesession, ++gmeactivetrack)))
				{
					fprintf (stderr, "[GME] gme_start_track(): %s\n", result);
					return;
				}
				if (gmetrackinfo)
				{
					gme_free_info (gmetrackinfo);
					gmetrackinfo = 0;
				}
				if ((result = gme_track_info (gmesession, &gmetrackinfo, gmeactivetrack)))
				{
					fprintf (stderr, "[GME] gme_track_info(): %s\n", result);
				}
			}
		}
		result = gme_play (gmesession, length1 << 1, gmebuf + (pos1<<1));
		if (result)
		{
			fprintf (stderr, "[GME] gme_play(): %s\n", result);
			break;
		}

		cpifaceSession->ringbufferAPI->head_add_samples (gmebufpos, length1);
		cpifaceSession->ringbufferAPI->get_head_samples (gmebufpos, &pos1, &length1, &pos2, &length2);
	}
}

OCP_INTERNAL void gmeIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->InPause || (gmelooped == 3))
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

			gmeIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (gmebufpos, &pos1, &length1, &pos2, &length2);

			if (gmebufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2); // limiting targetlength here, saves us from doing this per sample later
					gmelooped |= 2;
				} else {
					gmelooped &= ~2;
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

						rs = gmebuf[(pos1<<1) + 0];
						ls = gmebuf[(pos1<<1) + 1];

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
				gmelooped &= ~2;

				while (targetlength && length1)
				{
					while (targetlength && length1)
					{
						uint32_t wpm1, wp0, wp1, wp2;
						int32_t rc0, rc1, rc2, rc3, rvm1,rv1,rv2;
						int32_t lc0, lc1, lc2, lc3, lvm1,lv1,lv2;
						unsigned int progress;
						int16_t rs, ls;

						/* will the interpolation overflow? */
						if ((length1+length2) <= 3)
						{
							gmelooped |= 2;
							break;
						}
						/* will we overflow the wavebuf if we advance? */
						if ((length1+length2) < ((gmebufrate+gmebuffpos)>>16))
						{
							gmelooped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)gmebuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)gmebuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)gmebuf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)gmebuf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)gmebuf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)gmebuf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)gmebuf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)gmebuf[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,gmebuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,gmebuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,gmebuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,gmebuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,gmebuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,gmebuffpos);
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

						gmebuffpos+=gmebufrate;
						progress = gmebuffpos>>16;
						gmebuffpos &= 0xffff;
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
			} /* if (gmebufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (gmebufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();

}



static void gmeSetSpeed (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	gme_set_tempo (gmesession, (double)sp / 256.0f);
}

static void gmeSetPitch (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	gmebufrate = 256 * sp;
}

static void gmeSetVolume (void)
{
	volr = voll = vol * 4;
	if (bal < 0)
	{
		voll = (voll * (64 + bal)) >> 6;
	} else {
		volr = (volr * (64 - bal)) >> 6;
	}
}

static void gmeSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			gmeSetSpeed(val);
			break;
		case mcpMasterPitch:
			gmeSetPitch(val);
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			gmeSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			gmeSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			gmeSetVolume();
			break;
	}
}

static int gmeGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

OCP_INTERNAL int gmeOpenPlayer (struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	uint32_t gmeRate;
	enum plrRequestFormat format;
	unsigned char *data;
	uint64_t datalen;
	const char *result;
	uint32_t gmebuflen;

	assert (!gmesession);

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	datalen = file->filesize(file);
	if (datalen > 2*1024*1024)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GME] File too big\n");
		return errFormStruc;
	}
	data = malloc (datalen);
	if (!data)
	{
		return errAllocMem;
	}
	file->seek_set (file, 0);
	if (file->read (file, data, datalen) != datalen)
	{
		free (data);
		return errFileRead;
	}

	if ((datalen > 428) &&
	    (!memcmp (data, "GYMX", 4)) &&
	    (data[424] || data[425] || data[426] || data[427]))
	{
		/* stock libgme does not support compressed GYM files */
		uLong unpackedsize =  (uint_fast32_t)(data[424])        |
		                     ((uint_fast32_t)(data[425]) <<  8) |
		                     ((uint_fast32_t)(data[426]) << 16) |
		                     ((uint_fast32_t)(data[427]) << 24);
		if (unpackedsize < 2*1024*1024)
		{
			uint8_t *newdata = calloc (428 + unpackedsize, 1);
			if (!newdata)
			{
				return errAllocMem;
			}
			memcpy (newdata, data, 424); /* copy header except packed size */
			if (uncompress (newdata + 428, &unpackedsize, data + 428, datalen - 428) == Z_OK)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GME] Predecompressed GYMX file, %" PRIu64" bytes decompressed into %lu\n", datalen - 428, unpackedsize);
				free (data);
				data = newdata;
				datalen = 428 + unpackedsize;
			}
		}
	}

	gmeRate=0;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&gmeRate, &format, file, cpifaceSession))
	{
		free (data);
		return errPlay;
	}
	if ((result = gme_open_data (data, datalen, &gmesession, gmeRate)))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GME]: %s\n", result);
		free (data);
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
		gmesession = 0;
		return errFormMiss;
	}
	free (data);
	data = 0;

	if ((result = gme_start_track (gmesession, gmeactivetrack = 0)))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GME]: %s\n", result);
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
		gme_delete (gmesession);
		gmesession = 0;
		return errFormMiss;
	}
	if ((result = gme_track_info (gmesession, &gmetrackinfo, gmeactivetrack)))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GME] gme_track_info(): %s\n", result);
	}

	gmelooped = 0;
	gmebuflen = gmeRate >> 4;
	gmebuf = malloc(gmebuflen <<  2 /* stereo + 16bit */);
	if (!gmebuf)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
		gme_delete (gmesession);
		gmesession = 0;
		return errAllocMem;
	}

	gmebufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, gmebuflen);
	if (!gmebufpos)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);

		free (gmebuf);
		gmebuf = 0;

		gme_delete (gmesession);
		gmesession = 0;

		return errAllocMem;
	}
	gmebuffpos=0;

	cpifaceSession->mcpSet = gmeSet;
	cpifaceSession->mcpGet = gmeGet;

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeCanSpeedPitchUnlock);

	return errOk;
}

void gmeClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->plrDevAPI)
	{
		cpifaceSession->plrDevAPI->Stop(cpifaceSession);
	}

	if (gmetrackinfo)
	{
		gme_free_info (gmetrackinfo);
		gmetrackinfo = 0;
	}

	if (gmesession)
	{
		gme_delete (gmesession);
		gmesession = 0;
	}

	if (gmebufpos)
	{
		cpifaceSession->ringbufferAPI->free (gmebufpos);
		gmebufpos = 0;
	}

	free(gmebuf);
	gmebuf = 0;
}

OCP_INTERNAL void gmeSetLoop (int s)
{
	doloop = s;
}

OCP_INTERNAL void gmeStartSong (struct cpifaceSessionAPI_t *cpifaceSession, int song)
{
	if (gmesession && (song >= 0) && (song < gme_track_count (gmesession)))
	{
		const char *result;
		result = gme_start_track (gmesession, gmeactivetrack = song);
		if (result)
		{
			fprintf (stderr, "[GME] gme_start_track(): %s\n", result);
		}
		if (gmetrackinfo)
		{
			gme_free_info (gmetrackinfo);
			gmetrackinfo = 0;
		}
		if ((result = gme_track_info (gmesession, &gmetrackinfo, gmeactivetrack)))
		{
			fprintf (stderr, "[GME] gme_track_info(): %s\n", result);
		}
	}
}

OCP_INTERNAL int gmeIsLooped (void)
{
	return gmelooped == 3;
}

OCP_INTERNAL void gmeGetInfo (struct gmeinfo *info)
{
	info->track = gmeactivetrack;
	info->numtracks   = gme_track_count (gmesession);
	info->system      = (gmetrackinfo && gmetrackinfo->system   ) ? gmetrackinfo->system    : "";
	info->game        = (gmetrackinfo && gmetrackinfo->game     ) ? gmetrackinfo->game      : "";
	info->song        = (gmetrackinfo && gmetrackinfo->song     ) ? gmetrackinfo->song      : "";
	info->author      = (gmetrackinfo && gmetrackinfo->author   ) ? gmetrackinfo->author    : "";
	info->copyright   = (gmetrackinfo && gmetrackinfo->copyright) ? gmetrackinfo->copyright : "";
	info->comment     = (gmetrackinfo && gmetrackinfo->comment  ) ? gmetrackinfo->comment   : "";
	info->dumper      = (gmetrackinfo && gmetrackinfo->dumper   ) ? gmetrackinfo->dumper    : "";
	info->length      = gmetrackinfo ? gmetrackinfo->length : -1;
	info->introlength = gmetrackinfo ? gmetrackinfo->intro_length : -1;
	info->looplength  = gmetrackinfo ? gmetrackinfo->loop_length : -1;
	info->playlength  = gmetrackinfo ? gmetrackinfo->play_length : -1;
}
