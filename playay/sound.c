/* aylet 0.3, a .AY music file player.
 * Copyright (C) 2001-2010 Russell Marks and Ian Collier.
 * See main.c for licence.
 *
 * sound.c - the sound emulation itself, based on the beeper/AY code I
 *           wrote for Fuse.
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

#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "types.h"
#include "dev/player.h"

#include "main.h"
#include "z80.h"

#include "sound.h"
#include "driver.h"

#define AY_CLOCK		1773400
#define AY_CLOCK_CPC		1000000

#define AMPL_BEEPER_00          ((int)(14.*256.*(-2.5+0.34)/5.0))
#define AMPL_BEEPER_01          ((int)(14.*256.*(-2.5+0.66)/5.0))
#define AMPL_BEEPER_10          ((int)(14.*256.*(-2.5+3.56)/5.0))
#define AMPL_BEEPER_11          ((int)(14.*256.*(-2.5+3.70)/5.0))
#define AMPL_AY_TONE		(28*256)	/* three of these */

/* max. number of sub-frame AY port writes allowed;
 * given the number of port writes theoretically possible in a
 * 50th I think this should be plenty.
 */
#define AY_CHANGE_MAX		8000

static int sound_framesiz;

static uint32_t ay_tone_levels[16];

static int16_t *sound_buf;
static int sound_oldval;

/* foo_subcycles are fixed-point with low 16 bits as fractional part.
 * The other bits count as the chip does.
 */
static uint32_t ay_tone_tick[3],ay_tone_high[3],ay_noise_tick;
static uint32_t ay_tone_subcycles,ay_env_subcycles;
static uint32_t ay_env_internal_tick,ay_env_tick;
static uint32_t ay_tick_incr;
static uint32_t ay_clock;
static uint32_t ay_tone_period[3],ay_noise_period,ay_env_period;

#define CLOCK_RESET(clock) {ay_clock=clock; ay_tick_incr=(int)(65536.*clock/sound_freq);}

/* AY registers */
/* we have 16 so we can fake an 8910 if needed (XXX any point?) */
static uint8_t sound_ay_registers[16];

struct ay_change_tag
  {
  uint32_t tstates;
  uint16_t ofs;
  uint8_t reg,val;
  };

static struct ay_change_tag ay_change[AY_CHANGE_MAX];
static int ay_change_count;

static int fading=0,fadetotal;
static int sfadetime;

static void sound_ay_init(void)
{
/* AY output doesn't match the claimed levels; these levels are based
 * on the measurements posted to comp.sys.sinclair in Dec 2001 by
 * Matthew Westcott, adjusted as I described in a followup to his post,
 * then scaled to 0..0xffff.
 */
static int levels[16]=
  {
  0x0000, 0x0385, 0x053D, 0x0770,
  0x0AD7, 0x0FD5, 0x15B0, 0x230C,
  0x2B4C, 0x43C1, 0x5A4B, 0x732F,
  0x9204, 0xAFF1, 0xD921, 0xFFFF
  };
int f;

/* scale the values down to fit */
for(f=0;f<16;f++)
  ay_tone_levels[f]=(levels[f]*AMPL_AY_TONE+0x8000)/0xffff;

ay_noise_tick=0;
ay_noise_period=1;
ay_env_internal_tick=ay_env_tick=0;
ay_env_period=1;
ay_tone_subcycles=ay_env_subcycles=0;
for(f=0;f<3;f++)
  ay_tone_tick[f]=ay_tone_high[f]=0,ay_tone_period[f]=1;

CLOCK_RESET(AY_CLOCK);

ay_change_count=0;
}


int __attribute__ ((visibility ("internal"))) sound_init(void)
{
/*
	if(!ay_driver_init(&sound_freq,&sound_stereo))
		return(0);
*/
	sound_framesiz=sound_freq/50;

	sound_buf=malloc(sizeof(int16_t)*sound_framesiz*6);
	if(sound_buf==NULL)
	{
		sound_end();
		return(0);
	}

	sound_oldval=AMPL_BEEPER_00;

	sound_ay_init();

	return(1);
}


void __attribute__ ((visibility ("internal"))) sound_end(void)
{
	if(sound_buf)
	{
		free(sound_buf);
		sound_buf = 0;
	}
/*  ay_driver_end();
  }*/
}

/* not great having this as a macro to inline it, but it's only
 * a fairly short routine, and it saves messing about.
 * (XXX ummm, possibly not so true any more :-))
 */
#define AY_GET_SUBVAL(chan)	(level*2*ay_tone_tick[chan]/tone_count)

#define AY_DO_TONE(var,chan) \
  is_low=0;								\
  if(is_on)								\
    {									\
    (var)=0;								\
    if(level)								\
      {									\
      if(ay_tone_high[chan])						\
        (var)= (level);							\
      else								\
        (var)=-(level),is_low=1;					\
      }									\
    }									\
  									\
  ay_tone_tick[chan]+=tone_count;					\
  count=0;								\
  while(ay_tone_tick[chan]>=ay_tone_period[chan])			\
    {									\
    count++;								\
    ay_tone_tick[chan]-=ay_tone_period[chan];				\
    ay_tone_high[chan]=!ay_tone_high[chan];				\
    									\
    /* has to be here, unfortunately... */				\
    if(is_on && count==1 && level && ay_tone_tick[chan]<tone_count)	\
      {									\
      if(is_low)							\
        (var)+=AY_GET_SUBVAL(chan);					\
      else								\
        (var)-=AY_GET_SUBVAL(chan);					\
      }									\
    }									\
  									\
  /* if it's changed more than once during the sample, we can't */	\
  /* represent it faithfully. So, just hope it's a sample.      */	\
  /* (That said, this should also help avoid aliasing noise.)   */	\
  if(is_on && count>1)							\
    (var)=-(level)

/* bitmasks for envelope */
#define AY_ENV_CONT	8
#define AY_ENV_ATTACK	4
#define AY_ENV_ALT	2
#define AY_ENV_HOLD	1

static void sound_ay_overlay(struct ay_driver_frame_state_t *states)
{
	static int rng=1;
	static int noise_toggle=0;
	static int env_first=1,env_rev=0,env_counter=15;
	int tone_level[3];
	int mixer,envshape;
	int f,g,level,count;
	int env_level;
	int16_t *ptr;
	struct ay_change_tag *change_ptr=ay_change;
	int changes_left=ay_change_count;
	int reg,r;
	int is_low,is_on;
	int chan1,chan2,chan3;
	int frametime=ay_tsmax*50;
	uint32_t tone_count;

	assert (sound_framesiz>0);

	/* convert change times to sample offsets */
	for(f=0;f<ay_change_count;f++)
	{
		ay_change[f].ofs=(ay_change[f].tstates*sound_freq)/frametime;
	}

	for(f=0,ptr=sound_buf;f<sound_framesiz;f++)
	{
		/* update ay registers. All this sub-frame change stuff
		 * is pretty hairy, but how else would you handle the
		 * samples in Robocop? :-) It also clears up some other
		 * glitches.
		 */
		while(changes_left && f>=change_ptr->ofs)
		{
			reg=change_ptr->reg;

			if (reg >= 0x16)
			{
				switch (change_ptr->val)
				{
					default:
					case 0x00:
						sound_oldval = AMPL_BEEPER_00;
						break;
					case 0x08:
						sound_oldval = AMPL_BEEPER_01;
						break;
					case 0x10:
						sound_oldval = AMPL_BEEPER_10;
						break;
					case 0x18:
						sound_oldval = AMPL_BEEPER_11;
						break;
				}
			} else {
				sound_ay_registers[reg]=change_ptr->val;

				/* fix things as needed for some register changes */
				switch(reg)
				{
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:
					case 5:
						r=reg>>1;
						/* a zero-len period is the same as 1 */
						ay_tone_period[r]= ( (sound_ay_registers[ reg | 1 ] & 15 ) << 8) | sound_ay_registers[ reg & ~1 ];

						if(!ay_tone_period[r])
						{
							ay_tone_period[r]=1;
						}

						/* important to get this right, otherwise e.g. Ghouls 'n' Ghosts
						 * has really scratchy, horrible-sounding vibrato.
						 */
						if(ay_tone_tick[r]>=ay_tone_period[r]*2)
							ay_tone_tick[r]%=ay_tone_period[r]*2;
						break;
					case 6:
						ay_noise_tick=0;
						ay_noise_period=(sound_ay_registers[reg]&31);
						if (!ay_noise_period)
						{
							ay_noise_period = 1;
						}
						break;
					case 11:
					case 12:
						/* this one *isn't* fixed-point */
						ay_env_period=sound_ay_registers[11]|(sound_ay_registers[12]<<8);
						if (!ay_env_period)
						{
							ay_env_period = 1;
						}
						break;
					case 13:
						ay_env_internal_tick=ay_env_tick=ay_env_subcycles=0;
						env_first=1;
						env_rev=0;
						env_counter=(sound_ay_registers[13]&AY_ENV_ATTACK)?0:15;
						break;
				}
			}
			change_ptr++;
			changes_left--;
		}

		/* the tone level if no enveloping is being used */
		for(g=0;g<3;g++)
			tone_level[g]=ay_tone_levels[sound_ay_registers[8+g]&15];

		/* envelope */
		envshape=sound_ay_registers[13];
		env_level=ay_tone_levels[env_counter];

		for(g=0;g<3;g++)
			if(sound_ay_registers[8+g]&16)
				tone_level[g]=env_level;

		/* envelope output counter gets incr'd every 16 AY cycles.
		 * Has to be a while, as this is sub-output-sample res.
		 */
		ay_env_subcycles+=ay_tick_incr;
		while(ay_env_subcycles>=(16<<16))
		{
			ay_env_subcycles-=(16<<16);
			ay_noise_tick++;
			ay_env_tick++;

			while(ay_env_tick>=ay_env_period)
			{
				ay_env_tick-=ay_env_period;

				/* do a 1/16th-of-period incr/decr if needed */
				if(env_first || ((envshape&AY_ENV_CONT) && !(envshape&AY_ENV_HOLD)))
				{
					if(env_rev)
						env_counter-=(envshape&AY_ENV_ATTACK)?1:-1;
					else
						env_counter+=(envshape&AY_ENV_ATTACK)?1:-1;
					if(env_counter<0)
						env_counter=0;
					if(env_counter>15)
						env_counter=15;
				}

				ay_env_internal_tick++;
				while(ay_env_internal_tick>=16)
				{
					ay_env_internal_tick-=16;

					/* end of cycle */
					if(!(envshape&AY_ENV_CONT))
					{
						env_counter=0;
					} else {
						if(envshape&AY_ENV_HOLD)
						{
							if(env_first && (envshape&AY_ENV_ALT))
							{
								env_counter=(env_counter?0:15);
							}
						} else {
							/* non-hold */
							if(envshape&AY_ENV_ALT)
							{
								env_rev=!env_rev;
							} else {
								env_counter=(envshape&AY_ENV_ATTACK)?0:15;
							}
						}
					}

					env_first=0;
				}
			}
		}

		/* generate tone+noise... or neither.
		 * (if no tone/noise is selected, the chip just shoves the
		 * level out unmodified. This is used by some sample-playing
		 * stuff.)
		 */
		chan1=tone_level[0];
		chan2=tone_level[1];
		chan3=tone_level[2];
		mixer=sound_ay_registers[7];

		ay_tone_subcycles+=ay_tick_incr;
		tone_count=ay_tone_subcycles>>(3+16);
		ay_tone_subcycles&=(8<<16)-1;

		level=chan1; is_on=!(mixer&1);
		AY_DO_TONE(chan1,0);
		if((mixer&0x08)==0 && noise_toggle)
			chan1=0;

		level=chan2; is_on=!(mixer&2);
		AY_DO_TONE(chan2,1);
		if((mixer&0x10)==0 && noise_toggle)
			chan2=0;

		level=chan3; is_on=!(mixer&4);
		AY_DO_TONE(chan3,2);
		if((mixer&0x20)==0 && noise_toggle)
			chan3=0;

		(*ptr++) = chan1;
		(*ptr++) = chan2;
		(*ptr++) = chan3;
		(*ptr++) = sound_oldval;
		(*ptr++) = noise_toggle ? -15000 : 15000;
		(*ptr++) = env_level * 4 - 32760;

		/* update noise RNG/filter */
		while(ay_noise_tick>=ay_noise_period)
		{
			ay_noise_tick-=ay_noise_period;

			if((rng&1)^((rng&2)?1:0))
				noise_toggle=!noise_toggle;

			/* rng is 17-bit shift reg, bit 0 is output.
			 * input is bit 0 xor bit 2.
			 */
			rng|=((rng&1)^((rng&4)?1:0))?0x20000:0;
			rng>>=1;
		}
	}

	states->clockrate = ay_clock;
	states->channel_a_period = ay_tone_period[0];
	states->channel_b_period = ay_tone_period[1];
	states->channel_c_period = ay_tone_period[2];
	states->noise_period = ay_noise_period;
	states->mixer = mixer & 0x3f;
	states->amplitude_a = sound_ay_registers[ 8] & 0x1f;
	states->amplitude_b = sound_ay_registers[ 9] & 0x1f;
	states->amplitude_c = sound_ay_registers[10] & 0x1f;
	states->envelope_period = ay_env_period;
	states->envelope_shape = envshape & 0x0f;
}


/* don't make the change immediately; record it for later,
 * to be made by sound_frame() (via sound_ay_overlay()).
 */
void __attribute__ ((visibility ("internal"))) sound_ay_write(int reg,int val,unsigned long tstates)
{
	if(reg>=15) return;

	if(ay_change_count<AY_CHANGE_MAX)
	{
		ay_change[ay_change_count].tstates=tstates;
		ay_change[ay_change_count].reg=reg;
		ay_change[ay_change_count].val=val;
		ay_change_count++;
	}
}


/* no need to call this initially, but should be called
 * on reset otherwise.
 */
void __attribute__ ((visibility ("internal"))) sound_ay_reset(void)
{
int f;

ay_change_count=0;
for(f=0;f<16;f++)
  sound_ay_write(f,0,0);
for(f=0;f<3;f++)
  ay_tone_high[f]=0;
ay_tone_subcycles=ay_env_subcycles=0;
fading=sfadetime=0;
sound_oldval=AMPL_BEEPER_00;

CLOCK_RESET(AY_CLOCK);	/* in case it was CPC before */
}


void __attribute__ ((visibility ("internal"))) sound_ay_reset_cpc(void)
{
sound_ay_reset();

CLOCK_RESET(AY_CLOCK_CPC);
}


/* returns zero if this frame was completely silent */
int __attribute__ ((visibility ("internal"))) sound_frame(struct ay_driver_frame_state_t *states)
{
int16_t *ptr;
int f,silent;
static int chk0=-1, chk1=-1, chk2=-1, chk3=-1;
int fulllen=sound_framesiz*6;

sound_ay_overlay(states);

/* check for a silent frame.
 * bit nasty, but it's the only way to be sure. :-)
 * We check pre-fade, and make a separate check for having faded-out
 * later. This is to avoid problems with beeper `silence' which is
 * really a constant high/low level (something similar is also
 * possible with the AY).
 *
 * To cope with beeper and arguably-buggy .ay files, we have to treat
 * *any* non-varying level as silence. Fair enough in a way, as it
 * will indeed be silent, but a bit of a pain.
 */

silent=1;

ptr=sound_buf;
for(f=0;f<fulllen;f++)
{
  if(*ptr++!=chk0)
  {
    silent=0;
    break;
  }
  if(*ptr++!=chk1)
  {
    silent=0;
    break;
  }
  if(*ptr++!=chk2)
  {
    silent=0;
    break;
  }
  if(*ptr++!=chk3)
  {
    silent=0;
    break;
  }
  ptr++;
  ptr++;
}

/* apply overall fade if we're in the middle of one. */
if(fading)
  {
  if(sfadetime<=0)
    memset(sound_buf,0,fulllen*sizeof(int16_t)),silent=1;
  else
    {
    ptr=sound_buf;
    for(f=0;f<sound_framesiz;f++,ptr++)
      {
      /* XXX kludgey, but needed to avoid overflow */
      sfadetime--;
      *ptr=(*ptr)*(sfadetime>>4)/(fadetotal>>4);

	ptr++;
	*ptr=(*ptr)*(sfadetime>>4)/(fadetotal>>4);

	ptr++;
	*ptr=(*ptr)*(sfadetime>>4)/(fadetotal>>4);

	ptr++;
	*ptr=(*ptr)*(sfadetime>>4)/(fadetotal>>4);

	ptr++;
	ptr++;
      }
    }
  }

ay_driver_frame(sound_buf,fulllen*sizeof(int16_t));

ay_change_count=0;

return(!silent);
}

#if 0
/* don't do a real frame, just play silence to keep things sane. */
void __attribute__ ((visibility ("internal"))) sound_frame_blank(void)
{
static int first=1;
static signed short buf[2048];		/* should be plenty */
int fulllen=sound_framesiz*(sound_stereo+1);

if(first)
  {
  first=0;
  memset(buf,0,sizeof(buf));
  }

/* just in case it's *not* plenty... :-) */
if(sizeof(buf)<fulllen)
  {
  usleep(20000);
  return;
  }

ay_driver_frame(buf,fulllen);
}
#endif
void __attribute__ ((visibility ("internal"))) sound_start_fade(int fadetime_in_sec)
{
fading=1;
sfadetime=fadetotal=fadetime_in_sec*sound_freq;
}

void __attribute__ ((visibility ("internal"))) sound_beeper(int on, unsigned long tstates)
{
	if(ay_change_count<AY_CHANGE_MAX)
	{
		ay_change[ay_change_count].tstates=tstates;
		ay_change[ay_change_count].reg=0x16;
		ay_change[ay_change_count].val=on;
		ay_change_count++;
	}
}
