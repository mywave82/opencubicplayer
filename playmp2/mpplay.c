/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * MPPlay - Player for MPEG Audio Layer 1/2/3 files
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
 *  -ss040910   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss041004   Stian Skjelstad <stian@nixia.no>
 *    -close file on exec
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <mad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "stuff/timer.h"
#include "stuff/poll.h"

#include "dev/player.h"
#include "dev/deviplay.h"
#include "stuff/imsrtns.h"
#include "dev/plrasm.h"
#include "mpplay.h"

/* options */
static int inpause;
static int looped;

static unsigned long amplify; /* TODO */
static unsigned long voll,volr;
static int pan;
static int srnd;

/* the synth */
static struct mad_stream stream;
static struct mad_frame frame;
static struct mad_synth synth;
static unsigned int data_in_synth;
int __attribute__ ((visibility ("internal"))) mpeg_Bitrate; /* bitrate of the last decoded frame */
static int mpegrate; /* samp-rate in the mpeg */
static int mpegstereo;
static unsigned char *GuardPtr;
/* Are resourses in-use (needs to be freed at Close) ?*/
static int active=0;

/* file-buffer */
#define MPEG_BUFSZ    40000   /* 2.5 s at 128 kbps; 1 s at 320 kbps */
static unsigned char data[MPEG_BUFSZ];
static int data_length;
static int eof;

/* stats about the file-buffer */
static FILE *file;
static uint32_t ofs;
static uint32_t fl;
static uint32_t ft;
static uint32_t datapos;
static uint32_t newpos;

/* devp pre-buffer zone */
static uint16_t *buf16 = 0; /* here we dump out data before it goes live */
/* devp buffer zone */
static uint32_t bufpos; /* devp write head location */
static uint32_t buflen; /* devp buffer-size in samples */
static void *plrbuf; /* the devp buffer */
static int stereo; /* boolean */
static int bit16; /* boolean */
static int signedout; /* boolean */
static int reversestereo; /* boolean */
static int donotloop=1;

/* mpegIdler dumping locations */
static uint8_t *mpegbuf = 0;     /* the buffer */
static uint32_t mpegbuflen;  /* total buffer size */
/*static uint32_t mpeglen;*/     /* expected wave length */
static uint32_t mpegbufread; /* actually this is the write head */
static uint32_t mpegbufpos;  /* read pos */
static uint32_t mpegbuffpos; /* read fine-pos.. when mpegbufrate has a fraction */
static uint32_t mpegbufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */


/* clipper threadlock since we use a timer-signal */
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


static inline mad_fixed_t clip(mad_fixed_t sample)
{
	enum {
		MIN = -MAD_F_ONE,
		MAX =  MAD_F_ONE - 1
	};

	if (sample > MAX)
		sample = MAX;
	else if (sample < MIN)
		sample = MIN;
	return sample;
}

static inline signed long audio_linear_round(unsigned int bits, mad_fixed_t sample)
{
	/* round */
	sample += (1L << (MAD_F_FRACBITS - bits));

	/* clip */
	sample=clip(sample);

	/* quantize and scale */
	return sample >> (MAD_F_FRACBITS + 1 - bits);
}

static void audio_pcm_s16(unsigned char *_data, unsigned int nsamples, mad_fixed_t const *left, mad_fixed_t const *right)
{
	unsigned int len;
	signed short *data=(signed short *)_data;
	int ls, rs;
	unsigned short xormask=0x0000;

	len = nsamples;
	if (srnd)
		xormask=0xffff;

	if (right) /* stereo */
		while (len--)
		{
			rs = audio_linear_round(16, *left++);
			ls = audio_linear_round(16, *right++);
			PANPROC;
			data[0] = rs;
			data[1] = (int16_t)ls^xormask;
			data += 2;
		}
	else /* mono */
		while (len--)
		{
			rs = ls = audio_linear_round(16, *left++);
			PANPROC;
			data[0] = rs;
			data[1] = (signed short)ls^xormask;
		}
}

static inline uint_fast32_t id3_tag_query(const unsigned char *data, uint_fast32_t length)
{
	if (length>=3)
		if ((data[0]=='T')&&(data[1]=='A')&&(data[2]=='G'))
			return 128;
	if (length>=10)
		if ((data[0]=='I')&&(data[1]=='D')&&(data[2]=='3')&&(data[3]!=0xff)&&(data[4]!=0xff))
		{
			uint_fast32_t size = (data[6]<<21)|(data[7]<<14)|(data[8]<<7)|(data[9]);
			if (data[5]&0x10) /* do we have a footer? */
				size+=10; /* size of footer */
			return size+10; /* size of header */
		}
	return 0; /* we can't tell */
}

static int stream_for_frame(void)
{
	if (data_in_synth)
		return 1;
	if (datapos!=newpos) /* force buffer flush */
	{
		datapos=newpos;
		fseek(file, datapos+ofs, SEEK_SET);
		data_length=0;
		stream.next_frame=0;
		stream.error=MAD_ERROR_BUFLEN;
		GuardPtr=0;
	}

	if (GuardPtr&&(stream.this_frame==GuardPtr))
	{
	/*
		fprintf(stderr, "EOF-KNOCKING\n");*/
		return 0;
	}
	while(1)
	{
		if(stream.buffer==NULL || stream.error==MAD_ERROR_BUFLEN)
		{
			uint32_t len, target;
			if (stream.next_frame)
			{
				if (GuardPtr)
					GuardPtr-=((data + data_length) - stream.next_frame);
				memmove(data, stream.next_frame, data_length = ((data + data_length) - stream.next_frame));
				stream.next_frame=0;
			}
			target = MPEG_BUFSZ - data_length;
			if (target>4096)
				target=4096;
			if (target>(fl-datapos))
				target=fl-datapos;
			if (target)
				len = fread(data + data_length, 1, target, file);
			else
				len=0;
			if (!len)
			{
				len=0;
				if ((feof(file))||!target)
				{
					if (donotloop||target)
					{
						eof=1;
						GuardPtr=data + data_length;
						assert(MPEG_BUFSZ - data_length >= MAD_BUFFER_GUARD);
						while (len < MAD_BUFFER_GUARD)
							data[data_length + len++] = 0;
/*
						fprintf(stderr, "Guard set len to %d\n", len);*/
					} else {
						eof=0;
						datapos = newpos = 0;
						fseek(file, ofs, SEEK_SET);
						return 0;
					}
				}
			} else {
				GuardPtr=0;
				datapos += len;
				newpos += len;
			}
			if (len)
				mad_stream_buffer(&stream, data, data_length += len);
		}
		stream.error=0;

		/* decode data */
		if (mad_frame_decode(&frame, &stream) == -1)
		{
			if ((stream.error==MAD_ERROR_BUFLEN))
			{
				if (eof)
					return 0;
				else
					continue;
			}
			if (!MAD_RECOVERABLE(stream.error))
			{
/*
				fprintf(stderr, "!MAD_RECOVERABLE\n");*/
				return 0;
			}
			if (stream.error==MAD_ERROR_BADCRC)
			{
/*
				fprintf(stderr, "Bad CRC\n");*/
				mad_frame_mute(&frame);
				continue;
			} else if (stream.error==MAD_ERROR_LOSTSYNC)
			{
				int tagsize;
				if (stream.this_frame==GuardPtr)
					return 0;
				tagsize = id3_tag_query(stream.this_frame, stream.bufend - stream.this_frame);
				if (tagsize>0)
				{
					mad_stream_skip(&stream, tagsize);
					continue;
				}
/*
				fprintf(stderr, "MAD_ERROR_LOSTSYNC\n");*/
			}
/*
			fprintf(stderr, "error: %s\n", mad_stream_errorstr(&stream));*/

			continue;
		}
		mad_synth_frame(&synth, &frame);
		data_in_synth=synth.pcm.length;
		mpeg_Bitrate=frame.header.bitrate/1000;
		mpegstereo=synth.pcm.channels==2;
		return 1;
	}


}

static void mpegIdler(void)
{
	size_t clean;

	if (!active)
		return;

	clean=(mpegbufpos+mpegbuflen-mpegbufread)%mpegbuflen;
	if (clean<8)
		return;

	clean-=8;
	while (clean)
	{
		size_t read=clean;

		if (!data_in_synth)
			if (!stream_for_frame())
				break;
		if (read>(data_in_synth<<2)) /* 16bit + stereo as always */
			read=data_in_synth<<2;

		if ((mpegbufread+read)>mpegbuflen)
			read=mpegbuflen-mpegbufread;
		if (synth.pcm.channels==1)
			audio_pcm_s16(mpegbuf+mpegbufread, read>>2, synth.pcm.samples[0]+synth.pcm.length-data_in_synth, synth.pcm.samples[0]+synth.pcm.length-data_in_synth);
		else
			audio_pcm_s16(mpegbuf+mpegbufread, read>>2, synth.pcm.samples[0]+synth.pcm.length-data_in_synth, synth.pcm.samples[1]+synth.pcm.length-data_in_synth);
		mpegbufread=(mpegbufread+read)%mpegbuflen;
		clean-=read;
		data_in_synth-=read>>2; /* 16bit + stereo as always */
	}
}

void __attribute__ ((visibility ("internal"))) mpegIdle(void)
{
	uint32_t bufplayed;
	uint32_t bufdelta;
	uint32_t pass2;
	int quietlen;

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	quietlen=0;
	/* Where is our devp reading head? */
	bufplayed=plrGetBufPos()>>(stereo+bit16);
	bufdelta=(buflen+bufplayed-bufpos)%buflen;

	/* No delta on the devp? */
	if (!bufdelta)
	{
		clipbusy--;
		if (plrIdle)
			plrIdle();
		return;
	}

	/* fill up our buffers */
	mpegIdler();

	if (inpause)
		quietlen=bufdelta;
	else /*if (mpegbuflen!=mpeglen)*/ /* EOF of the mpegstream? */
	{           /* should not the crap below match up easy with imuldiv(mpeglen>>2, 65536, mpegbufrate) ??? TODO */
		uint32_t towrap=imuldiv((((mpegbuflen+mpegbufread-mpegbufpos-1)%mpegbuflen)>>(1 /* we are always given stereo */ + 1 /* we are always given 16bit */)), 65536, mpegbufrate);
		if (bufdelta>towrap)
		{
			/* will the eof hit inside the delta? */
			if (eof) /* make sure we did hit eof, and just not out of data situasion due to streaming latency */
			{
				quietlen=bufdelta-towrap;
				looped=1;
			} else {
				bufdelta=towrap;
			}
		}
	}

	bufdelta-=quietlen;

	if (bufdelta)
	{
		uint32_t i;
		if (mpegbufrate==0x10000) /* 1.0... just copy into buf16 direct until we run out of target buffer or source buffer */
		{
			uint32_t o=0;
			while (o<bufdelta)
			{
				uint32_t w=(bufdelta-o)*4;
				if ((mpegbuflen-mpegbufpos)<w)
					w=mpegbuflen-mpegbufpos;
				memcpy(buf16+2*o, mpegbuf+mpegbufpos, w);
				o+=w>>2;
				mpegbufpos+=w;
				if (mpegbufpos>=mpegbuflen)
					mpegbufpos-=mpegbuflen;
			}
		} else { /* re-sample intil we don't have more target-buffer or source-buffer */
			int32_t c0, c1, c2, c3, ls, rs, vm1,v1,v2, wpm1;
			uint32_t wp1, wp2;
			for (i=0; i<bufdelta; i++)
			{
				wpm1=mpegbufpos-4; if (wpm1<0) wpm1+=mpegbuflen;
				wp1=mpegbufpos+4; if (wp1>=mpegbuflen) wp1-=mpegbuflen;
				wp2=mpegbufpos+8; if (wp2>=mpegbuflen) wp2-=mpegbuflen;

				c0 = *(uint16_t *)(mpegbuf+mpegbufpos)^0x8000;
				vm1= *(uint16_t *)(mpegbuf+wpm1)^0x8000;
				v1 = *(uint16_t *)(mpegbuf+wp1)^0x8000;
				v2 = *(uint16_t *)(mpegbuf+wp2)^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3,mpegbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3,mpegbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3,mpegbuffpos);
				ls = c3+c0;
				if (ls>65535)
					ls=65535;
				else if (ls<0)
					ls=0;

				c0 = *(uint16_t *)(mpegbuf+mpegbufpos+2)^0x8000;
				vm1= *(uint16_t *)(mpegbuf+wpm1+2)^0x8000;
				v1 = *(uint16_t *)(mpegbuf+wp1+2)^0x8000;
				v2 = *(uint16_t *)(mpegbuf+wp2+2)^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3,mpegbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3,mpegbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3,mpegbuffpos);
				rs = c3+c0;
				if (rs>65535)
					rs=65535;
				else if(rs<0)
					rs=0;
				buf16[2*i]=(uint16_t)ls^0x8000;
				buf16[2*i+1]=(uint16_t)rs^0x8000;

				mpegbuffpos+=mpegbufrate;
				mpegbufpos+=(mpegbuffpos>>16)*4;
				mpegbuffpos&=0xFFFF;
				if (mpegbufpos>=mpegbuflen)
					mpegbufpos-=mpegbuflen;
			}
		}
		/* when we copy out of buf16, pass the buffer-len that wraps around end-of-buffer till pass2 */
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

unsigned char __attribute__ ((visibility ("internal"))) mpegOpenPlayer(FILE *mpeg, size_t offset, size_t fsize)
{
#define _MPEG_BUFSZ    256000   /* 2.5 s at 128 kbps; 1 s at 320 kbps */
#if 0
	unsigned char data[_MPEG_BUFSZ];
	mad_timer_t timer;
#endif
	if (!(file=mpeg))
		return 0;
/*
	if (fcntl(fileno(file), F_SETFD, FD_CLOEXEC))
		perror("WARNING: mpplay.c: fcntl(file, FSET_FD, FD_CLOEXEC)");*/
	ofs=offset;
	fl=fsize;
	newpos=datapos=0;
	data_length=0;
	eof=0;
	data_in_synth=0;

	inpause=0;
	looped=0;
	voll=256;
	volr=256;
	pan=64;
	srnd=0;
	mpegSetAmplify(65536);

	mad_stream_init(&stream);
	mad_frame_init(&frame);
	mad_synth_init(&synth);
	mad_stream_options(&stream, MAD_OPTION_IGNORECRC);
	GuardPtr=NULL;

#if 0
	timer.seconds=0;
	timer.fraction=0;

	while(1)
	{
		if(stream.buffer==NULL || stream.error==MAD_ERROR_BUFLEN)
		{
			int len, target;
			if (stream.next_frame)
			{
				if (GuardPtr)
					GuardPtr-=((data + data_length) - stream.next_frame);
				memmove(data, stream.next_frame, data_length = ((data + data_length) - stream.next_frame));
				stream.next_frame=0;
			}
			target = _MPEG_BUFSZ - data_length;
			if (target>(fl-datapos))
				target=fl-datapos;
			if (target)
				len = fread(data + data_length, 1, target, file);
			else
				len=0;
			if (!len)
			{
				if ((feof(file))||!target)
					eof=1;
			} else {
				GuardPtr=0;
				datapos += len;
			}
			if (len)
				mad_stream_buffer(&stream, data, data_length += len);
		}
		stream.error=0;
		/* decode data */
		if (mad_frame_decode(&frame, &stream) == -1)
		{
			if ((stream.error==MAD_ERROR_BUFLEN))
			{
				if (eof)
					break;
				else
					continue;
			}
			if (!MAD_RECOVERABLE(stream.error))
				break;
			if (stream.error==MAD_ERROR_BADCRC)
			{
				mad_frame_mute(&frame);
				continue;
			} else if (stream.error==MAD_ERROR_LOSTSYNC)
			{
				int tagsize;
				if (stream.this_frame==GuardPtr)
					break;;
				tagsize = id3_tag_query(stream.this_frame, stream.bufend - stream.this_frame);
				if (tagsize>0)
				{
					mad_stream_skip(&stream, tagsize);
					continue;
				}
			}
			continue;
		}
		mad_timer_add(&timer, frame.header.duration);
	}
	ft=timer.seconds;
#else
	ft=0;
#endif
	eof=0;
	datapos = 0;
	data_length=0;
	GuardPtr=NULL;
	stream.error=0;
	stream.buffer=0;
	stream.this_frame=0;
	fseek(file, 0, SEEK_SET);

	if (!stream_for_frame())
		goto error_out;
	mpegrate=synth.pcm.samplerate;

	plrSetOptions(mpegrate, (PLR_SIGNEDOUT|PLR_16BIT)|PLR_STEREO);

	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);

	mpegbufrate=imuldiv(65536, mpegrate, plrRate);

	mpegbuflen=32768;
	if (!(mpegbuf=malloc(mpegbuflen)))
		goto error_out;
	mpegbufpos=0;
	mpegbuffpos=0;
	mpegbufread=1<<(1/* stereo */+1 /*16bit*/);
	GuardPtr=0;

	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize))
		goto error_out;

	if (!(buf16=malloc(sizeof(uint16_t)*buflen*2)))
	{
		plrClosePlayer();
		goto error_out;
	}
	bufpos=0;

	if (!pollInit(mpegIdle))
	{
		free(buf16);
		plrClosePlayer();
		goto error_out;
	}

	active=1;
	return 1;

error_out:
	if (mpegbuf)
	{
		free(mpegbuf);
		mpegbuf=0;
	}
	mad_synth_finish(&synth);
	mad_frame_finish(&frame);
	mad_stream_finish(&stream);
	return 0;
}

void __attribute__ ((visibility ("internal"))) mpegClosePlayer(void)
{
	if (active)
	{
		pollClose();
		plrClosePlayer();

		mad_synth_finish(&synth);
		mad_frame_finish(&frame);
		mad_stream_finish(&stream);

		active=0;
	}
	if (buf16)
	{
		free(buf16);
		buf16=0;
	}
	if (mpegbuf)
	{
		free(mpegbuf);
		mpegbuf=0;
	}
}

void __attribute__ ((visibility ("internal"))) mpegSetLoop(uint8_t s)
{
	donotloop=!s;
}
char __attribute__ ((visibility ("internal"))) mpegIsLooped(void)
{
	return looped;
}
void __attribute__ ((visibility ("internal"))) mpegPause(uint8_t p)
{
	inpause=p;
}
void __attribute__ ((visibility ("internal"))) mpegSetAmplify(uint32_t amp)
{
	amplify=amp;
}
void __attribute__ ((visibility ("internal"))) mpegSetSpeed(uint16_t sp)
{
	if (sp<32)
		sp=32;
	mpegbufrate=imuldiv(256*sp, mpegrate, plrRate);
}
void __attribute__ ((visibility ("internal"))) mpegSetVolume(uint8_t vol_, int8_t bal_, int8_t pan_, uint8_t opt)
{
	pan=pan_;
	volr=voll=vol_*4;
	if (bal_<0)
		volr=(volr*(64+bal_))>>6;
	else
		voll=(voll*(64-bal_))>>6;
	srnd=opt;
}
void __attribute__ ((visibility ("internal"))) mpegGetInfo(struct mpeginfo *info)
{
	info->pos=datapos;
	info->len=fl;
	info->timelen=ft;
	info->rate=mpegrate;
	info->stereo=mpegstereo;
	info->bit16=1;
}
int32_t __attribute__ ((visibility ("internal"))) mpegGetPos(void)
{
	return datapos;
}
void __attribute__ ((visibility ("internal"))) mpegSetPos(uint32_t pos)
{
	/*if (pos<0)
		pos=0;*/
	if (pos>fl)
		pos=fl;
	newpos=pos;
}
