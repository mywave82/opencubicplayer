/* OpenCP Module Player
 * copyright (c) 2010-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ASM emulated routines for FPU mixer
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

#include "asm_emu/x86.h"
dwmixfa_state_t dwmixfa_state;

/*#define ASM_DEBUG 1*/
#ifdef ASM_DEBUG
#include <stdarg.h>
#include <stdio.h>
static void debug_printf(const char* format, ...)
{
        va_list args;

	fprintf(stderr, "[dwmixfa.c]: ");
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

}
#else
#define debug_printf(format, args...) ((void)0)
#endif


#define MAXVOICES MIXF_MAXCHAN
#define FLAG_DISABLED (~MIXF_PLAYING)
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
int      isstereo;              /* flag for stereo output */
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
#if 0
static float eins=1.0;
#endif
#endif
static const float minuseins=-1.0;
static const float clampmax=32767.0;
static const float clampmin=-32767.0;
static const float cremoveconst=0.992;
static const float minampl=0.0001; /* what the fuck? why is this a float? - stian */
#if 0
static uint32_t magic1;  /* 32bit in assembler used */
static uint16_t clipval; /* 16bit in assembler used */
static uint32_t mixlooplen; /* 32bit in assembler used, decimal. lenght of loop in samples*/
static uint32_t __attribute__ ((used)) looptype; /* 32bit in assembler used, local version of voiceflags[N] */
static float __attribute__ ((used)) ffrq;
static float __attribute__ ((used)) frez;
static float __attribute__ ((used)) __fl1;
static float __attribute__ ((used)) __fb1;

#endif

typedef void(*clippercall)(float *input, void *output, uint_fast32_t count);

static void clip_16s(float *input, void *output, uint_fast32_t count);
static void clip_16u(float *input, void *output, uint_fast32_t count);
static void clip_8s(float *input, void *output, uint_fast32_t count);
static void clip_8u(float *input, void *output, uint_fast32_t count);

static const clippercall clippers[4] = {clip_8s, clip_8u, clip_16s, clip_16u};

/* additional data come from globals:
	mixlooplen = length of sample loop  R
	volr                                R
	voll                                R
	fadeleft                            R
	faderight                           R
	looptype = sample flags             RW
*/
typedef void(*mixercall)(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mix_0   (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_n  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_n  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_i  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_i  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_i2 (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_i2 (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_nf (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_nf (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_if (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_if (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_i2f(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_i2f(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);

static const mixercall mixers[16] = {
	mixm_n,   mixs_n,   mixm_i,  mixs_i,
	mixm_i2,  mixs_i2,  mix_0,   mix_0,
	mixm_nf,  mixs_nf,  mixm_if, mixs_if,
	mixm_i2f, mixs_i2f, mix_0,   mix_0
};

static void writecallback(uint_fast16_t selector, uint_fast32_t addr, int size, uint_fast32_t data)
{
}

static uint_fast32_t readcallback(uint_fast16_t selector, uint_fast32_t addr, int size)
{
	return 0;
}

void prepare_mixer (void)
{
	struct assembler_state_t state;

	init_assembler_state(&state, writecallback, readcallback);
	asm_xorl(&state, state.eax, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.fadeleft);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.faderight);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.volrl);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.volrr);
	asm_xorl(&state, state.ecx, &state.ecx);
prepare_mixer_fillloop:
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.volleft[state.ecx]);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.volright[state.ecx]);
	asm_incl(&state, &state.ecx);
	asm_cmpl(&state, MAXVOICES, state.ecx);
	asm_jne(&state, prepare_mixer_fillloop);
}

static inline void clearbufm(float **edi_buffer, uint32_t *count)
{
	struct assembler_state_t state;

	debug_printf("clearbufm {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, 0x12345678/**edi_buffer*/, &state.edi);
	asm_movl(&state, *count, &state.ecx);

	asm_flds(&state, cremoveconst);
	asm_flds(&state, dwmixfa_state.fadeleft);
clearbufm_clloop:
		asm_fsts(&state, *edi_buffer+0);
		asm_fmul(&state, 1, 0);
		asm_leal(&state, state.edi+4, &state.edi); *edi_buffer+=1;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, clearbufm_clloop);

	asm_fstps(&state, &dwmixfa_state.fadeleft);
	asm_fstp_st(&state, 0);

	asm_movl(&state, state.ecx, count);
	debug_printf("}\n");
}

static inline void clearbufs(float **edi_buffer, uint32_t *count)
{
	struct assembler_state_t state;

	debug_printf("clearbufs {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, 0x12345678/**edi_buffer*/, &state.edi);
	asm_movl(&state, *count, &state.ecx);

	asm_flds(&state, cremoveconst);
	asm_flds(&state, dwmixfa_state.faderight);
	asm_flds(&state, dwmixfa_state.fadeleft);
clearbufs_clloop:
		asm_fsts(&state, *edi_buffer+0);
		asm_fmul(&state, 2, 0);
		asm_fxch_st(&state, 1);
		asm_fsts(&state, *edi_buffer+1);
		asm_fmul(&state, 2, 0);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); *edi_buffer+=2;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, clearbufs_clloop);
	asm_fstps(&state, &dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.faderight);
	asm_fstp_st(&state, 0);

	asm_movl(&state, state.ecx, count);

	debug_printf("}\n");
}


void mixer (void)
{
	void *fadeleft_ptr = &dwmixfa_state.fadeleft;
	void *faderight_ptr = &dwmixfa_state.faderight;
	void *volr_ptr = &dwmixfa_state.volr;
	void *voll_ptr = &dwmixfa_state.voll;
	void *__fl1_ptr = &dwmixfa_state.__fl1;
	void *__fb1_ptr = &dwmixfa_state.__fb1;

	struct assembler_state_t state;
	float *edi_mirror;
	void *edi_mirror2;
	float *esi_mirror2;
	float *eax_mirror;
	float *ebp_mirror;
	mixercall ecx_mirror;
	clippercall eax_mirror2;
	struct mixfpostprocregstruct *esi_mirror;

	init_assembler_state(&state, writecallback, readcallback);

	debug_printf("mixer {\n");

	asm_pushl(&state, state.ebp);
	asm_finit(&state);
	asm_xorl(&state, state.ebx, &state.ebx);
	asm_movl(&state, *(uint32_t *)fadeleft_ptr, &state.eax);
	asm_andl(&state, 0x7fffffff, &state.eax);
	asm_cmpl(&state, state.eax, minampl); /* TODO, comparing of floats, typecasted to uint32_t */
	asm_ja(&state, mixer_nocutfl);
	asm_movl(&state, state.ebx, (uint32_t *)fadeleft_ptr); /* mixing of float and integer numbers.... "great" */
mixer_nocutfl:
	asm_movl(&state, *(uint32_t *)faderight_ptr, &state.eax);
	asm_andl(&state, 0x7fffffff, &state.eax);
	asm_cmpl(&state, state.eax, minampl); /* TODO, comparing of floats, typecasted to uint32_t */
	asm_ja(&state, mixer_nocutfr);
	asm_movl(&state, state.ebx, (uint32_t *)faderight_ptr); /* mixing of float and integer numbers.... "great" */
mixer_nocutfr:
	asm_movl(&state, 0x12345678/*tempbuf*/, &state.edi); edi_mirror = dwmixfa_state.tempbuf;
	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_orl(&state, state.ecx, &state.ecx);
	asm_jz(&state, mixer_endall);
	asm_movl(&state, dwmixfa_state.isstereo, &state.eax);
	asm_orl(&state, state.eax, &state.eax);
	asm_jnz(&state, mixer_clearst);
		clearbufm(&edi_mirror, &state.ecx);
	asm_jmp(&state, mixer_clearend);
mixer_clearst:
		clearbufs(&edi_mirror, &state.ecx);
mixer_clearend:
	asm_movl(&state, dwmixfa_state.nvoices, &state.ecx);
	asm_decl(&state, &state.ecx);

mixer_MixNext:
	debug_printf("Doing channel: %d\n", state.ecx);
	asm_movl(&state, dwmixfa_state.voiceflags[state.ecx], &state.eax);
	asm_testl(&state, MIXF_PLAYING, state.eax);
	asm_jz(&state, mixer_SkipVoice);

	asm_movl(&state, state.eax, &dwmixfa_state.looptype);

	asm_movl(&state, *(uint32_t *)&dwmixfa_state.volleft[state.ecx], &state.eax);
	asm_movl(&state, *(uint32_t *)&dwmixfa_state.volright[state.ecx], &state.ebx);
	asm_movl(&state, state.eax, (uint32_t *)voll_ptr);
	asm_movl(&state, state.ebx, (uint32_t *)volr_ptr);

	asm_movl(&state, *(uint32_t *)&dwmixfa_state.rampleft[state.ecx], &state.eax);
	asm_movl(&state, *(uint32_t *)&dwmixfa_state.rampright[state.ecx], &state.ebx);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.volrl);
	asm_movl(&state, state.ebx, (uint32_t *)&dwmixfa_state.volrr);

	asm_movl(&state, *(uint32_t *)&dwmixfa_state.ffreq[state.ecx], &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.ffrq);
	asm_movl(&state, *(uint32_t *)&dwmixfa_state.freso[state.ecx], &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.frez);
	asm_movl(&state, *(uint32_t *)&dwmixfa_state.fl1[state.ecx], &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.__fl1);
	asm_movl(&state, *(uint32_t *)&dwmixfa_state.fb1[state.ecx], &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.__fb1);

	asm_movl(&state, dwmixfa_state.looplen[state.ecx], &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.mixlooplen);

	asm_movl(&state, dwmixfa_state.freqw[state.ecx], &state.ebx);
	asm_movl(&state, dwmixfa_state.freqf[state.ecx], &state.esi);

	asm_movl(&state, 0x12345678, &state.eax); eax_mirror = dwmixfa_state.smpposw[state.ecx];

	asm_movl(&state, dwmixfa_state.smpposf[state.ecx], &state.edx);

	asm_movl(&state, 0x12345678, &state.ebp); ebp_mirror = dwmixfa_state.loopend[state.ecx];

	asm_pushl(&state, state.ecx);
	asm_movl(&state, 0x12345678, &state.edi); edi_mirror = dwmixfa_state.tempbuf;
	asm_movl(&state, dwmixfa_state.isstereo, &state.ecx);
	asm_orl(&state, dwmixfa_state.voiceflags[state.ecx], &state.ecx);
	asm_andl(&state, 15, &state.ecx);
	/*asm_movl(&state, 0x12345678, &state.ecx);*/ ecx_mirror = mixers[state.ecx];
		ecx_mirror(edi_mirror, &eax_mirror, &state.edx, state.ebx, state.esi, ebp_mirror);
	asm_popl(&state, &state.ecx);
/*
	asm_movl(&state, eax, smposw[state.ecx]);*/dwmixfa_state.smpposw[state.ecx] = eax_mirror;
	asm_movl(&state, state.edx, &dwmixfa_state.smpposf[state.ecx]);

	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.voiceflags[state.ecx]);

	/* update volumes */
	asm_movl(&state, *(uint32_t *)voll_ptr, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.volleft[state.ecx]);
	asm_movl(&state, *(uint32_t *)volr_ptr, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.volright[state.ecx]);

	asm_movl(&state, *(uint32_t *)__fl1_ptr, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.fl1[state.ecx]);
	asm_movl(&state, *(uint32_t *)__fb1_ptr, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.fb1[state.ecx]);

mixer_SkipVoice:
		asm_decl(&state, &state.ecx);
	asm_jns(&state, mixer_MixNext);

	asm_movl(&state, 0x12345678 /*postprocs*/, &state.esi); esi_mirror = dwmixfa_state.postprocs;
mixer_PostprocLoop:
/*
	asm_orl(&state, state.esi, state.esi);*/ write_zf(state.eflags, !esi_mirror);
	asm_jz(&state, mixer_PostprocEnd);
	asm_movl(&state, dwmixfa_state.nsamples, &state.edx);
	asm_movl(&state, dwmixfa_state.isstereo, &state.ecx);
	asm_movl(&state, dwmixfa_state.samprate, &state.ebx);
	asm_movl(&state, 0x12345678, &state.eax); eax_mirror = dwmixfa_state.tempbuf;
	/* call *state.esi*/ esi_mirror->Process(eax_mirror, state.edx, state.ebx, state.ecx);
	asm_movl(&state, state.esi+12, &state.esi); esi_mirror = esi_mirror->next;

	asm_jmp(&state, mixer_PostprocLoop);

mixer_PostprocEnd:

	asm_movl(&state, dwmixfa_state.outfmt, &state.eax);
/*
	{
		int i;
		for (i=0;i<nsamples;i++)
		{
			fprintf(stderr, "%f\n", tempbuf[i]);
			if (i==8)
				break;
		}
	}
*/
	/*asm_movl(&state, clippers[state.eax], &state.eax);*/ eax_mirror2 = clippers[state.eax];

	asm_movl(&state, 0x12345678/*outbuf*/, &state.edi); edi_mirror2 = dwmixfa_state.outbuf;
	asm_movl(&state, 0x12345678/*tempbuf*/, &state.esi); esi_mirror2 = dwmixfa_state.tempbuf;
	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);

	asm_movl(&state, dwmixfa_state.isstereo, &state.edx);
	asm_orl(&state, state.edx, &state.edx);
	asm_jz(&state, mixer_clipmono);
	asm_addl(&state, state.ecx, &state.ecx);
mixer_clipmono:
	/* call *state.eax*/ eax_mirror2(esi_mirror2, edi_mirror2, state.ecx);

mixer_endall:
	asm_popl(&state, &state.ebp);

	debug_printf("}\n");

}

static void mix_0   (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mix_0 {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mix_0_next:
		asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
		asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
mix_0_looped:
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mix_0_LoopHandler);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mix_0_next);
mix_0_ende:
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);


	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mix_0_LoopHandler:
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mix_0_loopme);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mix_0_ende);
mix_0_loopme:
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	asm_jmp(&state, mix_0_looped);
}

static void mixm_n  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_n {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, dwmixfa_state.voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mixm_n_next:
	asm_flds(&state, *ebp_mirror);
	asm_fld(&state, 1);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
/*mixm_n_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_n_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_n_next);
mixm_n_ende:
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("mixer }\n");
	return;

mixm_n_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_n_loopme);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_n_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_n_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.fadeleft);

	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixm_n_ende);

mixm_n_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_n_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_n_ende);
	asm_jmp(&state, mixm_n_next);
}

static void mixs_n  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_n {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, dwmixfa_state.voll);
	asm_flds(&state, dwmixfa_state.volr);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mixs_n_next:
	asm_flds(&state, *ebp_mirror);
	asm_addl(&state, state.esi, &state.edx);if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_st(&state, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, dwmixfa_state.volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);

/*mixs_n_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_n_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_n_next);
mixs_n_ende:
	asm_fstps(&state, &dwmixfa_state.volr);
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("mixer }\n");
	return;

mixs_n_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_n_loopme);
	asm_fxch_st(&state, 1);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);

	asm_fxch_st(&state, 2);
mixs_n_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, dwmixfa_state.volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_n_fill);
	asm_fxch_st(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);

	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixs_n_ende);

mixs_n_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_n_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_n_ende);
	asm_jmp(&state, mixs_n_next);
}

static void mixm_i  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_i {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, minuseins);
	asm_flds(&state, dwmixfa_state.voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_shrl(&state, 2, &state.ebp);
	asm_orl(&state, 0x3f800000, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.magic1);
mixm_i_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fld(&state, 0);
	asm_fld(&state, 3);
	asm_fadds(&state, dwmixfa_state.magic1);
	asm_fxch_st(&state, 1);
	asm_fsubrs(&state, ebp_mirror[1]);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_st(&state, 1);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_faddp_stst(&state, 0, 1);
	asm_fld(&state, 1);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
	asm_orl(&state, 0x3f800000, &state.eax);
/*mixm_i_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.magic1);
		asm_jae(&state, mixm_i_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i_next);
mixm_i_ende:
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_fstp_st(&state, 0);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_i_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_i_loopme);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_i_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.fadeleft);

	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixm_i_ende);

mixm_i_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_i_ende);
	asm_jmp(&state, mixm_i_next);
}

static void mixs_i  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_i {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, minuseins);
	asm_flds(&state, dwmixfa_state.voll);
	asm_flds(&state, dwmixfa_state.volr);
	asm_shrl(&state, 2, &state.ebp);

	asm_pushl(&state, state.ebp);



	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_shrl(&state, 2, &state.ebp);
	asm_orl(&state, 0x3f800000, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.magic1);


mixs_i_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fld(&state, 0);
	asm_fld(&state, 4);
	asm_fadds(&state, dwmixfa_state.magic1);
	asm_fxch_st(&state, 1);
	asm_fsubrs(&state, ebp_mirror[1]);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_st(&state, 1);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_faddp_stst(&state, 0, 1);
	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, dwmixfa_state.volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
	asm_orl(&state, 0x3f800000, &state.eax);
/*mixs_i_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.magic1);
		asm_jae(&state, mixs_i_LoopHandler);

		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_i_next);
mixs_i_ende:
	asm_fstps(&state, &dwmixfa_state.volr);
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_fstp_st(&state, 0);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;

	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_i_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_i_loopme);
	asm_fxch_st(&state, 2);
	asm_fstp_st(&state, 0);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_st(&state, 2);
mixs_i_fill:
	/*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, dwmixfa_state.volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_i_fill);

	asm_fld(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.faderight);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixs_i_ende);

mixs_i_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_i_ende);
	asm_jmp(&state, mixs_i_next);
}

static void mixm_i2 (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_i2 {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, dwmixfa_state.voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 24, &state.eax);
mixm_i2_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fmuls(&state, dwmixfa_state.ct0[state.eax]);
	asm_flds(&state, ebp_mirror[1]);
	asm_fmuls(&state, dwmixfa_state.ct1[state.eax]);
	asm_flds(&state, ebp_mirror[2]);
	asm_fmuls(&state, dwmixfa_state.ct2[state.eax]);
	asm_flds(&state, ebp_mirror[3]);
	asm_fmuls(&state, dwmixfa_state.ct3[state.eax]);
	asm_fxch_st(&state, 2);
	asm_faddp_stst(&state, 0, 3);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_faddp_stst(&state, 0, 2);
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_movl(&state, state.edx, &state.eax);
	asm_faddp_stst(&state, 0, 1);
	asm_shrl(&state, 24, &state.eax);
	asm_fld(&state, 1);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
/*mixm_i2_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i2_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i2_next);
mixm_i2_ende:
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_i2_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_pushl(&state, state.eax);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_i2_loopme);
	asm_popl(&state, &state.eax);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_i2_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i2_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.fadeleft);

	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixm_i2_ende);

mixm_i2_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i2_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_i2_ende);
	asm_jmp(&state, mixm_i2_next);
}

static void mixs_i2 (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_i2 {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, dwmixfa_state.voll);
	asm_flds(&state, dwmixfa_state.volr);

	asm_shrl(&state, 2, &state.ebp);

	asm_pushl(&state, state.ebp);


	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 24, &state.eax);

mixs_i2_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fmuls(&state, dwmixfa_state.ct0[state.eax]);
	asm_flds(&state, ebp_mirror[1]);
	asm_fmuls(&state, dwmixfa_state.ct1[state.eax]);
	asm_flds(&state, ebp_mirror[2]);
	asm_fmuls(&state, dwmixfa_state.ct2[state.eax]);
	asm_flds(&state, ebp_mirror[3]);
	asm_fmuls(&state, dwmixfa_state.ct3[state.eax]);
	asm_fxch_st(&state, 2);
	asm_faddp_stst(&state, 0, 3);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_faddp_stst(&state, 0, 2);
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_movl(&state, state.edx, &state.eax);
	asm_faddp_stst(&state, 0, 1);
	asm_shrl(&state, 24, &state.eax);
	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, dwmixfa_state.volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
/*mixs_i2_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i2_LoopHandler);

		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_i2_next);
mixs_i2_ende:
	asm_fstps(&state, &dwmixfa_state.volr);
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_i2_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1);
	asm_pushl(&state, state.eax);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_i2_loopme);
	asm_popl(&state, &state.eax);
	asm_fxch_st(&state, 1);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_st(&state, 2);
mixs_i2_fill: /*  sample ends -> fill rest of buffer with last sample value */

		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, dwmixfa_state.volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_i2_fill);

	asm_fxch_st(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixs_i2_ende);

mixs_i2_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i2_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_i2_ende);
	asm_jmp(&state, mixs_i2_next);
}

static void mixm_nf (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_nf {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, dwmixfa_state.voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mixm_nf_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fsubs(&state, dwmixfa_state.__fl1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_flds(&state, dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_fadds(&state, dwmixfa_state.__fl1);
	asm_fsts(&state, &dwmixfa_state.__fl1);

	asm_fld(&state, 1);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
/*ixm_nf_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_nf_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_nf_next);
mixm_nf_ende:
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_nf_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_nf_loopme);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_nf_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_nf_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.fadeleft);

	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixm_nf_ende);

mixm_nf_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_nf_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_nf_ende);
	asm_jmp(&state, mixm_nf_next);
}

static void mixs_nf (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_nf {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, dwmixfa_state.voll);
	asm_flds(&state, dwmixfa_state.volr);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mixs_nf_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fsubs(&state, dwmixfa_state.__fl1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_flds(&state, dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_fadds(&state, dwmixfa_state.__fl1);
	asm_fsts(&state, &dwmixfa_state.__fl1);

	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, dwmixfa_state.volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
/*mixs_nf_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_nf_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_nf_next);
mixs_nf_ende:
	asm_fstps(&state, &dwmixfa_state.volr);
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_nf_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_stst(&state, 0, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_stst(&state, 0, 1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_nf_loopme);
	asm_fxch_stst(&state, 0, 1);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_stst(&state, 0, 2);
mixs_nf_fill:
	/*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, dwmixfa_state.volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_nf_fill);

	asm_fxch_st(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixs_nf_ende);

mixs_nf_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_nf_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_nf_ende);
	asm_jmp(&state, mixs_nf_next);
}

static void mixm_if (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_if {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, minuseins);
	asm_flds(&state, dwmixfa_state.voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_shrl(&state, 2, &state.ebp);
	asm_orl(&state, 0x3f800000, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.magic1);
mixm_if_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fld(&state, 0);
	asm_fld(&state, 3);
	asm_fadds(&state, dwmixfa_state.magic1);
	asm_fxch_st(&state, 1);
	asm_fsubrs(&state, ebp_mirror[1]);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_st(&state, 1);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_faddp_stst(&state, 0, 1);

	asm_fsubs(&state, dwmixfa_state.__fl1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_flds(&state, dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_fadds(&state, dwmixfa_state.__fl1);
	asm_fsts(&state, &dwmixfa_state.__fl1);

	asm_fld(&state, 1);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
	asm_orl(&state, 0x3f800000, &state.eax);
/*mixm_if_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.magic1);
		asm_jae(&state, mixm_if_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_if_next);
mixm_if_ende:
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_fstp_st(&state, 0);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_if_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_if_loopme);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_if_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_if_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.fadeleft);

	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixm_if_ende);

mixm_if_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_if_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_if_ende);
	asm_jmp(&state, mixm_if_next);
}

static void mixs_if (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_if {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, minuseins);
	asm_flds(&state, dwmixfa_state.voll);
	asm_flds(&state, dwmixfa_state.volr);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_shrl(&state, 2, &state.ebp);
	asm_orl(&state, 0x3f800000, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.magic1);
mixs_if_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fld(&state, 0);
	asm_fld(&state, 4);
	asm_fadds(&state, dwmixfa_state.magic1);
	asm_fxch_st(&state, 1);
	asm_fsubrs(&state, ebp_mirror[1]);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_st(&state, 1);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_faddp_stst(&state, 0, 1);

	asm_fsubs(&state, dwmixfa_state.__fl1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_flds(&state, dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_fadds(&state, dwmixfa_state.__fl1);
	asm_fsts(&state, &dwmixfa_state.__fl1);

	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, dwmixfa_state.volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
	asm_orl(&state, 0x3f800000, &state.eax);
/*mixs_if_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_movl(&state, state.eax, (uint32_t *)&dwmixfa_state.magic1);
		asm_jae(&state, mixs_if_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_if_next);
mixs_if_ende:
	asm_fstps(&state, &dwmixfa_state.volr);
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_fstp_st(&state, 0);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_if_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_if_loopme);
	asm_fxch_st(&state, 2);
	asm_fstp_st(&state, 0);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_st(&state, 2);
mixs_if_fill:
	/*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, dwmixfa_state.volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_if_fill);
	/*asm_fmul(&state, 1, 0);*/
	asm_fld(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.faderight);

	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixs_if_ende);

mixs_if_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_if_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_if_ende);
	asm_jmp(&state, mixs_if_next);
}

static void mixm_i2f(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_i2f {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, dwmixfa_state.voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 24, &state.eax);
mixm_i2f_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fmuls(&state, dwmixfa_state.ct0[state.eax]);
	asm_flds(&state, ebp_mirror[1]);
	asm_fmuls(&state, dwmixfa_state.ct1[state.eax]);
	asm_flds(&state, ebp_mirror[2]);
	asm_fmuls(&state, dwmixfa_state.ct2[state.eax]);
	asm_flds(&state, ebp_mirror[3]);
	asm_fmuls(&state, dwmixfa_state.ct3[state.eax]);
	asm_fxch_st(&state, 2);
	asm_faddp_stst(&state, 0, 3);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_faddp_stst(&state, 0, 2);
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_movl(&state, state.edx, &state.eax);
	asm_faddp_stst(&state, 0, 1);

	asm_fsubs(&state, dwmixfa_state.__fl1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_flds(&state, dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_fadds(&state, dwmixfa_state.__fl1);
	asm_fsts(&state, &dwmixfa_state.__fl1);

	asm_shrl(&state, 24, &state.eax);
	asm_fld(&state, 1);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
/*mixm_i2f_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i2f_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i2f_next);
mixm_i2f_ende:
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_i2f_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_pushl(&state, state.eax);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_i2f_loopme);
	asm_popl(&state, &state.eax);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_i2f_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i2f_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.fadeleft);

	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixm_i2f_ende);

mixm_i2f_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i2f_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_i2f_ende);
	asm_jmp(&state, mixm_i2f_next);
}

static void mixs_i2f(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_i2f {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, dwmixfa_state.nsamples, &state.ecx);
	asm_flds(&state, dwmixfa_state.voll);
	asm_flds(&state, dwmixfa_state.volr);
	asm_shrl(&state, 2, &state.ebp);

	asm_pushl(&state, state.ebp);

	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 24, &state.eax);

mixs_i2f_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fmuls(&state, dwmixfa_state.ct0[state.eax]);
	asm_flds(&state, ebp_mirror[1]);
	asm_fmuls(&state, dwmixfa_state.ct1[state.eax]);
	asm_flds(&state, ebp_mirror[2]);
	asm_fmuls(&state, dwmixfa_state.ct2[state.eax]);
	asm_flds(&state, ebp_mirror[3]);
	asm_fmuls(&state, dwmixfa_state.ct3[state.eax]);
	asm_fxch_st(&state, 2);
	asm_faddp_stst(&state, 0, 3);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_faddp_stst(&state, 0, 2);
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_movl(&state, state.edx, &state.eax);
	asm_faddp_stst(&state, 0, 1);

	asm_fsubs(&state, dwmixfa_state.__fl1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_flds(&state, dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &dwmixfa_state.__fb1);
	asm_fmuls(&state, dwmixfa_state.ffrq);
	asm_fadds(&state, dwmixfa_state.__fl1);
	asm_fsts(&state, &dwmixfa_state.__fl1);

	asm_shrl(&state, 24, &state.eax);
	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, dwmixfa_state.volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, dwmixfa_state.volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
/*mixs_i2f_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i2f_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_i2f_next);
mixs_i2f_ende:
	asm_fstps(&state, &dwmixfa_state.volr);
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_i2f_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1);
	asm_pushl(&state, state.eax);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_i2f_loopme);
	asm_popl(&state, &state.eax);
	asm_fxch_st(&state, 1);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_st(&state, 2);
mixs_i2f_fill:
	/*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, dwmixfa_state.volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, dwmixfa_state.volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_i2f_fill);

	asm_fxch_st(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &dwmixfa_state.fadeleft);
	asm_fstps(&state, &dwmixfa_state.faderight);
	asm_fxch_st(&state, 1);
	asm_movl(&state, dwmixfa_state.looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &dwmixfa_state.looptype);
	asm_jmp(&state, mixs_i2f_ende);

mixs_i2f_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, dwmixfa_state.mixlooplen, &state.ebp); ebp_mirror -= dwmixfa_state.mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i2f_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_i2f_ende);
	asm_jmp(&state, mixs_i2f_next);
}

static void clip_16s(float *input, void *output, uint_fast32_t count)
{
	struct assembler_state_t state;
	float *esi_mirror;
	uint16_t *edi_mirror;

	debug_printf("clip_16s {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*input*/ 0x12345678, &state.esi); esi_mirror = input;
	asm_movl(&state, /*_output*/0x87654321, &state.edi); edi_mirror = output;
	asm_movl(&state, count, &state.ecx);

	asm_flds(&state, clampmin);
	asm_flds(&state, clampmax);
	asm_movw(&state, 32767, &state.bx);
	asm_movw(&state, -32768, &state.dx);

clip_16s_lp:
	asm_flds(&state, *esi_mirror);
	asm_fcom_st(&state, 1);
	asm_fnstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_ja(&state, clip_16s_max);
	asm_fcom_st(&state, 2);
	asm_fstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_jb(&state, clip_16s_min);
	asm_fistps(&state, edi_mirror);
clip_16s_next:
	asm_addl(&state, 4, &state.esi); esi_mirror++;
	asm_addl(&state, 2, &state.edi); edi_mirror++;
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, clip_16s_lp);
	asm_jmp(&state, clip_16s_ende);
clip_16s_max:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, edi_mirror);
	asm_jmp(&state, clip_16s_next);

clip_16s_min:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.dx, edi_mirror);
	asm_jmp(&state, clip_16s_next);

clip_16s_ende:
	asm_fstp_st(&state, 0);
	asm_fstp_st(&state, 0);
	debug_printf("}\n");
}

static void clip_16u(float *input, void *output, uint_fast32_t count)
{
	struct assembler_state_t state;
	float *esi_mirror;
	uint16_t *edi_mirror;

	debug_printf("clip_16u {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*input*/ 0x12345678, &state.esi); esi_mirror = input;
	asm_movl(&state, /*_output*/0x87654321, &state.edi); edi_mirror = output;
	asm_movl(&state, count, &state.ecx);

	asm_flds(&state, clampmin);
	asm_flds(&state, clampmax);
	asm_movw(&state, 32767, &state.bx);
	asm_movw(&state, -32768, &state.dx);

clip_16u_lp:
	asm_flds(&state, *esi_mirror);
	asm_fcom_st(&state, 1);
	asm_fnstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_ja(&state, clip_16u_max);
	asm_fcom_st(&state, 2);
	asm_fstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_jb(&state, clip_16u_min);
	asm_fistps(&state, &dwmixfa_state.clipval);
	asm_movw(&state, dwmixfa_state.clipval, &state.ax);
clip_16u_next:
	asm_xorw(&state, 0x8000, &state.ax);
	asm_movw(&state, state.ax, edi_mirror);
	asm_addl(&state, 4, &state.esi); esi_mirror++;
	asm_addl(&state, 2, &state.edi); edi_mirror++;
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, clip_16u_lp);
	asm_jmp(&state, clip_16u_ende);
clip_16u_max:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, &state.ax);
	asm_jmp(&state, clip_16u_next);

clip_16u_min:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, &state.ax);
	asm_jmp(&state, clip_16u_next);

clip_16u_ende:
	asm_fstp_st(&state, 0);
	asm_fstp_st(&state, 0);
	debug_printf("}\n");
}

static void clip_8s(float *input, void *output, uint_fast32_t count)
{
	struct assembler_state_t state;
	float *esi_mirror;
	uint8_t *edi_mirror;

	debug_printf("clip_8s {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*input*/ 0x12345678, &state.esi); esi_mirror = input;
	asm_movl(&state, /*_output*/0x87654321, &state.edi); edi_mirror = output;
	asm_movl(&state, count, &state.ecx);

	asm_flds(&state, clampmin);
	asm_flds(&state, clampmax);
	asm_movw(&state, 32767, &state.bx);
	asm_movw(&state, -32768, &state.dx);

clip_8s_lp:
	asm_flds(&state, *esi_mirror);
	asm_fcom_st(&state, 1);
	asm_fnstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_ja(&state, clip_8s_max);
	asm_fcom_st(&state, 2);
	asm_fstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_jb(&state, clip_8s_min);
	asm_fistps(&state, &dwmixfa_state.clipval);
	asm_movw(&state, dwmixfa_state.clipval, &state.ax);
clip_8s_next:
	asm_movb(&state, state.ah, edi_mirror);
	asm_addl(&state, 4, &state.esi); esi_mirror++;
	asm_addl(&state, 1, &state.edi); edi_mirror++;
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, clip_8s_lp);
	asm_jmp(&state, clip_8s_ende);
clip_8s_max:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, &state.ax);
	asm_jmp(&state, clip_8s_next);

clip_8s_min:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.dx, &state.ax);
	asm_jmp(&state, clip_8s_next);

clip_8s_ende:
	asm_fstp_st(&state, 0);
	asm_fstp_st(&state, 0);
	debug_printf("}\n");
}

static void clip_8u(float *input, void *output, uint_fast32_t count)
{
	struct assembler_state_t state;
	float *esi_mirror;
	uint8_t *edi_mirror;

	debug_printf("clip_8u {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*input*/ 0x12345678, &state.esi); esi_mirror = input;
	asm_movl(&state, /*_output*/0x87654321, &state.edi); edi_mirror = output;
	asm_movl(&state, count, &state.ecx);

	asm_flds(&state, clampmin);
	asm_flds(&state, clampmax);
	asm_movw(&state, 32767, &state.bx);
	asm_movw(&state, -32768, &state.dx);

clip_8u_lp:
	asm_flds(&state, *esi_mirror);
	asm_fcom_st(&state, 1);
	asm_fnstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_ja(&state, clip_8u_max);
	asm_fcom_st(&state, 2);
	asm_fstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_jb(&state, clip_8u_min);
	asm_fistps(&state, &dwmixfa_state.clipval);
	asm_movw(&state, dwmixfa_state.clipval, &state.ax);
clip_8u_next:
	asm_xorw(&state, 0x8000, &state.ax);
	asm_movb(&state, state.ah, edi_mirror);
	asm_addl(&state, 4, &state.esi); esi_mirror++;
	asm_addl(&state, 1, &state.edi); edi_mirror++;
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, clip_8u_lp);
	asm_jmp(&state, clip_8u_ende);
clip_8u_max:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, &state.ax);
	asm_jmp(&state, clip_8u_next);

clip_8u_min:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.dx, &state.ax);
	asm_jmp(&state, clip_8u_next);

clip_8u_ende:
	asm_fstp_st(&state, 0);
	asm_fstp_st(&state, 0);
	debug_printf("}\n");
}

void getchanvol (int n, int len)
{
	struct assembler_state_t state;

	float *ebp_mirror;
	float *edi_mirror;

	debug_printf("getchanvol {\n");

	init_assembler_state(&state, writecallback, readcallback);

	state.ecx = len; /* assembler entry config */

	asm_pushl(&state, state.ebp);
	asm_fldz(&state);
	asm_movl(&state, state.ecx, &dwmixfa_state.nsamples);

	asm_movl(&state, dwmixfa_state.voiceflags[state.eax], &state.ebx);
	asm_testl(&state, MIXF_PLAYING, state.ebx);
	asm_jz(&state, getchanvol_SkipVoice);
	asm_movl(&state, dwmixfa_state.looplen[state.eax], &state.ebx);
	asm_movl(&state, state.ebx, &dwmixfa_state.mixlooplen);
	asm_movl(&state, dwmixfa_state.freqw[state.eax], &state.ebx);
	asm_movl(&state, dwmixfa_state.freqf[state.eax], &state.esi);
	asm_movl(&state, dwmixfa_state.smpposf[state.eax], &state.edx);
	asm_movl(&state, /*loopend[state.eax]*/0x12345678, &state.edi); edi_mirror = dwmixfa_state.loopend[state.eax];
	asm_shrl(&state, 2, &state.edi); /* this is fucked up logic :-p */
	asm_movl(&state, /*smpposw[state.eax]*/0x87654321, &state.ebp); ebp_mirror = dwmixfa_state.smpposw[state.eax];
	asm_shrl(&state, 2, &state.ebp); /* this is fucked up logic :-p */
/*getchanvol_next:*/
	asm_flds(&state, *ebp_mirror); /* (,%ebp,4)*/
	asm_testl(&state, 0x80000000, *(uint32_t *)ebp_mirror); /* sign og *ebp_mirror */
	asm_jnz(&state, getchanvol_neg);
	asm_faddp_stst(&state, 0, 1);
	asm_jmp(&state, getchanvol_goon);
getchanvol_neg:
	asm_fsubp_stst(&state, 0, 1);
getchanvol_goon:
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
getchanvol_looped:
/*
	asm_cmpl(&state, state.edi, state.ebp);*/
	if (ebp_mirror == edi_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror>edi_mirror) /* pos > loopend */
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, getchanvol_LoopHandler);
	asm_jmp(&state, getchanvol_SkipVoice);
getchanvol_LoopHandler:
	asm_testl(&state, MIXF_LOOPED, dwmixfa_state.voiceflags[state.eax]);
	asm_jz(&state, getchanvol_SkipVoice);
	asm_subl(&state, dwmixfa_state.looplen[state.eax], &state.ebp); ebp_mirror -= dwmixfa_state.looplen[state.eax];
	asm_jmp(&state, getchanvol_looped);
getchanvol_SkipVoice:
	asm_fidivl(&state, dwmixfa_state.nsamples);
	asm_fldx(&state, read_fpu_st(&state, 0));
	asm_fmuls(&state, dwmixfa_state.volleft[state.eax]);
	asm_fstps(&state, &dwmixfa_state.voll);
	asm_fmuls(&state, dwmixfa_state.volright[state.eax]);
	asm_fstps(&state, &dwmixfa_state.volr);

	asm_popl(&state, &state.ebp);
	debug_printf("}\n");
}
