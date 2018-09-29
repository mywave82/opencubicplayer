/* aylet 0.3, a .AY music file player.
 * Copyright (C) 2001-2010 Russell Marks and Ian Collier.
 * See main.c for licence.
 *
 * ayplay.c - the sound emulation itself, based on the beeper/AY code I
 *            wrote for Fuse.
 */

/* some AY details (volume levels, white noise RNG algorithm) based on
 * info from MAME's ay8910.c - MAME's licence explicitly permits free
 * use of info (even encourages it).
 */

/* NB: I know some of this stuff looks fairly CPU-hogging.
 * For example, the AY code tracks changes with sub-frame timing
 * in a rather hairy way, and there's subsampling here and there.
 * But if you measure the CPU use, it doesn't actually seem
 * very high at all. And I speak as a Cyrix owner. :-)
 *
 * (I based that on testing in Fuse, but I doubt it's that much
 * worse here. (It's actually a bit better, I think.))
 */

/* This file is based on main.c, and is VERY modified
 *    - Stian Skjelstad
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
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
#include "dev/ringbuffer.h"
#include "dev/plrasm.h"

#include "ayplay.h"
#include "main.h"
#include "sound.h"
#include "z80.h"

/* from main.c */
__attribute__ ((visibility ("internal"))) struct aydata_tag aydata;
__attribute__ ((visibility ("internal"))) struct time_tag ay_tunetime;
__attribute__ ((visibility ("internal"))) int ay_track=0;
static int new_ay_track=0;
__attribute__ ((visibility ("internal"))) unsigned long ay_tstates,ay_tsmax;
static int ay_current_reg=0;
static int done_fade=0;
static int fadetime=10;     /* fadeout time *after* that in sec, 0=none */
static int stopafter=0;     /* TODO */
static int silent_max=4*50; /* max frames of silence before skipping */
static int ay_looped;

/* the memory is a flat all-RAM 64k */
__attribute__ ((visibility ("internal"))) unsigned char ay_mem[64*1024];
#define AYLET_VER               "0.3"
#define FRAME_STATES_48         (3500000/50)
#define FRAME_STATES_128        (3546900/50)
#define FRAME_STATES_CPC        (4000000/50)
static int do_cpc=0;


/* options */
static int inpause;

/*static unsigned long amplify;  TODO */
static unsigned long voll,volr;
static int pan;
static int srnd;

/* devp pre-buffer zone */
static int16_t *buf16; /* stupid dump that should go away */
static uint16_t *_buf8; /* here we dump out data before it goes live... it no-longer is 8bit, but the name sticks */
static size_t _buf8_n; /* samples */
/* devp buffer zone */
static uint32_t bufpos; /* devp write head location */
static uint32_t buflen; /* devp buffer-size in samples */
static void *plrbuf; /* the devp buffer */
static int stereo; /* boolean */
static int bit16; /* boolean */
static int signedout; /* boolean */
static int reversestereo; /* boolean */
static int donotloop=1;


/* ayIdler dumping locations */
static int16_t *aybuf;     /* the buffer */
static struct ringbuffer_t *aybufpos = 0;
//static uint32_t aybuflen;  /* total buffer size */
/*static uint32_t aylen;*/     /* expected wave length */
//static uint32_t aybufread; /* actually this is the write head */
//static uint32_t aybufpos;  /* read pos */
static uint32_t aybuffpos; /* read fine-pos.. */
static uint32_t aybufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */

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
	if (srnd) \
	{ \
		ls ^= 0xffff; \
	} \
} while(0)

/* from main.c */
unsigned int __attribute__ ((visibility ("internal"))) ay_in(int h,int l)
{
	/* presumably nothing? XXX */
	return 255;
}

/* from main.c */
unsigned int __attribute__ ((visibility ("internal"))) ay_out(int h,int l,int a)
{
	static int cpc_f4=0;

	/* unlike a real speccy, it seems we should only emulate exact port
	 * number matches, rather than using bitmasks.
	 */
	if(do_cpc<1)
		switch(l)
		{
			case 0xfd:
				switch(h)
				{
					case 0xff:
						do_cpc=-1;
write_reg:
						ay_current_reg=(a&15);
						break;
					case 0xbf:
						do_cpc=-1;
write_dat:
						sound_ay_write(ay_current_reg,a,ay_tstates);
						break;
					default:
						/* ok, since we do at least have low byte=FDh,
						 * do bitmask for top byte to allow for
						 * crappy .ay conversions. But don't disable
						 * CPC autodetect, just in case.
						 */
						if((h&0xc0)==0xc0) goto write_reg;
						if((h&0xc0)==0x80) goto write_dat;
				}
				break;

			case 0xfe:
				do_cpc=-1;
				sound_beeper(a&0x10);
				break;
		}

	if(do_cpc>-1)
		switch(h)
		{
			case 0xf6:
				switch(a&0xc0)
				{
					case 0x80: /* write */
						sound_ay_write(ay_current_reg,cpc_f4,ay_tstates);
						break;

					case 0xc0: /* select */
						ay_current_reg=(cpc_f4&15);
						break;
				}
				break;

			case 0xf4:
				cpc_f4=a;
				if(!do_cpc)
				{
					/* restart as a more CPC-ish emulation */
					do_cpc=1;
					sound_ay_reset_cpc();
					ay_tsmax=FRAME_STATES_CPC;
					if(ay_tstates>ay_tsmax) ay_tstates-=ay_tsmax;
				}
				break;
		}

	return 0; /* additional t-states */
}

/* from main.c */
static void mem_init(int track)
{
	static uint8_t intz[]=
	{
		0xf3,           /* di */
		0xcd,0,0,       /* call init */
		0xed,0x5e,      /* loop: im 2 */
		0xfb,           /* ei */
		0x76,           /* halt */
		0x18,0xfa       /* jr loop */
	};
	static uint8_t intnz[]=
	{
		0xf3,           /* di */
		0xcd,0,0,       /* call init */
		0xed,0x56,      /* loop: im 1 */
		0xfb,           /* ei */
		0x76,           /* halt */
		0xcd,0,0,       /* call interrupt */
		0x18,0xf7       /* jr loop */
	};
	int init,ay_1st_block,ourinit,interrupt;
	uint8_t *ptr;
	int addr,len,ofs;

#define GETWORD(x) (((*(x))<<8)|(*(x+1)))

	init=GETWORD(aydata.tracks[track].data_stacketc+2);
	interrupt=GETWORD(aydata.tracks[track].data_stacketc+4);
	ay_1st_block=GETWORD(aydata.tracks[track].data_memblocks);

	memset(ay_mem+0x0000,0xc9,0x0100);
	memset(ay_mem+0x0100,0xff,0x3f00);
	memset(ay_mem+0x4000,0x00,0xc000);
	ay_mem[0x38]=0xfb;      /* ei */

	/* call first AY block if no init */
	ourinit=(init?init:ay_1st_block);

	if(!interrupt)
		memcpy(ay_mem,intz,sizeof(intz));
	else
	{
		memcpy(ay_mem,intnz,sizeof(intnz));
		ay_mem[ 9]=interrupt%256;
		ay_mem[10]=interrupt/256;
	}

	ay_mem[2]=ourinit%256;
	ay_mem[3]=ourinit/256;

	/* now put the memory blocks in place */
	ptr=aydata.tracks[track].data_memblocks;
	while((addr=GETWORD(ptr))!=0)
	{
		len=GETWORD(ptr+2);
		ofs=GETWORD(ptr+4);
		if(ofs>=0x8000) ofs=-0x10000+ofs;

		/* range check */
		if(ptr-4-aydata.filedata+ofs>=aydata.filelen ||
				ptr-4-aydata.filedata+ofs<0)
		{
			ptr+=6;
			continue;
		}

		/* fix any broken length */
		if(ptr+4+ofs+len>=aydata.filedata+aydata.filelen)
			len=aydata.filedata+aydata.filelen-(ptr+4+ofs);
		if(addr+len>0x10000)
			len=0x10000-addr;

		memcpy(ay_mem+addr,ptr+4+ofs,len);
		ptr+=6;
	}
}

/* from main.c */
static int read_ay_file(FILE *in)
{
	unsigned char *data,*buf,*ptr,*ptr2;
	int data_alloc=16384,buf_alloc=16384,data_ofs=0;
	int data_len;
	int ret,tmp,f;

	/* given the loopy format, it's much easier to deal with in memory.
	 * But I'm avoiding mmap() in case I want to tweak this to run from
	 * a pipe at some point.
	 */
	if((buf=malloc(buf_alloc))==NULL) /* can't happen under glibc */
		return 0;

	if((data=malloc(data_alloc))==NULL) /* can't happen under glibc */
	{
		free(buf);
		return 0;
	}

	while((ret=fread(buf,1,buf_alloc,in))>0)
	{
		if(data_ofs+ret>=data_alloc)
		{
			unsigned char *oldptr=data;

			data_alloc+=buf_alloc;
			if((data=realloc(data,data_alloc))==NULL) /* can't happen under glibc */
			{
				fclose(in);
				free(oldptr);
				free(buf);
				return 0;
			}
		}

		memcpy(data+data_ofs,buf,ret);
		data_ofs+=ret;
	}

	free(buf);

	if(ferror(in))
	{
		free(data);
		return 0;
	}

	data_len=data_ofs;

	if(memcmp(data,"ZXAYEMUL",8)!=0)
	{
		free(data);
		return 0;
	}

	/* for the rest, we don't parse that much; just make copies of the
	 * offset `pointers' as real pointers, and save all the `top-level'
	 * stuff.
	 */

	aydata.tracks=NULL;

#define READWORD(x)     (x)=256*(*ptr++); (x)|=*ptr++
#define READWORDPTR(x)  READWORD(tmp); \
		if(tmp>=0x8000) tmp=-0x10000+tmp; \
		if(ptr-data-2+tmp>=data_len || ptr-data-2+tmp<0) \
		  { \
                  free(data); \
                  if(aydata.tracks) free(aydata.tracks); \
		  return(0); \
                  } \
		(x)=ptr-2+tmp
#define CHECK_ASCIIZ(x) \
		if(!memchr((x),0,data+data_len-(x))) \
		  { \
                  free(data); \
                  if(aydata.tracks) free(aydata.tracks); \
		  return(0); \
                  }

	ptr=data+8;
	aydata.filever=*ptr++;
	aydata.playerver=*ptr++;
	ptr+=2;  /* skip `custom player' stuff */
	READWORDPTR(aydata.authorstr);
	CHECK_ASCIIZ(aydata.authorstr);
	READWORDPTR(aydata.miscstr);
	CHECK_ASCIIZ(aydata.miscstr);
	aydata.num_tracks=1+*ptr++;
	aydata.first_track=*ptr++;

	/* skip to track info */
	READWORDPTR(ptr2);
	ptr=ptr2;

	if((aydata.tracks=malloc(aydata.num_tracks*sizeof(struct ay_track_tag)))==NULL) /* can't happen under glibc */
	{
		free(data);
		return 0;
	}

	for(f=0;f<aydata.num_tracks;f++)
	{
		READWORDPTR(aydata.tracks[f].namestr);
		CHECK_ASCIIZ(aydata.tracks[f].namestr);
		READWORDPTR(aydata.tracks[f].data);
	}

	for(f=0;f<aydata.num_tracks;f++)
	{
		if(aydata.tracks[f].data-data+10>data_len-4)
		{
			free(aydata.tracks);
			free(data);
			return 0;
		}

		ptr=aydata.tracks[f].data+10;
		READWORDPTR(aydata.tracks[f].data_stacketc);
		READWORDPTR(aydata.tracks[f].data_memblocks);

		ptr=aydata.tracks[f].data+4;
		READWORD(aydata.tracks[f].fadestart);
		READWORD(aydata.tracks[f].fadelen);
	}

	/* ok then, that's as much parsing as we do here. */

	aydata.filedata=data;
	aydata.filelen=data_len;
	return 1;
}

/* from main.c */
static void tunetime_reset(void)
{
	ay_tunetime.min=ay_tunetime.sec=ay_tunetime.subsecframes=0;
	done_fade=0;
}

/* rets zero if we want to exit the emulation (i.e. exit track) */
int __attribute__ ((visibility ("internal"))) ay_do_interrupt(void)
{
	static int count=0;
	static int silent_for=0;

	count++;
	if(count>=4) count=0;

	/* check for fade needed */
	if(!done_fade && stopafter && ay_tunetime.min*60+ay_tunetime.sec>=stopafter)
	{
		done_fade=1;
		sound_start_fade(fadetime);
	}

	/* incr time */
	ay_tunetime.subsecframes++;
	if(ay_tunetime.subsecframes>=50)
	{
		ay_tunetime.subsecframes=0;
		ay_tunetime.sec++;
		if(ay_tunetime.sec>=60)
		{
			ay_tunetime.sec=0;
			ay_tunetime.min++;
		}
	}

	if(!sound_frame(1/*count==0 || !highspeed*/))
	{
		if((++silent_for) >= silent_max)
		{
			if ( ((ay_track+1) >= aydata.num_tracks) && donotloop)
			{
				ay_looped |= 1;
			} else {
				/* do next track, or file, or just stop */
				silent_for=0;
				new_ay_track=ay_track+1;
				if(new_ay_track>=aydata.num_tracks)
				{
					new_ay_track=0;
				}
			}
		}
	} else {
		ay_looped &= ~1;
		silent_for = 0;
	}

	return 0;
}

void __attribute__ ((visibility ("internal"))) ay_driver_frame(uint16_t *stereo_samples, size_t bytes)
{
	_buf8=stereo_samples;
	_buf8_n=bytes>>1;
}

static void ayIdler(void)
{
	{
		int pos1, pos2;
		int length1, length2;

		while (!_buf8_n)
		{
			if (donotloop && (ay_looped&1))
			{
				return;
			}

			if (new_ay_track!=ay_track)
			{
				ay_track=new_ay_track;
				ay_current_reg=0;
				sound_ay_reset();
				mem_init(ay_track);
				tunetime_reset();
				ay_tsmax=FRAME_STATES_128;
				do_cpc=0;
				ay_z80_init(aydata.tracks[ay_track].data,
				            aydata.tracks[ay_track].data_stacketc);
			}

			ay_z80loop();
		}

		ringbuffer_get_head_samples (aybufpos, &pos1, &length1, &pos2, &length2);

		if (length1>_buf8_n)
			length1=_buf8_n;
		memcpy (aybuf + (pos1<<1), _buf8, length1<<2);
		_buf8 += length1<<1;
		_buf8_n -= length1;
		ringbuffer_head_add_samples (aybufpos, length1);
	} while (0);
}

void __attribute__ ((visibility ("internal"))) ayIdle(void)
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
	ayIdler();

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
		int buf16_filled = 0;

		/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
		ringbuffer_get_tail_samples (aybufpos, &pos1, &length1, &pos2, &length2);

		if (aybufrate==0x10000)
		{
			int16_t *t = buf16;

			if (bufdelta>(length1+length2))
			{
				bufdelta=(length1+length2);
				ay_looped |= 2;

			} else {
				ay_looped &= ~2;
			}

			for (buf16_filled=0; buf16_filled<bufdelta; buf16_filled++)
			{
				int16_t rs, ls;

				if (!length1)
				{
					pos1 = pos2;
					length1 = length2;
					pos2 = 0;
					length2 = 0;
				}

				if (!length1)
				{
					fprintf (stderr, "playay: ERROR, length1 == 0, in ayIdle\n");
					_exit(1);
				}

				rs = aybuf[pos1<<1];
				ls = aybuf[(pos1<<1) + 1];

				PANPROC;

				*(t++) = rs;
				*(t++) = ls;

				pos1++;
				length1--;
			}

			ringbuffer_tail_consume_samples (aybufpos, buf16_filled); /* add this rate buf16_filled == tail_used */
		} else {
			/* We are going to perform cubic interpolation of rate conversion... this bit is tricky */
			unsigned int accumulated_progress = 0;

			ay_looped &= ~2;

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
					ay_looped |= 2;
					break;
				}
				/* will we overflow the wavebuf if we advance? */
				if ((length1+length2) < ((aybufrate+aybuffpos)>>16))
				{
					ay_looped |= 2;
					break;
				}

				switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
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

				rvm1 = (uint16_t)aybuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
				lvm1 = (uint16_t)aybuf[(wpm1<<1)+1]^0x8000;
				 rc0 = (uint16_t)aybuf[(wp0<<1)+0]^0x8000;
				 lc0 = (uint16_t)aybuf[(wp0<<1)+1]^0x8000;
				 rv1 = (uint16_t)aybuf[(wp1<<1)+0]^0x8000;
				 lv1 = (uint16_t)aybuf[(wp1<<1)+1]^0x8000;
				 rv2 = (uint16_t)aybuf[(wp2<<1)+0]^0x8000;
				 lv2 = (uint16_t)aybuf[(wp2<<1)+1]^0x8000;

				rc1 = rv1-rvm1;
				rc2 = 2*rvm1-2*rc0+rv1-rv2;
				rc3 = rc0-rvm1-rv1+rv2;
				rc3 =  imulshr16(rc3,aybuffpos);
				rc3 += rc2;
				rc3 =  imulshr16(rc3,aybuffpos);
				rc3 += rc1;
				rc3 =  imulshr16(rc3,aybuffpos);
				rc3 += rc0;
				if (rc3<0)
					rc3=0;
				if (rc3>65535)
					rc3=65535;

				lc1 = lv1-lvm1;
				lc2 = 2*lvm1-2*lc0+lv1-lv2;
				lc3 = lc0-lvm1-lv1+lv2;
				lc3 =  imulshr16(lc3,aybuffpos);
				lc3 += lc2;
				lc3 =  imulshr16(lc3,aybuffpos);
				lc3 += lc1;
				lc3 =  imulshr16(lc3,aybuffpos);
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

				aybuffpos+=aybufrate;
				progress = aybuffpos>>16;
				aybuffpos &= 0xffff;

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
			ringbuffer_tail_consume_samples (aybufpos, accumulated_progress);
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

int __attribute__ ((visibility ("internal"))) ayOpenPlayer(FILE *file)
{
	aydata.filedata=NULL;
	aydata.tracks=NULL;
	_buf8_n=0;

	if (!plrPlay)
		return 0;

	if(!read_ay_file(file)) /* 0 meens error */
		return 0;

	plrSetOptions(44100, (PLR_SIGNEDOUT|PLR_16BIT)|PLR_STEREO);
	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);

	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize))
	{
		free(aydata.tracks);
		free(aydata.filedata);

		aybufpos = 0;
		aydata.tracks = 0;
		aydata.filedata = 0;
		return 0;
	}

	inpause=0;
	ay_looped=0;
	aySetVolume(64, 0, 64, 0);
/*
	aySetAmplify(amplify);   TODO */
	buf16=malloc(sizeof(uint16_t)*buflen*2);
	if (!buf16)
	{
		plrClosePlayer();

		free(aydata.tracks);
		free(aydata.filedata);

		aydata.tracks = 0;
		aydata.filedata = 0;

		return 0;
	}
	bufpos=0;
	aybuf=malloc(16384<<2/*stereo+16bit*/);
	if (!aybuf)
	{
		plrClosePlayer();

		free(buf16);
		free(aydata.tracks);
		free(aydata.filedata);

		aydata.tracks = 0;
		aydata.filedata = 0;
		buf16 = 0;

		return 0;
	}

	aybufpos = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, 16384);
	if (!aybufpos)
	{
		plrClosePlayer();

		free(buf16);
		free(aybuf);
		free(aydata.tracks);
		free(aydata.filedata);

		aydata.tracks = 0;
		aydata.filedata = 0;
		buf16 = 0;
		aybuf = 0;

		return 0;

	}

	aybuffpos=0;

	ay_track=0;
/*
	if(go_to_last)
	{
		go_to_last=0;
		ay_track=aydata.num_tracks-1;
	}*/

	if (!sound_init())
	{
		plrClosePlayer();

		ringbuffer_free (aybufpos);

		free(buf16);
		free(aybuf);
		free(aydata.tracks);
		free(aydata.filedata);

		aybufpos = 0;
		aydata.tracks = 0;
		aydata.filedata = 0;
		buf16 = 0;
		aybuf = 0;

		return 0;
	}

	ay_current_reg=0;
	sound_ay_reset();
	mem_init(ay_track);
	tunetime_reset();
	ay_tsmax=FRAME_STATES_128;
	do_cpc=0;
	ay_z80_init(aydata.tracks[ay_track].data,
	            aydata.tracks[ay_track].data_stacketc);

	if (!pollInit(ayIdle))
	{
		sound_end();

		plrClosePlayer();

		ringbuffer_free (aybufpos);

		free(buf16);
		free(aybuf);
		free(aydata.tracks);
		free(aydata.filedata);

		aybufpos = 0;
		aydata.tracks = 0;
		aydata.filedata = 0;
		buf16 = 0;
		aybuf = 0;

		return 0;
	}

	return 1;
}

void __attribute__ ((visibility ("internal"))) ayClosePlayer(void)
{
	pollClose();

	sound_end();

	plrClosePlayer();

	ringbuffer_free (aybufpos);

	free(buf16);
	free(aybuf);
	free(aydata.tracks);
	free(aydata.filedata);

	aybufpos = 0;
	aydata.tracks = 0;
	aydata.filedata = 0;
	buf16 = 0;
	aybuf = 0;
}

int __attribute__ ((visibility ("internal"))) ayIsLooped(void)
{
	return ay_looped == 3;
}

void __attribute__ ((visibility ("internal"))) aySetLoop(unsigned char s)
{
	donotloop=!s;
}

void __attribute__ ((visibility ("internal"))) ayPause(unsigned char p)
{
	inpause=p;
}

void __attribute__ ((visibility ("internal"))) aySetSpeed(uint16_t sp)
{
	if (sp<32)
		sp=32;
	aybufrate=256*sp;
}

void __attribute__ ((visibility ("internal"))) aySetVolume(unsigned char vol_, signed char bal_, signed char pan_, unsigned char opt)
{
	pan=pan_;
	if (reversestereo)
	{
		pan = -pan;
	}
	volr=voll=vol_*4;
	if (bal_<0)
		voll=(voll*(64+bal_))>>6;
	else
		volr=(volr*(64-bal_))>>6;
	srnd=opt;
}

void __attribute__ ((visibility ("internal"))) ayGetInfo(struct ayinfo *info)
{
	info->track=ay_track+1;
	info->numtracks=aydata.num_tracks;
	info->trackname=(char *)aydata.tracks[ay_track].namestr;
	info->filever=aydata.filever;
	info->playerver=aydata.playerver;
}


void __attribute__ ((visibility ("internal"))) ayStartSong(int song)
{
	new_ay_track=song-1;
	ringbuffer_reset(aybufpos);
}
