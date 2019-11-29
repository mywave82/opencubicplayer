/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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

#include <stddef.h>
#include "types.h"

#define FLAG_DISABLED (~MIXF_PLAYING)

dwmixfa_state_t dwmixfa_state;

static const float __attribute__ ((used)) clampmax=32767.0;
static const float __attribute__ ((used)) clampmin=-32767.0;
static const float __attribute__ ((used)) cremoveconst=0.992;
#if 1
#define minampl 0x38d1b717 /* float 0.0001, represented as in integer */
#else
static const float __attribute__ ((used)) minampl=0.0001;
#endif

#ifdef __PIC__
# if __GNUC_PREREQ (4, 7)
#  define GET_PC_THUNK_STR(reg) "__x86.get_pc_thunk." #reg
# else
#  define GET_PC_THUNK_STR(reg) "__i686.get_pc_thunk." #reg
# endif
#endif


void start_dwmixfa(void)
{
	__asm__ __volatile__ (
		".equ tempbuf_ofs,    %c0\n"
		".equ outbuf_ofs,     %c1\n"
		".equ nsamples_ofs,   %c2\n"
		".equ nvoices_ofs,    %c3\n"
		".equ freqw_ofs,      %c4\n"
		".equ freqf_ofs,      %c5\n"
		".equ smpposw_ofs,    %c6\n"
		".equ smpposf_ofs,    %c7\n"
		".equ loopend_ofs,    %c8\n"
		".equ looplen_ofs,    %c9\n"
		:
		: "n" (offsetof(dwmixfa_state_t, tempbuf)),
		  "n" (offsetof(dwmixfa_state_t, outbuf)),
		  "n" (offsetof(dwmixfa_state_t, nsamples)),
		  "n" (offsetof(dwmixfa_state_t, nvoices)),
		  "n" (offsetof(dwmixfa_state_t, freqw[0])),
		  "n" (offsetof(dwmixfa_state_t, freqf[0])),
		  "n" (offsetof(dwmixfa_state_t, smpposw[0])),
		  "n" (offsetof(dwmixfa_state_t, smpposf[0])),
		  "n" (offsetof(dwmixfa_state_t, loopend[0])),
		  "n" (offsetof(dwmixfa_state_t, looplen[0]))
	);
	__asm__ __volatile__ (
		".equ volleft_ofs,    %c0\n"
		".equ volright_ofs,   %c1\n"
		".equ rampleft_ofs,   %c2\n"
		".equ rampright_ofs,  %c3\n"
		".equ voiceflags_ofs, %c4\n"
		".equ ffreq_ofs,      %c5\n"
		".equ freso_ofs,      %c6\n"
		".equ fadeleft_ofs,   %c7\n"
		".equ faderight_ofs,  %c8\n"
		".equ fl1_ofs,        %c9\n"
		:
		: "n" (offsetof(dwmixfa_state_t, volleft[0])),
		  "n" (offsetof(dwmixfa_state_t, volright[0])),
		  "n" (offsetof(dwmixfa_state_t, rampleft[0])),
		  "n" (offsetof(dwmixfa_state_t, rampright[0])),
		  "n" (offsetof(dwmixfa_state_t, voiceflags[0])),
		  "n" (offsetof(dwmixfa_state_t, ffreq[0])),
		  "n" (offsetof(dwmixfa_state_t, freso[0])),
		  "n" (offsetof(dwmixfa_state_t, fadeleft)),
		  "n" (offsetof(dwmixfa_state_t, faderight)),
		  "n" (offsetof(dwmixfa_state_t, fl1[0]))
	);
	__asm__ __volatile__ (
		".equ fb1_ofs,        %c0\n"
		".equ isstereo_ofs,   %c1\n"
		".equ outfmt_ofs,     %c2\n"
		".equ voll_ofs,       %c3\n"
		".equ volr_ofs,       %c4\n"
		".equ ct0_ofs,        %c5\n"
		".equ ct1_ofs,        %c6\n"
		".equ ct2_ofs,        %c7\n"
		".equ ct3_ofs,        %c8\n"
		".equ postprocs_ofs,  %c9\n"
		:
		: "n" (offsetof(dwmixfa_state_t, fb1[0])),
		  "n" (offsetof(dwmixfa_state_t, isstereo)),
		  "n" (offsetof(dwmixfa_state_t, outfmt)),
		  "n" (offsetof(dwmixfa_state_t, voll)),
		  "n" (offsetof(dwmixfa_state_t, volr)),
		  "n" (offsetof(dwmixfa_state_t, ct0[0])),
		  "n" (offsetof(dwmixfa_state_t, ct1[0])),
		  "n" (offsetof(dwmixfa_state_t, ct2[0])),
		  "n" (offsetof(dwmixfa_state_t, ct3[0])),
		  "n" (offsetof(dwmixfa_state_t, postprocs))
	);
	__asm__ __volatile__ (
		".equ samprate_ofs,   %c0\n"
		".equ volrl_ofs,      %c1\n"
		".equ volrr_ofs,      %c2\n"
		".equ clipval_ofs,    %c3\n"
		".equ mixlooplen_ofs, %c4\n"
		".equ looptype_ofs,   %c5\n"
		".equ magic1_ofs,     %c6\n"
		".equ ffrq_ofs,       %c7\n"
		".equ frez_ofs,       %c8\n"
		:
		: "n" (offsetof(dwmixfa_state_t, samprate)),
		  "n" (offsetof(dwmixfa_state_t, volrl)),
		  "n" (offsetof(dwmixfa_state_t, volrr)),
		  "n" (offsetof(dwmixfa_state_t, clipval)),
		  "n" (offsetof(dwmixfa_state_t, mixlooplen)),
		  "n" (offsetof(dwmixfa_state_t, looptype)),
		  "n" (offsetof(dwmixfa_state_t, magic1)),
		  "n" (offsetof(dwmixfa_state_t, ffrq)),
		  "n" (offsetof(dwmixfa_state_t, frez))
	);
	__asm__ __volatile__ (
		".equ __fl1_ofs,      %c0\n"
		".equ __fb1_ofs,      %c1\n"
		:
		: "n" (offsetof(dwmixfa_state_t, __fl1)),
		  "n" (offsetof(dwmixfa_state_t, __fb1))
	);
#if 0
	volrl=volrl;
	volrr=volrr;
	eins=eins;
	minuseins=minuseins;
	clampmin=clampmin;
	clampmax=clampmax;
	cremoveconst=cremoveconst;
	minampl=minampl;
	magic1=magic1;
	clipval=clipval;
	mixlooplen=mixlooplen;
	looptype=looptype;
	ffrq=ffrq;
	frez=frez;
	__fl1=__fl1;
	__fb1=__fb1;
#endif
}

#include <string.h>
void prepare_mixer (void)
{
#if 0
	dwmixfa_state.fadeleft = 0;
	dwmixfa_state.faderight = 0;
	dwmixfa.volrl = 0;
	dwmixfa.volrr = 0;
	memset(dwmixfa_state.volleft, 0, sizeof (dwmixfa_state.volleft));
	memset(dwmixfa_state.volright, 0, sizeof (dwmixfa_state.volright));
#endif
	__asm__ __volatile__
	(
#ifdef __PIC__
/* save EBX, and setup EBX PIC, probably overkill, since we are in the same .so file as the caller.... */
/* store pointer to dwmixfa_state into EBX. Non PIC code already has this in place from stub */
		"pushl %%ebx\n"

		"call " GET_PC_THUNK_STR(bx) "\n"
		"addl $_GLOBAL_OFFSET_TABLE_, %%ebx\n"
		"movl dwmixfa_state@GOT(%%ebx), %%ebx\n"
#endif

/* clear EAX */
		"xorl %%eax, %%eax\n"

/* Clear private volrl and volrr */
		"movl %%eax, volrl_ofs(%%ebx)\n" /* volrl is not in global struct */
		"movl %%eax, volrr_ofs(%%ebx)\n" /* volrr is not in glocal struct */

/* clear dwmixfa_state.fadeleft and dwmixfa_state.faderight */
		"movl %%eax, fadeleft_ofs(%%ebx)\n" /* fadeleft */
		"movl %%eax, faderight_ofs(%%ebx)\n" /* faderight */

/* clear ECX, and count up to MAXVOICES */
		"xorl %%ecx, %%ecx\n"
	"prepare_mixer_fillloop:\n"
/* clear dwmixfa_state.volleft[] and dwmixfa_state.volright[] */
		"movl %%eax, volleft_ofs(%%ebx,%%ecx,4)\n" /* volleft */
		"movl %%eax, volright_ofs(%%ebx,%%ecx,4)\n" /* volright */
		"incl %%ecx\n"
		"cmpl %0, %%ecx\n" /* MAXVOICES */
		"jne prepare_mixer_fillloop\n"
#ifdef __PIC__
/* restore EBX */
		"pop %%ebx\n"
#endif

		:
		: "n" (MAXVOICES)
#ifndef __PIC__
		  ,"b" (&dwmixfa_state)
#endif
		: "eax", "ecx"
	);
}

void mixer (void)
{
#ifdef DEBUG
	fprintf(stderr, "mixer()");
	fprintf(stderr, "tempbuf=%p\n", tempbuf);
	fprintf(stderr, "outbuf=%p\n", outbuf);
	fprintf(stderr, "nsamples=%d (samples to mix)\n", (int)nsamples);
	fprintf(stderr, "nvoices=%d (voices to mix)\n", (int)nvoices);
	{
		int i;
		for (i=0;i<nvoices;i++)
		{
			fprintf(stderr, "freqw.f[%d]=%u.%u\n", i, (unsigned int)freqw[i], (unsigned int)freqf[i]);
			fprintf(stderr, "smpposw.f[%d]=%p.%u\n", i, smpposw[i], (unsigned int)smpposf[i]);
			fprintf(stderr, "loopend[%d]=%p\n", i, loopend[i]);
			fprintf(stderr, "looplen[%d]=%u\n", i, (unsigned int)looplen[i]);
			fprintf(stderr, "volleft[%d]=%f\n", i, volleft[i]);
			fprintf(stderr, "volright[%d]=%f\n", i, volright[i]);
			fprintf(stderr, "rampleft[%d]=%f\n", i, rampleft[i]);
			fprintf(stderr, "rampright[%d]=%f\n", i, rampright[i]);
			fprintf(stderr, "voiceflags[%d]=0x%08x\n", i, (unsigned int)voiceflags[i]);
			fprintf(stderr, "ffreq[%d]=%f\n", i, ffreq[i]);
			fprintf(stderr, "freso[%d]=%f\n", i, freso[i]);
			fprintf(stderr, "fl1[%d]=%f\n", i, fl1[i]);
			fprintf(stderr, "fb1[%d]=%f\n", i, fb1[i]);
		}
	}
	fprintf(stderr, "fadeleft=%f\n", fadeleft);
	fprintf(stderr, "faderight=%f\n", faderight);
	fprintf(stderr, "isstereo=%d\n", isstereo);
	fprintf(stderr, "outfmt=%d\n", outfmt);
	/* ct0, ct1, ct2, ct3 */
	fprintf(stderr, "\n");
#endif

	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"call " GET_PC_THUNK_STR(bx) "\n"
		"addl $_GLOBAL_OFFSET_TABLE_, %%ebx\n"
		"pushl %%ebx\n"
		"movl dwmixfa_state@GOT(%%ebx), %%ebx\n"
#else
		"movl $dwmixfa_state, %%ebx\n"
#endif
		"pushl %%ebp\n"

		"finit\n"


/* STACK: +4 (optional) caller EBX (PIC)
          +0            caller EBP
*/
		/* range check for declick values */
		"  xorl %%edx, %%edx\n"
		"  movl fadeleft_ofs(%%ebx), %%eax\n"
		"  andl $0x7fffffff, %%eax\n"
#if 1
		"  cmpl %1, %%eax\n" /* %1 == minampl */
#else
		"  cmpl minampl, %%eax\n"
#endif
		"  jbe mixer_nocutfl\n"
		"  movl %%edx, fadeleft_ofs(%%ebx)\n"
	"  mixer_nocutfl:\n"
		"  movl faderight_ofs(%%ebx), %%eax\n"
		"  andl $0x7fffffff, %%eax\n"
#if 1
		"  cmpl %1, %%eax\n" /* %1 == minampl */
#else
		"  cmpl minampl, %%eax\n"
#endif
		"  jbe mixer_nocutfr\n"
		"  movl %%edx, faderight_ofs(%%ebx)\n"
	"  mixer_nocutfr:\n"

		/* clear and declick buffer */
		"  movl tempbuf_ofs(%%ebx), %%edi\n"
		"  movl nsamples_ofs(%%ebx), %%ecx\n"
		"  orl %%ecx, %%ecx\n"
		"  jz mixer_endall\n"
		"  movl isstereo_ofs(%%ebx), %%eax\n"  /* STEREO DEP 1 EAX */
		"  orl %%eax, %%eax\n"      /* STEREO DEP 1 EAX */
		"  jnz mixer_clearst\n"     /* STEREO DEP 1 BRANCH */
		"    call clearbufm\n"      /* STEREO DEP 1 BRANCH */
		"    jmp mixer_clearend\n"  /* STEREO DEP 1 BRANCH */
	"  mixer_clearst:\n"                /* STEREO DEP 1 BRANCH */
		"  call clearbufs\n"        /* STEREO DEP 1 BRANCH */
	"  mixer_clearend:\n"               /* STEREO DEP 1 BRANCH */

		"  movl nvoices_ofs(%%ebx), %%ecx\n"
		"  decl %%ecx\n"

	"  mixer_MixNext:\n"
		"    movl voiceflags_ofs(%%ebx,%%ecx,4), %%eax\n" /* VOICEFLAGS DEP 2 EAX */
		"    testl %0, %%eax\n"                  /* VOICEFLAGS DEP 2 EAX */
		"    jz mixer_SkipVoice\n"               /* VOICEFLAGS DEP 2 BRANCH */

		/* set loop type */
		"    movl %%eax, looptype_ofs(%%ebx)\n"             /* VOICEFLAGS DEP 2 EAX,LOOPTYPE */

		/* calc l/r relative vols from vol/panning/amplification */
		"    movl volleft_ofs(%%ebx,%%ecx,4), %%eax\n"
		"    movl volright_ofs(%%ebx,%%ecx,4), %%edx\n"
		"    movl %%eax, voll_ofs(%%ebx)\n"
		"    movl %%edx, volr_ofs(%%ebx)\n"

		"    movl rampleft_ofs(%%ebx,%%ecx,4), %%eax\n"
		"    movl rampright_ofs(%%ebx,%%ecx,4), %%edx\n"
		"    movl %%eax, volrl_ofs(%%ebx)\n"
		"    movl %%edx, volrr_ofs(%%ebx)\n"

		/* set up filter vals */
		"    movl ffreq_ofs(%%ebx,%%ecx,4), %%eax\n"
		"    movl %%eax, ffrq_ofs(%%ebx)\n"
		"    movl freso_ofs(%%ebx,%%ecx,4), %%eax\n"
		"    movl %%eax, frez_ofs(%%ebx)\n"
		"    movl fl1_ofs(%%ebx,%%ecx,4), %%eax\n"
		"    movl %%eax, __fl1_ofs(%%ebx)\n"
		"    movl fb1_ofs(%%ebx,%%ecx,4), %%eax\n"
		"    movl %%eax, __fb1_ofs(%%ebx)\n"

		/* length of loop */
		"    movl looplen_ofs(%%ebx,%%ecx,4), %%eax\n"
		"    movl %%eax, mixlooplen_ofs(%%ebx)\n"

		/* sample delta: */

		"    movl freqf_ofs(%%ebx,%%ecx,4), %%esi\n" /* this used to be be sent as ESI parameter */
		"    pushl %%esi\n"
		"    movl freqw_ofs(%%ebx,%%ecx,4), %%esi\n" /* this used to configure ebx, we just store it on the stack for now */
		"    pushl %%esi\n"
/* STACK: +16 (optional) caller EBX (PIC)
	  +12 (optional) PIC
          +8             caller EBP
          +4             freqf_ofs parameter (used to be ESI)
          +0             freqw_ofs parameter (used to be EBX)
*/

		/* Sample base Pointer */
		"    movl smpposw_ofs(%%ebx,%%ecx,4), %%eax\n"
		"    movl smpposf_ofs(%%ebx,%%ecx,4), %%edx\n"

		/* Loop end Pointer */
		"    movl loopend_ofs(%%ebx,%%ecx,4), %%ebp\n"


		"    movl tempbuf_ofs(%%ebx), %%edi\n"

#warning ISSTEREO is masked in at reserved input bit for now...
		"    pushl %%ecx\n"
/* STACK: +20 (optional) caller EBX (PIC)
          +16 (optional) PIC
          +12            caller EBP
          +8             freqf_ofs parameter (used to be ESI)
          +4             freqw_ofs parameter (used to be EBX)
          +0             saved ECX
*/
		"    movl isstereo_ofs(%%ebx), %%ecx\n"             /* STEREO DEP 3, ECX */
		"    orl voiceflags_ofs(%%ebx,%%ecx,4), %%ecx\n"  /* VOICEFLAGS,STEREO DEP 3, ECX.. we can use looptype instead, less complex */
		"    andl $15, %%ecx\n"
#ifdef __PIC__
		"    movl 16(%%esp), %%esi\n"
		"    movl mixers@GOTOFF(%%esi, %%ecx, 4), %%ecx\n"
#else
		"    movl mixers(,%%ecx,4), %%ecx\n"     /* VOICEFLAGS,STEREO,MIXERS DEP 3,ECX */
#endif

		/* sample base ptr fraction part */
		"    call *%%ecx\n"                      /* this call modifies LOOPTYPE */

		"    popl %%ecx\n"

/* STACK: +16 (optional) caller EBX (PIC)
          +12 (optional) PIC
          +8             caller EBP
          +4             freqw_ofs parameter (used to be EBX)
          +0             freqw_ofs parameter (used to be EBX)
*/
#if 1
		"    addl $8, %%esp\n" /* popl and discard the two top entries on the stack */
#else
		"    popl %%ebp\n" /* we have two "junk" on the stack we need to remove, and this register is free */
		"    popl %%ebp\n" /* we have one "junk" on the stack we need to remove, and this register is free */
#endif
/* STACK: +4 (optional) caller EBX (PIC)
          +8 (optional) PIC
          +0            caller EBP
*/
		/* calculate sample relative position */
		"    movl %%eax, smpposw_ofs(%%ebx,%%ecx,4)\n"
		"    movl %%edx, smpposf_ofs(%%ebx,%%ecx,4)\n"

		/* update flags */
		"    movl looptype_ofs(%%ebx), %%eax\n"            /* VOICEFLAG DEP 4, EAX */
		"    movl %%eax, voiceflags_ofs(%%ebx,%%ecx,4)\n"/* VOICEFLAG DEP 4, EAX,VOICEFLAGS (copy back from LOOPTYPE) */

		/* update volumes */
		"    movl voll_ofs(%%ebx), %%eax\n"
		"    movl %%eax, volleft_ofs(%%ebx,%%ecx,4)\n"
		"    movl volr_ofs(%%ebx), %%eax\n"
		"    movl %%eax, volright_ofs(%%ebx,%%ecx,4)\n"

		/* update filter buffers */
		"    movl __fl1_ofs(%%ebx), %%eax\n"
		"    movl %%eax, fl1_ofs(%%ebx,%%ecx,4)\n"
		"    movl __fb1_ofs(%%ebx), %%eax\n"
		"    movl %%eax, fb1_ofs(%%ebx,%%ecx,4)\n"

	"    mixer_SkipVoice:\n"
		"    decl %%ecx\n"
		"  jns mixer_MixNext\n"

/* ryg990504 - changes for floatpostprocs start here */

/* how parameters are sent needs to be redone for gcc
 *
 * (and even gcc can been overriden for an arch due to optimization)
 *          - Stian    TODO TODO TODO TODO
 */
#warning this needs to be updated into more generic code
		"  movl postprocs_ofs(%%ebx), %%esi\n"

	"  mixer_PostprocLoop:\n"
		"    orl %%esi, %%esi\n"
		"    jz mixer_PostprocEnd\n"

		"    movl nsamples_ofs(%%ebx), %%edx\n"
		"    movl isstereo_ofs(%%ebx), %%ecx\n"
		"    movl tempbuf_ofs(%%ebx), %%eax\n"
		"    pushl %%ebx\n"
		"    movl samprate_ofs(%%ebx), %%ebx\n"
		"    call *%%esi\n"
		"    popl %%ebx\n"

		"    movl %c2(%%esi), %%esi\n"

		"  jmp mixer_PostprocLoop\n"

	"mixer_PostprocEnd:\n"

/* ryg990504 - changes for floatpostprocs end here */
		"  movl outbuf_ofs(%%ebx), %%edi\n"
		"  movl tempbuf_ofs(%%ebx), %%esi\n"
		"  movl nsamples_ofs(%%ebx), %%ecx\n"

		"  movl isstereo_ofs(%%ebx), %%edx\n"
		"  orl %%edx, %%edx\n"
		"  jz mixer_clipmono\n"
		"    addl %%ecx, %%ecx\n"
	"mixer_clipmono:\n"

		"  movl outfmt_ofs(%%ebx), %%eax\n"
#ifdef __PIC__
		"  movl 4(%%esp), %%ebx\n" /* put PIC in EBX */
		"  movl clippers@GOTOFF(%%ebx,%%eax,4), %%eax\n"
#else
		"  movl clippers(,%%eax,4), %%eax\n"
#endif

		"  call *%%eax\n"

	"mixer_endall:\n"
		"popl %%ebp\n"
#ifdef __PIC__
		"popl %%ebx\n"
		"popl %%ebx\n"
#endif
		:
		: "n"(MIXF_PLAYING),
		  "n"(minampl),
		  "n"(offsetof(struct mixfpostprocregstruct, next))
#ifdef __PIC__
		: "memory", "eax", "ecx", "edx", "edi", "esi"
#else
		: "memory", "eax", "ebx", "ecx", "edx", "edi", "esi"
#endif
	);
}

static __attribute__ ((used)) void dummy(void)
{
	__asm__ __volatile__
	(
		".cfi_endproc\n"
	);

/* clear routines:
 * edi : 32 bit float buffer
 * ecx : # of samples
 *
 * STACK
 *
 * +12 caller EBX if defined(__PIC__)
 * +8  PIC if defined(__PIC__)
 * +4  caller EBP
 * +0  return ptr
 */

/* clears and declicks tempbuffer (mono) */
	__asm__ __volatile__
	(
	".type clearbufm, @function\n"
	"clearbufm:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
#ifdef __PIC__
		"pushl %ebx\n"
		"movl 12(%esp), %ebx\n"
		"flds cremoveconst@GOTOFF(%ebx)\n"
		"popl %ebx\n"
#else
		"flds cremoveconst\n"         /* (fc) */
#endif
		"flds fadeleft_ofs(%ebx)\n"   /* (fl) (fc) */

	"clearbufm_clloop:\n"
		"  fsts (%edi)\n"
		"  fmul %st(1),%st\n"         /* (fl') (fc) */
		"  leal 4(%edi), %edi\n"
		"  decl %ecx\n"
		"jnz clearbufm_clloop\n"

		"fstps fadeleft_ofs(%ebx)\n"  /* (fc) */
		"fstp %st\n"                  /* - */

		"ret\n"
	".cfi_endproc\n"
	".size clearbufm, .-clearbufm\n"
	);


/* clears and declicks tempbuffer (stereo)
 * edi : 32 bit float buffer
 * ecx : # of samples
 */
	__asm__ __volatile__
	(
	".type clearbufs, @function\n"
	"clearbufs:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
#ifdef __PIC__
		"pushl %ebx\n"
		"movl 12(%esp), %ebx\n"
		"flds cremoveconst@GOTOFF(%ebx)\n"
		"popl %ebx\n"
#else
		"flds cremoveconst\n"          /* (fc) */
#endif
		"flds faderight_ofs(%ebx)\n"   /* (fr) (fc) */
		"flds fadeleft_ofs(%ebx)\n"    /* (fl) (fr) (fc) */

	"clearbufs_clloop:\n"
		"  fsts (%edi)\n"
		"  fmul %st(2), %st\n"         /* (fl') (fr) (fc) */
		"  fxch %st(1)\n"              /* (fr) (fl') (fc) */
		"  fsts 4(%edi)\n"
		"  fmul %st(2), %st\n"         /* (fr') (fl') (fc) */
		"  fxch %st(1)\n"              /* (fl') (fr') (fc) */
		"  leal 8(%edi),%edi\n"
		"  decl %ecx\n"
		"jnz clearbufs_clloop\n"

		"fstps fadeleft_ofs(%ebx)\n"   /* (fr') (fc) */
		"fstps faderight_ofs(%ebx)\n"  /* (fc) */
		"fstp %st\n"                   /* - */

		"ret\n"
	".cfi_endproc\n"
	".size clearbufs, .-clearbufs\n"
	);

/* STACK: +20 (optional) caller EBX (PIC)
          +16            caller EBP
          +12            freqf_ofs parameter (used to be ESI)
          +8             freqw_ofs parameter (used to be EBX)
          +4             saved ECX
          +0             caller return address
 * mixing routines:
 * eax = sample loop length.
 * ebx = dwmixfa_state, used to be delta to next sample (whole part)
 * ecx = # of samples to mix
 * edx = fraction of sample position
 * edi = dest ptr auf outbuffer
 * esi = PIC if in PIC mode (not used at the momement)
 * ebp = ptr to loop end
 */

	__asm__ __volatile__
	(
	".type mix_0, @function\n"
	"mix_0:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, MUTED
	 * quite sub-obtimal to do this with a loop, too, but this is really
	 * the only way to ensure maximum precision - and it's fully using
	 * the vast potential of the coder's lazyness.
	 */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
	"mix_0_next:\n"
		"  addl 16(%%esp), %%edx\n"
		"  adcl 12(%%esp), %%ebp\n"
	"mix_0_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  jae mix_0_LoopHandler\n"
		"  decl %%ecx\n"
		"jnz mix_0_next\n"
	"mix_0_ende:\n"
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"
	"mix_0_LoopHandler:\n"
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mix_0_loopme\n"
		/*"movl looptype_ofs(%%ebx), %%eax\n"*/
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mix_0_ende\n"
	"mix_0_loopme:\n"
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"jmp mix_0_looped\n"
	".cfi_endproc\n"
	".size mix_0, .-mix_0\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixm_n, @function\n"
	"mixm_n:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, mono w/o interpolation
	 */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
		"flds voll_ofs(%%ebx)\n"           /* (vl) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
	/* align dword we don't need.. alignment is 32bit by default on gnu i386*/
	"mixm_n_next:\n"                           /* (vl) */
		"  flds (,%%ebp,4)\n"              /* (wert) (vl) */
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  adcl 12(%%esp), %%ebp\n"
		"  fmulp %%st, %%st(1)\n"          /* (left) (vl) */
		"  fxch %%st(1)\n"                 /* (vl) (left) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (left) */
		"  fxch %%st(1)\n"                 /* (left) (vl) */
		"  fadds -4(%%edi)\n"              /* (lfinal) (vl') */
	"mixm_n_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  jae mixm_n_LoopHandler\n"
		"  fstps -4(%%edi)\n"              /* (vl') (-1) */
		"  decl %%ecx\n"
		"jnz mixm_n_next\n"
	"mixm_n_ende:\n"
		"fstps voll_ofs(%%ebx)\n"          /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixm_n_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_n_loopme\n"
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%esp), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) */
	"mixm_n_fill:\n" /*  sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) */
		"  fmul %%st(1), %%st\n"           /* (left) (wert) (vl) */
		"  fadds -4(%%edi)\n"              /* (wert) (vl) */
		"  fstps -4(%%edi)\n"              /* (wert) (vl) */
		"  fxch %%st(1)\n"                 /* (vl) (wert) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (wert) */
		"  fxch %%st(1)\n"                 /* (wert) (vl') */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_n_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"             /* (left) (vl) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (vl) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixm_n_ende\n"

	"mixm_n_loopme:\n" /* sample loops -> jump to loop start */
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_n_loopme\n"
		"decl %%ecx\n"
		"jz mixm_n_ende\n"
		"jmp mixm_n_next\n"
	".cfi_endproc\n"
	".size mixm_n, .-mixm_n\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixs_n, @function\n"
	"mixs_n:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, stereo w/o interpolation
	 */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
		"flds voll_ofs(%%ebx)\n"           /* (vl) */
		"flds volr_ofs(%%ebx)\n"           /* (vr) (vl) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
	/* align dword.... we are already align 32bit */
	"mixs_n_next:\n"
		"  flds (,%%ebp,4)\n"              /* (wert) (vr) (vl) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  adcl 12(%%esp), %%ebp\n"
		"  fld %%st(1)\n"                  /* (vr) (wert) (vr) (vl) */
		"  fld %%st(3)\n"                  /* (vl) (vr) (wert) (vr) (vl) */
		"  fmul %%st(2), %%st\n"           /* (left) (vr) (wert) (vr) (vl) */
		"  fxch %%st(4)\n"                 /* (vl)  (vr) (wert) (vr) (left) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr) (wert) (vr) (left) */
		"  fxch %%st(2)\n"                 /* (wert) (vr) (vl') (vr) (left) */
		"  fmulp %%st(1)\n"                /* (right) (vl') (vr) (left) */
		"  fxch %%st(2)\n"                 /* (vr) (vl') (right) (left) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl') (right) (left) */
		"  fxch %%st(3)\n"                 /* (left)  (vl') (right) (vr') */
		"  fadds -8(%%edi)\n"              /* (lfinal) (vl') <right> (vr') */
		"  fxch %%st(2)\n"                 /* (right) (vl') (lfinal) (vr') */
		"  fadds -4(%%edi)\n"              /* (rfinal) (vl') (lfinal) (vr') */
	"mixs_n_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  jae mixs_n_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"              /* (vl') (lfinal) (vr') */
		"  fxch %%st(1)\n"                 /* (lfinal) (vl) (vr) */
		"  fstps -8(%%edi)\n"              /* (vl) (vr) */
		"  fxch %%st(1)\n"                 /* (vr) (vl) */
		"  decl %%ecx\n"
		"jnz mixs_n_next\n"
	"mixs_n_ende:\n"
		"fstps volr_ofs(%%ebx)\n"          /* (vl) */
		"fstps voll_ofs(%%ebx)\n"          /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixs_n_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') (lfinal) (vr') */
		"fxch %%st(1)\n"                   /* (lfinal) (vl) (vr) */
		"fstps -8(%%edi)\n"                /* (vl) (vr) */
		"fxch %%st(1)\n"                   /* (vr) (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_n_loopme\n"
		"fxch %%st(1)\n"                   /* (vl) (vr) */
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%esp), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"                   /* (vr) (vl) (wert) */
	"mixs_n_fill:\n" /* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"           /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"                     /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"           /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"                 /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"
		"  fstps -8(%%edi)\n"              /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"              /* (vr) (vl) (wert) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"                 /* (vr') (vl') (wert) */
		"jnz mixs_n_fill\n"
	/* update click-removal fade values */
		"fxch %%st(2)\n"                   /* (wert) (vl) (vr) */
		"fld %%st\n"                       /* (wert) (wert) (vl) (vr) */
		"fmul %%st(2), %%st\n"             /* (left) (wert) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (wert) (left) (vl) (vr) */
		"fmul %%st(3), %%st\n"             /* (rite) (left) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (left) (rite) (vl) (vr) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (rite) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (rite) (fl') (vl) (vr) */
		"fadds faderight_ofs(%%ebx)\n"     /* (fr') (fl') (vl) (vr) */
		"fxch %%st(1)\n"                   /* (fl') (fr') (vl) (vr) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (fr') (vl) (vr) */
		"fstps faderight_ofs(%%ebx)\n"     /* (vl) (vr) */
		"fxch %%st(1)\n"                   /* (vr) (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixs_n_ende\n"

	"mixs_n_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_n_loopme\n"
		"decl %%ecx\n"
		"jz mixs_n_ende\n"
		"jmp mixs_n_next\n"
	".cfi_endproc\n"
	".size mixs_n, .-mixs_n\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixm_i, @function\n"
	"mixm_i:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, mono+interpolation */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
#if 1
		"fld1\n"                           /* (1) */
		"fchs\n"                           /* (-1) */
#else
		"flds minuseins\n"                 /* (-1) */
#endif
		"flds voll_ofs(%%ebx)\n"           /* (vl) (-1) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $9, %%eax\n"
		"shrl $2, %%ebp\n"
		"orl $0x3f800000, %%eax\n"
		"movl %%eax, magic1_ofs(%%ebx)\n"

	/* align dword... we don't need to align shit here? */
	"mixm_i_next:\n"                           /* (vl) (-1) */
		"  flds 0(,%%ebp,4)\n"             /* (a) (vl) (-1) */
		"  fld %%st(0)\n"                  /* (a) (a) (vl) (-1) */
		"  fld %%st(3)\n"                  /* (-1) (a) (a) (vl) (-1) */
		"  fadds magic1_ofs(%%ebx)\n"      /* (t) (a) (a) (vl) (-1) */
		"  fxch %%st(1)\n"                 /* (a) (t) (a) (vl) (-1) */
		"  fsubrs 4(,%%ebp,4)\n"           /* (b-a) (t) (a) (vl) (-1) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  adcl 12(%%esp), %%ebp\n"
		"  fmulp %%st(1)\n"                /* ((b-a)*t) (a) (vl) (-1) */
		"  movl %%edx, %%eax\n"
		"  shrl $9, %%eax\n"
		"  faddp %%st(1)\n"                /* (wert) (vl) (-1) */
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) (-1) */
		"  fmulp %%st, %%st(1)\n"          /* (left) (vl) (-1) */
		"  fxch %%st(1)\n"                 /* (vl) (left) (-1) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (left) (-1) */
		"  fxch %%st(1)\n"                 /* (left) (vl) (-1) */
		"  fadds -4(%%edi)\n"              /* (lfinal) (vl') (-1) */
		"  orl $0x3f800000, %%eax\n"
	"mixm_i_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  movl %%eax, magic1_ofs(%%ebx)\n"
		"  jae mixm_i_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"              /* (vl') (-1) */
		"  decl %%ecx\n"
		"jnz mixm_i_next\n"
	"mixm_i_ende:\n"
		"fstps voll_ofs(%%ebx)\n"          /* (whatever) */
		"fstp %%st\n"                      /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixm_i_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') (-1) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_i_loopme\n"
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%esp), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl)  (-1) */
	"mixm_i_fill:\n" /* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl)  (-1) */
		"  fmul %%st(1), %%st\n"           /* (left) (wert) (vl) (-1) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"              /* (wert) (vl) (-1) */
		"  fxch %%st(1)\n"                 /* (vl) (wert) (-1) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (wert) (-1) */
		"  fxch %%st(1)\n"                 /* (wert) (vl') (-1) */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_i_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"             /* (left) (vl) (-1) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (vl) (-1) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (vl) (-1) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixm_i_ende\n"

	"mixm_i_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_i_loopme\n"
		"decl %%ecx\n"
		"jz mixm_i_ende\n"
		"jmp mixm_i_next\n"
	".cfi_endproc\n"
	".size mixm_i, .-mixm_i\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixs_i, @function\n"
	"mixs_i:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, stereo+interpolation */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
#if 1
		"fld1\n"                           /* (1) */
		"fchs\n"                           /* (-1) */
#else
		"flds minuseins\n"                 /* (-1) */
#endif
		"flds voll_ofs(%%ebx)\n"           /* (vl) (-1) */
		"flds volr_ofs(%%ebx)\n"           /* (vr) (vl) (-1) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $9, %%eax\n"
		"shrl $2, %%ebp\n"
		"orl $0x3f800000, %%eax\n"
		"movl %%eax, magic1_ofs(%%ebx)\n"

	/* align dword... njet! */
	"mixs_i_next:\n"                           /* (vr) (vl) (-1) */
		"  flds 0(,%%ebp,4)\n"             /* (a) (vr) (vl) (-1) */
		"  fld %%st(0)\n"                  /* (a) (a) (vr) (vl) (-1) */
		"  fld %%st(4)\n"                  /* (-1) (a) (a) (vr) (vl) (-1) */
		"  fadds magic1_ofs(%%ebx)\n"      /* (t) (a) (a) (vr) (vl) (-1) */
		"  fxch %%st(1)\n"                 /* (a) (t) (a) (vr) (vl) (-1) */
		"  fsubrs 4(,%%ebp,4)\n"           /* (b-a) (t) (a) (vr) (vl) (-1) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  adcl 12(%%esp), %%ebp\n"
		"  fmulp %%st(1)\n"                /* ((b-a)*t) (a) (vr) (vl) (-1) */
		"  movl %%edx, %%eax\n"
		"  shrl $9, %%eax\n"
		"  faddp %%st(1)\n"                /* (wert) (vr) (vl) (-1) */
		"  fld %%st(1)\n"                  /* (vr) (wert) (vr) (vl) (-1) */
		"  fld %%st(3)\n"                  /* (vl) (vr) (wert) (vr) (vl) (-1) */
		"  fmul %%st(2), %%st\n"           /* (left) (vr) (wert) (vr) (vl) (-1) */
		"  fxch %%st(4)\n"                 /* (vl)  (vr) (wert) (vr) (left) (-1) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr) (wert) (vr) (left) (-1) */
		"  fxch %%st(2)\n"                 /* (wert) (vr) (vl') (vr) (left) (-1) */
		"  fmulp %%st(1)\n"                /* (right) (vl') (vr) (left) (-1) */
		"  fxch %%st(2)\n"                 /* (vr) (vl') (right) (left) (-1) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl') (right) (left) (-1) */
		"  fxch %%st(3)\n"                 /* (left)  (vl') (right) (vr') (-1) */
		"  fadds -8(%%edi)\n"              /* (lfinal) (vl') <right> (vr') (-1) */
		"  fxch %%st(2)\n"                 /* (right) (vl') (lfinal) (vr') (-1) */
		"  fadds -4(%%edi)\n"              /* (rfinal) (vl') (lfinal) (vr') (-1) */
		"  orl $0x3f800000, %%eax\n"
	"mixs_i_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  movl %%eax, magic1_ofs(%%ebx)\n"
		"  jae mixs_i_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"              /* (vl') (lfinal) <vr'> (-1) */
		"  fxch %%st(1)\n"                 /* (lfinal) (vl) (vr) (-1) */
		"  fstps -8(%%edi)\n"              /* (vl) (vr) (-1) */
		"  fxch %%st(1)\n"                 /* (vr) (vl) (-1) */
		"  decl %%ecx\n"
		"jnz mixs_i_next\n"
	"mixs_i_ende:\n"
		"fstps volr_ofs(%%ebx)\n"
		"fstps voll_ofs(%%ebx)\n"
		"fstp %%st\n"
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixs_i_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') (lfinal) <vr'> (-1) */
		"fxch %%st(1)\n"                   /* (lfinal) (vl) (vr) (-1) */
		"fstps -8(%%edi)\n"                /* (vl) (vr) (-1) */
		"fxch %%st(1)\n"                   /* (vr) (vl) (-1) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_i_loopme\n"
		"fxch %%st(2)\n"                   /* (-1) (vl) (vr) */
		"fstp %%st\n"                      /* (vl) (vr) */
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%esp), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"                   /* (vr) (vl) (wert) */
	"mixs_i_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"           /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"                     /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"           /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"                 /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"              /* (vr) (vl) (wert)            This should be (lfinal) (vr) (right) (vl) (wert) - stian */
		"  fstps -8(%%edi)\n"              /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"              /* (vr) (vl) (wert)            This should be (rfinal) (vr) (vl) (wert) - stian */
		"  fstps -4(%%edi)\n"              /* (vr) (vl) (wert) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"                 /* (vr') (vl') (wert) */
		"jnz mixs_i_fill\n"
	/* update click-removal fade values */
		"fld %%st(2)\n"                    /* (wert) (vr) (vl) (wert) */
		"fld %%st\n"                       /* (wert) (wert) (vr) (vl) (wert) */
		"fmul %%st(3), %%st\n"             /* (left) (wert) (vr) (vl) (wert) */
		"fxch %%st(1)\n"                   /* (wert) (left) (vr) (vl) (wert) */
		"fmul %%st(2), %%st\n"             /* (rite) (left) (vr) (vl) (wert) */
		"fxch %%st(1)\n"                   /* (left) (rite) (vr) (vl) (wert) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (rite) (vr) (vl) (wert) */
		"fxch %%st(1)\n"                   /* (rite) (fl') (vr) (vl) (wert) */
		"fadds faderight_ofs(%%ebx)\n"     /* (fr') (fl') (vr) (vl) (wert) */
		"fxch %%st(1)\n"                   /* (fl') (fr') (vr) (vl) (wert) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (fr') (vr) (vl) (wert) */
		"fstps faderight_ofs(%%ebx)\n"     /* (vr) (vl) (wert) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixs_i_ende\n"

	"mixs_i_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_i_loopme\n"
		"decl %%ecx\n"
		"jz mixs_i_ende\n"
		"jmp mixs_i_next\n"
	".cfi_endproc\n"
	".size mixs_i, .-mixs_i\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixm_i2, @function\n"
	"mixm_i2:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, mono w/ cubic interpolation */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
		"flds voll_ofs(%%ebx)\n"           /* (vl) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $24, %%eax\n"
	/* align dword we don't give a rats ass about */
	"mixm_i2_next:\n"                          /* (vl) */
		"  flds (,%%ebp,4)\n"              /* (w0) (vl) */
		"  fmuls ct0_ofs(%%ebx,%%eax,4)\n" /* (w0') (vl) */
		"  flds 4(,%%ebp,4)\n"             /* (w1) (w0') (vl) */
		"  fmuls ct1_ofs(%%ebx,%%eax,4)\n" /* (w1') (w0') (vl) */
		"  flds 8(,%%ebp,4)\n"             /* (w2) (w1') (w0') (vl) */
		"  fmuls ct2_ofs(%%ebx,%%eax,4)\n" /* (w2') (w1') (w0') (vl) */
		"  flds 12(,%%ebp,4)\n"            /* (w3) (w2') (w1') (w0') (vl) */
		"  fmuls ct3_ofs(%%ebx,%%eax,4)\n" /* (w3') (w2') (w1') (w0') (vl) */
		"  fxch %%st(2)\n"                 /* (w1') (w2') (w3') (w0') (vl) */
		"  faddp %%st, %%st(3)\n"          /* (w2') (w3') (w0+w1) (vl) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  faddp %%st, %%st(2)\n"          /* (w2+w3) (w0+w1) (vl) */ /* I find this to be wrong - Stian TODO  faddp %st %st(1) anybody ?  */
		"  adcl 12(%%esp), %%ebp\n"
		"  movl %%edx, %%eax\n"
		"  faddp %%st,%%st(1)\n"           /* (wert) (vl) */ /* But since we add them together here it all ends correct - Stian */
		"  shrl $24, %%eax\n"
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) */
		"  fmulp %%st, %%st(1)\n"          /* (left) (vl) */
		"  fxch %%st(1)\n"                 /* (vl) (left) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (left) */
		"  fxch %%st(1)\n"                 /* (left) (vl) */
		"  fadds -4(%%edi)\n"              /* (lfinal) (vl') */
	"mixm_i2_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  jae mixm_i2_LoopHandler\n"
		"  fstps -4(%%edi)\n"              /* (vl') */
		"  decl %%ecx\n"
		"jnz mixm_i2_next\n"
	"mixm_i2_ende:\n"
		"fstps voll_ofs(%%ebx)\n"          /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixm_i2_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') */
		"pushl %%eax\n"
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_i2_loopme\n"
		"popl %%eax\n"
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%esp), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) */
	"mixm_i2_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) */
		"  fmul %%st(1), %%st\n"           /* (left) (wert) (vl) */
		"  fadds -4(%%edi)\n"              /* (lfinal) (wert) (vl) */
		"  fstps -4(%%edi)\n"              /* (wert) (vl) */
		"  fxch %%st(1)\n"                 /* (vl) (wert) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (wert) */
		"  fxch %%st(1)\n"                 /* (wert) (vl') */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_i2_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"             /* (left) (vl) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (vl) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixm_i2_ende\n"

	"mixm_i2_loopme:\n"
	/* sample loops -> jump to loop start */
		"popl %%eax\n"
	"mixm_i2_loopme2:\n"
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_i2_loopme2\n"
		"decl %%ecx\n"
		"jz mixm_i2_ende\n"
		"jmp mixm_i2_next\n"
	".cfi_endproc\n"
	".size mixm_i2, .-mixm_i2\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixs_i2, @function\n"
	"mixs_i2:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, stereo w/ cubic interpolation */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
		"flds voll_ofs(%%ebx)\n"           /* (vl) */
		"flds volr_ofs(%%ebx)\n"           /* (vr) (vl) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $24, %%eax\n"
	/* align dword... see I care to do that */
	"mixs_i2_next:\n"
		"  flds (,%%ebp,4)\n"              /* (w0) (vr) (vl) */
		"  fmuls ct0_ofs(%%ebx,%%eax,4)\n" /* (w0') (vr) (vl) */
		"  flds 4(,%%ebp,4)\n"             /* (w1) (w0') (vr) (vl) */
		"  fmuls ct1_ofs(%%ebx,%%eax,4)\n" /* (w1') (w0') (vr) (vl) */
		"  flds 8(,%%ebp,4)\n"             /* (w2) (w1') (w0') (vr) (vl) */
		"  fmuls ct2_ofs(%%ebx,%%eax,4)\n" /* (w2') (w1') (w0') (vr) (vl) */
		"  flds 12(,%%ebp,4)\n"            /* (w3) (w2') (w1') (w0') (vr) (vl) */
		"  fmuls ct3_ofs(%%ebx,%%eax,4)\n" /* (w3') (w2') (w1') (w0') (vr) (vl) */
		"  fxch %%st(2)\n"                 /* (w1') (w2') (w3') (w0') (vr) (vl) */
		"  faddp %%st, %%st(3)\n"          /* (w2') (w3') (w0+w1) (vr) (vl) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  faddp %%st, %%st(2)\n"          /* (w2+w3) (w0+w1) (vr) (vl)     I find this comment to be wrong, be the next addp merges them all together - Stian*/
		"  adcl 12(%%esp), %%ebp\n"
		"  movl %%edx, %%eax\n"
		"  faddp %%st, %%st(1)\n"          /* wert) (vr) (vl) */
		"  shrl $24, %%eax\n"
		"  fld %%st(1)\n"                  /* (vr) (wert) (vr) (vl) */
		"  fld %%st(3)\n"                  /* (vl) (vr) (wert) (vr) (vl) */
		"  fmul %%st(2), %%st\n"           /* (left) (vr) (wert) (vr) (vl) */
		"  fxch %%st(4)\n"                 /* (vl)  (vr) (wert) (vr) (left) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr) (wert) (vr) (left) */
		"  fxch %%st(2)\n"                 /* (wert) (vr) (vl') (vr) (left) */
		"  fmulp %%st(1)\n"                /* (right) (vl') (vr) (left) */
		"  fxch %%st(2)\n"                 /* (vr) (vl') (right) (left) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl') (right) (left) */
		"  fxch %%st(3)\n"                 /* (left)  (vl') (right) (vr') */
		"  fadds -8(%%edi)\n"              /* (lfinal) (vl') <right> (vr') */
		"  fxch %%st(2)\n"                 /* (right) (vl') (lfinal) (vr') */
		"  fadds -4(%%edi)\n"              /* (rfinal) (vl') (lfinal) (vr') */
	"mixs_i2_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  jae mixs_i2_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"              /* (vl') (lfinal) (vr') */
		"  fxch %%st(1)\n"                 /* (lfinal) (vl) (vr)  */
		"  fstps -8(%%edi)\n"              /* (vl) (vr) */
		"  fxch %%st(1)\n"                 /* (vr) (vl) */
		"  decl %%ecx\n"
		"jnz mixs_i2_next\n"
	"mixs_i2_ende:\n"
		"fstps volr_ofs(%%ebx)\n"          /* (vl) */
		"fstps voll_ofs(%%ebx)\n"          /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixs_i2_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') (lfinal) (vr') */
		"fxch %%st(1)\n"                   /* (lfinal) (vl) (vr) */
		"fstps -8(%%edi)\n"                /* (vl) (vr) */
		"fxch %%st(1)\n"                   /* (vr) (vl) */
		"pushl %%eax\n"
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_i2_loopme\n"
		"popl %%eax\n"
		"fxch %%st(1)\n"                   /* (vl) (vr) */
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%esp), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"                   /* (vr) (vl) (wert) */
	"mixs_i2_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"           /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"                     /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"           /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"                 /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"
		"  fstps -8(%%edi)\n"              /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"              /* (vr) (vl) (wert) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"                 /* (vr') (vl') (wert) */
		"jnz mixs_i2_fill\n"
	/* update click-removal fade values */
		"fxch %%st(2)\n"                   /* (wert) (vl) (vr) */
		"fld %%st\n"                       /* (wert) (wert) (vl) (vr) */
		"fmul %%st(2), %%st\n"             /* (left) (wert) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (wert) (left) (vl) (vr) */
		"fmul %%st(3), %%st\n"             /* (rite) (left) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (left) (rite) (vl) (vr) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (rite) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (rite) (fl') (vl) (vr) */
		"fadds faderight_ofs(%%ebx)\n"     /* (fr') (fl') (vl) (vr) */
		"fxch %%st(1)\n"                   /* (fl') (fr') (vl) (vr) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (fr') (vl) (vr) */
		"fstps faderight_ofs(%%ebx)\n"     /* (vl) (vr) */
		"fxch %%st(1)\n"                   /* (vr) (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixs_i2_ende\n"

	"mixs_i2_loopme:\n"
	/* sample loops -> jump to loop start */
		"popl %%eax\n"
	"mixs_i2_loopme2:\n"
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_i2_loopme2\n"
		"decl %%ecx\n"
		"jz mixs_i2_ende\n"
		"jmp mixs_i2_next\n"
	".cfi_endproc\n"
	".size mixs_i2, .-mixs_i2\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixm_nf, @function\n"
	"mixm_nf:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, mono w/o interpolation, FILTERED */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
		"flds voll_ofs(%%ebx)\n"           /* (vl) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		/* align dword sucks */
	"mixm_nf_next:\n"                          /* (vl) */
		"  flds (,%%ebp,4)\n"              /* (wert) (vl) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1_ofs(%%ebx)\n"       /* (in-l) .. */
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*(in-l)) .. */
		"  flds __fb1_ofs(%%ebx)\n"        /* (b) (f*(in-l)) .. */
		"  fmuls frez_ofs(%%ebx)\n"        /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"          /* (b') .. */
		"  fsts __fb1_ofs(%%ebx)\n"
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*b') .. */
		"  fadds __fl1_ofs(%%ebx)\n"       /* (l') .. */
		"  fsts __fl1_ofs(%%ebx)\n"        /* (out) (vl) */

		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  adcl 12(%%esp), %%ebp\n"
		"  fmulp %%st, %%st(1)\n"          /* (left) (vl) */
		"  fxch %%st(1)\n"                 /* (vl) (left)  */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (left) */
		"  fxch %%st(1)\n"                 /* (left) (vl) */
		"  fadds -4(%%edi)\n"              /* (lfinal) (vl') */
	"mixm_nf_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  jae mixm_nf_LoopHandler\n"
		"  fstps -4(%%edi)\n"              /* (vl') */
		"  decl %%ecx\n"
		"jnz mixm_nf_next\n"
	"mixm_nf_ende:\n"
		"fstps voll_ofs(%%ebx)\n"          /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixm_nf_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_nf_loopme\n"
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%esp), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) */
	"mixm_nf_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) */
		"  fmul %%st(1), %%st\n"           /* (left) (wert) (vl) */
		"  fadds -4(%%edi)\n"              /* (lfinal) (wert) (vl) */
		"  fstps -4(%%edi)\n"              /* (wert) (vl) */
		"  fxch %%st(1)\n"                 /* (vl) (wert) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (wert) */
		"  fxch %%st(1)\n"                 /* (wert) (vl') */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_nf_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"             /* (left) (vl) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (vl) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixm_nf_ende\n"

	"mixm_nf_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_nf_loopme\n"
		"decl %%ecx\n"
		"jz mixm_nf_ende\n"
		"jmp mixm_nf_next\n"
	".cfi_endproc\n"
	".size mixm_nf, .-mixm_nf\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixs_nf, @function\n"
	"mixs_nf:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, stereo w/o interpolation, FILTERED */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
		"flds voll_ofs(%%ebx)\n"           /* (vl) */
		"flds volr_ofs(%%ebx)\n"           /* (vr) (vl) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
	/* align dword is for clows */
"mixs_nf_next:\n"
		"  flds (,%%ebp,4)\n"              /* (wert) (vr) (vl) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1_ofs(%%ebx)\n"       /* (in-l) .. */
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*(in-l)) .. */
		"  flds __fb1_ofs(%%ebx)\n"        /* (b) (f*(in-l)) .. */
		"  fmuls frez_ofs(%%ebx)\n"        /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"          /* (b') .. */
		"  fsts __fb1_ofs(%%ebx)\n"
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*b') .. */
		"  fadds __fl1_ofs(%%ebx)\n"       /* (l') .. */
		"  fsts __fl1_ofs(%%ebx)\n"        /* (out) (vr) (vl) */

		"  addl 16(%%esp), %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  adcl 12(%%esp), %%ebp\n"
		"  fld %%st(1)\n"                  /* (vr) (wert) (vr) (vl) */
		"  fld %%st(3)\n"                  /* (vl) (vr) (wert) (vr) (vl) */
		"  fmul %%st(2), %%st\n"           /* (left) (vr) (wert) (vr) (vl) */
		"  fxch %%st(4)\n"                 /* (vl)  (vr) (wert) (vr) (left) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr) (wert) (vr) (left) */
		"  fxch %%st(2)\n"                 /* (wert) (vr) (vl') (vr) (left) */
		"  fmulp %%st(1)\n"                /* (right) (vl') (vr) (left) */
		"  fxch %%st(2)\n"                 /* (vr) (vl') (right) (left) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl') (right) (left) */
		"  fxch %%st(3)\n"                 /* (left)  (vl') (right) (vr') */
		"  fadds -8(%%edi)\n"              /* (lfinal) (vl') <right> (vr') */
		"  fxch %%st(2)\n"                 /* (right) (vl') (lfinal) (vr') */
		"  fadds -4(%%edi)\n"              /* (rfinal) (vl') (lfinal) (vr') */
	"mixs_nf_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  jae mixs_nf_LoopHandler\n"
       /* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"              /* (vl') (lfinal) (vr') */
		"  fxch %%st(1)\n"                 /* (lfinal) (vl) (vr) */
		"  fstps -8(%%edi)\n"              /* (vl) (vr) */
		"  fxch %%st(1)\n"                 /* (vr) (vl) */
		"  decl %%ecx\n"
		"jnz mixs_nf_next\n"
	"mixs_nf_ende:\n"
		"fstps volr_ofs(%%ebx)\n"          /* (vl) */
		"fstps voll_ofs(%%ebx)\n"          /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixs_nf_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') (lfinal) (vr') */
		"fxch %%st(1)\n"                   /* (lfinal) (vl) (vr) */
		"fstps -8(%%edi)\n"                /* (vl) (vr) */
		"fxch %%st(1)\n"                   /* (vr) (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_nf_loopme\n"
		"fxch %%st(1)\n"                   /* (vl) (vr) */
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%esp), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"                   /* (vr) (vl) (wert) */
	"mixs_nf_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"           /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"                     /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"           /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"                 /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"
		"  fstps -8(%%edi)\n"              /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"              /* (vr) (vl) (wert) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"                 /* (vr') (vl') (wert) */
		"jnz mixs_nf_fill\n"
	/* update click-removal fade values */
		"fxch %%st(2)\n"                   /* (wert) (vl) (vr) */
		"fld %%st\n"                       /* (wert) (wert) (vl) (vr) */
		"fmul %%st(2), %%st\n"             /* (left) (wert) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (wert) (left) (vl) (vr) */
		"fmul %%st(3), %%st\n"             /* (rite) (left) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (left) (rite) (vl) (vr) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (rite) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (rite) (fl') (vl) (vr) */
		"fadds faderight_ofs(%%ebx)\n"     /* (fr') (fl') (vl) (vr) */
		"fxch %%st(1)\n"                   /* (fl') (fr') (vl) (vr) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (fr') (vl) (vr) */
		"fstps faderight_ofs(%%ebx)\n"     /* (vl) (vr) */
		"fxch %%st(1)\n"                   /* (vr) (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixs_nf_ende\n"

	"mixs_nf_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_nf_loopme\n"
		"decl %%ecx\n"
		"jz mixs_nf_ende\n"
		"jmp mixs_nf_next\n"
	".cfi_endproc\n"
	".size mixs_nf, .-mixs_nf\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixm_if, @function\n"
	"mixm_if:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, mono+interpolation, FILTERED */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
#if 1
		"fld1\n"                           /* (1) */
		"fchs\n"                           /* (-1) */
#else
		"flds minuseins\n"                 /* (-1) */
#endif
		"flds voll_ofs(%%ebx)\n"           /* (vl) (-1) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $9, %%eax\n"
		"shrl $2, %%ebp\n"
		"orl $0x3f800000, %%eax\n"
		"movl %%eax, magic1_ofs(%%ebx)\n"

	/* align dword is for pussies */
	"mixm_if_next:\n"                          /* (vl) (-1) */
		"  flds 0(,%%ebp,4)\n"             /* (a) (vl) (-1) */
		"  fld %%st(0)\n"                  /* (a) (a) (vl) (-1) */
		"  fld %%st(3)\n"                  /* (-1) (a) (a) (vl) (-1) */
		"  fadds magic1_ofs(%%ebx)\n"      /* (t) (a) (a) (vl) (-1) */
		"  fxch %%st(1)\n"                 /* (a) (t) (a) (vl) (-1) */
		"  fsubrs 4(,%%ebp,4)\n"           /* (b-a) (t) (a) (vl) (-1) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  adcl 12(%%ebx), %%ebp\n"
		"  fmulp %%st(1)\n"                /* ((b-a)*t) (a) (vl) (-1) */
		"  movl %%edx, %%eax\n"
		"  shrl $9, %%eax\n"
		"  faddp %%st(1)\n"                /* (wert) (vl) (-1) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1_ofs(%%ebx)\n"       /* (in-l) .. */
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*(in-l)) .. */
		"  flds __fb1_ofs(%%ebx)\n"        /* (b) (f*(in-l)) .. */
		"  fmuls frez_ofs(%%ebx)\n"        /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"          /* (b') .. */
		"  fsts __fb1_ofs(%%ebx)\n"
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*b') .. */
		"  fadds __fl1_ofs(%%ebx)\n"       /* (l') .. */
		"  fsts __fl1_ofs(%%ebx)\n"        /* (out) (vl) (-1) */

		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) (-1) */
		"  fmulp %%st, %%st(1)\n"          /* (left) (vl) (-1) */
		"  fxch %%st(1)\n"                 /* (vl) (left) (-1) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (left) (-1) */
		"  fxch %%st(1)\n"                 /* (left) (vl) (-1) */
		"  fadds -4(%%edi)\n"              /* (lfinal) (vl') (-1) */
		"  orl $0x3f800000, %%eax\n"
	"mixm_if_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  movl %%eax, magic1_ofs(%%ebx)\n"
		"  jae mixm_if_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"              /* (vl') (-1) */
		"  decl %%ecx\n"
		"jnz mixm_if_next\n"
	"mixm_if_ende:\n"
		"fstps voll_ofs(%%ebx)\n"          /* (whatever) */
		"fstp %%st\n"                      /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixm_if_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') (-1) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_if_loopme\n"
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%ebx), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl)  (-1) */
	"mixm_if_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl)  (-1) */
		"  fmul %%st(1), %%st\n"           /* (left) (wert) (vl) (-1) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"              /* (wert) (vl) (-1) */
		"  fxch %%st(1)\n"                 /* (vl) (wert) (-1) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (wert) (-1) */
		"  fxch %%st(1)\n"                 /* (wert) (vl') (-1) */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_if_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"             /* (left) (vl) (-1) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (vl) (-1) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (vl) (-1) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixm_if_ende\n"

	"mixm_if_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_if_loopme\n"
		"decl %%ecx\n"
		"jz mixm_if_ende\n"
		"jmp mixm_if_next\n"
	".cfi_endproc\n"
	".size mixm_if, .-mixm_if\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixs_if, @function\n"
	"mixs_if:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, stereo+interpolation, FILTERED */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
#if 1
		"fld1\n"                           /* (1) */
		"fchs\n"                           /* (-1) */
#else
		"flds minuseins\n"                 /* (-1) */
#endif
		"flds voll_ofs(%%ebx)\n"           /* (vl) (-1) */
		"flds volr_ofs(%%ebx)\n"           /* (vr) (vl) (-1) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $9, %%eax\n"
		"shrl $2, %%ebp\n"
		"orl $0x3f800000, %%eax\n"
		"movl %%eax, magic1_ofs(%%ebx)\n"

	/* align dword is boring */
	"mixs_if_next:\n"                          /* (vr) (vl) (-1) */
		"  flds 0(,%%ebp,4)\n"             /* (a) (vr) (vl) (-1) */
		"  fld %%st(0)\n"                  /* (a) (a) (vr) (vl) (-1) */
		"  fld %%st(4)\n"                  /* (-1) (a) (a) (vr) (vl) (-1) */
		"  fadds magic1_ofs(%%ebx)\n"      /* (t) (a) (a) (vr) (vl) (-1) */
		"  fxch %%st(1)\n"                 /* (a) (t) (a) (vr) (vl) (-1) */
		"  fsubrs 4(,%%ebp,4)\n"           /* (b-a) (t) (a) (vr) (vl) (-1) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  adcl 12(%%ebx), %%ebp\n"
		"  fmulp %%st(1)\n"                /* ((b-a)*t) (a) (vr) (vl) (-1) */
		"  movl %%edx, %%eax\n"
		"  shrl $9, %%eax\n"
		"  faddp %%st(1)\n"                /* (wert) (vr) (vl) (-1) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1_ofs(%%ebx)\n"       /* (in-l) .. */
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*(in-l)) .. */
		"  flds __fb1_ofs(%%ebx)\n"        /* (b) (f*(in-l)) .. */
		"  fmuls frez_ofs(%%ebx)\n"        /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"          /* (b') .. */
		"  fsts __fb1_ofs(%%ebx)\n"
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*b') .. */
		"  fadds __fl1_ofs(%%ebx)\n"       /* (l') .. */
		"  fsts __fl1_ofs(%%ebx)\n"        /* (out) (vr) (vl) */

		"  fld %%st(1)\n"                  /* (vr) (wert) (vr) (vl) (-1) */
		"  fld %%st(3)\n"                  /* (vl) (vr) (wert) (vr) (vl) (-1) */
		"  fmul %%st(2), %%st\n"           /* (left) (vr) (wert) (vr) (vl) (-1) */
		"  fxch %%st(4)\n"                 /* (vl)  (vr) (wert) (vr) (left) (-1) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr) (wert) (vr) (left) (-1) */
		"  fxch %%st(2)\n"                 /* (wert) (vr) (vl') (vr) (left) (-1) */
		"  fmulp %%st(1)\n"                /* (right) (vl') (vr) (left) (-1) */
		"  fxch %%st(2)\n"                 /* (vr) (vl') (right) (left) (-1) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl') (right) (left) (-1) */
		"  fxch %%st(3)\n"                 /* (left)  (vl') (right) (vr') (-1) */
		"  fadds -8(%%edi)\n"              /* (lfinal) (vl') <right> (vr') (-1) */
		"  fxch %%st(2)\n"                 /* (right) (vl') (lfinal) (vr') (-1) */
		"  fadds -4(%%edi)\n"              /* (rfinal) (vl') (lfinal) (vr') (-1) */
		"  orl $0x3f800000, %%eax\n"
	"mixs_if_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  movl %%eax, magic1_ofs(%%ebx)\n"
		"  jae mixs_if_LoopHandler;\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"              /* (vl') (lfinal) <vr'> (-1) */
		"  fxch %%st(1)\n"                 /* (lfinal) (vl) (vr) (-1) */
		"  fstps -8(%%edi)\n"              /* (vl) (vr) (-1) */
		"  fxch %%st(1)\n"                 /* (vr) (vl) (-1) */
		"  decl %%ecx\n"
		"jnz mixs_if_next\n"
	"mixs_if_ende:\n"
		"fstps volr_ofs(%%ebx)\n"
		"fstps voll_ofs(%%ebx)\n"
		"fstp %%st\n"
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixs_if_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') (lfinal) (vr') (-1) */
		"fxch %%st(1)\n"                   /* (lfinal) (vl) (vr) (-1) */
		"fstps -8(%%edi)\n"                /* (vl) (vr) (-1) */
		"fxch %%st(1)\n"                   /* (vr) (vl) (-1) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_if_loopme\n"
		"fxch %%st(2)\n"                   /* (-1) (vl) (vr) */
		"fstp %%st\n"                      /* (vl) (vr) */
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%ebx), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"                   /* (vr) (vl) (wert) */
	"mixs_if_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"           /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"                     /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"           /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"                 /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"              /* (vr) (vl) (wert) */
		"  fstps -8(%%edi)\n"              /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"              /* (vr) (vl) (wert) */
		"  fstps -4(%%edi)\n"              /* (vr) (vl) (wert) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"                 /* (vr') (vl') (wert) */
		"jnz mixs_if_fill\n"
	/* update click-removal fade values */
		"fld %%st(2)\n"                    /* (wert) (vr) (vl) (wert) */
		"fld %%st\n"                       /* (wert) (wert) (vr) (vl) (wert) */
		"fmul %%st(3), %%st\n"             /* (left) (wert) (vr) (vl) (wert) */
		"fxch %%st(1)\n"                   /* (wert) (left) (vr) (vl) (wert) */
		"fmul %%st(2), %%st\n"             /* (rite) (left) (vr) (vl) (wert) */
		"fxch %%st(1)\n"                   /* (left) (rite) (vr) (vl) (wert) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (rite) (vr) (vl) (wert) */
		"fxch %%st(1)\n"                   /* (rite) (fl') (vr) (vl) (wert) */
		"fadds faderight_ofs(%%ebx)\n"     /* (fr') (fl') (vr) (vl) (wert) */
		"fxch %%st(1)\n"                   /* (fl') (fr') (vr) (vl) (wert) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (fr') (vr) (vl) (wert) */
		"fstps faderight_ofs(%%ebx)\n"     /* (vr) (vl) (wert) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixs_if_ende\n"

	"mixs_if_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_if_loopme\n"
		"decl %%ecx\n"
		"jz mixs_if_ende\n"
		"jmp mixs_if_next\n"
	".cfi_endproc\n"
	".size mixs_if, .-mixs_if\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mim_i2f, @function\n"
	"mixm_i2f:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, mono w/ cubic interpolation, FILTERED */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
		"flds voll_ofs(%%ebx)\n"           /* (vl) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $24, %%eax\n"
	/* align dword... how many times have we ignored this now? */
	"mixm_i2f_next:\n"                         /* (vl) */
		"  flds (,%%ebp,4)\n"              /* (w0) (vl) */
		"  fmuls ct0_ofs(%%ebx,%%eax,4)\n" /* (w0') (vl) */
		"  flds 4(,%%ebp,4)\n"             /* (w1) (w0') (vl) */
		"  fmuls ct1_ofs(%%ebx,%%eax,4)\n" /* (w1') (w0') (vl) */
		"  flds 8(,%%ebp,4)\n"             /* (w2) (w1') (w0') (vl) */
		"  fmuls ct2_ofs(%%ebx,%%eax,4)\n" /* (w2') (w1') (w0') (vl) */
		"  flds 12(,%%ebp,4)\n"            /* (w3) (w2') (w1') (w0') (vl) */
		"  fmuls ct3_ofs(%%ebx,%%eax,4)\n" /* (w3') (w2') (w1') (w0') (vl) */
		"  fxch %%st(2)\n"                 /* (w1') (w2') (w3') (w0') (vl) */
		"  faddp %%st, %%st(3)\n"          /* (w2') (w3') (w0+w1) (vl) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  faddp %%st, %%st(2)\n"          /* (w2+w3) (w0+w1) (vl) */
		"  adcl 12(%%ebx), %%ebp\n"
		"  movl %%edx, %%eax\n"
		"  faddp %%st, %%st(1)\n"          /* (wert) (vl) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1_ofs(%%ebx)\n"       /* (in-l) .. */
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*(in-l)) .. */
		"  flds __fb1_ofs(%%ebx)\n"        /* (b) (f*(in-l)) .. */
		"  fmuls frez_ofs(%%ebx)\n"        /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"          /* (b') .. */
		"  fsts __fb1_ofs(%%ebx)\n"
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*b') .. */
		"  fadds __fl1_ofs(%%ebx)\n"       /* (l') .. */
		"  fsts __fl1_ofs(%%ebx)\n"        /* (out) (vr) (vl) */

		"  shrl $24, %%eax\n"
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) */
		"  fmulp %%st, %%st(1)\n"          /* (left) (vl) */
		"  fxch %%st(1)\n"                 /* (vl) (left) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (left) */
		"  fxch %%st(1)\n"                 /* (left) (vl) */
		"  fadds -4(%%edi)\n"              /* (lfinal) (vl') */
	"mixm_i2f_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  jae mixm_i2f_LoopHandler\n"
		"  fstps -4(%%edi)\n"              /* (vl') */
		"  decl %%ecx\n"
		"jnz mixm_i2f_next\n"
	"mixm_i2f_ende:\n"
		"fstps voll_ofs(%%ebx)\n"          /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixm_i2f_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') */
		"pushl %%eax\n"
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_i2f_loopme\n"
		"popl %%eax\n"
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%ebx), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) */
	"mixm_i2f_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (wert) (vl) */
		"  fmul %%st(1), %%st\n"           /* (left) (wert) (vl) */
		"  fadds -4(%%edi)\n"              /* (lfinal) (wert) (vl) */
		"  fstps -4(%%edi)\n"              /* (wert) (vl) */
		"  fxch %%st(1)\n"                 /* (vl) (wert) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (wert) */
		"  fxch %%st(1)\n"                 /* (wert) (vl') */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_i2f_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"             /* (left) (vl) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (vl) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixm_i2f_ende\n"

	"mixm_i2f_loopme:\n"
	/* sample loops -> jump to loop start */
		"popl %%eax\n"
	"mixm_i2f_loopme2:\n"
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_i2f_loopme2\n"
		"decl %%ecx\n"
		"jz mixm_i2f_ende\n"
		"jmp mixm_i2f_next\n"
	".cfi_endproc\n"
	".size mixm_i2f, .-mixm_i2f\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	".type mixs_i2f, @function\n"
	"mixs_i2f:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* mixing, stereo w/ cubic interpolation, FILTERED */
		"movl nsamples_ofs(%%ebx), %%ecx\n"
		"flds voll_ofs(%%ebx)\n"           /* (vl) */
		"flds volr_ofs(%%ebx)\n"           /* (vr) (vl) */
		"shrl $2, %%ebp\n"
		"pushl %%ebp\n"
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $24, %%eax\n"
	/* and again we ignore align dword */
	"mixs_i2f_next:\n"
		"  flds (,%%ebp,4)\n"              /* (w0) (vr) (vl) */
		"  fmuls ct0_ofs(%%ebx,%%eax,4)\n" /* (w0') (vr) (vl) */
		"  flds 4(,%%ebp,4)\n"             /* (w1) (w0') (vr) (vl) */
		"  fmuls ct1_ofs(%%ebx,%%eax,4)\n" /* (w1') (w0') (vr) (vl) */
		"  flds 8(,%%ebp,4)\n"             /* (w2) (w1') (w0') (vr) (vl) */
		"  fmuls ct2_ofs(%%ebx,%%eax,4)\n" /* (w2') (w1') (w0') (vr) (vl) */
		"  flds 12(,%%ebp,4)\n"            /* (w3) (w2') (w1') (w0') (vr) (vl) */
		"  fmuls ct3_ofs(%%ebx,%%eax,4)\n" /* (w3') (w2') (w1') (w0') (vr) (vl) */
		"  fxch %%st(2)\n"                 /* (w1') (w2') (w3') (w0') (vr) (vl) */
		"  faddp %%st, %%st(3)\n"          /* (w2') (w3') (w0+w1) (vr) (vl) */
		"  addl 16(%%esp), %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  faddp %%st, %%st(2)\n"          /* (w2+w3) (w0+w1) (vr) (vl) */
		"  adcl 12(%%ebx), %%ebp\n"
		"  movl %%edx, %%eax\n"
		"  faddp %%st, %%st(1)\n"          /* (wert) (vr) (vl) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1_ofs(%%ebx)\n"       /* (in-l) .. */
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*(in-l)) .. */
		"  flds __fb1_ofs(%%ebx)\n"        /* (b) (f*(in-l)) .. */
		"  fmuls frez_ofs(%%ebx)\n"        /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"          /* (b') .. */
		"  fsts __fb1_ofs(%%ebx)\n"
		"  fmuls ffrq_ofs(%%ebx)\n"        /* (f*b') .. */
		"  fadds __fl1_ofs(%%ebx)\n"       /* (l') .. */
		"  fsts __fl1_ofs(%%ebx)\n"        /* (out) (vr) (vl) */

		"  shrl $24, %%eax\n"
		"  fld %%st(1)\n"                  /* (vr) (wert) (vr) (vl) */
		"  fld %%st(3)\n"                  /* (vl) (vr) (wert) (vr) (vl) */
		"  fmul %%st(2), %%st\n"           /* (left) (vr) (wert) (vr) (vl) */
		"  fxch %%st(4)\n"                 /* (vl)  (vr) (wert) (vr) (left) */
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr) (wert) (vr) (left) */
		"  fxch %%st(2)\n"                 /* (wert) (vr) (vl') (vr) (left) */
		"  fmulp %%st(1)\n"                /* (right) (vl') (vr) (left) */
		"  fxch %%st(2)\n"                 /* (vr) (vl') (right) (left) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl') (right) (left) */
		"  fxch %%st(3)\n"                 /* (left)  (vl') (right) (vr') */
		"  fadds -8(%%edi)\n"              /* (lfinal) (vl') <right> (vr') */
		"  fxch %%st(2)\n"                 /* (right) (vl') (lfinal) (vr') */
		"  fadds -4(%%edi)\n"              /* (rfinal) (vl') (lfinal) (vr') */
	"mixs_i2f_looped:\n"
		"  cmpl (%%esp), %%ebp\n"
		"  jae mixs_i2f_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"              /* (vl') (lfinal) (vr') */
		"  fxch %%st(1)\n"                 /* (lfinal) (vl) (vr) */
		"  fstps -8(%%edi)\n"              /* (vl) (vr) */
		"  fxch %%st(1)\n"                 /* (vr) (vl) */
		"  decl %%ecx\n"
		"jnz mixs_i2f_next\n"
	"mixs_i2f_ende:\n"
		"fstps volr_ofs(%%ebx)\n"          /* (vl) */
		"fstps voll_ofs(%%ebx)\n"          /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
		"popl %%ecx\n" /* just a garbage register */
		"ret\n"

	"mixs_i2f_LoopHandler:\n"
		"fstps -4(%%edi)\n"                /* (vl') (lfinal) (vr') */
		"fxch %%st(1)\n"                   /* (lfinal) (vl) (vr) */
		"fstps -8(%%edi)\n"                /* (vl) (vr) */
		"fxch %%st(1)\n"                   /* (vr) (vl) */
		"pushl %%eax\n"
		"movl looptype_ofs(%%ebx), %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_i2f_loopme\n"
		"popl %%eax\n"
		"fxch %%st(1)\n"                   /* (vl) (vr) */
		"subl 16(%%esp), %%edx\n"
		"sbbl 12(%%ebx), %%ebp\n"
		"flds (,%%ebp,4)\n"                /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"                   /* (vr) (vl) (wert) */
	"mixs_i2f_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"                  /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"           /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"                     /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"           /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"                 /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"
		"  fstps -8(%%edi)\n"              /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"              /* (vr) (vl) (wert) */
		"  fadds volrr_ofs(%%ebx)\n"       /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"                 /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl_ofs(%%ebx)\n"       /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"                 /* (vr') (vl') (wert) */
		"jnz mixs_i2f_fill\n"
	/* update click-removal fade values */
		"fxch %%st(2)\n"                   /* (wert) (vl) (vr) */
		"fld %%st\n"                       /* (wert) (wert) (vl) (vr) */
		"fmul %%st(2), %%st\n"             /* (left) (wert) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (wert) (left) (vl) (vr) */
		"fmul %%st(3), %%st\n"             /* (rite) (left) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (left) (rite) (vl) (vr) */
		"fadds fadeleft_ofs(%%ebx)\n"      /* (fl') (rite) (vl) (vr) */
		"fxch %%st(1)\n"                   /* (rite) (fl') (vl) (vr) */
		"fadds faderight_ofs(%%ebx)\n"     /* (fr') (fl') (vl) (vr) */
		"fxch %%st(1)\n"                   /* (fl') (fr') (vl) (vr) */
		"fstps fadeleft_ofs(%%ebx)\n"      /* (fr') (vl) (vr) */
		"fstps faderight_ofs(%%ebx)\n"     /* (vl) (vr) */
		"fxch %%st(1)\n"                   /* (vr) (vl) */
		"movl looptype_ofs(%%ebx), %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype_ofs(%%ebx)\n"
		"jmp mixs_i2f_ende\n"

	"mixs_i2f_loopme:\n"
	/* sample loops -> jump to loop start */
		"popl %%eax\n"
	"mixs_i2f_loopme2:\n"
		"subl mixlooplen_ofs(%%ebx), %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_i2f_loopme2\n"
		"decl %%ecx\n"
		"jz mixs_i2f_ende\n"
		"jmp mixs_i2f_next\n"
	".cfi_endproc\n"
	".size mixs_i2f, .-mixs_i2f\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

/*
 * clip routines:
 * ebx is PIC, if in PIC mode
 * esi : 32 bit float input buffer
 * edi : 8/16 bit output buffer
 * ecx : # of samples to clamp (*2 if stereo!)
 */
	__asm__ __volatile__
	(
	".type clip_16s, @function\n"
	"clip_16s:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* convert/clip samples, 16bit signed */
#ifdef __PIC__
		"flds clampmin@GOTOFF(%ebx)\n"            /* (min) */
		"flds clampmax@GOTOFF(%ebx)\n"            /* (max) (min) */
		"movl dwmixfa_state@GOT(%ebx), %ebx\n"
#else
		"flds clampmin\n"            /* (min) */
		"flds clampmax\n"            /* (max) (min) */
#endif
		"movw $32767, %bp\n"
		"movw $-32768, %dx\n"

	"clip_16s_lp:\n"
		"  flds (%esi)\n"            /* (ls) (max) (min) */
		"  fcom %st(1)\n"
		"  fnstsw %ax\n"
		"  sahf\n"
		"  ja clip_16s_max\n"
		"  fcom %st(2)\n"
		"  fstsw %ax\n"
		"  sahf\n"
		"  jb clip_16s_min\n"
		"  fistps (%edi)\n"          /* (max) (min) fi*s is 16bit */
	"clip_16s_next:\n"
		"  addl $4, %esi\n"
		"  addl $2, %edi\n"
		"  decl %ecx\n"
		"jnz clip_16s_lp\n"
		"jmp clip_16s_ende\n"

	"clip_16s_max:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %bp, (%edi)\n"
		"jmp clip_16s_next\n"
	"clip_16s_min:\n"
		"fstp %st\n"
		"movw %dx, (%edi)\n"
		"jmp clip_16s_next\n"

	"clip_16s_ende:\n"
		"fstp %st\n"                 /* (min) */
		"fstp %st\n"                 /* - */
		"ret\n"
	".cfi_endproc\n"
	".size clip_16s, .-clip_16s\n"
	);

	__asm__ __volatile__
	(
	".type clip_16u, @function\n"
	"clip_16u:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* convert/clip samples, 16bit unsigned */
#ifdef __PIC__
		"flds clampmin@GOTOFF(%ebx)\n"            /* (min) */
		"flds clampmax@GOTOFF(%ebx)\n"            /* (max) (min) */
		"movl dwmixfa_state@GOT(%ebx), %ebx\n"
#else
		"flds clampmin\n"            /* (min) */
		"flds clampmax\n"            /* (max) (min) */
#endif
		"movw $32767, %bp\n"
		"movw $-32768, %dx\n"

	"clip_16u_lp:\n"
		"  flds (%esi)\n"            /* (ls) (max) (min) */
		"  fcom %st(1)\n"
		"  fnstsw %ax\n"
		"  sahf\n"
		"  ja clip_16u_max\n"
		"  fcom %st(2)\n"
		"  fstsw %ax\n"
		"  sahf\n"
		"  jb clip_16u_min\n"
		"  fistps clipval_ofs(%ebx)\n"         /* (max) (min) */
		"  movw clipval_ofs(%ebx), %ax\n"
	"clip_16u_next:\n"
		"  xorw $0x8000, %ax\n"
		"  movw %ax, (%edi)\n"
		"  addl $4, %esi\n"
		"  addl $2, %edi\n"
		"  decl %ecx\n"
		"jnz clip_16u_lp\n"
		"jmp clip_16u_ende\n"

	"clip_16u_max:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %bp, %ax\n"
		"jmp clip_16u_next\n"
	"clip_16u_min:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %dx, %ax\n"
		"jmp clip_16u_next\n"

	"clip_16u_ende:\n"
		"fstp %st\n"                 /* (min) */
		"fstp %st\n"                 /* - */
		"ret\n"
	".cfi_endproc\n"
	".size clip_16u, .-clip_16u\n"
	);

	__asm__ __volatile__
	(
	".type clip_8s, @function\n"
	"clip_8s:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* convert/clip samples, 8bit signed */
#ifdef __PIC__
		"flds clampmin@GOTOFF(%ebx)\n"            /* (min) */
		"flds clampmax@GOTOFF(%ebx)\n"            /* (max) (min) */
		"movl dwmixfa_state@GOT(%ebx), %ebx\n"
#else
		"flds clampmin\n"            /* (min) */
		"flds clampmax\n"            /* (max) (min) */
#endif
		"movw $32767, %bp\n"
		"movw $-32768, %dx\n"

	"clip_8s_lp:\n"
		"  flds (%esi)\n"            /* (ls) (max) (min) */
		"  fcom %st(1)\n"
		"  fnstsw %ax\n"
		"  sahf\n"
		"  ja clip_8s_max\n"
		"  fcom %st(2)\n"
		"  fstsw %ax\n"
		"  sahf\n"
		"  jb clip_8s_min\n"
		"  fistps clipval_ofs(%ebx)\n"         /* (max) (min) */
		"  movw clipval_ofs(%ebx), %ax\n"
	"clip_8s_next:\n"
		"  movb %ah, (%edi)\n"
		"  addl $4, %esi\n"
		"  addl $1, %edi\n"
		"  decl %ecx\n"
		"jnz clip_8s_lp\n"
		"jmp clip_8s_ende\n"

	"clip_8s_max:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %bp, %ax\n"
		"jmp clip_8s_next\n"
	"clip_8s_min:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %dx, %ax\n"
		"jmp clip_8s_next\n"

	"clip_8s_ende:\n"
		"fstp %st\n"                 /* (min) */
		"fstp %st\n"                 /* - */
		"ret\n"
	".cfi_endproc\n"
	".size clip_8s, .-clip_8s\n"
	);

	__asm__ __volatile__
	(
	".type clip_8u, @function\n"
	"clip_8u:\n"
	".cfi_startproc\n"
	".cfi_def_cfa_offset 4\n"
	/* convert/clip samples, 8bit unsigned */
#ifdef __PIC__
		"flds clampmin@GOTOFF(%ebx)\n"            /* (min) */
		"flds clampmax@GOTOFF(%ebx)\n"            /* (max) (min) */
		"movl dwmixfa_state@GOT(%ebx), %ebx\n"
#else
		"flds clampmin\n"            /* (min) */
		"flds clampmax\n"            /* (max) (min) */
#endif
		"movw $32767, %bp\n"
		"movw $-32767, %dx\n"

	"clip_8u_lp:\n"
		"  flds (%esi)\n"            /* (ls) (max) (min) */
		"  fcom %st(1)\n"
		"  fnstsw %ax\n"
		"  sahf\n"
		"  ja clip_8u_max\n"
		"  fcom %st(2)\n"
		"  fstsw %ax\n"
		"  sahf\n"
		"  jb clip_8u_min\n"
		"  fistps clipval_ofs(%ebx)\n"         /* (max) (min) */
		"  movw clipval_ofs(%ebx), %ax\n"
	"clip_8u_next:\n"
		"  xorw $0x8000, %ax\n"
		"  movb %ah, (%edi)\n"
		"  addl $4, %esi\n"
		"  addl $1, %edi\n"
		"  decl %ecx\n"
		"jnz clip_8u_lp\n"
		"jmp clip_8u_ende\n"

	"clip_8u_max:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %bp, %ax\n"
		"jmp clip_8u_next\n"
	"clip_8u_min:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %dx, %ax\n"
		"jmp clip_8u_next\n"

	"clip_8u_ende:\n"
		"fstp %st\n"                 /* (min) */
		"fstp %st\n"                 /* - */
		"ret\n"
	".cfi_endproc\n"
	".size clip_8u, .-clip_8u\n"
	);

	__asm__ __volatile__
	(
	".cfi_startproc\n"
	".data\n"
	"clippers:\n"
		".long clip_8s, clip_8u, clip_16s, clip_16u\n"

	"mixers:\n"
	/* bit 0 stereo/mono output  (this needs to move when we add support for stereo input samples)
         * bit 1 interpolate
         * bit 2 interpolateq
         * bit 3 FILTER
         */
		".long mixm_n, mixs_n, mixm_i, mixs_i\n"     /* 0,0,0,0  0,0,0,1  0,0,1,0  0,0,1,1 */
		".long mixm_i2, mixs_i2, mix_0, mix_0\n"     /* 0,1,0,0  0,1,0,1  0,1,1,0  0,1,1,1 */
		".long mixm_n, mixs_nf, mixm_if, mixs_if\n"  /* 1,0,0,0  1,0,0,1  1,0,1,0  1,0,1,1 */
		".long mixm_i2f, mixs_i2f, mix_0, mix_0\n"   /* 1,1,0,0  1,1,0,1  1,1,1,0  1,1,1,1 */
	".previous\n"
	);
}

void getchanvol (int n, int len)
{
	int d0;
	__asm__ __volatile__
	(
	/* parm
	 * eax : channel #
	 * ecx : no of samples
	 */
#ifdef __PIC__
		"pushl %%ebx\n"
		"call " GET_PC_THUNK_STR(bx) "\n"
		"addl $_GLOBAL_OFFSET_TABLE_, %%ebx\n"
		"movl dwmixfa_state@GOT(%%ebx), %%ebx\n"
#else
		"movl $dwmixfa_state, %%ebx\n"
#endif
		"pushl %%ebp\n"
		"fldz\n"                     /* (0) */

		"movl voiceflags_ofs(%%ebx,%%eax,4), %%esi\n"
		"testl %3, %%esi\n"
		"jz getchanvol_SkipVoice\n"
		"movl freqf_ofs(%%ebx,%%eax,4), %%esi\n"
		"movl smpposf_ofs(%%ebx,%%eax,4), %%edx\n"
		"movl loopend_ofs(%%ebx,%%eax,4), %%edi\n"
		"shrl $2, %%edi\n"
		"movl smpposw_ofs(%%ebx,%%eax,4), %%ebp\n"
		"shrl $2, %%ebp\n"

		"movl %%ecx, nsamples_ofs(%%ebx)\n"

	"getchanvol_next:\n"                 /* (sum) */
		"  flds (,%%ebp,4)\n"        /* (wert) (sum) */
		"  testl $0x80000000, (,%%ebp,4)\n"
		"  jnz getchanvol_neg\n"
		"  faddp %%st, %%st(1)\n"    /* (sum') */
		"  jmp getchanvol_goon\n"
	"getchanvol_neg:\n"
		"  fsubp %%st, %%st(1)\n"
	"getchanvol_goon:\n"
		"  addl %%esi, %%edx\n"
		"  adcl freqw_ofs(%%ebx,%%eax,4), %%ebp\n" /* used to use ebx instead of memory reference, but we now we need the EBX register */
	"getchanvol_looped:\n"
		"  cmpl %%edi, %%ebp\n"
		"  jae getchanvol_LoopHandler\n"
		"  decl %%ecx\n"
		"jnz getchanvol_next\n"
		"jmp getchanvol_SkipVoice\n"
	"getchanvol_LoopHandler:\n"
		"testl %2, voiceflags_ofs(%%ebx,%%eax,4)\n"
		"jz getchanvol_SkipVoice\n"
		"subl looplen_ofs(%%ebx,%%eax,4), %%ebp\n"
		"jmp getchanvol_looped\n"
	"getchanvol_SkipVoice:\n"
		"fidivl nsamples_ofs(%%ebx)\n"          /* (sum'') */
		"fld %%st(0)\n"                         /* (s) (s) */
		"fmuls volleft_ofs(%%ebx,%%eax,4)\n"    /* (sl) (s) */
		"fstps voll_ofs(%%ebx)\n"               /* (s) */
		"fmuls volright_ofs(%%ebx,%%eax,4)\n"   /* (sr) */
		"fstps volr_ofs(%%ebx)\n"               /* - */

		"popl %%ebp\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&c" (d0)
		: "a" (n),
		  "n" (MIXF_LOOPED),
		  "n" (MIXF_PLAYING),
		  "0" (len)
#ifdef __PIC__
		: "edx", "edi", "esi"
#else
		: "ebx", "edx", "edi", "esi"
#endif
	);
}

void stop_dwmixfa(void)
{
}

