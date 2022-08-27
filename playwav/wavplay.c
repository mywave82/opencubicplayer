/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * WAVPlay - wave file player
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb980717   Tammo Hinrichs <kb@nwn.de>
 *    -added a few lines in idle routine to make win95 background
 *     playing possible
 */

#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "stuff/imsrtns.h"
#include "wave.h"

#ifdef PLAYWAVE_DEBUG
# define PRINT(fmt, args...) fprintf(stderr, "%s %s: " fmt, __FILE__, __func__, ##args)
#else
# define PRINT(a, ...) do {} while(0)
#endif

/* options */
static int wav_inpause;
static int wav_looped;

static uint32_t voll,volr;
static int vol;
static int bal;
static int pan;
static int srnd;

static volatile int active;

static char opt25[26];
static char opt50[51];

static uint32_t waveRate; /* devp rate */

static struct ocpfilehandle_t *wavefile;
static uint32_t waverate; /* wavefile rate */
static uint32_t wavepos;
static uint32_t wavelen;
static int waveneedseek;
static int wavestereo;
static int wave16bit;

static int donotloop;

static uint32_t waveoffs;
static  int16_t *wavebuf=0;
static struct ringbuffer_t *wavebufpos = 0;
static uint32_t wavebuffpos;
static uint32_t wavebufrate;

static volatile int clipbusy=0;

#ifdef PLAYWAVE_DEBUG
static const char *compression_code_str(uint_fast16_t code)
{
	switch (code)
	{
		case 1:
			return "PCM/uncompressed";
		case 2:
			return "Microsoft ADPCM";
		case 3:
			return "Floating point PCM";
		case 5:
			return "Digispeech CVSD / IBM PS/2 Speech Adapter (Motorola MC3418)";
		case 6:
			return "ITU G.711 a-law";
		case 7:
			return "ITU G.711 mu-law";
		case 0x10:
			return "OKI ADPCM";
		case 0x11:
			return "DVI ADPCM";
		case 0x15:
			return "Digispeech DIGISTD";
		case 0x16:
			return "Digispeech DigiFix";
		case 0x17:
			return "IMA ADPCM";
		case 0x20:
			return "ITU G.723 ADPCM (Yamaha)";
		case 0x22:
			return "DSP Group TrueSpeech";
		case 0x31:
			return "GSM6.10";
		case 0x49:
			return "GSM 6.10";
		case 0x64:
			return "ITU G.721 ADPCM";
		case 0x70:
			return "Lernout & Hauspie CELP";
		case 0x72:
			return "Lernout & Hauspie SBC";
		case 0x80:
			return "MPEG";
		case 65535:
			return "Experimental";
		case 0:
		default:
			return "Unknown";
	}
}
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


static void wpIdler(struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (!active)
		return;

	while (1)
	{
		size_t read;
		long result = 0;
		int pos1, pos2;
		int length1, length2;

		cpifaceSession->ringbufferAPI->get_head_samples (wavebufpos, &pos1, &length1, &pos2, &length2);

		if (!length1)
		{
			return;
		}
		read = length1;

		/* check if we are going to read until EOF, and if so, do we allow loop or not */
		if ((wavepos+read>=wavelen))
		{
			read=(wavelen-wavepos);
		}

		if (read)
		{
			if (waveneedseek)
			{
				waveneedseek = 0;
				wavefile->seek_set (wavefile, (wavepos<<(wave16bit+wavestereo))+waveoffs);
			}
			result = wavefile->read (wavefile, wavebuf+(pos1<<1), read<<(wave16bit + wavestereo));
			if (result<=0)
			{
				fprintf (stderr, "[playwav] fread() failed: %s\n", strerror (errno));
				if (wave16bit)
				{
					memset (wavebuf+(pos1<<1), 0x00, read<<(1 + wavestereo));
				} else {
					memset (wavebuf+(pos1<<1), 0x80, read<<(wavestereo));
				}
				result = read;
			} else {
				result >>= (wave16bit + wavestereo);
			}
			/* The wavebuffer is always 16bit signed stereo, so expand if needed */
			if (wavestereo)
			{
				if (!wave16bit)
				{
					int i;
					int16_t *dst = wavebuf + (pos1<<1);
					uint8_t *src = (uint8_t *)dst;
					for (i=((result-1)<<1); i>=0; i--)
					{
						dst[i] = (src[i] | (((uint16_t)src[i]) << 8)) ^ 0x8080;
					}
				}
			} else {
				if (!wave16bit)
				{
					int i;
					int16_t *dst = wavebuf + (pos1<<1);
					uint8_t *src = (uint8_t *)dst;
					for (i=result-1; i>=0; i--)
					{
						dst[(i<<1) + 1] = dst[i<<1] = (src[i] | (((uint16_t)src[i]) << 8)) ^ 0x8080;
					}
				} else {
					int i;
					int16_t *dst = wavebuf + (pos1<<1);
					int16_t *src = dst;
					for (i=result-1; i>=0; i--)
					{
						dst[(i<<1) + 1] = dst[i<<1] = src[i];
					}

				}
			}
			cpifaceSession->ringbufferAPI->head_add_samples (wavebufpos, result);

			if ((wavepos+result) >= wavelen)
			{
				if (donotloop)
				{
					wav_looped |= 1;
					wavepos = wavelen;
					break;
				} else {
					wav_looped &= ~1;
					wavepos = 0;
					waveneedseek = 1;
				}
			} else {
				wavepos += result;
			}
		} else {
			break;
		}
	}
}

void  __attribute__ ((visibility ("internal"))) wpIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (wav_inpause || (wav_looped == 3))
	{
		plrAPI->Pause (1);
	} else {
		void *targetbuf;
		unsigned int targetlength; /* in samples */

		plrAPI->Pause (0);

		plrAPI->GetBuffer (&targetbuf, &targetlength);

		if (targetlength)
		{
			int16_t *t = targetbuf;
			unsigned int accumulated_target = 0;
			unsigned int accumulated_source = 0;
			int pos1, length1, pos2, length2;

			/* fill up our buffers */
			wpIdler(cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (wavebufpos, &pos1, &length1, &pos2, &length2);

			if (wavebufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2);
					wav_looped |= 2;
				} else {
					wav_looped &= ~2;
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

						rs = ((int16_t*)wavebuf)[(pos1<<1) + 0];
						ls = ((int16_t*)wavebuf)[(pos1<<1) + 1];

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
				wav_looped &= ~2;

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
							wav_looped |= 2;
							break;
						}
						/* will we overflow the wavebuf if we advance? */
						if ((length1+length2) < ((wavebufrate+wavebuffpos)>>16))
						{
							wav_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						lvm1 = (uint16_t)wavebuf[wpm1*2+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						rvm1 = (uint16_t)wavebuf[wpm1*2+1]^0x8000;
						 lc0 = (uint16_t)wavebuf[wp0*2+0]^0x8000;
						 rc0 = (uint16_t)wavebuf[wp0*2+1]^0x8000;
						 lv1 = (uint16_t)wavebuf[wp1*2+0]^0x8000;
						 rv1 = (uint16_t)wavebuf[wp1*2+1]^0x8000;
						 lv2 = (uint16_t)wavebuf[wp2*2+0]^0x8000;
						 rv2 = (uint16_t)wavebuf[wp2*2+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,wavebuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,wavebuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,wavebuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,wavebuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,wavebuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,wavebuffpos);
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

						wavebuffpos+=wavebufrate;
						progress = wavebuffpos>>16;
						wavebuffpos &= 0xffff;
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
			} /* if (wavebufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (wavebufpos, accumulated_source);
			plrAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	plrAPI->Idle();

	clipbusy--;
}

char __attribute__ ((visibility ("internal"))) wpLooped(void)
{
	return wav_looped == 3;
}

void __attribute__ ((visibility ("internal"))) wpSetLoop(uint8_t s)
{
	donotloop=!s;
}

void __attribute__ ((visibility ("internal"))) wpPause(uint8_t p)
{
	wav_inpause=p;
}

static void wpSetSpeed(uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	wavebufrate=imuldiv(256*sp, waverate, waveRate);
}

static void wpSetVolume (void)
{
	volr=voll=vol*4;
	if (bal<0)
		volr=(volr*(64+bal))>>6;
	else
		voll=(voll*(64-bal))>>6;
}

static void wpSet(int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			wpSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			wpSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			wpSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			wpSetVolume();
			break;
	}
}

static int wpGet(int ch, int opt)
{
	return 0;
}


uint32_t __attribute__ ((visibility ("internal"))) wpGetPos (struct cpifaceSessionAPI_t *cpifaceSession)
{
	return (wavepos + wavelen - cpifaceSession->ringbufferAPI->get_tail_available_samples (wavebufpos))%wavelen;
}

void __attribute__ ((visibility ("internal"))) wpGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct waveinfo *info)
{
	info->pos=wpGetPos(cpifaceSession);
	info->len=wavelen;
	info->rate=waverate;
	info->stereo=wavestereo;
	info->bit16=wave16bit;
	info->opt25=opt25;
	info->opt50=opt50;
}

void __attribute__ ((visibility ("internal"))) wpSetPos (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t pos)
{
	PRINT("wpSetPos called for pos %lu", (unsigned long)pos);

	pos=(pos+wavelen)%wavelen;

	waveneedseek=1;
	wavepos=pos;
	cpifaceSession->ringbufferAPI->reset (wavebufpos);
}

uint8_t __attribute__ ((visibility ("internal"))) wpOpenPlayer(struct ocpfilehandle_t *wavf, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;
	uint32_t temp;
	uint32_t fmtlen;
	uint16_t sh;

	if (!plrAPI)
	{
		return 0;
	}

	wavefile = wavf;
	wavefile->ref (wavefile);

	wavefile->seek_set (wavefile, 0);

	if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
	{
		fprintf(stderr, "[WAVE]: fread failed #1\n");
		goto error_out_wavefile;
	}
	PRINT("comparing header for RIFF: 0x%08x 0x%08x\n", temp, uint32_little(0x46464952));
	if (temp!=uint32_little(0x46464952))
	{
		fprintf (stderr, "[WAVE]: file does not have a RIFF header\n");
		goto error_out_wavefile;
	}

	if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
	{
		fprintf(stderr, "[WAVE] fread failed #2\n");
		goto error_out_wavefile;
	}
	PRINT("ignoring next 32bit: 0x%08x\n", temp);

	if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
	{
		fprintf(stderr, "[WAVE]: fread failed #3\n");
		goto error_out_wavefile;
	}
	PRINT("comparing next header for WAVE: 0x%08x 0x%08x\n", temp, uint32_little(0x45564157));

	if (temp!=uint32_little(0x45564157))
	{
		fprintf(stderr, "[WAVE]: file does not have a WAVE header\n");
		goto error_out_wavefile;
	}

	PRINT("going to locate \"fmt \" header\n");
	while (1)
	{
		if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
		{
			fprintf(stderr, "[WAVE]: fread failed #4\n");
			goto error_out_wavefile;
		}
		PRINT("checking 0x%08x 0x%08x\n", temp, uint32_little(0x20746d66));
		if (temp==uint32_little(0x20746D66))
			break;
		if (ocpfilehandle_read_uint32_le (wavefile, &temp))
		{
			fprintf(stderr, "[WAVE]: fread failed #5\n");
			goto error_out_wavefile;
		}
		PRINT("failed, skiping next %d bytes\n", temp);
		wavefile->seek_cur (wavefile, temp);
	}

	if (ocpfilehandle_read_uint32_le (wavefile, &fmtlen))
	{
		fprintf(stderr, "[WAVE]: fread failed #6\n");
		goto error_out_wavefile;
	}
	PRINT("fmtlen=%d (must be bigger or equal to 16)\n", fmtlen);
	if (fmtlen<16)
	{
		fprintf(stderr, "[WAVE]: format length %d < 16\n", fmtlen);
		goto error_out_wavefile;
	}

	if (ocpfilehandle_read_uint16_le (wavefile, &sh))
	{
		fprintf(stderr, "[WAVE]: fread failed #7\n");
		goto error_out_wavefile;
	}
	PRINT("compression code (only \"1 PCM\" is supported): \"%d %s\"\n", sh, compression_code_str(sh));
	if ((sh!=1))
	{
		fprintf(stderr, "[WAVE]: not uncomressed raw pcm data\n");
		goto error_out_wavefile;
	}

	if (ocpfilehandle_read_uint16_le (wavefile, &sh))
	{
		fprintf(stderr, "[WAVE]: fread failed #8\n");
		goto error_out_wavefile;
	}
	PRINT("number of channels: %u\n", sh);
	if ((sh==0)||(sh>2))
	{
		fprintf(stderr, "[WAVE]: unsupported number of channels: %u (must be mono or stereo)\n", sh);
		goto error_out_wavefile;
	}
	wavestereo=(sh==2);

	if (ocpfilehandle_read_uint32_le (wavefile, &waverate))
	{
		fprintf(stderr, "[WAVE]: fread failed #9\n");
		goto error_out_wavefile;
	}
	PRINT("waverate %d\n", (int)waverate);

	if (ocpfilehandle_read_uint32_le (wavefile, &temp))
	{
		fprintf(stderr, "[WAVE]: fread failed #10\n");
		goto error_out_wavefile;
	}
	PRINT("average number of bytes per second: %d\n", (int)temp);

	if (wavefile->read (wavefile, &sh, sizeof (sh)) != sizeof (sh))
	{
		fprintf(stderr, "[WAVE]: fread failed #11\n");
		goto error_out_wavefile;
	}
	PRINT("block align: %d\n", (int)sh);

	if (ocpfilehandle_read_uint16_le (wavefile, &sh))
	{
		fprintf(stderr, "[WAVE]: fread failed #12\n");
		goto error_out_wavefile;
	}
	PRINT("bits per sample: %d\n", (int)sh);

	if ((sh!=8)&&(sh!=16))
	{
		fprintf(stderr, "[WAVE]: unsupported bits per sample: %d\n", (int)sh);
		goto error_out_wavefile;
	}
	wave16bit=(sh==16);
	wavefile->seek_cur (wavefile, fmtlen - 16);

	PRINT("going to locate \"data\" header\n");
	while (1)
	{
		if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
		{
			fprintf(stderr, "[WAVE]: fread failed #13\n");
			goto error_out_wavefile;
		}
		PRINT("checking 0x%08x 0x%08x\n", temp, uint32_little(0x61746164));
		if (temp==uint32_little(0x61746164))
			break;
		if (ocpfilehandle_read_uint32_le (wavefile, &temp))
		{
			fprintf(stderr, "[WAVE]: fread failed #14\n");
			goto error_out_wavefile;
		}
		PRINT("failed, skiping next %d bytes\n", temp);
		wavefile->seek_cur (wavefile, temp);
	}

	if (ocpfilehandle_read_uint32_le (wavefile, &wavelen))
	{
		fprintf(stderr, "[WAVE]: fread failed #15\n");
		goto error_out_wavefile;
	}
	PRINT("datalength: %d\n", (int)wavelen);

	waveoffs = wavefile->getpos (wavefile);
	PRINT("waveoffs: %d\n", waveoffs);

	snprintf (opt25, sizeof (opt25), "PCM %dbit, %s", wave16bit?16:8, wavestereo?"stereo":"mono");
	snprintf (opt50, sizeof (opt50), "RIFF WAVE PCM %dbit integer, %s, %dHz", wave16bit?16:8, wavestereo?"stereo":"mono", waverate);

	if (!wavelen)
	{
		fprintf(stderr, "[WAVE]: no audio data\n");
		goto error_out_wavefile;
	}
	wavelen >>= (wave16bit + wavestereo);
	wavepos = 0;

	wavebuf=malloc(32*1024);
	if (!wavebuf)
	{
		fprintf(stderr, "[WAVE]: malloc failed\n");
		goto error_out_wavefile;
	}

	wavebufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, 8*1024);

	waveRate = waverate;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!plrAPI->Play (&waveRate, &format, wavf, cpifaceSession))
	{
		fprintf(stderr, "playwav: plrOpenPlayer() failed\n");
		goto error_out_wavebuf;
	}

	wavebufrate=imuldiv(65536, waverate, waveRate);
	wavebuffpos = 0;
	waveneedseek = 0;

	wav_inpause=0;
	wav_looped=0;

	active=1;

	cpifaceSession->mcpSet = wpSet;
	cpifaceSession->mcpGet = wpGet;

	cpifaceSession->mcpAPI->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	return 1;

	//plrAPI->Stop();
error_out_wavebuf:
	free (wavebuf);
	wavebuf=0;
error_out_wavefile:
	wavefile->unref (wavefile);
	wavefile = 0;

	return 0;
}

void __attribute__ ((visibility ("internal"))) wpClosePlayer(struct cpifaceSessionAPI_t *cpifaceSession)
{
	active=0;

	PRINT("Freeing resources\n");

	plrAPI->Stop();

	if (wavebufpos)
	{
		cpifaceSession->ringbufferAPI->free (wavebufpos);
		wavebufpos = 0;
	}

	if (wavebuf)
	{
		free(wavebuf);
		wavebuf = 0;
	}

	if (wavefile)
	{
		wavefile->unref (wavefile);
		wavefile = 0;
	}
}
