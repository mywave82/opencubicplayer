/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include "types.h"
#include "stuff/imsrtns.h"
#include "stuff/timer.h"
#include "stuff/poll.h"
#include "dev/player.h"
#include "dev/deviplay.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"

#include "oggplay.h"

static int current_section;

static int stereo; /* 32 bit booleans are fast */
static int bit16;
static int signedout;
static uint32_t samprate;
static uint8_t reversestereo;

/* static unsigned long amplify; TODO */
static unsigned long voll,volr;
static int pan;
static int srnd;

static int16_t *buf16=NULL;
static uint32_t bufpos;
static uint32_t buflen;
static void *plrbuf;

/*static uint32_t amplify;*/

static OggVorbis_File ov;
static int oggstereo;
static int oggrate;
static uint32_t oggpos; /* absolute sample position in the source stream */
static uint32_t ogglen; /* absolute length in samples positions of the source stream */
static int oggneedseek;

static int16_t *oggbuf=NULL;
static struct ringbuffer_t *oggbufpos = 0;
//static uint32_t oggbuflen;
//static uint32_t oggbufpos;
static uint_fast32_t oggbuffpos;
//static int32_t oggbufread;
static uint_fast32_t oggbufrate;
static volatile int active;
static int looped;
static int donotloop;

static int inpause;

static volatile int clipbusy=0;

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

static void oggIdler(void)
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
			fprintf (stderr, "[playogg]: warning, frame position is broken in file (got=%ld, expected=%d)\n", ov_pcm_tell(&ov), oggpos);
		}

		ringbuffer_get_head_bytes (oggbufpos, &pos1, &length1, &pos2, &length2);

		if (!length1)
		{
			return;
		}
		read = length1;

		/* check if we are going to read until EOF, and if so, do we allow loop or not */
		if ((oggpos+(read>>(1+oggstereo)))>=ogglen)
		{
			read=(ogglen-oggpos)<<(1+oggstereo);
		}

		if (read)
		{
#ifndef WORDS_BIGENDIAN
			result=ov_read(&ov, (char *)oggbuf+pos1, read, 0, 2, 1, &current_section);
#else
			result=ov_read(&ov, (char *)oggbuf+pos1, read, 1, 2, 1, &current_section);
#endif

			if (result<=0) /* broken data... we can survive */
			{
				memsetw((char *)oggbuf+pos1, 0x0000, read>>1);
				fprintf (stderr, "[playogg] ov_read failed: %ld\n", result);
				result=read;
			}

			ringbuffer_head_add_bytes (oggbufpos, result);
		}

		if ((oggpos+(result>>(1+oggstereo))) >= ogglen)
		{
			if (donotloop)
			{
				looped |= 1;
				oggpos = ogglen;
				break;
			} else {
				looped &= ~1;
				oggpos = 0;
				oggneedseek = 1;
			}
		} else {
			oggpos += result>>(1+oggstereo);
		}
	}
}

void __attribute__ ((visibility ("internal"))) oggIdle(void)
{
	uint32_t bufdelta;
	uint32_t pass2;

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
	oggIdler();

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
		int buf1, len1, buf2, len2;
		int len;
		int i;
		int buf16_filled = 0;

		/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
		ringbuffer_get_tail_samples (oggbufpos, &buf1, &len1, &buf2, &len2);

		len = len1 + len2;

		/* are the speed 1:1, if so filling up buf16 is very easy */
		if (oggbufrate==0x10000)
		{
			int16_t rs, ls;
			int16_t *t = buf16;

			if (oggstereo)
			{
				while (bufdelta && len)
				{
					rs = oggbuf[buf1<<1];
					ls = oggbuf[(buf1<<1) + 1];

					PANPROC;

					*(t++) = rs;
					*(t++) = ls;

					buf16_filled++;
					len--;
					bufdelta--;
					buf1++;

					if (!--len1)
					{
						buf1 = buf2;
						len1 = len2;
						buf2 = -1;
						len2 = 0;
					}
				}
			} else {
				while (bufdelta && len)
				{
					rs = ls = oggbuf[buf1];

					PANPROC;

					*(t++) = rs;
					*(t++) = ls;

					buf16_filled++;
					len--;
					bufdelta--;
					buf1++;

					if (!--len1)
					{
						buf1 = buf2;
						len1 = len2;
						buf2 = -1;
						len2 = 0;
					}
				}
			}
			ringbuffer_tail_consume_samples (oggbufpos, buf16_filled); /* add this rate buf16_filled == tail_used */
		} else {
			/* We are going to perform cubic interpolation of rate conversion... this bit is tricky */
			int tail_used = 0;
			int16_t rs, ls;

			uint_fast32_t oggbuffpos_new = oggbuffpos, len_consume;

			oggbuffpos_new = oggbuffpos + oggbufrate;
			len_consume = oggbuffpos_new >> 16;
			oggbuffpos_new &= 0xffff;

			len-=3; /* we need to look 3 samples into the future - or if we wanted to be perfect 1 sample into the passed and 2 into the future */
			if (oggstereo)
			{
				while (bufdelta && (len > len_consume))
				{
					int32_t rc0, rc1, rc2, rc3, rvm1,rv1,rv2;
					int32_t lc0, lc1, lc2, lc3, lvm1,lv1,lv2;

					switch (len1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
					{
						default:
							rvm1 = (uint16_t)oggbuf[(buf1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
							lvm1 = (uint16_t)oggbuf[(buf1<<1)+1]^0x8000;
							 rc0 = (uint16_t)oggbuf[(buf1<<1)+2]^0x8000;
							 lc0 = (uint16_t)oggbuf[(buf1<<1)+3]^0x8000;
							 rv1 = (uint16_t)oggbuf[(buf1<<1)+4]^0x8000;
							 lv1 = (uint16_t)oggbuf[(buf1<<1)+5]^0x8000;
							 rv2 = (uint16_t)oggbuf[(buf1<<1)+6]^0x8000;
							 lv2 = (uint16_t)oggbuf[(buf1<<1)+7]^0x8000;
							break;
						case 3:
							rvm1 = (uint16_t)oggbuf[(buf1<<1)+0]^0x8000;
							lvm1 = (uint16_t)oggbuf[(buf1<<1)+1]^0x8000;
							 rc0 = (uint16_t)oggbuf[(buf1<<1)+2]^0x8000;
							 lc0 = (uint16_t)oggbuf[(buf1<<1)+3]^0x8000;
							 rv1 = (uint16_t)oggbuf[(buf1<<1)+4]^0x8000;
							 lv1 = (uint16_t)oggbuf[(buf1<<1)+5]^0x8000;
							 rv2 = (uint16_t)oggbuf[(buf2<<1)+0]^0x8000;
							 lv2 = (uint16_t)oggbuf[(buf2<<1)+1]^0x8000;
							break;
						case 2:
							rvm1 = (uint16_t)oggbuf[(buf1<<1)+0]^0x8000;
							lvm1 = (uint16_t)oggbuf[(buf1<<1)+1]^0x8000;
							 rc0 = (uint16_t)oggbuf[(buf1<<1)+2]^0x8000;
							 lc0 = (uint16_t)oggbuf[(buf1<<1)+3]^0x8000;
							 rv1 = (uint16_t)oggbuf[(buf2<<1)+0]^0x8000;
							 lv1 = (uint16_t)oggbuf[(buf2<<1)+1]^0x8000;
							 rv2 = (uint16_t)oggbuf[(buf2<<1)+2]^0x8000;
							 lv2 = (uint16_t)oggbuf[(buf2<<1)+3]^0x8000;
							break;
						case 1:
							rvm1 = (uint16_t)oggbuf[(buf1<<1)+0]^0x8000;
							lvm1 = (uint16_t)oggbuf[(buf1<<1)+1]^0x8000;
							 rc0 = (uint16_t)oggbuf[(buf2<<1)+0]^0x8000;
							 lc0 = (uint16_t)oggbuf[(buf2<<1)+1]^0x8000;
							 rv1 = (uint16_t)oggbuf[(buf2<<1)+2]^0x8000;
							 lv1 = (uint16_t)oggbuf[(buf2<<1)+3]^0x8000;
							 rv2 = (uint16_t)oggbuf[(buf2<<1)+4]^0x8000;
							 lv2 = (uint16_t)oggbuf[(buf2<<1)+5]^0x8000;
							break;
					}

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

					buf16[(buf16_filled<<1)+0] = rs;
					buf16[(buf16_filled<<1)+1] = ls;

					buf16_filled++;
					bufdelta--;

					{
						oggbuffpos+=oggbufrate;

						tail_used += len_consume;
						len -= len_consume;

						if (len_consume >= len1)
						{
							len_consume -= len1;

							buf1 = buf2 + len_consume;
							len1 = len2 - len_consume;
							buf2 = -1;
							len2 = 0;
						} else {
							len1 -= len_consume;
							buf1 += len_consume;
						}

						oggbuffpos = oggbuffpos_new;

						oggbuffpos_new = oggbuffpos + oggbufrate;
						len_consume = oggbuffpos_new >> 16;
						oggbuffpos_new &= 0xffff;
					}
				}
			} else {
				while (bufdelta && (len > len_consume))
				{
					int32_t c0, c1, c2, c3, vm1,v1,v2;

					/* we temporary need data to be unsigned */
					switch (len1)
					{
						default:
							vm1 = oggbuf[buf1  ]^0x8000;
							 c0 = oggbuf[buf1+1]^0x8000;
							 v1 = oggbuf[buf1+2]^0x8000;
							 v2 = oggbuf[buf1+3]^0x8000;
							break;
						case 3:
							vm1 = oggbuf[buf1  ]^0x8000;
							 c0 = oggbuf[buf1+1]^0x8000;
							 v1 = oggbuf[buf1+2]^0x8000;
							 v2 = oggbuf[buf2  ]^0x8000;
							break;
						case 2:
							vm1 = oggbuf[buf1  ]^0x8000;
							 c0 = oggbuf[buf1+1]^0x8000;
							 v1 = oggbuf[buf2  ]^0x8000;
							 v2 = oggbuf[buf2+1]^0x8000;
							break;
						case 1:
							vm1 = oggbuf[buf1  ]^0x8000;
							 c0 = oggbuf[buf2  ]^0x8000;
							 v1 = oggbuf[buf2+1]^0x8000;
							 v2 = oggbuf[buf2+2]^0x8000;
							break;
					}

					c1 = v1-vm1;
					c2 = 2*vm1-2*c0+v1-v2;
					c3 = c0-vm1-v1+v2;
					c3 =  imulshr16(c3,oggbuffpos);
					c3 += c2;
					c3 =  imulshr16(c3,oggbuffpos);
					c3 += c1;
					c3 =  imulshr16(c3,oggbuffpos);
					c3 += c0;
					if (c3<0)
						c3=0;
					if (c3>65535)
						c3=65535;

					rs = ls = c3 ^ 0x8000;

					PANPROC;

					buf16[(buf16_filled<<1)+0] = rs;
					buf16[(buf16_filled<<1)+1] = ls;

					buf16_filled++;
					bufdelta--;

					{
						oggbuffpos+=oggbufrate;

						tail_used += len_consume;
						len -= len_consume;

						if (len_consume >= len1)
						{
							len_consume -= len1;

							buf1 = buf2 + len_consume;
							len1 = len2 - len_consume;
							buf2 = -1;
							len2 = 0;
						} else {
							len1 -= len_consume;
							buf1 += len_consume;
						}

						oggbuffpos = oggbuffpos_new;

						oggbuffpos_new = oggbuffpos + oggbufrate;
						len_consume = oggbuffpos_new >> 16;
						oggbuffpos_new &= 0xffff;
					}
				}
			}
			ringbuffer_tail_consume_samples (oggbufpos, tail_used);
		}

		if (buf16_filled)
		{
			looped &= ~2;
		} else {
			looped |= 2;
		}

#warning instead of pass2 logic, we could initially make bufdelta only use first or second segment!!!
		if ((bufpos+buf16_filled)>buflen)
			pass2=bufpos+buf16_filled-buflen;
		else
			pass2=0;

		buf16_filled-=pass2;
		if (bit16)
		{
			if (stereo)
			{
				int16_t *p=(int16_t *)plrbuf+2*bufpos;
				int16_t *b=buf16;
				if (signedout)
				{
					for (i=0; i<buf16_filled; i++)
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
					for (i=0; i<buf16_filled; i++)
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
					for (i=0; i<buf16_filled; i++)
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
					for (i=0; i<buf16_filled; i++)
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
					for (i=0; i<buf16_filled; i++)
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
					for (i=0; i<buf16_filled; i++)
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
					for (i=0; i<buf16_filled; i++)
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
					for (i=0; i<buf16_filled; i++)
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
		bufpos+=buf16_filled+pass2;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	plrAdvanceTo(bufpos<<(stereo+bit16));

	if (plrIdle)
		plrIdle();

	clipbusy--;
}

void __attribute__ ((visibility ("internal"))) oggSetAmplify(uint32_t amp)
{
/*
	amplify=amp;
	float v[9];
	float ampf=(float)vols[9]*(amplify/65536.0)/65536.0;
	int i;
	for (i=0; i<9; i++)
		v[i]=ampf*vols[i];
	rawogg.ioctl(ampegdecoder::ioctlsetstereo, v, 4*9);
	*/
}

static int close_func(void *datasource)
{
	return 0;
}

int __attribute__ ((visibility ("internal"))) oggOpenPlayer(FILE *oggf)
{
	struct vorbis_info *vi;
	long res;
	if (!plrPlay)
		return 0;

	fseek(oggf, 0, SEEK_SET);
	if(ov_open(oggf, &ov, NULL, -1) < 0)
		return -1; /* we don't bother to do more exact */
	ov.callbacks.close_func=close_func;


	vi=ov_info(&ov,-1);
	oggstereo=vi->channels-1; //vi->channels>=2;
	oggrate=vi->rate;

	plrSetOptions(oggrate, (PLR_SIGNEDOUT|PLR_16BIT)|((oggstereo)?PLR_STEREO:0));
	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);
	samprate=plrRate;

	oggbufrate=imuldiv(65536, oggrate, samprate);

	ogglen=ov_pcm_total(&ov, -1);
	if (!ogglen)
		return 0;

	oggbuf=malloc(4096 * 4);
	if (!oggbuf)
		return 0;
	oggbufpos = ringbuffer_new_samples ((oggstereo?RINGBUFFER_FLAGS_STEREO:0) | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, 4096);
	if (!oggbufpos)
	{
		free(oggbuf);
		oggbuf = 0;
		return 0;
	}
	oggbuffpos=0;
	current_section=0;
	oggneedseek=0;
#ifdef WORDS_BIGENDIAN
	if ((res=ov_read(&ov, (char *)oggbuf, 4096*4-1024, 1, 2, 1, &current_section))<0)
#else
	if ((res=ov_read(&ov, (char *)oggbuf, 4096*4-1024, 0, 2, 1, &current_section))<0)
#endif
	{
		oggpos=0;
	} else {
		oggpos=res>>(1 + oggstereo);
		ringbuffer_head_add_bytes (oggbufpos, res);
	}

	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize))
	{
		ringbuffer_free (oggbufpos);
		oggbufpos = 0;

		free(oggbuf);
		oggbuf = 0;
		return 0;
	}

	inpause=0;
	looped=0;
	oggSetVolume(64, 0, 64, 0);
/*
	oggSetAmplify(amplify);   TODO */

	buf16=malloc(sizeof(uint16_t)*buflen*2);
	if (!buf16)
	{
		plrClosePlayer();

		ringbuffer_free (oggbufpos);
		oggbufpos = 0;

		free(oggbuf);
		oggbuf = 0;
		return 0;
	}
	bufpos=0;

	if (!pollInit(oggIdle))
	{
		plrClosePlayer();

		free (buf16);
		buf16 = 0;

		ringbuffer_free (oggbufpos);
		oggbufpos = 0;

		free(oggbuf);
		oggbuf = 0;

		return 0;
	}
	active=1;

	return 1;
}

void __attribute__ ((visibility ("internal"))) oggClosePlayer(void)
{
	active=0;

	pollClose();

	plrClosePlayer();

	ringbuffer_free (oggbufpos);
	oggbufpos = 0;

	free(oggbuf);
	oggbuf=NULL;

	free(buf16);
	buf16=NULL;

	ov_clear(&ov);
}

char __attribute__ ((visibility ("internal"))) oggLooped(void)
{
	return looped == 3;
}

void __attribute__ ((visibility ("internal"))) oggSetLoop(uint8_t s)
{
	donotloop=!s;
}

void __attribute__ ((visibility ("internal"))) oggPause(uint8_t p)
{
	inpause=p;
}

void __attribute__ ((visibility ("internal"))) oggSetSpeed(uint16_t sp)
{
	if (sp<32)
		sp=32;
	oggbufrate=imuldiv(256*sp, oggrate, samprate);
}

void __attribute__ ((visibility ("internal"))) oggSetVolume(uint8_t vol_, int8_t bal_, int8_t pan_, uint8_t opt)
{
	pan=pan_;
	if (reversestereo)
	{
		pan = -pan;
	}
	volr=voll=vol_*4;
	if (bal_<0)
		volr=(volr*(64+bal_))>>6;
	else
		voll=(voll*(64-bal_))>>6;
	srnd=opt;
}

uint32_t __attribute__ ((visibility ("internal"))) oggGetPos(void)
{
#warning TODO, deduct devp buffer!
	return (oggpos+ogglen-ringbuffer_get_tail_available_samples(oggbufpos))%ogglen;
}

void __attribute__ ((visibility ("internal"))) oggGetInfo(struct ogginfo *i)
{
	static int lastsafe=0;
	i->pos=oggGetPos();
	i->len=ogglen;
	i->rate=oggrate;
	i->stereo=oggstereo;
	i->bit16=1;
	if ((i->bitrate=ov_bitrate_instant(&ov))<0)
		i->bitrate=lastsafe;
	else
		lastsafe=i->bitrate;
	i->bitrate/=1000;
}

void __attribute__ ((visibility ("internal"))) oggSetPos(uint32_t pos)
{
	pos=(pos+ogglen)%ogglen;

	oggneedseek=1;
	oggpos=pos;
	ringbuffer_reset(oggbufpos);
}

