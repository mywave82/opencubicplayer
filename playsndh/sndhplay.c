/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * sndhplay.c - the glue between OpenCubicPlayer and psgplay.
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

#include "config.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"

#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#include "sndhplay.h"
#include "sndhtype.h"

#include "psgplay-git/include/system/unix/file.h"
#include "psgplay-git/include/system/unix/option.h"
#include "psgplay-git/include/psgplay/psgplay.h"
#include "psgplay-git/include/psgplay/sndh.h"
#ifdef SNDH_MUSTPROVIDE_ICE
#include "psgplay-git/include/ice/ice.h"
#endif

#include "psgplay-git/include/internal/psgplay.h"

static struct psgplay *pp;
static struct file f;
static jmp_buf psg_jmp;

static int                  sndh_doloop;
static int                  sndh_looped;
static int                  sndh_donotloop;
static uint32_t             sndh_buflen;
static struct ringbuffer_t *sndh_bufpos;
static uint32_t             sndh_buffpos; /* read fine-pos.. when sndh_bufrate has a fraction */
static uint32_t             sndh_bufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */
static uint16_t            *sndh_buf;
static int                  sndh_subtune;
static int                  sndh_subtune_queue;
static int                  sndh_subtune_failures;
static int                  sndh_subtunes;
static uint32_t             sndh_Rate;
float time_stop; //OPTION_TIME_UNDEFINED, OPTION_STOP_NEVER or use sndh_tag_subtune_time()
struct sndhMeta_t          *sndh_Meta;

#define TIMESLOTS 128
static struct timeslot
{
	int in_sndh_buf; /* snd_buf */
	int in_devp;  /* devp */
	int subtune;
	struct cf2149_regs cf2149;
	uint8_t dma_active_and_repeat_and_mono;
	const struct plrDevAPI_t *plrDevAPI;
} timeslots[TIMESLOTS];

struct AudioPower_t
{
	uint8_t A, B, C, L, R;
};

#define RAWAUDIOSLOTS 128
static struct rawaudio
{
	int in_sndh_buf; /* snd_buf */
	int in_devp;  /* devp */
	int fill;
	int size;
	struct psgplay_digital *data;
	const struct plrDevAPI_t *plrDevAPI;
	struct AudioPower_t AudioPower;
} rawaudioslots[RAWAUDIOSLOTS];

static struct channel_info_t Registers;
static struct rawaudio RawAudio[2]; /* two chunks of 4096 samples covers the need for 'o' normally */

static void Calculate_AudioPower(struct rawaudio *w)
{
	int32_t sumA, sumB, sumC, sumL, sumR;
	int32_t biasA, biasB, biasC, biasL, biasR;
	int i;
	if (w->fill <= 1)
	{
		w->AudioPower.A = 0;
		w->AudioPower.B = 0;
		w->AudioPower.C = 0;
		w->AudioPower.L = 0;
		w->AudioPower.R = 0;
		return;
	}
	sumA = 0;
	sumB = 0;
	sumC = 0;
	sumL = 0;
	sumR = 0;
	for (i = 0; i < w->fill; i++)
	{
		sumA += w->data[i].psg.lva.u5;
		sumB += w->data[i].psg.lvb.u5;
		sumC += w->data[i].psg.lvc.u5;
		sumL += w->data[i].sound.left;
		sumR += w->data[i].sound.right;
	}
	biasA = sumA / w->fill;
	biasB = sumB / w->fill;
	biasC = sumC / w->fill;
	biasL = sumL / w->fill;
	biasR = sumR / w->fill;

	sumA = 0;
	sumB = 0;
	sumC = 0;
	sumL = 0;
	sumR = 0;

	for (i = 0; i < w->fill; i++)
	{
		int32_t diff;
		diff = (int32_t)w->data[i].psg.lva.u5 - biasA;
		sumA += (diff > 0) ? diff : -diff;

		diff = (int32_t)w->data[i].psg.lvb.u5 - biasB;
		sumB += (diff > 0) ? diff : -diff;

		diff = (int32_t)w->data[i].psg.lvc.u5 - biasC;
		sumC += (diff > 0) ? diff : -diff;

		diff = (int32_t)w->data[i].sound.left - biasL;
		sumL += (diff > 0) ? diff : -diff;

		diff = (int32_t)w->data[i].sound.right - biasR;
		sumR += (diff > 0) ? diff : -diff;
	}

	sumA *= 4; sumA /= w->fill;
	sumB *= 4; sumB /= w->fill;
	sumC *= 4; sumC /= w->fill;
	           sumL /= w->fill * 256;
	           sumR /= w->fill * 256;

	w->AudioPower.A = (sumA > 64) ? 64 : sumA;
	w->AudioPower.B = (sumB > 64) ? 64 : sumB;
	w->AudioPower.C = (sumC > 64) ? 64 : sumC;
	w->AudioPower.L = (sumL > 64) ? 64 : sumL;
	w->AudioPower.R = (sumR > 64) ? 64 : sumR;
}

static struct timeslot *register_slot_get (void)
{
	int i;
	for (i=0; i < TIMESLOTS; i++)
	{
		if (timeslots[i].in_sndh_buf) continue;
		if (timeslots[i].in_devp) continue;
		return timeslots + i;
	}
	return 0;
}

static struct rawaudio *rawaudio_slot_get (void)
{
	int i;
	for (i=0; i < RAWAUDIOSLOTS; i++)
	{
		if (rawaudioslots[i].in_sndh_buf) continue;
		if (rawaudioslots[i].in_devp) continue;
		return rawaudioslots + i;
	}
	return 0;
}

static unsigned long voll,volr;
static int bal;
static int vol;
static int pan;
static int srnd;

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

OCP_INTERNAL int sndhIsLooped (void)
{
#ifdef PLAYSNDH_DEBUG
	if (sndh_looped) fprintf(stderr, "sndhIsLooped(): sndh_looped = %d\n", sndh_looped);
#endif
	return sndh_looped == 3;
}

OCP_INTERNAL void sndhSetLoop (unsigned char s)
{
	if (sndh_donotloop != !s)
	{
		sndh_donotloop=!s;
		if (setjmp (psg_jmp))
		{
			fprintf (stderr, "[SNDH] sndhSetLoop(), psgplay_stop_at_time() failed\n");
			return;
		}
		if ((time_stop >= 0) && (sndh_donotloop))
		{
			psgplay_stop_at_time (pp, time_stop);
		} else {
			psgplay_unstop (pp);
		}
	}
}

static void sndhSetSpeed(uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	sndh_bufrate=256*sp;
}

static void sndhSetVolume(void)
{
	volr = voll = vol * 4;
	if (bal < 0)
		voll = (voll * (64 + bal)) >> 6;
	else
		volr = (volr * (64 - bal)) >> 6;
}

static void sndhSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			sndhSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			sndhSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			sndhSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			sndhSetVolume();
			break;
	}
}

static int sndhGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

OCP_INTERNAL void sndhStat(struct sndhStat_t *stat)
{
	stat->SubTune_active = sndh_subtune;
	stat->SubTune_onqueue = sndh_subtune_queue;
	stat->SubTunes = sndh_subtunes;
}


OCP_INTERNAL void sndhClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i;

	if (cpifaceSession->plrDevAPI)
	{
		cpifaceSession->plrDevAPI->Stop(cpifaceSession);
	}

	if (pp)
	{
		psgplay_free(pp);
		pp = 0;
	}

	cpifaceSession->ringbufferAPI->free (sndh_bufpos);
	sndh_bufpos = 0;

	free (sndh_buf);
	sndh_buf = 0;

	free (f.data);
	f.data = 0;
	f.path = 0;

	for (i=0; i < RAWAUDIOSLOTS; i++)
	{
		free (rawaudioslots[i].data);
	}
	free (RawAudio[0].data);
	free (RawAudio[1].data);

	if (sndh_Meta)
	{
		sndhMetaFree (sndh_Meta);
		sndh_Meta = 0;
	}
}

static psgplay_digital_to_stereo_cb ocp_psg_mix_option (void)
{
#warning Stereo model
	return psgplay_digital_to_stereo_empiric;
	//return psgplay_digital_to_stereo_linear;
	//return psgplay_digital_to_stereo_balance;
	//return psgplay_digital_to_stereo_volume;
}

static void *ocp_psg_mix_arg(void)
{
        return NULL;
	/* return &option.psg_balance; */
	/* return &option.psg_volume; */
}

void pr_bug(const char *file, int line,
        const char *func, const char *expr)
{
        fprintf(stderr, "BUG: %s:%d: %s: %s\n", file, line, func, expr);

        longjmp (psg_jmp, 2);
}

void pr_fatal_error(const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
	vfprintf (stderr, fmt, ap);
        va_end(ap);

        longjmp (psg_jmp, 1);
}

OCP_INTERNAL struct channel_info_t *sndhRegisters(void)
{
	return &Registers;
}

static void register_delay_callback_from_devp (void *arg, int samples_ago)
{
#ifndef ATARI_STE_EXT_OSC
# define ATARI_STE_EXT_OSC          8010613 /* OSC U303 */
#endif
#define PSG_CLOCK (ATARI_STE_EXT_OSC / 4)
	struct timeslot *state = (struct timeslot *)arg;
	state->in_devp = 0;

	uint32_t period;

	period = state->cf2149.lo_a.period | ((uint16_t)(state->cf2149.hi_a.period) << 8);
	if (period == 0)
	{
		period = 1;
	}
	Registers.frequency_a = PSG_CLOCK / (period * 16);

	period = state->cf2149.lo_b.period | ((uint16_t)(state->cf2149.hi_b.period) << 8);
	if (period == 0)
	{
		period = 1;
	}
	Registers.frequency_b = PSG_CLOCK / (period * 16);

	period = state->cf2149.lo_c.period | ((uint16_t)(state->cf2149.hi_c.period) << 8);
	if (period == 0)
	{
		period = 1;
	}
	Registers.frequency_c = PSG_CLOCK / (period * 16);

	period = state->cf2149.noise.period;
	if (period == 0)
	{
		period = 1;
	}
	Registers.frequency_noise = PSG_CLOCK / (period * 16 * 2);

	Registers.mixer_control = state->cf2149.u8[CF2149_REG_IOMIX];
	Registers.level_a = state->cf2149.u8[CF2149_REG_LEVEL_A];
	Registers.level_b = state->cf2149.u8[CF2149_REG_LEVEL_B];
	Registers.level_c = state->cf2149.u8[CF2149_REG_LEVEL_C];

	period = state->cf2149.envelope_lo.period | ((uint16_t)(state->cf2149.envelope_hi.period) << 8);
	if (period == 0)
	{
		period = 1;
	}
	Registers.frequency_envelope = PSG_CLOCK / (period * 256);

	Registers.envelope_shape = state->cf2149.envelope_shape.ctrl;
	Registers.dma_active_and_repeat_and_mono = state->dma_active_and_repeat_and_mono;
	if (Registers.dma_active_and_repeat_and_mono & 1)
	{
		switch (state->dma_active_and_repeat_and_mono >> 3 & 0x03)
		{
			default:
			case 0: Registers.dma_samplerate = 6258; break;
			case 1: Registers.dma_samplerate = 12517; break;
			case 2: Registers.dma_samplerate = 25033; break;
			case 3: Registers.dma_samplerate = 50066; break;
		}
	} else {
		Registers.dma_samplerate = 0;
	}
	sndh_subtune = state->subtune;
}

static void register_delay_callback_from_sndh_buf (void *arg, int samples_ago)
{
	struct timeslot *state = (struct timeslot *)arg;

	int samples_until = samples_ago * sndh_bufrate / 65536;

	state->in_sndh_buf = 0;
	state->in_devp = 1;
	state->plrDevAPI->OnBufferCallback (samples_until, register_delay_callback_from_devp, state);
}

static void rawaudio_delay_callback_from_devp (void *arg, int samples_ago)
{
	struct rawaudio *state = (struct rawaudio *)arg;
	int size;
	struct psgplay_digital *data;

	/* swap with RawAudio */
	size = RawAudio[1].size;
	data = RawAudio[1].data;

	RawAudio[1] = RawAudio[0];
	RawAudio[0] = *state;

	state->size = size;
	state->data = data;

	Registers.power_a = state->AudioPower.A;
	Registers.power_b = state->AudioPower.B;
	Registers.power_c = state->AudioPower.C;
	Registers.power_l = state->AudioPower.L;
	Registers.power_r = state->AudioPower.R;

	state->in_devp = 0;
}

static void rawaudio_delay_callback_from_sndh_buf (void *arg, int samples_ago)
{
	struct rawaudio *state = (struct rawaudio *)arg;

	int samples_until = samples_ago * sndh_bufrate / 65536;

	state->in_sndh_buf = 0;
	state->in_devp = 1;
	state->plrDevAPI->OnBufferCallback (samples_until, rawaudio_delay_callback_from_devp, state);
}

static void sndhIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int pos1, pos2;
	int length1, length2;

	assert (sizeof (struct psgplay_stereo) == 4);

	if (sndh_donotloop && (sndh_looped&1))
	{
		return;
	}

	cpifaceSession->ringbufferAPI->get_head_samples (sndh_bufpos, &pos1, &length1, &pos2, &length2);

	if (setjmp (psg_jmp))
	{
		fprintf (stderr, "[SNDH] psgplay_read_stereo() failed\n");
		sndh_looped |= 3;
		return;
	}

	while (length1)
	{
		struct timeslot *slot;

		if ((sndh_looped & 1) && sndh_donotloop)
			break;

		if ((unsigned int)length1>(sndh_Rate/50))
			length1=sndh_Rate/50;

		ssize_t n = 0;
		ssize_t r = psgplay_read_stereo (pp, (struct psgplay_stereo *)sndh_buf + pos1, length1, &n);
#ifdef PLAYSNDH_DEBUG
		fprintf(stderr, "sndhIdler 1: psgplay_read_stereo %d => %d (%d)\n", length1, (int)r, (int)n);
#endif
		if (!r)
		{
			if (sndh_subtune_queue < sndh_subtunes)
			{
				sndhStartTune (cpifaceSession, sndh_subtune_queue + 1);
			} else {
				if (!sndh_donotloop)
				{
					sndhStartTune (cpifaceSession, 1);
				}
			}
			r = psgplay_read_stereo (pp, (struct psgplay_stereo *)sndh_buf + pos1, length1, &n);
#ifdef PLAYSNDH_DEBUG
		fprintf(stderr, "sndhIdler 2: psgplay_read_stereo %d => %d (%d)\n", length1, (int)r, (int)n);
#endif
		}
		if (!r)
		{
			sndh_subtune_failures++;
			if (sndh_subtune_failures >= sndh_subtunes)
			{
				sndh_looped |= 1;
				break;
			}
		} else {
			sndh_subtune_failures = 0;
			sndh_looped &= ~1;
		}

		slot = register_slot_get ();
		if (slot)
		{
			slot->cf2149 = pp->machine.psg.cf2149.state.regs;
			slot->dma_active_and_repeat_and_mono =
				(pp->machine.sound.cf300588.state.regs.mode.rate << 3) | /* two bits */
				(pp->machine.sound.cf300588.state.regs.mode.mono << 2) | /* one bit */
				(pp->machine.sound.cf300588.state.regs.ctrl.play_repeat << 1) | /* one bit */
				 pp->machine.sound.cf300588.state.regs.ctrl.play; /* one bit */
			slot->in_sndh_buf = 1;
			slot->subtune = sndh_subtune_queue;
			slot->plrDevAPI = cpifaceSession->plrDevAPI;
			cpifaceSession->ringbufferAPI->add_tail_callback_samples (sndh_bufpos, 0, register_delay_callback_from_sndh_buf, slot);
		}

		if (n)
		{
			struct rawaudio *r = rawaudio_slot_get();
			if (r)
			{
				if (r->size <= n)
				{
					free (r->data);
					r->size = n;
					r->data = malloc (n * sizeof (r->data[0]));
					if (!r->data)
					{
						r->size = 0;
						goto skip;
					}
				}
				r->fill = n;
				memcpy (r->data, pp->d, n * sizeof (pp->d[0]));
				Calculate_AudioPower (r);
				r->in_sndh_buf = 1;
				r->plrDevAPI = cpifaceSession->plrDevAPI;
				cpifaceSession->ringbufferAPI->add_tail_callback_samples (sndh_bufpos, 0, rawaudio_delay_callback_from_sndh_buf, r);
			}
		}

skip:
		cpifaceSession->ringbufferAPI->head_add_samples (sndh_bufpos, r);
		cpifaceSession->ringbufferAPI->get_head_samples (sndh_bufpos, &pos1, &length1, &pos2, &length2);
	}
}

OCP_INTERNAL void sndhIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->InPause || (sndh_looped == 3))
	{
		cpifaceSession->plrDevAPI->Pause (1);
	} else {
		void *targetbuf;
		unsigned int targetlength; /* in samples */

		cpifaceSession->plrDevAPI->Pause (0);

		cpifaceSession->plrDevAPI->GetBuffer (&targetbuf, &targetlength);

		if (targetlength)
		{
			int16_t *t = targetbuf;
			unsigned int accumulated_target = 0;
			unsigned int accumulated_source = 0;
			int pos1, length1, pos2, length2;

			sndhIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (sndh_bufpos, &pos1, &length1, &pos2, &length2);

			if (sndh_bufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2); // limiting targetlength here, saves us from doing this per sample later
					sndh_looped |= 2;
				} else {
					sndh_looped &= ~2;
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

						rs = sndh_buf[(pos1<<1) + 0];
						ls = sndh_buf[(pos1<<1) + 1];

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
				sndh_looped &= ~2;

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
							sndh_looped |= 2;
							break;
						}
						/* will we overflow the wavebuf if we advance? */
						if ((length1+length2) < ((sndh_bufrate+sndh_buffpos)>>16))
						{
							sndh_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)sndh_buf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)sndh_buf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)sndh_buf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)sndh_buf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)sndh_buf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)sndh_buf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)sndh_buf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)sndh_buf[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,sndh_buffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,sndh_buffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,sndh_buffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,sndh_buffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,sndh_buffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,sndh_buffpos);
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

						sndh_buffpos+=sndh_bufrate;
						progress = sndh_buffpos>>16;
						sndh_buffpos &= 0xffff;
						accumulated_source+=progress;
						pos1+=progress;
						length1-=progress;
						targetlength--;

						if (length1 < 0)
						{
							length2 += length1;
							length1 = 0;
						}

						accumulated_target++;
					} /* while (targetlength && length1) */
					length1 = length2;
					length2 = 0;
					pos1 = pos2;
					pos2 = 0;
				} /* while (targetlength && length1) */
			} /* if (sndh_bufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (sndh_bufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();
}

OCP_INTERNAL int sndhOpenPlayer(struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	cpifaceSession->dirdb->GetName_internalstr(file->dirdb_ref, (const char **)&f.path);
	f.size = file->filesize(file);
	if (f.size > 1024*1024)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SNDH] File too big\n");
		return errFormStruc;
	}
	f.data = malloc (f.size);
	if (!f.data)
	{
		return errAllocMem;
	}
	if (f.size < 64)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SNDH] File too small\n");
		return errFormStruc;
	}

	file->seek_set (file, 0);
	if (file->read (file, f.data, f.size) != f.size)
	{
		free (f.data);
		f.data = 0;
		return errFileRead;
	}

#ifdef SNDH_MUSTPROVIDE_ICE
again:
#endif
	if( memcmp(f.data + 12,"SNDH", 4 ) != 0)
	{
#ifdef SNDH_MUSTPROVIDE_ICE
		if (memcmp (f.data, "ICE!", 4) == 0)
		{
			size_t uncompressedsize;
			ssize_t realuncompressedsize;
			char *uncompresseddata = 0;
			int retval = errFormStruc;
			if (f.size < 2*1024*1024)
			{
				uncompressedsize = ice_decrunched_size (f.data, f.size);
				if ((uncompressedsize < 20) || (uncompressedsize > 2 * 1024 * 1024))
				{
					free (f.data);
					f.data = 0;
					goto iceout;
				}
				uncompresseddata = malloc (uncompressedsize);
				if (!uncompresseddata)
				{
					goto iceout;
				}
				realuncompressedsize = ice_decrunch (uncompresseddata, f.data, f.size);
				if ((realuncompressedsize < 20) || (realuncompressedsize > 2 * 1024 * 1024))
				{
					goto iceout;
				}
				free (f.data);
				f.data = uncompresseddata;
				f.size = realuncompressedsize;
				goto again;
iceout:
				free (f.data);
				f.data = 0;
				free (uncompresseddata);
				return retval;
			}
		}
#endif
		cpifaceSession->cpiDebug (cpifaceSession, "[SNDH] File signature is not SNDH\n");
		free(f.data);
		f.data = 0;
		return errFormSig;
	}

	sndh_Rate = 0;
	format = PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&sndh_Rate, &format, file, cpifaceSession))
	{
		free(f.data);
		f.data = 0;
		return errPlay;
	}

	memset (timeslots, 0, sizeof (timeslots));
	memset (rawaudioslots, 0, sizeof (rawaudioslots));
	memset (&RawAudio, 0, sizeof (RawAudio));
	memset (&Registers, 0, sizeof (Registers));

	sndh_buflen  = sndh_Rate / 4;
	sndh_doloop  = 0;
	sndh_looped  = 0;
	sndh_bufrate = 0x10000; /* 1.0 */
	sndh_bufpos  = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, sndh_buflen);
	sndh_buffpos = 0;
	if (!sndh_bufpos)
	{
		sndhClosePlayer (cpifaceSession);
		return errAllocMem;
	}
	sndh_buf = malloc (sndh_buflen * 2 * 2);
	if (!sndh_buf)
	{
		sndhClosePlayer (cpifaceSession);
		return errAllocMem;
	}

	if (!sndh_tag_subtune_count(&sndh_subtunes, f.data, f.size))
	{
		sndh_subtunes = 1;
	}
	if (!sndh_tag_default_subtune(&sndh_subtune, f.data, f.size))
	{
		sndh_subtune = 1;
	}
	if ((sndh_subtune < 0) || (sndh_subtune > sndh_subtunes))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SNDH] Default sub-song %d out of range %d\n", sndh_subtune, sndh_subtunes);
		sndh_subtune = 1;
	}
	sndh_subtune_queue = sndh_subtune;
	sndh_subtune_failures = 0;

	time_stop = OPTION_TIME_UNDEFINED; //OPTION_TIME_UNDEFINED, OPTION_STOP_NEVER or use sndh_tag_subtune_time()
	if (!sndh_tag_subtune_time(&time_stop, sndh_subtune, f.data, f.size))
	{
		time_stop = OPTION_STOP_NEVER;
	}
	//const ssize_t sample_stop   = time_stop >= 0 ? time_stop * sndh_Rate + 0.5 : -1;

	sndh_tag_subtune_name (cpifaceSession->mdbdata.title, sizeof (cpifaceSession->mdbdata.title), sndh_subtune_queue, f.data, f.size);

	if (setjmp (psg_jmp))
	{
		fprintf (stderr, "[SNDH] psgplay_init() failed\n");
		sndhClosePlayer (cpifaceSession);
		return errGen;
	}
	pp = psgplay_init (f.data, f.size, sndh_subtune, sndh_Rate);
	if (!pp)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SNDH] failed to initialize PSG play\n");
		sndhClosePlayer (cpifaceSession);
		return errFormSig;
	}

	if (setjmp (psg_jmp))
	{
		fprintf (stderr, "[SNDH] psgplay_digital_to_stereo_callback() failed\n");
		sndhClosePlayer (cpifaceSession);
		return errGen;
	}
	psgplay_digital_to_stereo_callback (pp, ocp_psg_mix_option(), ocp_psg_mix_arg());

#ifdef PLAYSNDH_DEBUG
	fprintf (stderr, "OPTION_TIME_UNDEFINED=%d\n", OPTION_TIME_UNDEFINED);
	fprintf (stderr, "OPTION_STOP_NEVER=%d\n", OPTION_STOP_NEVER);
	fprintf (stderr, "time_stop=%f\n", time_stop);
#endif
	if ((time_stop >= 0) && (sndh_donotloop))
	{
		if (setjmp (psg_jmp))
		{
			fprintf (stderr, "[SNDH] psgplay_stop_at_time() failed\n");
			sndhClosePlayer (cpifaceSession);
			return errGen;
		}
		psgplay_stop_at_time (pp, time_stop);
	}

	sndh_Meta = sndhReadInfos (f.data, f.size);

	cpifaceSession->mcpSet = sndhSet;
	cpifaceSession->mcpGet = sndhGet;
	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	return errOk;
}

OCP_INTERNAL void sndhStartTune (struct cpifaceSessionAPI_t *cpifaceSession, int tune)
{
	struct psgplay *pp_new = 0;

	time_stop = OPTION_TIME_UNDEFINED; //OPTION_TIME_UNDEFINED, OPTION_STOP_NEVER or use sndh_tag_subtune_time()
	if (!sndh_tag_subtune_time(&time_stop, sndh_subtune, f.data, f.size))
	{
		time_stop = OPTION_STOP_NEVER;
	}
	//const ssize_t sample_stop   = time_stop >= 0 ? time_stop * sndh_Rate + 0.5 : -1;

	if ((tune < 1) ||
	    (tune > sndh_subtunes))
	{
		return;
	}

	sndh_tag_subtune_name (cpifaceSession->mdbdata.title, sizeof (cpifaceSession->mdbdata.title), tune, f.data, f.size);

	if (setjmp (psg_jmp))
	{
		fprintf (stderr, "[SNDH] sndhStartTune(), psgplay_init() failed\n");
		return;
	}
	pp_new = psgplay_init (f.data, f.size, tune, sndh_Rate);
	if (!pp_new)
	{
		fprintf (stderr, "[SNDH] sndhStartTune(), failed to initialize PSG play\n");
		return;
	}

	psgplay_digital_to_stereo_callback (pp_new, ocp_psg_mix_option(), ocp_psg_mix_arg());
#ifdef PLAYSNDH_DEBUG
	fprintf (stderr, "time_stop=%f\n", time_stop);
#endif
	if ((time_stop >= 0) && (sndh_donotloop))
	{
		if (setjmp (psg_jmp))
		{
			fprintf (stderr, "[SNDH] sndhStartTune(), psgplay_stop_at_time() failed\n");
			psgplay_free (pp_new);
			return;
		}
		psgplay_stop_at_time (pp_new, time_stop);
	}
	psgplay_free (pp);
	pp = pp_new;
	sndh_subtune_queue = tune;
}

OCP_INTERNAL int sndhGetPChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt)
{
#define AUDIO_CLOCK (ATARI_STE_EXT_OSC / 16)
	int stereo = (opt&mcpGetSampleStereo)?1:0;
	uint32_t step = imuldiv(0x00010000, AUDIO_CLOCK, (signed)rate);
	struct psgplay_digital *src = RawAudio[0].data;
	int length1 = RawAudio[0].fill;
	int length2 = RawAudio[1].fill;
	uint32_t posf = 0;

	while (len)
	{
		if (stereo)
		{
			switch (ch)
			{
				case 0: s[0] = s[1] = (((uint16_t)(src->psg.lva.u8) << 8) | src->psg.lva.u8) ^ 0x8000; s+= 2; break;
				case 1: s[0] = s[1] = (((uint16_t)(src->psg.lvb.u8) << 8) | src->psg.lvb.u8) ^ 0x8000; s += 2; break;
				case 2: s[0] = s[1] = (((uint16_t)(src->psg.lvc.u8) << 8) | src->psg.lvc.u8) ^ 0x8000; s += 2; break;
				case 3: s[0] = s[1] = src->sound.left; s += 2; break;
				case 4: s[0] = s[1] = src->sound.right; s += 2; break;
				default:
					*(s++) = 0;
					*(s++) = 0;
					break;
			}
		} else {
			switch (ch)
			{
				case 0: *(s++) = (((uint16_t)(src->psg.lva.u8) << 8) | src->psg.lva.u8) ^ 0x8000; break;
				case 1: *(s++) = (((uint16_t)(src->psg.lvb.u8) << 8) | src->psg.lvb.u8) ^ 0x8000; break;
				case 2: *(s++) = (((uint16_t)(src->psg.lvc.u8) << 8) | src->psg.lvc.u8) ^ 0x8000; break;
				case 3: *(s++) = src->sound.left; break;
				case 4: *(s++) = src->sound.right; break;
				default:
					*(s++) = 0;
					break;
			}
		}
		len--;

		posf += step;

		while (posf >= 0x00010000)
		{
			posf -= 0x00010000;

			src ++;
			length1--;

			if (!length1)
			{
				if (length2)
				{
					length1 = length2;
					length2 = 0;
					src = RawAudio[1].data;
				} else {
					memset (s, 0, (len<<stereo)<<2);
					return 0;//!!sndh_muted[ch];
				}
			}
		}
	}
	return 0;//!!sndh_muted[ch];
}

OCP_INTERNAL struct sndhMeta_t *sndhGetMeta (void)
{
	return sndh_Meta;
}

