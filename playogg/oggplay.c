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

static uint16_t *buf16=NULL;
static uint32_t bufpos;
static uint32_t buflen;
static void *plrbuf;

/*static uint32_t amplify;*/

static OggVorbis_File ov;
static int oggstereo;
static int oggrate;
static uint32_t oggpos;
static uint32_t ogglen;
static uint8_t *oggbuf=NULL;
static uint32_t oggbuflen;
static uint32_t oggbufpos;
static uint32_t oggbuffpos;
static int32_t oggbufread;
static uint32_t oggbufrate;
static int active;
static int looped;
static int donotloop;
static uint32_t bufloopat;

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
} while(0)

static void oggIdler(void)
{
	size_t clean;

	if ((ogglen==oggbuflen)||!active)
		return;

	clean=(oggbufpos+oggbuflen-oggbufread)%oggbuflen;
	if (clean<8)
		return;

	clean-=8;
	while (clean)
	{
		size_t read=clean;
		long result;
		if ((ov_pcm_tell(&ov)<<(1+oggstereo))!=oggpos)
#ifdef HAVE_OV_PCM_SEEK_LAP
			ov_pcm_seek_lap(&ov, oggpos>>(1+oggstereo));
#else
			ov_pcm_seek(&ov, oggpos>>(1+oggstereo));
#endif
		if ((oggbufread+read)>oggbuflen)
			read=oggbuflen-oggbufread;
		if ((oggpos+read)>=ogglen)
		{
			read=ogglen-oggpos;
			bufloopat=oggbufread+read;
		}
		if (read>0x10000)
			read=0x10000;
#ifndef WORDS_BIGENDIAN
		result=ov_read(&ov, (char *)oggbuf+oggbufread, read, 0, 2, 1, &current_section);
#else
		result=ov_read(&ov, (char *)oggbuf+oggbufread, read, 1, 2, 1, &current_section);
#endif
		if (result<=0) /* broken data... we can survive */
		{
			memsetw(oggbuf+oggbufread, (uint16_t)0x8000, read>>1);
			result=read;
		} else {
			int16_t rs, ls;
			int samples=result>>1;
			int16_t *buffer=(int16_t *)(oggbuf+oggbufread);

			if (oggstereo)
			{
				samples>>=1;
				while (samples)
				{
					rs=buffer[0];
					ls=buffer[1];
					PANPROC;
					buffer[0]=rs;
					if (srnd)
						buffer[1]=ls^0xffff;
					else
						buffer[1]=ls;
					samples--;
					buffer+=2;
				}
			} else while (samples)
			{
					rs=buffer[0];
					ls=buffer[0];
					PANPROC;
					buffer[0]=((rs+ls)>>1);
					samples--;
					buffer++;
			}

		}
		oggbufread=(oggbufread+result)%oggbuflen;
		oggpos=(oggpos+result)%ogglen;
		clean-=result;
	}
}


void __attribute__ ((visibility ("internal"))) oggIdle(void)
{
	uint32_t bufplayed;
	uint32_t bufdelta;
	uint32_t pass2;
	int quietlen=0;
	uint32_t toloop;

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	bufplayed=plrGetBufPos()>>(stereo+bit16);

	bufdelta=(buflen+bufplayed-bufpos)%buflen;

	if (!bufdelta)
	{
		clipbusy--;
		if (plrIdle)
			plrIdle();
		return;
	}
	oggIdler();
	if (oggbuflen!=ogglen)
	{
		uint32_t towrap=imuldiv((((oggbuflen+oggbufread-oggbufpos-1)%oggbuflen)>>(oggstereo+1)), 65536, oggbufrate);
		if (bufdelta>towrap)
			/*quietlen=bufdelta-towrap;*/
			bufdelta=towrap;
	}

	if (inpause)
		quietlen=bufdelta;

	toloop=imuldiv(((bufloopat-oggbufpos)>>(1+oggstereo)), 65536, oggbufrate);
	if (looped)
		toloop=0;

	bufdelta-=quietlen;

	if (bufdelta>=toloop)
	{
		looped=1;
		if (donotloop)
		{
			quietlen+=bufdelta-toloop;
			bufdelta=toloop;
		}
	}


	if (bufdelta)
	{
		uint32_t i;

		if (oggbufrate==0x10000)
		{
			if (oggstereo)
			{
				uint32_t o=0;
				while (o<bufdelta)
				{
					uint32_t w=(bufdelta-o)*4;
					if ((oggbuflen-oggbufpos)<w)
						w=oggbuflen-oggbufpos;
					memcpy(buf16+2*o, oggbuf+oggbufpos, w);
					o+=w>>2;
					oggbufpos+=w;
					if (oggbufpos>=oggbuflen)
						oggbufpos-=oggbuflen;
				}
			} else {
				uint32_t o=0;
				while (o<bufdelta)
				{
					uint32_t w=(bufdelta-o)*2;
					if ((oggbuflen-oggbufpos)<w)
						w=oggbuflen-oggbufpos;
					memcpy(buf16+o, oggbuf+oggbufpos, w);
					o+=w>>1;
					oggbufpos+=w;
					if (oggbufpos>=oggbuflen)
						oggbufpos-=oggbuflen;
				}
			}
		} else if (oggstereo)
		{
			int32_t oggm1, c0, c1, c2, c3, ls, rs, vm1,v1,v2;
			uint32_t ogg1, ogg2;
			for (i=0; i<bufdelta; i++)
			{

				oggm1=oggbufpos-4; if (oggm1<0) oggm1+=oggbuflen;
				ogg1=oggbufpos+4; if (ogg1>=oggbuflen) ogg1-=oggbuflen;
				ogg2=oggbufpos+8; if (ogg2>=oggbuflen) ogg2-=oggbuflen;


				c0 = *(uint16_t *)(oggbuf+oggbufpos)^0x8000;
				vm1= *(uint16_t *)(oggbuf+oggm1)^0x8000;
				v1 = *(uint16_t *)(oggbuf+ogg1)^0x8000;
				v2 = *(uint16_t *)(oggbuf+ogg2)^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3,oggbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3,oggbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3,oggbuffpos);
				ls = c3+c0;
				if (ls<0)
					ls=0;
				if (ls>65535)
					ls=65535;

				c0 = *(uint16_t *)(oggbuf+oggbufpos+2)^0x8000;
				vm1= *(uint16_t *)(oggbuf+oggm1+2)^0x8000;
				v1 = *(uint16_t *)(oggbuf+ogg1+2)^0x8000;
				v2 = *(uint16_t *)(oggbuf+ogg2+2)^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3,oggbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3,oggbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3,oggbuffpos);
				rs = c3+c0;
				if (rs<0)
					rs=0;
				if (rs>65535)
					rs=65535;

				buf16[2*i]=(uint16_t)ls^0x8000;
				buf16[2*i+1]=(uint16_t)rs^0x8000;

				oggbuffpos+=oggbufrate;
				oggbufpos+=(oggbuffpos>>16)*4;
				oggbuffpos&=0xFFFF;
				if (oggbufpos>=oggbuflen)
					oggbufpos-=oggbuflen;
			}
		} else {
			int32_t oggm1, c0, c1, c2, c3, vm1,v1,v2;
			uint32_t ogg1, ogg2;
			for (i=0; i<bufdelta; i++)
			{

				oggm1=oggbufpos-2; if (oggm1<0) oggm1+=oggbuflen;
				ogg1=oggbufpos+2; if (ogg1>=oggbuflen) ogg1-=oggbuflen;
				ogg2=oggbufpos+4; if (ogg2>=oggbuflen) ogg2-=oggbuflen;

				c0 = *(uint16_t *)(oggbuf+oggbufpos)^0x8000;
				vm1= *(uint16_t *)(oggbuf+oggm1)^0x8000;
				v1 = *(uint16_t *)(oggbuf+ogg1)^0x8000;
				v2 = *(uint16_t *)(oggbuf+ogg2)^0x8000;
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

				buf16[i]=(uint16_t)c3^0x8000;

				oggbuffpos+=oggbufrate;
				oggbufpos+=(oggbuffpos>>16)*2;
				oggbuffpos&=0xFFFF;
				if (oggbufpos>=oggbuflen)
					oggbufpos-=oggbuflen;
			}
		}

		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		bufdelta-=pass2;
		if (bit16)
		{
			if (stereo)
			{
				if (reversestereo)
				{
					int16_t *p=(int16_t *)plrbuf+2*bufpos;
					int16_t *b=(int16_t *)buf16;
					if (signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1];
							p[1]=b[0];
							p+=2;
							b+=2;
						}
						p=(int16_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1];
							p[1]=b[0];
							p+=2;
							b+=2;
						}
					} else {
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1]^0x8000;
							p[1]=b[0]^0x8000;
							p+=2;
							b+=2;
						}
						p=(int16_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1]^0x8000;
							p[1]=b[0]^0x8000;
							p+=2;
							b+=2;
						}
					}
				} else {
					int16_t *p=(int16_t *)plrbuf+2*bufpos;
					int16_t *b=(int16_t *)buf16;
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
				}
			} else {
				int16_t *p=(int16_t *)plrbuf+bufpos;
				int16_t *b=(int16_t *)buf16;
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
				if (reversestereo)
				{
					uint8_t *p=(uint8_t *)plrbuf+2*bufpos;
					uint8_t *b=(uint8_t *)buf16;
					if (signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[3];
							p[1]=b[1];
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[3];
							p[1]=b[1];
							p+=2;
							b+=4;
						}
					} else {
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[3]^0x80;
							p[1]=b[1]^0x80;
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[3]^0x80;
							p[1]=b[1]^0x80;
							p+=2;
							b+=4;
						}
					}
				} else {
					uint8_t *p=(uint8_t *)plrbuf+2*bufpos;
					uint8_t *b=(uint8_t *)buf16;
					if (signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1];
							p[1]=b[3];
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1];
							p[1]=b[3];
							p+=2;
							b+=4;
						}
					} else {
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1]^0x80;
							p[1]=b[3]^0x80;
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1]^0x80;
							p[1]=b[3]^0x80;
							p+=2;
							b+=4;
						}
					}
				}
			} else {
				uint8_t *p=(uint8_t *)plrbuf+bufpos;
				uint8_t *b=(uint8_t *)buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[1];
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
						p[0]=b[1]^0x80;
						p++;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[1]^0x80;
						p++;
						b+=2;
					}
				}
			}
		}
		bufpos+=bufdelta+pass2;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	bufdelta=quietlen;
	if (bufdelta)
	{
		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		if (bit16)
		{
			plrClearBuf((uint16_t *)plrbuf+(bufpos<<stereo), (bufdelta-pass2)<<stereo, !signedout);
			if (pass2)
				plrClearBuf((uint16_t *)plrbuf, pass2<<stereo, !signedout);
		} else {
			plrClearBuf(buf16, bufdelta<<stereo, !signedout);
			plr16to8((uint8_t *)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((uint8_t *)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
		}
		bufpos+=bufdelta;
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
	if (!plrPlay)
		return 0;

	fseek(oggf, 0, SEEK_SET);
	if(ov_open(oggf, &ov, NULL, -1) < 0)
		return -1; /* we don't bother to do more exact */
	ov.callbacks.close_func=close_func;


	vi=ov_info(&ov,-1);
	oggstereo=vi->channels>=2;
	oggrate=vi->rate;

	plrSetOptions(oggrate, (PLR_SIGNEDOUT|PLR_16BIT)|((oggstereo)?PLR_STEREO:0));
	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);
	samprate=plrRate;

	oggbufrate=imuldiv(65536, oggrate, samprate);

	ogglen=ov_pcm_total(&ov, -1)<<(1+oggstereo);
	if (!ogglen)
		return 0;

	oggbuflen=16384;
	if (oggbuflen>ogglen)
	{
		oggbuflen=ogglen;
		bufloopat=oggbuflen;
	} else
		bufloopat=0x40000000;
	oggbuf=malloc(oggbuflen);
	if (!oggbuf)
		return 0;
	ogglen=ogglen&~((1<<(oggstereo+1))-1);
	oggbufpos=0;
	oggbuffpos=0;
	current_section=0;
#ifdef WORDS_BIGENDIAN
	if ((oggbufread=oggpos=ov_read(&ov, (char *)oggbuf, oggbuflen, 1, 2, 1, &current_section))<0)
#else
	if ((oggbufread=oggpos=ov_read(&ov, (char *)oggbuf, oggbuflen, 0, 2, 1, &current_section))<0)
#endif
		oggbufread=oggpos=0;

	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize))
		return 0;

	inpause=0;
	looped=0;
	voll=256;
	volr=256;
	pan=64;
	srnd=0;
	oggSetVolume(64, 0, 64, 0);
/*
	oggSetAmplify(amplify);   TODO */

	buf16=malloc(sizeof(uint16_t)*buflen*2);
	if (!buf16)
	{
		plrClosePlayer();
		free(oggbuf);
		return 0;
	}
	bufpos=0;

	if (!pollInit(oggIdle))
	{
		plrClosePlayer();
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

	free(oggbuf);
	free(buf16);
	oggbuf=NULL;
	buf16=NULL;

	ov_clear(&ov);
}

char __attribute__ ((visibility ("internal"))) oggLooped(void)
{
	return looped;
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
	volr=voll=vol_*4;
	if (bal_<0)
		volr=(volr*(64+bal_))>>6;
	else
		voll=(voll*(64-bal_))>>6;
	srnd=opt;
}

uint32_t __attribute__ ((visibility ("internal"))) oggGetPos(void)
{
	if (ogglen==oggbuflen)
		return oggbufpos>>(oggstereo+1);
	else
		return ((oggpos+ogglen-oggbuflen+((oggbufpos-oggbufread+oggbuflen)%oggbuflen))%ogglen)>>(oggstereo+1);
}

void __attribute__ ((visibility ("internal"))) oggGetInfo(struct ogginfo *i)
{
	static int lastsafe=0;
	i->pos=oggGetPos();
	i->len=ogglen>>(oggstereo+1);
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
	pos=((pos<<(1+oggstereo))+ogglen)%ogglen;
	if (ogglen==oggbuflen)
		oggbufpos=pos;
	else
	{
		if (((pos+oggbuflen)>oggpos)&&(pos<oggpos))
			oggbufpos=(oggbufread-(oggpos-pos)+oggbuflen)%oggbuflen;
		else
		{
			oggpos=pos;
			oggbufpos=0;
			oggbufread=1<<(1+oggstereo);
		}
	}
}

