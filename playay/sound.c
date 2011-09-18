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

/* configuration */
#if 0
int soundfd=-1;			/* file descriptor for the sound device */
int sixteenbit=0;		/* use sixteen-bit audio? */

int sound_enabled=0;
int sound_freq=44100;
int sound_stereo=1;		/* true for stereo *output sample* (only) */
#endif
__attribute__ ((visibility ("internal"))) int sound_stereo_beeper=0;	/* beeper pseudo-stereo */
__attribute__ ((visibility ("internal"))) int sound_stereo_ay=1;		/* AY stereo separation */
__attribute__ ((visibility ("internal"))) int sound_stereo_ay_abc=0;	/* (AY stereo) true for ABC stereo, else ACB */
__attribute__ ((visibility ("internal"))) int sound_stereo_ay_narrow=0;	/* (AY stereo) true for narrow AY st. sep. */

#define AY_CLOCK		1773400
#define AY_CLOCK_CPC		1000000


/* assume all three tone channels together match the beeper volume.
 * (XXX maybe not - that makes beeper stuff annoyingly loud)
 * Must be <=127 for all channels; 40+(28*3) = 124.
 */
#define AMPL_BEEPER		(40*256)
#define AMPL_AY_TONE		(28*256)	/* three of these */

/* full range of beeper volume */
#define VOL_BEEPER		(AMPL_BEEPER*2)

/* max. number of sub-frame AY port writes allowed;
 * given the number of port writes theoretically possible in a
 * 50th I think this should be plenty.
 */
#define AY_CHANGE_MAX		8000

static int sound_framesiz;

static uint32_t ay_tone_levels[16];

static int16_t *sound_buf;
static int sound_oldpos,sound_fillpos,sound_oldval,sound_oldval_orig;

/* foo_subcycles are fixed-point with low 16 bits as fractional part.
 * The other bits count as the chip does.
 */
static uint32_t ay_tone_tick[3],ay_tone_high[3],ay_noise_tick;
static uint32_t ay_tone_subcycles,ay_env_subcycles;
static uint32_t ay_env_internal_tick,ay_env_tick;
static uint32_t ay_tick_incr;
static uint32_t ay_tone_period[3],ay_noise_period,ay_env_period;

static int beeper_last_subpos=0;

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


#define STEREO_BUF_SIZE 1024

static int pstereobuf[STEREO_BUF_SIZE];
static int pstereobufsiz,pstereopos;
static int psgap=250;
static int rstereobuf_l[STEREO_BUF_SIZE],rstereobuf_r[STEREO_BUF_SIZE];
static int rstereopos,rchan1pos,rchan2pos,rchan3pos;



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

ay_noise_tick=ay_noise_period=0;
ay_env_internal_tick=ay_env_tick=ay_env_period=0;
ay_tone_subcycles=ay_env_subcycles=0;
for(f=0;f<3;f++)
  ay_tone_tick[f]=ay_tone_high[f]=0,ay_tone_period[f]=1;

#define CLOCK_RESET(clock) ay_tick_incr=(int)(65536.*clock/sound_freq)

CLOCK_RESET(AY_CLOCK);

ay_change_count=0;
}


int __attribute__ ((visibility ("internal"))) sound_init(void)
{
int f;
/*
if(!ay_driver_init(&sound_freq,&sound_stereo))
  return(0);
*/
/* important to override these if not using stereo */
if(!sound_stereo)
  {
  sound_stereo_ay=0;
  sound_stereo_beeper=0;
  }

/*sound_enabled=1;*/
sound_framesiz=sound_freq/50;

if((sound_buf=malloc(sizeof(int16_t)*sound_framesiz*(sound_stereo+1)))==NULL)
  {
  sound_end();
  return(0);
  }

sound_oldval=sound_oldval_orig=0;
sound_oldpos=-1;
sound_fillpos=0;

sound_ay_init();

if(sound_stereo_beeper)
  {
  for(f=0;f<STEREO_BUF_SIZE;f++)
    pstereobuf[f]=0;
  pstereopos=0;
  pstereobufsiz=(sound_freq*psgap)/22000;
  }

if(sound_stereo_ay)
  {
  int pos=(sound_stereo_ay_narrow?3:6)*sound_freq/8000;

  for(f=0;f<STEREO_BUF_SIZE;f++)
    rstereobuf_l[f]=rstereobuf_r[f]=0;
  rstereopos=0;

  /* the actual ACB/ABC bit :-) */
  rchan1pos=-pos;
  if(sound_stereo_ay_abc)
    rchan2pos=0,  rchan3pos=pos;
  else
    rchan2pos=pos,rchan3pos=0;
  }

return(1);
}


void __attribute__ ((visibility ("internal"))) sound_end(void)
{
/*if(sound_enabled)
  {*/
  if(sound_buf)
    free(sound_buf);
/*  ay_driver_end();
  sound_enabled=0;
  }*/
}


/* write sample to buffer as pseudo-stereo */
static void sound_write_buf_pstereo(int16_t *out,int c)
{
int bl=(c-pstereobuf[pstereopos])/2;
int br=(c+pstereobuf[pstereopos])/2;

if(bl<-AMPL_BEEPER) bl=-AMPL_BEEPER;
if(br<-AMPL_BEEPER) br=-AMPL_BEEPER;
if(bl> AMPL_BEEPER) bl= AMPL_BEEPER;
if(br> AMPL_BEEPER) br= AMPL_BEEPER;

*out=bl; out[1]=br;

pstereobuf[pstereopos]=c;
pstereopos++;
if(pstereopos>=pstereobufsiz)
  pstereopos=0;
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


/* add val, correctly delayed on either left or right buffer,
 * to add the AY stereo positioning. This doesn't actually put
 * anything directly in soundbuf, though.
 */
#define GEN_STEREO(pos,val) \
  if((pos)<0)							\
    {								\
    rstereobuf_l[rstereopos]+=(val);				\
    rstereobuf_r[(rstereopos-pos)%STEREO_BUF_SIZE]+=(val);	\
    }								\
  else								\
    {								\
    rstereobuf_l[(rstereopos+pos)%STEREO_BUF_SIZE]+=(val);	\
    rstereobuf_r[rstereopos]+=(val);				\
    }



/* bitmasks for envelope */
#define AY_ENV_CONT	8
#define AY_ENV_ATTACK	4
#define AY_ENV_ALT	2
#define AY_ENV_HOLD	1


static void sound_ay_overlay(void)
{
static int rng=1;
static int noise_toggle=0;
static int env_first=1,env_rev=0,env_counter=15;
int tone_level[3];
int mixer,envshape;
int f,g,level,count;
int16_t *ptr;
struct ay_change_tag *change_ptr=ay_change;
int changes_left=ay_change_count;
int reg,r;
int is_low,is_on;
int chan1,chan2,chan3;
int frametime=ay_tsmax*50;
uint32_t tone_count,noise_count;

/* convert change times to sample offsets */
for(f=0;f<ay_change_count;f++)
  ay_change[f].ofs=(ay_change[f].tstates*sound_freq)/frametime;

for(f=0,ptr=sound_buf;f<sound_framesiz;f++)
  {
  /* update ay registers. All this sub-frame change stuff
   * is pretty hairy, but how else would you handle the
   * samples in Robocop? :-) It also clears up some other
   * glitches.
   */
  while(changes_left && f>=change_ptr->ofs)
    {
    sound_ay_registers[reg=change_ptr->reg]=change_ptr->val;
    change_ptr++; changes_left--;

    /* fix things as needed for some register changes */
    switch(reg)
      {
      case 0: case 1: case 2: case 3: case 4: case 5:
        r=reg>>1;
        /* a zero-len period is the same as 1 */
        ay_tone_period[r]=(sound_ay_registers[reg&~1]|
                           (sound_ay_registers[reg|1]&15)<<8);
        if(!ay_tone_period[r])
          ay_tone_period[r]++;

        /* important to get this right, otherwise e.g. Ghouls 'n' Ghosts
         * has really scratchy, horrible-sounding vibrato.
         */
        if(ay_tone_tick[r]>=ay_tone_period[r]*2)
          ay_tone_tick[r]%=ay_tone_period[r]*2;
        break;
      case 6:
        ay_noise_tick=0;
        ay_noise_period=(sound_ay_registers[reg]&31);
        break;
      case 11: case 12:
        /* this one *isn't* fixed-point */
        ay_env_period=sound_ay_registers[11]|(sound_ay_registers[12]<<8);
        break;
      case 13:
        ay_env_internal_tick=ay_env_tick=ay_env_subcycles=0;
        env_first=1;
        env_rev=0;
        env_counter=(sound_ay_registers[13]&AY_ENV_ATTACK)?0:15;
        break;
      }
    }

  /* the tone level if no enveloping is being used */
  for(g=0;g<3;g++)
    tone_level[g]=ay_tone_levels[sound_ay_registers[8+g]&15];

  /* envelope */
  envshape=sound_ay_registers[13];
  level=ay_tone_levels[env_counter];

  for(g=0;g<3;g++)
    if(sound_ay_registers[8+g]&16)
      tone_level[g]=level;

  /* envelope output counter gets incr'd every 16 AY cycles.
   * Has to be a while, as this is sub-output-sample res.
   */
  ay_env_subcycles+=ay_tick_incr;
  noise_count=0;
  while(ay_env_subcycles>=(16<<16))
    {
    ay_env_subcycles-=(16<<16);
    noise_count++;
    ay_env_tick++;
    while(ay_env_tick>=ay_env_period)
      {
      ay_env_tick-=ay_env_period;

      /* do a 1/16th-of-period incr/decr if needed */
      if(env_first ||
         ((envshape&AY_ENV_CONT) && !(envshape&AY_ENV_HOLD)))
        {
        if(env_rev)
          env_counter-=(envshape&AY_ENV_ATTACK)?1:-1;
        else
          env_counter+=(envshape&AY_ENV_ATTACK)?1:-1;
        if(env_counter<0) env_counter=0;
        if(env_counter>15) env_counter=15;
        }

      ay_env_internal_tick++;
      while(ay_env_internal_tick>=16)
        {
        ay_env_internal_tick-=16;

        /* end of cycle */
        if(!(envshape&AY_ENV_CONT))
          env_counter=0;
        else
          {
          if(envshape&AY_ENV_HOLD)
            {
            if(env_first && (envshape&AY_ENV_ALT))
              env_counter=(env_counter?0:15);
            }
          else
            {
            /* non-hold */
            if(envshape&AY_ENV_ALT)
              env_rev=!env_rev;
            else
              env_counter=(envshape&AY_ENV_ATTACK)?0:15;
            }
          }

        env_first=0;
        }

      /* don't keep trying if period is zero */
      if(!ay_env_period) break;
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

  /* write the sample(s) */
  if(!sound_stereo)
    {
    /* mono */
    (*ptr++)+=chan1+chan2+chan3;
    }
  else
    {
    if(!sound_stereo_ay)
      {
      /* stereo output, but mono AY sound; still,
       * incr separately in case of beeper pseudostereo.
       */
      (*ptr++)+=chan1+chan2+chan3;
      (*ptr++)+=chan1+chan2+chan3;
      }
    else
      {
      /* stereo with ACB AY positioning.
       * Here we use real stereo positions for the channels.
       * Just because, y'know, it's cool and stuff. No, really. :-)
       * This is a little tricky, as it works by delaying sounds
       * on the left or right channels to model the delay you get
       * in the real world when sounds originate at different places.
       */
      GEN_STEREO(rchan1pos,chan1);
      GEN_STEREO(rchan2pos,chan2);
      GEN_STEREO(rchan3pos,chan3);
      (*ptr++)+=rstereobuf_l[rstereopos];
      (*ptr++)+=rstereobuf_r[rstereopos];
      rstereobuf_l[rstereopos]=rstereobuf_r[rstereopos]=0;
      rstereopos++;
      if(rstereopos>=STEREO_BUF_SIZE)
        rstereopos=0;
      }
    }

  /* update noise RNG/filter */
  ay_noise_tick+=noise_count;
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

    /* don't keep trying if period is zero */
    if(!ay_noise_period) break;
    }
  }
}


/* don't make the change immediately; record it for later,
 * to be made by sound_frame() (via sound_ay_overlay()).
 */
void __attribute__ ((visibility ("internal"))) sound_ay_write(int reg,int val,unsigned long tstates)
{
/*if(!sound_enabled) return;*/

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
sound_oldval=sound_oldval_orig=0;

CLOCK_RESET(AY_CLOCK);	/* in case it was CPC before */
}


void __attribute__ ((visibility ("internal"))) sound_ay_reset_cpc(void)
{
sound_ay_reset();

CLOCK_RESET(AY_CLOCK_CPC);
}


/* write stereo or mono beeper sample, and incr ptr */
#define SOUND_WRITE_BUF_BEEPER(ptr,val) \
  do						\
    {						\
    if(sound_stereo_beeper)			\
      {						\
      sound_write_buf_pstereo((ptr),(val));	\
      (ptr)+=2;					\
      }						\
    else					\
      {						\
      *(ptr)++=(val);				\
      if(sound_stereo)				\
        *(ptr)++=(val);				\
      }						\
    }						\
  while(0)


/* returns zero if this frame was completely silent */
int __attribute__ ((visibility ("internal"))) sound_frame(int really_play)
{
static int silent_level=-1;
int16_t *ptr;
int f,silent,chk;
int fulllen=sound_framesiz*(sound_stereo+1);

ptr=sound_buf+(sound_stereo?sound_fillpos*2:sound_fillpos);
for(f=sound_fillpos;f<sound_framesiz;f++)
  SOUND_WRITE_BUF_BEEPER(ptr,sound_oldval);

sound_ay_overlay();

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
chk=*ptr++;
for(f=1;f<fulllen;f++)
  {
  if(*ptr++!=chk)
    {
    silent=0;
    break;
    }
  }

/* even if they're all the same, it doesn't count if the
 * level's changed since last time...
 */
if(chk!=silent_level)
  silent=0;

/* save last sample for comparison next time */
silent_level=sound_buf[fulllen-1];

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
      if(sound_stereo)
        {
        ptr++;
	*ptr=(*ptr)*(sfadetime>>4)/(fadetotal>>4);
        }
      }
    }
  }

if(really_play)
  ay_driver_frame((uint16_t *)sound_buf,fulllen);

sound_oldpos=-1;
sound_fillpos=0;

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

void __attribute__ ((visibility ("internal"))) sound_beeper(int on)
{
int16_t *ptr;
int newpos;
int subpos;
int val,subval;
int f;

/*if(!sound_enabled) return;*/

val=(on?-AMPL_BEEPER:AMPL_BEEPER);

if(val==sound_oldval_orig) return;

/* XXX a lookup table might help here... */
newpos=(ay_tstates*sound_framesiz)/ay_tsmax;
/* XXX long long may be dodgy portability-wise... */
subpos=(((uint64_t)ay_tstates)*sound_framesiz*VOL_BEEPER)/ay_tsmax-VOL_BEEPER*newpos;

/* if we already wrote here, adjust the level.
 */
if(newpos==sound_oldpos)
  {
  /* adjust it as if the rest of the sample period were all in
   * the new state. (Often it will be, but if not, we'll fix
   * it later by doing this again.)
   */
  if(on)
    beeper_last_subpos+=VOL_BEEPER-subpos;
  else
    beeper_last_subpos-=VOL_BEEPER-subpos;
  }
else
  beeper_last_subpos=(on?VOL_BEEPER-subpos:subpos);

subval=AMPL_BEEPER-beeper_last_subpos;

if(newpos>=0)
  {
  /* fill gap from previous position */
  ptr=sound_buf+(sound_stereo?sound_fillpos*2:sound_fillpos);
  for(f=sound_fillpos;f<newpos && f<sound_framesiz;f++)
    SOUND_WRITE_BUF_BEEPER(ptr,sound_oldval);

  if(newpos<sound_framesiz)
    {
    /* newpos may be less than sound_fillpos, so... */
    ptr=sound_buf+(sound_stereo?newpos*2:newpos);

    /* write subsample value */
    SOUND_WRITE_BUF_BEEPER(ptr,subval);
    }
  }

sound_oldpos=newpos;
sound_fillpos=newpos+1;
sound_oldval=sound_oldval_orig=val;
}
