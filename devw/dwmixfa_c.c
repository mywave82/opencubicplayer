/* OpenCP Module Player
 * copyright (c) 2011-'26 Jindřich Makovička <makovick@gmail.com>
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

dwmixfa_state_t dwmixfa_state;

static const float cremoveconst = 0.992;
static const float minampl = 0.0001;

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

typedef void(*mixercall)(float *destptr, dwmixfa_channel_t * const c);

void
prepare_mixer (void)
{
	int i;

	dwmixfa_state.fadeleft  = 0.0;
	dwmixfa_state.faderight = 0.0;

	for (i = 0; i < MAXVOICES; i++)
	{
		dwmixfa_state.ch[i].mono_volleft  = 0.0;
		dwmixfa_state.ch[i].mono_volright = 0.0;
		dwmixfa_state.ch[i].stereo_volleft[0]  = 0.0;
		dwmixfa_state.ch[i].stereo_volleft[1]  = 0.0;
		dwmixfa_state.ch[i].stereo_volright[0] = 0.0;
		dwmixfa_state.ch[i].stereo_volright[1] = 0.0;
	}
}

static inline
void clearbufs(float *samples, int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		*samples++ = dwmixfa_state.fadeleft;
		*samples++ = dwmixfa_state.faderight;
		dwmixfa_state.fadeleft *= cremoveconst;
		dwmixfa_state.faderight *= cremoveconst;
	}
}


static void
mix_0(float *destptr,
      dwmixfa_channel_t * const c)
{
	int i;

	for (i = 0; i < dwmixfa_state.nsamples; i++)
	{
		c->smpposf += c->freqf;
		c->smpposw += c->freqw + (c->smpposf >> 16);
		c->smpposf &= 0xffff;
		while (c->smpposw >= c->loopend)
		{
			if (!(c->voiceflags & MIXF_LOOPED))
			{
				c->voiceflags &= ~MIXF_PLAYING;
				goto out;
			}
			assert(c->looplen > 0);
			c->smpposw -= c->looplen;
		}
	}
out:
	;
}

static inline float
filter_none(const float sample, dwmixfa_channel_t * const c)
{
	return sample;
}

static inline float
filter_mixf(const float sample, dwmixfa_channel_t * const c)
{
	c->fb1 = c->fb1 * c->freso + c->ffreq * ( sample - c->fl1 );

	return c->fl1 += c->ffreq * c->fb1;
}

static inline float
interp_none(const float* samples, const uint_fast16_t sample_pos_fract)
{
	return *samples;
}

static inline float
interp_lin(const float* samples, const uint_fast16_t sample_pos_fract)
{
	return samples[0]
	        + (float)sample_pos_fract / 65536.0
	        * (samples[1] - samples[0]);
}

static inline float
interp_cub(const float* samples, const uint_fast16_t sample_pos_fract)
{
	int idx = sample_pos_fract >> 8;
	return samples[0] * dwmixfa_state.ct0[idx]
	        + samples[1] * dwmixfa_state.ct1[idx]
	        + samples[2] * dwmixfa_state.ct2[idx]
	        + samples[3] * dwmixfa_state.ct3[idx];
}

#define MIX_TEMPLATE_M(NAME, INTERP, FILTER, PROTECT)                   \
static void                                                             \
mix##NAME(float *destptr,                                               \
       dwmixfa_channel_t * const c)                                     \
{                                                                       \
    int i = 0;                                                          \
    float sample;                                                       \
    float sbuf[3];                                                      \
    int restore = 0;                                                    \
    assert (PROTECT <= 3);                                              \
                                                                        \
    /* Do we need to add data past loopend, to simplify linear and      \
       cubic interpolation sample read-out? */                          \
    assert(PROTECT <= SAMPEND);                                         \
    if (PROTECT && (c->voiceflags & MIXF_LOOPED))                       \
    {                                                                   \
        restore = 1;                                                    \
        for (i = 0; i < PROTECT; i++)                                   \
        {                                                               \
           sbuf[i] = c->loopend[i];                                     \
           c->loopend[i] = (c->loopend - c->looplen)[i];                \
        }                                                               \
    }                                                                   \
                                                                        \
    for (i = 0; i < dwmixfa_state.nsamples; i++)                        \
      {                                                                 \
        sample = filter_##FILTER(interp_##INTERP(c->smpposw, c->smpposf), c); \
        *destptr++       += c->mono_volleft * sample;                   \
        c->mono_volleft  += c->mono_rampleft;                           \
        *destptr++       += c->mono_volright * sample;                  \
        c->mono_volright += c->mono_rampright;                          \
                                                                        \
        c->smpposf += c->freqf;                                         \
        c->smpposw += c->freqw + (c->smpposf >> 16);                    \
        c->smpposf &= 0xffff;                                           \
                                                                        \
        while (c->smpposw >= c->loopend)                                \
          {                                                             \
            if (!(c->voiceflags & MIXF_LOOPED)) {                       \
                c->voiceflags &= ~MIXF_PLAYING;                         \
                goto fade;                                              \
            }                                                           \
            assert(c->looplen > 0);                                     \
            c->smpposw -= c->looplen;                                   \
          }                                                             \
      }                                                                 \
    goto out;                                                           \
                                                                        \
fade:                                                                   \
                                                                        \
    for (; i < dwmixfa_state.nsamples; i++)                             \
      {                                                                 \
        *destptr++       += c->mono_volleft  * sample;                  \
        c->mono_volleft  += c->mono_rampleft;                           \
        *destptr++       += c->mono_volright * sample;                  \
        c->mono_volright += c->mono_rampright;                          \
    }                                                                   \
                                                                        \
    dwmixfa_state.fadeleft  += c->mono_volleft  * sample;               \
    dwmixfa_state.faderight += c->mono_volright * sample;               \
                                                                        \
out:                                                                    \
    if (PROTECT && restore)                                             \
    {                                                                   \
        for (i = PROTECT - 1; i >= 0; i--)                              \
        {                                                               \
            c->loopend[i] = sbuf[i];                                    \
        }                                                               \
    }                                                                   \
}

static inline void
filter_none_S(const float inL, const float inR, float * const outL, float * const outR, dwmixfa_channel_t * const c)
{
	*outL = inL;
	*outR = inR;
}

static inline void
filter_mixf_S(const float inL, const float inR, float * const outL, float * const outR, dwmixfa_channel_t * const c)
{
	c->fb1 = c->fb1 * c->freso + c->ffreq * ( inL - c->fl1 );
	*outL = c->fl1 += c->ffreq * c->fb1;

	c->fb2 = c->fb2 * c->freso + c->ffreq * ( inR - c->fl2 );
	*outR = c->fl2 += c->ffreq * c->fb2;
}

static inline void
interp_none_S(const float* samples, const uint_fast16_t sample_pos_fract, float * const outL, float * const outR)
{
	*outL = samples[0];
	*outR = samples[1];
}

static inline void
interp_lin_S(const float* samples, const uint_fast16_t sample_pos_fract, float * const outL, float * const outR)
{
	*outL = samples[0]
	        + (float)sample_pos_fract / 65536.0
	        * (samples[2] - samples[0]);
	*outR = samples[1]
	        + (float)sample_pos_fract / 65536.0
	        * (samples[3] - samples[1]);
}

static inline void
interp_cub_S(const float* samples, const uint_fast16_t sample_pos_fract, float * const outL, float * const outR)
{
	int idx = sample_pos_fract >> 8;
	*outL = samples[0] * dwmixfa_state.ct0[idx]
	      + samples[2] * dwmixfa_state.ct1[idx]
	      + samples[4] * dwmixfa_state.ct2[idx]
	      + samples[6] * dwmixfa_state.ct3[idx];
	*outR = samples[1] * dwmixfa_state.ct0[idx]
	      + samples[3] * dwmixfa_state.ct1[idx]
	      + samples[5] * dwmixfa_state.ct2[idx]
	      + samples[7] * dwmixfa_state.ct3[idx];
}

#define MIX_TEMPLATE_S(NAME, INTERP, FILTER, PROTECT)                   \
static void                                                             \
mix##NAME(float *destptr,                                               \
       dwmixfa_channel_t * const c)                                     \
{                                                                       \
    int i = 0;                                                          \
    float sampleL, sampleR;                                             \
    float sbuf[6];                                                      \
    int restore = 0;                                                    \
    assert (PROTECT <= 6);                                              \
                                                                        \
    /* Do we need to add data past loopend, to simplify linear and      \
       cubic interpolation sample read-out? */                          \
    assert(PROTECT <= SAMPEND);                                         \
    if (PROTECT && (c->voiceflags & MIXF_LOOPED))                       \
    {                                                                   \
        restore = 1;                                                    \
        for (i = 0; i < PROTECT; i++)                                   \
        {                                                               \
           sbuf[i] = c->loopend[i];                                     \
           c->loopend[i] = (c->loopend - c->looplen)[i];                \
        }                                                               \
    }                                                                   \
                                                                        \
    for (i = 0; i < dwmixfa_state.nsamples; i++)                        \
      {                                                                 \
        float iL, iR;                                                   \
        interp_##INTERP##_S(c->smpposw, c->smpposf, &iL, &iR);          \
        filter_##FILTER##_S(iL, iR, &sampleL, &sampleR, c);             \
        *destptr++  += c->stereo_volleft[0]  * sampleL + c->stereo_volleft[1]  * sampleR; \
        *destptr++  += c->stereo_volright[0] * sampleL + c->stereo_volright[1] * sampleR; \
        c->stereo_volleft[0]  += c->stereo_rampleft[0];                 \
        c->stereo_volleft[1]  += c->stereo_rampleft[1];                 \
        c->stereo_volright[0] += c->stereo_rampright[0];                \
        c->stereo_volright[1] += c->stereo_rampright[1];                \
        c->smpposf += c->freqf;                                         \
        c->smpposw += (c->freqw + (c->smpposf >> 16))<<1;               \
        c->smpposf &= 0xffff;                                           \
                                                                        \
        while (c->smpposw >= c->loopend)                                \
          {                                                             \
            if (!(c->voiceflags & MIXF_LOOPED)) {                       \
                c->voiceflags &= ~MIXF_PLAYING;                         \
                goto fade;                                              \
            }                                                           \
            assert(c->looplen > 0);                                     \
            c->smpposw -= c->looplen;                                   \
          }                                                             \
      }                                                                 \
    goto out;                                                           \
                                                                        \
fade:                                                                   \
                                                                        \
    for (; i < dwmixfa_state.nsamples; i++)                             \
      {                                                                 \
        *destptr++  += c->stereo_volleft[0]  * sampleL + c->stereo_volleft[1]  * sampleR; \
        *destptr++  += c->stereo_volright[0] * sampleR + c->stereo_volright[1] * sampleR; \
        c->stereo_volleft[0]  += c->stereo_rampleft[0];                 \
        c->stereo_volleft[1]  += c->stereo_rampleft[1];                 \
        c->stereo_volright[0] += c->stereo_rampright[0];                \
        c->stereo_volright[1] += c->stereo_rampright[1];                \
    }                                                                   \
                                                                        \
    dwmixfa_state.fadeleft  += c->stereo_volleft[0]  * sampleL + c->stereo_volleft[1]  * sampleR; \
    dwmixfa_state.faderight += c->stereo_volright[0] * sampleL + c->stereo_volright[1] * sampleR; \
                                                                        \
out:                                                                    \
    if (PROTECT && restore)                                             \
    {                                                                   \
        for (i = PROTECT - 1; i >= 0; i--)                              \
        {                                                               \
            c->loopend[i] = sbuf[i];                                    \
        }                                                               \
    }                                                                   \
}

/* mono source sample, destination always stereo */
MIX_TEMPLATE_M(ms_n,   none, none, 0)
MIX_TEMPLATE_M(ms_i,   lin,  none, 1)
MIX_TEMPLATE_M(ms_i2,  cub,  none, 3)
MIX_TEMPLATE_M(ms_nf,  none, mixf, 0)
MIX_TEMPLATE_M(ms_if,  lin,  mixf, 1)
MIX_TEMPLATE_M(ms_i2f, cub,  mixf, 3)

/* stereo source sample, destination always stereo */
MIX_TEMPLATE_S(ss_n,   none, none, 0)
MIX_TEMPLATE_S(ss_i,   lin,  none, 2)
MIX_TEMPLATE_S(ss_i2,  cub,  none, 6)
MIX_TEMPLATE_S(ss_nf,  none, mixf, 0)
MIX_TEMPLATE_S(ss_if,  lin,  mixf, 2)
MIX_TEMPLATE_S(ss_i2f, cub,  mixf, 6)

static const mixercall mixers[16] = {
	mixms_n,   mixms_i,  mixms_i2,  mix_0,
	mixms_nf,  mixms_if, mixms_i2f, mix_0,

	mixss_n,   mixss_i,  mixss_i2,  mix_0,
	mixss_nf,  mixss_if, mixss_i2f, mix_0
};

void
mixer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i;
	int voice;

	if (fabsf(dwmixfa_state.fadeleft) < minampl)
		dwmixfa_state.fadeleft = 0.0;

	if (fabsf(dwmixfa_state.faderight) < minampl)
		dwmixfa_state.faderight = 0.0;

	if (dwmixfa_state.nsamples == 0)
		return;

	clearbufs(dwmixfa_state.tempbuf, dwmixfa_state.nsamples);

	for (voice = dwmixfa_state.nvoices - 1; voice >= 0; voice--)
	{
		mixercall mixer;

		if (!(dwmixfa_state.ch[voice].voiceflags & MIXF_PLAYING))
			continue;

		mixer = mixers[dwmixfa_state.ch[voice].voiceflags & (MIXF_INTERPOLATE | MIXF_INTERPOLATEQ | MIXF_FILTER | MIXF_PLAYSTEREO)];
		mixer(dwmixfa_state.tempbuf, &dwmixfa_state.ch[voice]);
	}

	for (i=0; i < dwmixfa_state.postprocs; i++)
	{
		dwmixfa_state.postproc[i]->Process(cpifaceSession, dwmixfa_state.tempbuf, dwmixfa_state.nsamples, dwmixfa_state.samprate);
	}

	clippers[0](dwmixfa_state.tempbuf, dwmixfa_state.outbuf, 2 /* stereo */ * dwmixfa_state.nsamples);
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
getchanvol(int n, int len, float * const voll, float * const volr)
{
	float *sample_pos = dwmixfa_state.ch[n].smpposw;
	int sample_pos_fract = dwmixfa_state.ch[n].smpposf;
	int i;

	if (!(dwmixfa_state.ch[n].voiceflags & MIXF_PLAYING))
	{
		*voll = 0;
		*volr = 0;
		return;
	}

	if (dwmixfa_state.ch[n].voiceflags & MIXF_PLAYSTEREO)
	{
		float sumL = 0.0;
		float sumR = 0.0;

		for (i = 0; i < dwmixfa_state.nsamples; i++)
		{
			sumL += fabsf(sample_pos[0]);
			sumR += fabsf(sample_pos[1]);

			sample_pos_fract += dwmixfa_state.ch[n].freqf;
			sample_pos += (dwmixfa_state.ch[n].freqw + (sample_pos_fract >> 16))<<1;
			sample_pos_fract &= 0xffff;
			while (sample_pos >= dwmixfa_state.ch[n].loopend)
			{
				if (!(dwmixfa_state.ch[n].voiceflags & MIXF_LOOPED))
				{
					dwmixfa_state.ch[n].voiceflags &= ~MIXF_PLAYING;
					goto outs;
				}
				assert(dwmixfa_state.ch[n].looplen > 0);
				sample_pos -= dwmixfa_state.ch[n].looplen;
			}
		}
outs:
		sumL /= dwmixfa_state.nsamples;
		sumR /= dwmixfa_state.nsamples;
		*voll = sumL * dwmixfa_state.ch[n].stereo_volleft[0]  + sumR * dwmixfa_state.ch[n].stereo_volleft[1];
		*volr = sumL * dwmixfa_state.ch[n].stereo_volright[0] + sumR * dwmixfa_state.ch[n].stereo_volright[1];
	} else {
		float sum = 0.0;

		for (i = 0; i < dwmixfa_state.nsamples; i++)
		{
			sum += fabsf(*sample_pos);

			sample_pos_fract += dwmixfa_state.ch[n].freqf;
			sample_pos += dwmixfa_state.ch[n].freqw + (sample_pos_fract >> 16);
			sample_pos_fract &= 0xffff;
			while (sample_pos >= dwmixfa_state.ch[n].loopend)
			{
				if (!(dwmixfa_state.ch[n].voiceflags & MIXF_LOOPED))
				{
					dwmixfa_state.ch[n].voiceflags &= ~MIXF_PLAYING;
					goto outm;
				}
				assert(dwmixfa_state.ch[n].looplen > 0);
				sample_pos -= dwmixfa_state.ch[n].looplen;
			}
		}
outm:
		sum /= dwmixfa_state.nsamples;
		*voll = sum * dwmixfa_state.ch[n].mono_volleft;
		*volr = sum * dwmixfa_state.ch[n].mono_volright;
	}
}
