/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '05-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "dev/deviplay.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "stuff/imsrtns.h"
#include "stuff/poll.h"
#include "wave.h"

#ifdef WAVE_DEBUG
# define PRINT(fmt, args...) fprintf(stderr, "%s %s: " fmt, __FILE__, __func__, ##args)
#else
# define PRINT(a, ...) do {} while(0)
#endif

static uint8_t stereo;
static uint8_t bit16;
static uint8_t signedout;
static uint32_t samprate;
static uint8_t reversestereo;

static int16_t  *buf16=0;
static uint32_t bufpos;
static uint32_t buflen;
static void *plrbuf;

static int vol, bal;
static uint32_t voll,volr;
static int pan;
static int srnd;

static struct ocpfilehandle_t *wavefile;
#define rawwave wavefile

static int wavestereo;
static int wave16bit;
static uint32_t waverate;
static uint32_t wavepos;
static uint32_t wavelen;
static int waveneedseek;

static uint32_t waveoffs;
static  int16_t *wavebuf=0;
static struct ringbuffer_t *wavebufpos = 0;
static uint32_t wavebuffpos;
static uint32_t wavebufrate;

static volatile int active;
static int looped;
static int donotloop;

static int inpause;

static volatile int clipbusy=0;

static int (*_GET)(int ch, int opt);
static void (*_SET)(int ch, int opt, int val);

#ifdef WAVE_DEBUG
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


static void wpIdler(void)
{
	if (!active)
		return;

	while (1)
	{
		size_t read;
		long result = 0;
		int pos1, pos2;
		int length1, length2;

		ringbuffer_get_head_samples (wavebufpos, &pos1, &length1, &pos2, &length2);

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
/* The wavebuffer should always be 16bit stereo, so expand if needed */
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
			ringbuffer_head_add_samples (wavebufpos, result);

			if ((wavepos+result) >= wavelen)
			{
				if (donotloop)
				{
					looped |= 1;
					wavepos = wavelen;
					break;
				} else {
					looped &= ~1;
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

void  __attribute__ ((visibility ("internal"))) wpIdle(void)
{
	uint_fast32_t bufdelta;
	uint_fast32_t pass2;

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	{
		uint32_t bufplayed;

		bufplayed=plrGetBufPos()>>(stereo+bit16);

		bufdelta=(buflen+bufplayed-bufpos)%buflen;
	}

	if (!bufdelta)
	{
		clipbusy--;
		if (plrIdle)
			plrIdle();
		return;
	}
	wpIdler();


	if (inpause)
	{ /* If we are in pause, we fill buffer with the correct type of zeroes */
		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		if (bit16)
		{
			plrClearBuf((uint16_t *)plrbuf+(bufpos<<stereo), (bufdelta-pass2)<<stereo, signedout);
			if (pass2)
				plrClearBuf((uint16_t *)plrbuf, pass2<<stereo, signedout);
		} else {
			plrClearBuf(buf16, bufdelta<<stereo, signedout);
			plr16to8((uint8_t *)plrbuf+(bufpos<<stereo), (uint16_t *)buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((uint8_t *)plrbuf, (uint16_t *)buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
		}
		bufpos+=bufdelta;
		if (bufpos>=buflen)
			bufpos-=buflen;
	} else {
		int pos1, length1, pos2, length2;
		int i;
		unsigned int buf16_filled = 0;

		/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
		ringbuffer_get_tail_samples (wavebufpos, &pos1, &length1, &pos2, &length2);

		/* are the speed 1:1, if so filling up buf16 is very easy */
		if (wavebufrate==0x10000)
		{
			int16_t *t = buf16;

			if (bufdelta>(length1+length2))
			{
				bufdelta=(length1+length2);
				looped |= 2;

			} else {
				looped &= ~2;
			}

			//while (buf16_filled < bufdelta)
			for (buf16_filled=0; buf16_filled<bufdelta; buf16_filled++)
			{
				int16_t rs;
				int16_t ls;

				if (!length1)
				{
					pos1 = pos2;
					length1 = length2;
					pos2 = 0;
					length2 = 0;
				}

				if (!length1)
				{
					fprintf (stderr, "playwav: ERROR, length1 == 0, in wpIdle\n");
					_exit(1);
				}

				rs = ((int16_t*)wavebuf)[(pos1<<1)    ];
				ls = ((int16_t*)wavebuf)[(pos1<<1) + 1];

				PANPROC;

				*(t++) = rs;
				*(t++) = ls;

				pos1++;
				length1--;
			}

			ringbuffer_tail_consume_samples (wavebufpos, buf16_filled); /* add this rate buf16_filled == tail_used */
		} else {
			/* We are going to perform cubic interpolation of rate conversion... this bit is tricky */
			unsigned int accumulated_progress = 0;

			looped &= ~2;

			for (buf16_filled=0; buf16_filled<bufdelta; buf16_filled++)
			{
				uint32_t wpm1, wp0, wp1, wp2;
				int32_t rc0, rc1, rc2, rc3, rvm1,rv1,rv2;
				int32_t lc0, lc1, lc2, lc3, lvm1,lv1,lv2;
				unsigned int progress;
				int16_t rs, ls;

				/* will the interpolation overflow? */
				if ((length1+length2) <= 3)
				{
					looped |= 2;
					break;
				}
				/* will we overflow the wavebuf if we advance? */
				if ((length1+length2) < ((wavebufrate+wavebuffpos)>>16))
				{
					looped |= 2;
					break;
				}

				switch (length1)
				{
					case 1:
						wpm1 = pos1;
						wp0  = pos2;
						wp1  = pos2+1;
						wp2  = pos2+2;
						break;
					case 2:
						wpm1 = pos1;
						wp0  = pos1+1;
						wp1  = pos2;
						wp2  = pos2+1;
						break;
					case 3:
						wpm1 = pos1;
						wp0  = pos1+1;
						wp1  = pos1+2;
						wp2  = pos2;
						break;
					default:
						wpm1 = pos1;
						wp0  = pos1+1;
						wp1  = pos1+2;
						wp2  = pos1+3;
						break;
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

				buf16[buf16_filled*2+0] = rs;
				buf16[buf16_filled*2+1] = ls;

				wavebuffpos+=wavebufrate;
				progress = wavebuffpos>>16;
				wavebuffpos &= 0xffff;

				accumulated_progress += progress;

				/* did we wrap? if so, progress up to the wrapping point */
				if (progress >= length1)
				{
					progress -= length1;
					pos1 = pos2;
					length1 = length2;
					pos2 = 0;
					length2 = 0;
				}
				if (progress)
				{
					pos1 += progress;
					length1 -= progress;
				}
			}
			ringbuffer_tail_consume_samples (wavebufpos, accumulated_progress);
		}

		bufdelta=buf16_filled;

		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		bufdelta-=pass2;

		if (bit16)
		{
			if (stereo)
			{
				int16_t *p=(int16_t *)plrbuf+2*bufpos;
				int16_t *b=buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0];
						p[1]=b[1];
						p+=2;
						b+=2;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0];
						p[1]=b[1];
						p+=2;
						b+=2;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0]^0x8000;
						p[1]=b[1]^0x8000;
						p+=2;
						b+=2;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0]^0x8000;
						p[1]=b[1]^0x8000;
						p+=2;
						b+=2;
					}
				}
			} else {
				int16_t *p=(int16_t *)plrbuf+bufpos;
				int16_t *b=buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0];
						p++;
						b++;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0];
						p++;
						b++;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0]^0x8000;
						p++;
						b++;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0]^0x8000;
						p++;
						b++;
					}
				}
			}
		} else {
			if (stereo)
			{
				uint8_t *p=(uint8_t *)plrbuf+2*bufpos;
				int16_t *b=buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0]>>8;
						p[1]=b[1]>>8;
						p+=2;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0]>>8;
						p[1]=b[1]>>8;
						p+=2;
						b+=2;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=(b[0]>>8)^0x80;
						p[1]=(b[0]>>8)^0x80;
						p+=2;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=(b[0]>>8)^0x80;
						p[1]=(b[1]>>8)^0x80;
						p+=2;
						b+=2;
					}
				}
			} else {
				uint8_t *p=(uint8_t *)plrbuf+bufpos;
				int16_t *b=buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=(b[0]+b[1])>>9;
						p++;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[1];
						p++;
						b+=2;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=((b[0]+b[1])>>9)^0x80;
						p++;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=((b[0]+b[1])>>9)^0x80;
						p++;
						b+=2;
					}
				}
			}
		}
		bufpos+=buf16_filled;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	plrAdvanceTo(bufpos<<(stereo+bit16));

	if (plrIdle)
		plrIdle();

	clipbusy--;
}

char __attribute__ ((visibility ("internal"))) wpLooped(void)
{
	return looped == 3;
}

void __attribute__ ((visibility ("internal"))) wpSetLoop(uint8_t s)
{
	donotloop=!s;
}

void __attribute__ ((visibility ("internal"))) wpPause(uint8_t p)
{
	inpause=p;
}

static void wpSetSpeed(uint16_t sp)
{
	if (sp<32)
		sp=32;
	wavebufrate=imuldiv(256*sp, waverate, samprate);
}

static void wpSetVolume (void)
{
	volr=voll=vol*4;
	if (bal<0)
		volr=(volr*(64+bal))>>6;
	else
		voll=(voll*(64-bal))>>6;
}

static void SET(int ch, int opt, int val)
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
			if (reversestereo)
			{
				pan = -pan;
			}
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
static int GET(int ch, int opt)
{
	return 0;
}


uint32_t __attribute__ ((visibility ("internal"))) wpGetPos(void)
{
	return (wavepos + wavelen - ringbuffer_get_tail_available_samples (wavebufpos))%wavelen;
}

void __attribute__ ((visibility ("internal"))) wpGetInfo(struct waveinfo *i)
{
	i->pos=wpGetPos();
	i->len=wavelen;
	i->rate=waverate;
	i->stereo=wavestereo;
	i->bit16=wave16bit;
}

void __attribute__ ((visibility ("internal"))) wpSetPos(uint32_t pos)
{
	PRINT("wpSetPos called for pos %lu", (unsigned long)pos);

	pos=(pos+wavelen)%wavelen;

	waveneedseek=1;
	wavepos=pos;
	ringbuffer_reset(wavebufpos);
}

uint8_t __attribute__ ((visibility ("internal"))) wpOpenPlayer(struct ocpfilehandle_t *wavf)
{
	uint32_t temp;
	uint32_t fmtlen;
	uint16_t sh;

	if (!plrPlay)
		return 0;

	if (wavefile)
	{
		wavefile->unref (wavefile);
		wavefile = 0;
	}
	wavefile = wavf;
	wavefile->ref (wavefile);

	wavefile->seek_set (wavefile, 0);

	if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
	{
		fprintf(stderr, __FILE__ ": fread failed #1\n");
		return 0;
	}
	PRINT("comparing header for RIFF: 0x%08x 0x%08x\n", temp, uint32_little(0x46464952));
	if (temp!=uint32_little(0x46464952))
		return 0;

	if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
	{
		fprintf(stderr, __FILE__ ": fread failed #2\n");
		return 0;
	}
	PRINT("ignoring next 32bit: 0x%08x\n", temp);

	if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
	{
		fprintf(stderr, __FILE__ ": fread failed #3\n");
		return 0;
	}
	PRINT("comparing next header for WAVE: 0x%08x 0x%08x\n", temp, uint32_little(0x45564157));

	if (temp!=uint32_little(0x45564157))
		return 0;

	PRINT("going to locate \"fmt \" header\n");
	while (1)
	{
		if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
		{
			fprintf(stderr, __FILE__ ": fread failed #4\n");
			return 0;
		}
		PRINT("checking 0x%08x 0x%08x\n", temp, uint32_little(0x20746d66));
		if (temp==uint32_little(0x20746D66))
			break;
		if (ocpfilehandle_read_uint32_le (wavefile, &temp))
		{
			fprintf(stderr, __FILE__ ": fread failed #5\n");
			return 0;
		}
		PRINT("failed, skiping next %d bytes\n", temp);
		wavefile->seek_cur (wavefile, temp);
	}
	if (ocpfilehandle_read_uint32_le (wavefile, &fmtlen))
	{
		fprintf(stderr, __FILE__ ": fread failed #6\n");
		return 0;
	}

	PRINT("fmtlen=%d (must be bigger or equal to 16)\n", fmtlen);
	if (fmtlen<16)
		return 0;
	if (ocpfilehandle_read_uint16_le (wavefile, &sh))
	{
		fprintf(stderr, __FILE__ ": fread failed #7\n");
		return 0;
	}
	PRINT("compression code (only 1/pcm is supported): %d %s\n", sh, compression_code_str(sh));
	if ((sh!=1))
	{
		fprintf(stderr, __FILE__ ": not uncomressed raw pcm data\n");
		return 0;
	}

	if (ocpfilehandle_read_uint16_le (wavefile, &sh))
	{
		fprintf(stderr, __FILE__ ": fread failed #8\n");
		return 0;
	}
	PRINT("number of channels: %d\n", (int)sh);
	if ((sh==0)||(sh>2))
	{
		fprintf(stderr, __FILE__ ": unsupported number of channels: %d\n", sh);
		return 0;
	}
	wavestereo=(sh==2);

	if (ocpfilehandle_read_uint32_le (wavefile, &waverate))
	{
		fprintf(stderr, __FILE__ ": fread failed #9\n");
		return 0;
	}
	PRINT("waverate %d\n", (int)waverate);

	if (ocpfilehandle_read_uint32_le (wavefile, &temp))
	{
		fprintf(stderr, __FILE__ ": fread failed #10\n");
		return 0;
	}
	PRINT(stderr, __FILE__ ": average number of bytes per second: %d\n", (int)temp);

	if (wavefile->read (wavefile, &sh, sizeof (sh)) != sizeof (sh))
	{
		fprintf(stderr, __FILE__ ": fread failed #11\n");
		return 0;
	}
	PRINT("block align: %d\n", (int)sh);

	if (ocpfilehandle_read_uint16_le (wavefile, &sh))
	{
		fprintf(stderr, __FILE__ ": fread failed #12\n");
		return 0;
	}
	PRINT("bits per sample: %d\n", (int)sh);

	if ((sh!=8)&&(sh!=16))
	{
		fprintf(stderr, __FILE__ ": unsupported bits per sample: %d\n", (int)sh);
		return 0;
	}
	wave16bit=(sh==16);
	wavefile->seek_cur (wavefile, fmtlen - 16);

	PRINT("going to locate \"data\" header\n");
	while (1)
	{
		if (wavefile->read (wavefile, &temp, sizeof (temp)) != sizeof (temp))
		{
			fprintf(stderr, __FILE__ ": fread failed #13\n");
			return 0;
		}
		PRINT("checking 0x%08x 0x%08x\n", temp, uint32_little(0x61746164));
		if (temp==uint32_little(0x61746164))
			break;
		if (ocpfilehandle_read_uint32_le (wavefile, &temp))
		{
			fprintf(stderr, __FILE__ ": fread failed #14\n");
			return 0;
		}
		PRINT("failed, skiping next %d bytes\n", temp);
		wavefile->seek_cur (wavefile, temp);
	}

	if (ocpfilehandle_read_uint32_le (wavefile, &wavelen))
	{
		fprintf(stderr, __FILE__ ": fread failed #15\n");
		return 0;
	}
	PRINT("datalength: %d\n", (int)wavelen);

	waveoffs = wavefile->getpos (wavefile);
	PRINT("waveoffs: %d\n", waveoffs);

	if (!wavelen)
	{
		fprintf(stderr, __FILE__ ": no data\n");
		return 0;
	}

	wavebuf=malloc(16*1024);
	if (!wavebuf)
	{
		return 0;
	}

	wavelen >>= (wave16bit + wavestereo);
	wavebufpos = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, 4*1024);

	wavepos = 0;

	plrSetOptions(waverate, PLR_STEREO|PLR_16BIT);

	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize * plrRate / 1000, wavf))
	{
		goto undowavebuf;
	}

	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);
	samprate=plrRate;

	wavebufrate=imuldiv(65536, waverate, samprate);
	wavebuffpos = 0;
	waveneedseek = 0;

	inpause=0;
	looped=0;

	buf16=malloc(sizeof(uint16_t)*(buflen*2));
	if (!buf16)
	{
		goto undoopen;
	}
	bufpos=0;

	if (!pollInit(wpIdle))
	{
		goto undobuf16;
	}

	active=1;

	_SET=mcpSet;
	_GET=mcpGet;
	mcpSet=SET;
	mcpGet=GET;
	mcpNormalize (mcpNormalizeDefaultPlayP);

	return 1;

undobuf16:
	free (buf16);
	buf16=0;
undoopen:
	plrClosePlayer();
undowavebuf:
	free (wavebuf);
	wavebuf=0;

	return 0;
}

void __attribute__ ((visibility ("internal"))) wpClosePlayer(void)
{
	active=0;

	PRINT("Freeing resources\n");

	pollClose();

	plrClosePlayer();

	if (wavebufpos)
	{
		ringbuffer_free (wavebufpos);
		wavebufpos = 0;
	}

	if (wavebuf)
	{
		free(wavebuf);
		wavebuf = 0;
	}
	if (buf16)
	{
		free(buf16);
		buf16 = 0;
	}

	if (wavefile)
	{
		wavefile->unref (wavefile);
		wavefile = 0;
	}

	if (_SET)
	{
		mcpSet = _SET;
		_SET = 0;
	}
	if (_GET)
	{
		mcpGet = _GET;
		_GET = 0;
	}
}
