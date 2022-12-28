/* OpenCP Module Player
 * copyright (c) 2011-'22 Jindřich Makovička <makovick@gmail.com>
 *
 * C routines for FPU mixer
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

#include <assert.h>
#include <math.h>

#define MAXVOICES MIXF_MAXCHAN

#define state dwmixfa_state
dwmixfa_state_t state;

#if 0
float   *tempbuf;               /* pointer to 32 bit mix buffer (nsamples * 4) */
void    *outbuf;                /* pointer to mixed buffer (nsamples * 2) */
uint32_t nsamples;              /* # of samples to mix */
uint32_t nvoices;               /* # of voices to mix */
uint32_t freqw[MAXVOICES];      /* frequency (whole part) */
uint32_t freqf[MAXVOICES];      /* frequency (fractional part) */
float   *smpposw[MAXVOICES];    /* sample position (whole part (pointer!)) */
uint32_t smpposf[MAXVOICES];    /* sample position (fractional part) */
float   *loopend[MAXVOICES];    /* pointer to loop end */
uint32_t looplen[MAXVOICES];    /* loop length in samples */
float    volleft[MAXVOICES];    /* float: left volume (1.0=normal) */
float    volright[MAXVOICES];   /* float: rite volume (1.0=normal) */
float    rampleft[MAXVOICES];   /* float: left volramp (dvol/sample) */
float    rampright[MAXVOICES];  /* float: rite volramp (dvol/sample) */
uint32_t voiceflags[MAXVOICES]; /* voice status flags */
float    ffreq[MAXVOICES];      /* filter frequency (0<=x<=1) */
float    freso[MAXVOICES];      /* filter resonance (0<=x<1) */
float    fadeleft=0.0;          /* 0 */
float    fl1[MAXVOICES];        /* filter lp buffer */
float    fb1[MAXVOICES];        /* filter bp buffer */
float    faderight=0.0;         /* 0 */
int      outfmt;                /* output format */
float    voll=0.0;
float    volr=0.0;
float    ct0[256];              /* interpolation tab for s[-1] */
float    ct1[256];              /* interpolation tab for s[0] */
float    ct2[256];              /* interpolation tab for s[1] */
float    ct3[256];              /* interpolation tab for s[2] */
struct mixfpostprocregstruct *postprocs;
                                /* pointer to postproc list */
uint32_t samprate;              /* sampling rate */

static float volrl;
static float volrr;
#endif

static const float cremoveconst = 0.992;
static const float minampl = 0.0001;

#if 0
static uint32_t mixlooplen; /* 32bit in assembler used, decimal. lenght of loop in samples*/
static uint32_t looptype; /* 32bit in assembler used, local version of voiceflags[N] */
static float ffrq;
static float frez;
static float __fl1;
static float __fb1;
#endif

typedef void(*clippercall)(float *input, void *output, uint_fast32_t count);

static void clip_16s(float *input, void *output, uint_fast32_t count);
#if 0
static void clip_16u(float *input, void *output, uint_fast32_t count);
static void clip_8s(float *input, void *output, uint_fast32_t count);
static void clip_8u(float *input, void *output, uint_fast32_t count);

static const clippercall clippers[4] = {clip_8s, clip_8u, clip_16s, clip_16u};
#else

static const clippercall clippers[1] = {clip_16s};
#endif


typedef void(*mixercall)(float *destptr, float **sample_pos, uint32_t *sample_pos_fract, uint32_t sample_pitch, uint32_t sample_pitch_fract, float *loopend);

void
prepare_mixer (void)
{
	int i;

	state.fadeleft  = 0.0;
	state.faderight = 0.0;
	state.volrl = 0.0;
	state.volrr = 0.0;

	for (i = 0; i < MAXVOICES; i++)
		state.volleft[i] = dwmixfa_state.volright[i] = 0.0;
}

static inline
void clearbufs(float *samples, int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		*samples++ = state.fadeleft;
		*samples++ = state.faderight;
		state.fadeleft *= cremoveconst;
		state.faderight *= cremoveconst;
	}
}


static void
mix_0(float *destptr,
      float **sample_pos, uint32_t *sample_pos_fract,
      uint32_t sample_pitch, uint32_t sample_pitch_fract,
      float *loopend)
{
	int i;

	for (i = 0; i < state.nsamples; i++)
	{
		*sample_pos_fract += sample_pitch_fract;
		*sample_pos += sample_pitch + (*sample_pos_fract >> 16);
		*sample_pos_fract &= 0xffff;
		while (*sample_pos >= loopend)
		{
			if (!(state.looptype & MIXF_LOOPED))
			{
				state.looptype &= ~MIXF_PLAYING;
				goto out;
			}
			assert(state.mixlooplen > 0);
			*sample_pos -= state.mixlooplen;
		}
	}
out:
	;
}

static inline float
filter_none(float sample)
{
	return sample;
}

static inline float
filter_mixf(float sample)
{
	state.__fb1  =  state.__fb1  *  state.frez  +  state.ffrq  *  (  sample  -  state.__fl1  );

	return state.__fl1  +=  state.__fb1;
}

static inline float
interp_none(float* samples, uint_fast16_t sample_pos_fract)
{
	return *samples;
}

static inline float
interp_lin(float* samples, uint_fast16_t sample_pos_fract)
{
	return samples[0]
	        + (float)sample_pos_fract / 65536.0
	        * (samples[1] - samples[0]);
}

static inline float
interp_cub(float* samples, uint_fast16_t sample_pos_fract)
{
	int idx = sample_pos_fract >> 8;
	return samples[0] * state.ct0[idx]
	        + samples[1] * state.ct1[idx]
	        + samples[2] * state.ct2[idx]
	        + samples[3] * state.ct3[idx];
}

#define MIX_TEMPLATE(NAME, STEREO, INTERP, FILTER)                      \
static void                                                             \
mix##NAME(float *destptr,                                               \
       float **sample_pos, uint32_t *sample_pos_fract,                  \
       uint32_t sample_pitch, uint32_t sample_pitch_fract,              \
       float *loopend)                                                  \
{                                                                       \
    int i = 0;                                                          \
    float sample;                                                       \
                                                                        \
    for (i = 0; i < state.nsamples; i++)                                \
      {                                                                 \
        sample = filter_##FILTER(interp_##INTERP(*sample_pos, *sample_pos_fract)); \
        *destptr++ += state.voll * sample;                              \
        state.voll += state.volrl;                                      \
        if (STEREO) {                                                   \
            *destptr++ += state.volr * sample;                          \
            state.volr += state.volrr;                                  \
        }                                                               \
                                                                        \
        *sample_pos_fract += sample_pitch_fract;                        \
        *sample_pos += sample_pitch + (*sample_pos_fract >> 16);        \
        *sample_pos_fract &= 0xffff;                                    \
                                                                        \
        while (*sample_pos >= loopend)                                  \
          {                                                             \
            if (!(state.looptype & MIXF_LOOPED)) {                      \
                state.looptype &= ~MIXF_PLAYING;                        \
                goto fade;                                              \
            }                                                           \
            assert(state.mixlooplen > 0);                               \
            *sample_pos -= state.mixlooplen;                            \
          }                                                             \
      }                                                                 \
    return;                                                             \
                                                                        \
fade:                                                                   \
                                                                        \
    for (; i < state.nsamples; i++)                                     \
      {                                                                 \
        *destptr++ += state.voll * sample;                              \
        state.voll += state.volrl;                                      \
        if (STEREO) {                                                   \
            *destptr++ += state.volr * sample;                          \
            state.volr += state.volrr;                                  \
        }                                                               \
    }                                                                   \
                                                                        \
    state.fadeleft += state.voll * sample;                              \
    if (STEREO) {                                                       \
        state.faderight += state.volr * sample;                         \
    }                                                                   \
}

MIX_TEMPLATE(s_n, 1, none, none)
MIX_TEMPLATE(s_i, 1, lin, none)
MIX_TEMPLATE(s_i2, 1, cub, none)
MIX_TEMPLATE(s_nf, 1, none, mixf)
MIX_TEMPLATE(s_if, 1, lin, mixf)
MIX_TEMPLATE(s_i2f, 1, cub, mixf)

#if 0

MIX_TEMPLATE(m_n, 0, none, none)
MIX_TEMPLATE(m_i, 0, lin, none)
MIX_TEMPLATE(m_i2, 0, cub, none)
MIX_TEMPLATE(m_nf, 0, none, mixf)
MIX_TEMPLATE(m_if, 0, lin, mixf)
MIX_TEMPLATE(m_i2f, 0, cub, mixf)

static const mixercall mixers[16] = {
	mixm_n,   mixs_n,   mixm_i,  mixs_i,
	mixm_i2,  mixs_i2,  mix_0,   mix_0,
	mixm_nf,  mixs_nf,  mixm_if, mixs_if,
	mixm_i2f, mixs_i2f, mix_0,   mix_0
};

#else

static const mixercall mixers[8] = {
	mixs_n,   mixs_i,
	mixs_i2,  mix_0,
	mixs_nf,  mixs_if,
	mixs_i2f, mix_0
};

#endif

void
mixer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int voice;
	struct mixfpostprocregstruct *pp;

	if (fabsf(state.fadeleft) < minampl)
		state.fadeleft = 0.0;

	if (fabsf(state.faderight) < minampl)
		state.faderight = 0.0;

	if (state.nsamples == 0)
		return;

	clearbufs(state.tempbuf, state.nsamples);

	for (voice = state.nvoices - 1; voice >= 0; voice--)
	{
		mixercall mixer;

		if (!(state.voiceflags[voice] & MIXF_PLAYING))
			continue;

		state.looptype = state.voiceflags[voice];
		state.voll = state.volleft[voice];
		state.volr = state.volright[voice];
		state.volrl = state.rampleft[voice];
		state.volrr = state.rampright[voice];

		state.ffrq = state.ffreq[voice];
		state.frez = state.freso[voice];
		state.__fl1 = state.fl1[voice];
		state.__fb1 = state.fb1[voice];

		state.mixlooplen = state.looplen[voice];

/*
		assert((state.freqf[voice] & 0xffff) == 0);
		assert((state.smpposf[voice] & 0xffff) == 0);
*/
		mixer = mixers[state.voiceflags[voice] & 0x7];
		state.smpposf[voice] >>= 16;
		mixer(state.tempbuf,
		      &state.smpposw[voice], &state.smpposf[voice],
		      state.freqw[voice], state.freqf[voice] >> 16,
		      state.loopend[voice]);
		state.smpposf[voice] <<= 16;

		state.voiceflags[voice] = state.looptype;
		state.volleft[voice] = state.voll;
		state.volright[voice] = state.volr;
		state.fl1[voice] = state.__fl1;
		state.fb1[voice] = state.__fb1;
	}

	for (pp = state.postprocs; pp; pp = pp->next)
		pp->Process(cpifaceSession, state.tempbuf, state.nsamples, state.samprate);

	clippers[0](state.tempbuf, state.outbuf, 2 /* stereo */ * state.nsamples);
}

static void
clip_16s(float *input, void *output, uint_fast32_t count)
{
	int16_t *out = output;
	int i;

	for (i = 0; i < count; i++, input++, out++)
	{
		int_fast32_t s = *input;
		if (s > 32767)
			*out = 32767;
		else if (s < -32768)
			*out = -32768;
		else
			*out = s;
	}
}

#if 0
static void clip_16u(float *input, void *output, uint_fast32_t count)
{
	uint16_t *out = output;
	int i;

	for (i = 0; i < count; i++, input++, out++)
	{
		int_fast32_t s = *input;
		if (s > 32767)
			*out = 65535;
		else if (s < -32768)
			*out = 0;
		else
			*out = s + 32768;
	}
}

static void clip_8s(float *input, void *output, uint_fast32_t count)
{
	int8_t *out = output;
	int i;

	for (i = 0; i < count; i++, input++, out++)
	{
		int s = (int)(*input) >> 8;
		if (s > 127)
			*out = 127;
		else if (s < -128)
			*out = -128;
		else
			*out = s;
	}
}

static void clip_8u(float *input, void *output, uint_fast32_t count)
{
	uint8_t *out = output;
	int i;

	for (i = 0; i < count; i++, input++, out++)
	{
		int s = *input;
		if (s > 127)
			*out = 255;
		else if (s < -128)
			*out = 0;
		else
			*out = s + 128;
	}
}
#endif

void
getchanvol(int n, int len)
{
	float *sample_pos = state.smpposw[n];
	int sample_pos_fract = state.smpposf[n] >> 16;
	float sum = 0.0;
	int i;

	if (state.voiceflags[n] & MIXF_PLAYING)
	{
		for (i = 0; i < state.nsamples; i++)
		{
			sum += fabsf(*sample_pos);

			sample_pos_fract += state.freqf[n] >> 16;
			sample_pos += state.freqw[n] + (sample_pos_fract >> 16);
			sample_pos_fract &= 0xffff;
			while (sample_pos >= state.loopend[n])
			{
				if (!(state.voiceflags[n] & MIXF_LOOPED))
				{
					state.voiceflags[n] &= ~MIXF_PLAYING;
					goto out;
				}
				assert(state.looplen[n] > 0);
				sample_pos -= state.looplen[n];
			}
		}
	}

out:

	sum /= state.nsamples;
	state.voll = sum * state.volleft[n];
	state.volr = sum * state.volright[n];
}
