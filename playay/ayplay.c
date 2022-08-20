/* OpenCP Module Player
 * Copyright (C) 2001-2005 Russell Marks and Ian Collier.
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * This file is based on "aylet" main.c, but is VERY modified.
 *
 * ayplay.c - the sound emulation itself, based on the beeper/AY code I
 *            wrote for Fuse.
 *
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

/* aydumpbuffer <- dump of the last frame of audio (belongs to aylet internals)
 * aydumpbuffer_n  is the length on the last frame of audio
 *
 * we expunge aydumpbuffer into aybuf as fast as we can, when aydumpbuffer is empty, we rerun
 * the aylet internals.
 *
 * aybuf (16bit, stereo, 16384 samples long)
 * aybufpos ringbuffer_t tracker
 * aybuffpos
 * aybufrate -> conversion-rate converter if user requests speed changes
 *
 * We convert if needed to target 8/16bit, signed/unsigned, mono/stereo
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

#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "stuff/imsrtns.h"

#include "ayplay.h"
#include "main.h"
#include "sound.h"
#include "z80.h"

/* #define PLAYAY_DEBUG_OUTPUT 1 */
#ifdef PLAYAY_DEBUG_OUTPUT
static int debug_output = -1;
#endif
#ifdef PLAYAY_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format, args...) ((void)0)
#endif

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
static int ay_inpause;

static unsigned long voll,volr;
static int bal;
static int vol;
static int pan;
static int srnd;

/* pre-buffer zone */
static int16_t *aydumpbuffer; /* here we dump a single frame of data before aybuf swallows them*/
static size_t aydumpbuffer_n; /* samples */
static char ayMute[4];

#define MAX_BUF8_DELAYED_STATES 120 /* this will be 2 seconds of tracked data on a 60Hz system */
struct aydumpbuffer_delayed_states_t
{
	struct ay_driver_frame_state_t aydumpbuffer_states;
	int inaybuf;
	int indevp; /* if both of the in* flags are empty, entry is not in use */
};
static struct aydumpbuffer_delayed_states_t aydumpbuffer_delayed_states[MAX_BUF8_DELAYED_STATES];
static struct aydumpbuffer_delayed_states_t *aydumpbuffer_delayed_state = 0;
static struct aydumpbuffer_delayed_states_t aydumpbuffer_state_current;

static int donotloop=1;

static int16_t *aybuf;     /* the buffer */
static struct ringbuffer_t *aybufpos = 0;
static uint32_t aybuffpos; /* read fine-pos.. */
static uint32_t aybufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */

/* clipper threadlock since we use a timer-signal */
static volatile int clipbusy=0;

static struct aydumpbuffer_delayed_states_t *aydumpbuffer_delayed_states_slot_get(void)
{
	int i;
	for (i=0; i < MAX_BUF8_DELAYED_STATES; i++)
	{
		if (aydumpbuffer_delayed_states[i].inaybuf) continue;
		if (aydumpbuffer_delayed_states[i].indevp) continue;
		return aydumpbuffer_delayed_states + i;
	}
	return 0;
}

void __attribute__ ((visibility ("internal"))) ayGetChans(struct ay_driver_frame_state_t *dst)
{
	*dst = aydumpbuffer_state_current.aydumpbuffer_states;
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
				sound_beeper(a&0x18, ay_tstates);
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
static int read_ay_file (struct ocpfilehandle_t *in)
{
	unsigned char *data,*ptr,*ptr2;
	int tmp,f;
	uint64_t data_len;

	data_len = in->filesize(in);
	if (data_len > 1024*1024)
	{
		return 0;
	}
	data = malloc (data_len);
	if (!data)
	{
		return 0;
	}
	in->seek_set (in, 0);
	if (in->read (in, data, data_len) != data_len)
	{
		return 0;
	}

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

/* returns zero if we want to exit the emulation (i.e. exit track) */
static int silent_for=0;
int __attribute__ ((visibility ("internal"))) ay_do_interrupt(void)
{
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

	aydumpbuffer_delayed_state = aydumpbuffer_delayed_states_slot_get();
	if (!aydumpbuffer_delayed_state)
	{
		fprintf (stderr, "WARNING: aydumpbuffer_delayed_states_slot_get() gave null\n");
		return 0;
	}
	if(!sound_frame(&aydumpbuffer_delayed_state->aydumpbuffer_states))
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

void __attribute__ ((visibility ("internal"))) ay_driver_frame(int16_t *quad_samples, size_t bytes)
{
	int i;

	debug_printf ("ay_driver_frame (%p, %d)\n", quad_samples, (int)bytes);

#ifdef PLAYAY_DEBUG_OUTPUT
	write (debug_output, quad_samples, bytes);
#endif

	/* down-mix from 6 channels down to 2 channels */
	for (i=0; i < (bytes / 12); i++)
	{
		int16_t left = 0;
		int16_t right = 0;

		if (!ayMute[0])
		{
			left += quad_samples[(i*6)+0];
		}
		if (!ayMute[1])
		{
			left += (quad_samples[(i*6)+1]>>1);
			right +=  (quad_samples[(i*6)+1]>>1);
		}
		if (!ayMute[2])
		{
			right += quad_samples[(i*6)+2];
		}
		if (!ayMute[3])
		{
			left += (quad_samples[(i*6)+3]>>1);
			right += (quad_samples[(i*6)+3]>>1);
		}
		quad_samples[(i<<1)+0] = left;
		quad_samples[(i<<1)+1] = right;
	}
	aydumpbuffer=quad_samples;
	aydumpbuffer_n=bytes / 12;
}

void __attribute__ ((visibility ("internal"))) aySetMute (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int mute)
{
	cpifaceSession->MuteChannel[ch] = mute;
	switch (ch)
	{
		case 0: ayMute[0] = mute; break;
		case 1: ayMute[1] = mute; break;
		case 2: ayMute[2] = mute; break;
		case 3: ayMute[3] = mute; break;
	}
}

static void aydumpbuffer_delay_callback_from_devp (void *arg, int samples_ago)
{
	struct aydumpbuffer_delayed_states_t *state = arg;
	aydumpbuffer_state_current = *state;
	state->indevp = 0;
}

static void aydumpbuffer_delay_callback_from_aybuf_to_devp (void *arg, int samples_ago)
{
	struct aydumpbuffer_delayed_states_t *state = arg;

	int samples_until = samples_ago * aybufrate / 65536;

	debug_printf ("aydumpbuffer_delay_callback_from_aybuf_to_devp: arg=%p samples_ago=%d\n", arg, samples_ago);

	state->inaybuf = 0;
	state->indevp = 1;
	plrAPI->OnBufferCallback (-samples_until, aydumpbuffer_delay_callback_from_devp, state);
}

static void ayIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int pos1, pos2;
	int length1, length2;

	cpifaceSession->ringbufferAPI->get_head_samples (aybufpos, &pos1, &length1, &pos2, &length2);

	while (length1)
	{
		while (!aydumpbuffer_n)
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

		if (aydumpbuffer_delayed_state)
		{
			aydumpbuffer_delayed_state->inaybuf = 1;
			cpifaceSession->ringbufferAPI->add_tail_callback_samples (aybufpos, 0, aydumpbuffer_delay_callback_from_aybuf_to_devp, aydumpbuffer_delayed_state);
			aydumpbuffer_delayed_state = 0;
		}

		if (length1 > aydumpbuffer_n)
		{
			length1 = aydumpbuffer_n;
		}
		memcpy (aybuf + (pos1<<1), aydumpbuffer, length1<<2);
		aydumpbuffer += length1<<1;
		aydumpbuffer_n -= length1;
		cpifaceSession->ringbufferAPI->head_add_samples (aybufpos, length1);
		cpifaceSession->ringbufferAPI->get_head_samples (aybufpos, &pos1, &length1, &pos2, &length2);
	}
}

void __attribute__ ((visibility ("internal"))) ayIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (ay_inpause || (ay_looped == 3))
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

			ayIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (aybufpos, &pos1, &length1, &pos2, &length2);

			if (aybufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2); // limiting targetlength here, saves us from doing this per sample later
					ay_looped |= 2;
				} else {
					ay_looped &= ~2;
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

						rs = aybuf[(pos1<<1) + 0];
						ls = aybuf[(pos1<<1) + 1];

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
				ay_looped &= ~2;

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
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)aybuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)aybuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)aybuf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)aybuf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)aybuf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)aybuf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)aybuf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)aybuf[(wp2 <<1)+1]^0x8000;

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

						*(t++) = rs;
						*(t++) = ls;

						aybuffpos+=aybufrate;
						progress = aybuffpos>>16;
						aybuffpos &= 0xffff;
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
			} /* if (aybufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (aybufpos, accumulated_source);
			plrAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	plrAPI->Idle();

	clipbusy--;
}

static void aySetSpeed(uint16_t sp)
{
	if (sp<32)
		sp=32;
	aybufrate=256*sp;
}

static void aySetVolume(void)
{
	volr=voll=vol*4;
	if (bal<0)
		voll=(voll*(64+bal))>>6;
	else
		volr=(volr*(64-bal))>>6;
}

static void aySet (int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			aySetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			aySetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			aySetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			aySetVolume();
			break;
	}
}

static int ayGet (int ch, int opt)
{
	return 0;
}

int __attribute__ ((visibility ("internal"))) ayOpenPlayer(struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	uint32_t ayRate;
	enum plrRequestFormat format;

	aydata.filedata = NULL;
	aydata.tracks = NULL;
	aydumpbuffer_n = 0;

	if (!plrAPI)
		return 0;

	if(!read_ay_file(file)) /* 0 meens error */
		return 0;

	bzero (aydumpbuffer_delayed_states, sizeof (aydumpbuffer_delayed_states));

	ayRate=0;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!plrAPI->Play (&ayRate, &format, file, cpifaceSession))
	{
		goto errorout_aydata;
	}
	sound_freq = ayRate;

	ay_inpause = 0;
	ay_looped = 0;
	aybuf = malloc(16384 << 2 /* stereo + 16bit */);
	if (!aybuf)
	{
		goto errorout_plrAPI_Start;
	}

	aybufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, 16384);
	if (!aybufpos)
	{
		goto errorout_aybuf;
	}
	aybuffpos=0;

	ay_track=0;
/*
	if(go_to_last)
	{
		go_to_last=0;
		ay_track=aydata.num_tracks-1;
	}
*/
	bzero (ayMute, sizeof(ayMute));

	if (!sound_init())
	{
		goto errorout_ringbuffer_aybufpos;
	}

	bzero (&aydumpbuffer_state_current, sizeof (aydumpbuffer_state_current));
	aydumpbuffer_state_current.aydumpbuffer_states.clockrate = 1000000;;
	aydumpbuffer_state_current.aydumpbuffer_states.channel_a_period = 1;
	aydumpbuffer_state_current.aydumpbuffer_states.channel_b_period = 1;
	aydumpbuffer_state_current.aydumpbuffer_states.channel_c_period = 1;
	aydumpbuffer_state_current.aydumpbuffer_states.noise_period = 1;
	aydumpbuffer_state_current.aydumpbuffer_states.envelope_period = 1;

	ay_current_reg=0;
	sound_ay_reset();
	mem_init(ay_track);
	tunetime_reset();
	ay_tsmax=FRAME_STATES_128;
	do_cpc=0;
	ay_z80_init(aydata.tracks[ay_track].data,
	            aydata.tracks[ay_track].data_stacketc);

#ifdef PLAYAY_DEBUG_OUTPUT
	debug_output = open ("test.raw", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
#endif

	cpifaceSession->mcpSet=aySet;
	cpifaceSession->mcpGet=ayGet;

	cpifaceSession->mcpAPI->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	return 1;

	//sound_end();
errorout_ringbuffer_aybufpos:
	cpifaceSession->ringbufferAPI->free (aybufpos);
	aybufpos = 0;
errorout_aybuf:
	free(aybuf);
	aybuf = 0;
errorout_plrAPI_Start:
	plrAPI->Stop ();
errorout_aydata:
	free(aydata.tracks);
	aydata.tracks = 0;
	free(aydata.filedata);
	aydata.filedata = 0;
	return 0;
}

void __attribute__ ((visibility ("internal"))) ayClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	sound_end();

	plrAPI->Stop();

	if (aybufpos)
	{
		cpifaceSession->ringbufferAPI->free (aybufpos);
		aybufpos = 0;
	}

	free(aybuf);
	free(aydata.tracks);
	free(aydata.filedata);

	aydata.tracks = 0;
	aydata.filedata = 0;
	aybuf = 0;

#ifdef PLAYAY_DEBUG_OUTPUT
	close (debug_output);
	debug_output = -1;
#endif
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
	ay_inpause=p;
}

void __attribute__ ((visibility ("internal"))) ayGetInfo(struct ayinfo *info)
{
	info->track=ay_track+1;
	info->numtracks=aydata.num_tracks;
	info->trackname=(char *)aydata.tracks[ay_track].namestr;
	info->filever=aydata.filever;
	info->playerver=aydata.playerver;
}


void __attribute__ ((visibility ("internal"))) ayStartSong (struct cpifaceSessionAPI_t *cpifaceSession, int song)
{
	new_ay_track=song-1;
	cpifaceSession->ringbufferAPI->reset(aybufpos);
}
