/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Mixer asm routines for display etc
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -ss04????   Stian Skjelstad <stian@nixia.no>
 *    -ported to gcc assembler
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -made assembler optimizesafe
 */

#include "config.h"
#include <unistd.h>
#include "types.h"
#include "mix.h"
#include "mixasm.h"

#ifdef I386_ASM

#ifndef NULL
 #define NULL ((void *)0)
#endif

static void __attribute__ ((used)) dummy55 (void)
{
#ifdef __PIC__
__asm__ __volatile__
(
	".local interpolate\n"
	".comm  interpolate,4,4\n"
	".local play16bit\n"
	".comm  play16bit,4,4\n"
	".local playfloat\n"
	".comm  playfloat,4,4\n"
	".local render16bit\n"
	".comm  render16bit,4,4\n"
	".local voltabs\n"
	".comm  voltabs,8,8\n"
	".comm  mixIntrpolTab,4,4\n"
	".comm  mixIntrpolTab2,4,4\n"
	".local playrout\n"
	".comm playrout,4,4\n"
	".local inloop\n"
	".comm inloop,4,4\n"
);
#endif
__asm__ __volatile__
(
	".equ MIX_PINGPONGLOOP, %0\n"
	".equ MIX_PLAYING, %1\n"
	".equ MIX_LOOPED, %2\n"
	".equ MIX_PLAYFLOAT, %3\n"
	".equ MIX_PLAY16BIT, %4\n"
	".equ MIX_MAX, %5\n"
	".equ MIX_INTERPOLATE, %6\n"
	: :
	  "m" (*(char *)(MIX_PINGPONGLOOP))/*MCP_PINGPONGLOOP*/,        /*   0  */
	  "m" (*(char *)(MIX_PLAYING))/*(MCP_PLAYING)*/,                /*   1  */
	  "m" (*(char *)(MIX_LOOPED))/*(MCP_LOOPED)*/,                  /*   2  */
	  "m" (*(char *)(MIX_PLAYFLOAT))/*(MCP_PLAYFLOAT)*/,            /*   3  */
	  "m" (*(char *)(MIX_PLAY16BIT))/*(MCP_PLAY16BIT)*/,            /*   4  */
	  "m" (*(char *)(MIX_MAX))/*(MCP_MAX)*/,                        /*   5  */
	  "m" (*(char *)(MIX_INTERPOLATE))/*(MCP_INTERPOLATE)*/         /*   6  */
	  );
}
#ifndef __PIC__
int8_t (*mixIntrpolTab)[256][2];
int16_t (*mixIntrpolTab2)[256][2];
static __attribute__((used)) int interpolate=0;
static __attribute__((used)) int play16bit=0;
static __attribute__((used)) int playfloat=0;
static __attribute__((used)) int render16bit=0;
static __attribute__((used)) uint32_t *voltabs[2]={0,0};
static __attribute__((used)) void (*playrout)(const int32_t *buf, struct mixchannel *ch, int len);
static __attribute__((used)) int inloop;
#endif

void mixasm_remap_start(void){}

void mixPlayChannel(int32_t *buf, uint32_t len, struct mixchannel *ch, int stereo)
{
	int d0;
#ifdef __PIC__
	int ebx;
#endif
	__asm__ __volatile__
	(
#ifdef __PIC__
		" movl %%ebx, %14\n"
#endif
		"  movl $0, interpolate\n"
		"  movl $0, play16bit\n"
		"  movl $0, playfloat\n"
		"  movl $0, render16bit\n"

		"  testb $MIX_PLAYING, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"  jz pcexit\n"
		"  testb $MIX_INTERPOLATE, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"  jz mixPlayChannel_nointr\n"
		"     movl $1, interpolate\n"
		"     testb $MIX_MAX, %8(%%edi)\n"/*  %8 = ch->status */
		"     jz mixPlayChannel_nointr\n"
		"       movl $1, render16bit\n"
		"mixPlayChannel_nointr:\n"
		"  testb $MIX_PLAY16BIT, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"  jz mixPlayChannel_no16bit\n"
		"    movl $1, play16bit\n"
		"mixPlayChannel_no16bit:\n"
		"  testb $MIX_PLAYFLOAT, %8(%%edi)\n"
		                                  /*  %8= ch->status */
		"  jz mixPlayChannel_no32bit\n"
		"    movl $1, playfloat\n"
		"mixPlayChannel_no32bit:\n"

		"  cmpb $0, %1\n"                 /*  %1 = stereo */
		"  jne mixPlayChannel_pstereo\n"

		"mixPlayChannel_pmono:\n"
		"  movl %12(%%edi), %%eax\n"      /* %12 = ch->vol.voltabs[0] */
		"  movl %%eax, voltabs+0\n"
		"  movl $playmono, playrout\n"
		"  jmp mixPlayChannel_bigloop\n"

		"mixPlayChannel_pstereo:\n"
		"  movl %12(%%edi), %%eax\n"      /* %12 = ch->vol.voltabs[0] */
		"  movl %13(%%edi), %%ebx\n"      /* %13 = ch->vol.voltabs[1] */
		"  movl %%eax, voltabs\n"
		"  movl %%ebx, voltabs+4\n"
		"  movl $playodd, playrout\n"

		"mixPlayChannel_bigloop:\n"
		"  movl %2, %%ecx\n"              /*  %2 = len */
		"  movl %11(%%edi), %%ebx\n"      /* %11 = ch->step */
		"  movl %9(%%edi), %%edx\n"       /*  %9 = ch->pos */
		"  movw %10(%%edi), %%si\n"       /* %10 = ch->->fpos */
		"  movl $0, inloop\n"
		"  cmpl $0, %%ebx\n"

		"  je mixPlayChannel_playecx\n"
		"  jg mixPlayChannel_forward\n"
		"    negl %%ebx\n"
		"    movl %%edx, %%eax\n"
		"    testb $MIX_LOOPED, %8(%%edi)\n"
		                                  /*  %8 = ch->->status */
		"    jz mixPlayChannel_maxplaylen\n"
		"    cmpl %7(%%edi), %%edx\n"     /*  %7 = ch->loopstart */
		"    jb mixPlayChannel_maxplaylen\n"
		"    subl %7(%%edi), %%eax\n"     /*  %7 = ch->loopstart */
		"    movl $1, inloop\n"
		"    jmp mixPlayChannel_maxplaylen\n"
		"mixPlayChannel_forward:\n"
		"    movl %4(%%edi), %%eax\n"     /*  %4 = ch->length */
		"    negw %%si\n"
		"    sbbl %%edx, %%eax\n"
		"    testb $MIX_LOOPED, %8(%%edi)\n"
		                                  /*  %8 = ch->->status */
		"    jz mixPlayChannel_maxplaylen\n"
		"    cmpl %5(%%edi), %%edx\n"     /*  %5 = ch->loopend */
		"    jae mixPlayChannel_maxplaylen\n"
		"    subl %4(%%edi), %%eax\n"     /*  %4 = ch->length */
		"    addl %5(%%edi), %%eax\n"     /*  %5 = ch->loopend */
		"    movl $1, inloop\n"

		"mixPlayChannel_maxplaylen:\n"
		"  xorl %%edx, %%edx\n"
		"  shld $16, %%eax, %%edx\n"
		"  shll $16, %%esi\n"
		"  shld $16, %%esi, %%eax\n"
		"  addl %%ebx, %%eax\n"
		"  adcl $0, %%edx\n"
		"  subl $1, %%eax\n"
		"  sbbl $0, %%edx\n"
		"  cmpl %%ebx, %%edx\n"
		"  jae mixPlayChannel_playecx\n"
		"  divl %%ebx\n"
		"  cmpl %%eax, %%ecx\n"
		"  jb mixPlayChannel_playecx\n"
		"    movl %%eax, %%ecx\n"
		"    cmpl $0, inloop\n"
		"    jnz mixPlayChannel_playecx\n"
		"      andb $254, %8(%%edi)\n"    /*** 254 = MCP_PLAYING^255   no parameters left  ***
		                                   *  %8 = ch->status */
		"      movl %%ecx, %2\n"          /*  %2 = len */

		"mixPlayChannel_playecx:\n"
		"  pushl %%ebp\n"
		"  pushl %%edi\n"
		"  pushl %%ecx\n"
		"  movl %%ecx, %%eax\n"
		"  movl %3, %%ecx\n"              /*  %3 = buf */
		"  movl %%eax, %%ebp\n"

		"  call *playrout\n"

		"  popl %%ecx\n"
		"  popl %%edi\n"
		"  popl %%ebp\n"

		"  movl %%ecx, %%eax\n"
		"  shll $2, %%eax\n"
		"  cmpl $0, %1\n"                 /*  %1 = stereo */
		"  je mixPlayChannel_m2\n"
		"  shll $1, %%eax\n"
		"mixPlayChannel_m2:\n"
		"  addl %%eax, %3\n"              /*  %3 = buf */
		"  subl %%ecx, %2\n"              /*  %2 = len */

		"  movl %11(%%edi), %%eax\n"      /* %11 = ch->step */
		"  imul %%ecx\n"
		"  shld $16, %%eax, %%edx\n"
		"  addw %%ax, %10(%%edi)\n"       /* %10 = ch->->fpos */
		"  adcl %%edx, %9(%%edi)\n"       /*  %9 = ch->pos */
		"  movl %9(%%edi), %%eax\n"       /*  %9 = ch->pos */

		"  cmpl $0, inloop\n"
		"  jz pcexit\n"

		"  cmpl $0, %11(%%edi)\n"         /* %11 = ch->step */
		"  jge mixPlayChannel_forward2\n"
		"    cmpl %7(%%edi), %%eax\n"     /*  %7 = ch->->loopstart */
		"    jge pcexit\n"
		"    testb $MIX_PINGPONGLOOP, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"    jnz mixPlayChannel_pong\n"
		"      addl %6(%%edi), %%eax\n"   /*  %6 = ch->replen */
		"      jmp mixPlayChannel_loopiflen\n"
		"mixPlayChannel_pong:\n"
		"      negl %11(%%edi)\n"         /* %11 = ch->step */
		"      negw %10(%%edi)\n"         /* %10 = ch->->fpos */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %7(%%edi), %%eax\n"   /*  %7 = ch->->loopstart */
		"      addl %7(%%edi), %%eax\n"   /*  %7 = ch->->loopstart */
		"      jmp mixPlayChannel_loopiflen\n"
		"mixPlayChannel_forward2:\n"
		"    cmpl %5(%%edi), %%eax\n"     /*  %5 = ch->->loopend */
		"    jb pcexit\n"
		"    testb $MIX_PINGPONGLOOP, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"    jnz mixPlayChannel_ping\n"
		"      sub %6(%%edi), %%eax\n"    /*  %6 = ch->replen */
		"      jmp mixPlayChannel_loopiflen\n"
		"mixPlayChannel_ping:\n"
		"      negl %11(%%edi)\n"         /* %11 = ch->step */
		"      negw %10(%%edi)\n"         /* %10 = ch->->fpos */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %5(%%edi), %%eax\n"   /*  %5 = ch->->loopend */
		"      addl %5(%%edi), %%eax\n"   /*  %5 = ch->->loopend */

		"mixPlayChannel_loopiflen:\n"
		"  movl %%eax, %9(%%edi)\n"       /*  %9 = ch->pos */
		"  cmpl $0, %2\n"                 /*  %2 = len */
		"  jne mixPlayChannel_bigloop\n"

		"pcexit:\n"
#ifdef __PIC__
		"  movl %14, %%ebx\n"
#endif
		: "=&D" (d0)                                         /*   0  */
		: "m" (stereo),                                      /*   1  */
		  "m" (len),                                         /*   2  */
		  "m" (buf),                                         /*   3  */
		  "m" (((struct mixchannel *)NULL)->length),         /*   4  */
		  "m" (((struct mixchannel *)NULL)->loopend),        /*   5  */
		  "m" (((struct mixchannel *)NULL)->replen),         /*   6  */
		  "m" (((struct mixchannel *)NULL)->loopstart),      /*   7  */
		  "m" (((struct mixchannel *)NULL)->status),         /*   8  */
		  "m" (((struct mixchannel *)NULL)->pos),            /*   9  */
		  "m" (((struct mixchannel *)NULL)->fpos),           /*  10  */
		  "m" (((struct mixchannel *)NULL)->step),           /*  11  */
		  "m" (((struct mixchannel *)NULL)->vol.voltabs[0]), /*  12  */
		  "m" (((struct mixchannel *)NULL)->vol.voltabs[1]), /*  13  */
#ifdef __PIC__
		  "m" ((ebx)),                                       /*  14  */
#endif
		  "0" (ch)
		: "memory", "eax", "ecx", "edx", "esi"
	);
}

static void __attribute__ ((used)) dummy54 (void)
{
	__asm__ __volatile__
	(
		 "playmono:\n"
		 "  cmpl $0, interpolate\n"
		 "  jnz playmonoi\n"

		 "  cmpl $0, play16bit\n"
		 "  jnz playmono16\n"

		 "  cmpl $0, playfloat\n"
		 "  jnz playmono32\n"

		 "  cmp $0, %%ebp\n"
		 "  je done1\n"

		 "  movl voltabs+0, %%eax\n"
		 "  movl %%eax, (vol11-4)\n"
		 "  movl %0(%%edi), %%eax\n"      /*  %0 = ch->step */
		 "  shll $16, %%eax\n"
		 "  movl %%eax, (edx1-4)\n"
		 "  movl %0(%%edi), %%eax\n"      /*  %0 = ch->step */
		 "  sarl $16, %%eax\n"
		 "  movl %%eax, (ebp1-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp1:\n"
		"    movb (%%esi), %%bl\n"
		"    addl $1234, %%edx\n"
		"edx1:\n"
		"    movl 1234(,%%ebx,4), %%eax\n"
		"vol11:\n"
		"    adcl $1234, %%esi\n"
		"ebp1:\n"
		"    addl %%eax, (%%edi)\n"
		"    addl $4, %%edi\n"
		"  decl %%ebp\n"
		"  jnz lp1\n"

		"done1:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /* 0 */
		  "m" (((struct mixchannel *)NULL)->fpos), /* 1 */
		  "m" (((struct mixchannel *)NULL)->pos),  /* 2 */
		  "m" (((struct mixchannel *)NULL)->samp)  /* 3 */
	);

	__asm__ __volatile__
	(
		"playmono16:"
		"  cmp $0, %%ebp\n"
		"  je done2\n"

		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol2-4)\n"
		"  movl %1(%%edi), %%eax\n"       /*  %1 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx2-4)\n"
		"  movl %1(%%edi), %%eax\n"       /*  %1 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp2-4)\n"

		"  movw %2(%%edi), %%dx\n"        /*  %2 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %3(%%edi), %%esi\n"       /*  %3 = ch->pos */
		"  addl %0(%%edi), %%esi\n"       /*  %0 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp2:\n"
		"    movb 1(%%esi,%%esi), %%bl\n"
		"    addl $1234, %%edx\n"
		"edx2:\n"
		"    movl 1234(,%%ebx,4), %%eax\n"
		"vol2:\n"
		"    adcl $1234, %%esi\n"
		"ebp2:\n"
		"    addl %%eax, (%%edi)\n"
		"    addl $4, %%edi\n"
		"  decl %%ebp\n"
		"  jnz lp2\n"

		"done2:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->samp), /*  0  */
		  "m" (((struct mixchannel *)NULL)->step), /*  1  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  2  */
		  "m" (((struct mixchannel *)NULL)->pos)   /*  3  */
	);

	  __asm__ __volatile__
	(
		"playmono32:"
		"  cmp $0, %%ebp\n"
		"  je done3\n"

		"  flds %0(%%edi)\n"              /*  %0 = ch->vol.voltabs[0] */
		"  fmuls gscale\n"
		"  fstps scale\n"

		"  movl %1(%%edi), %%eax\n"       /*  %1 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx3-4)\n"
		"  movl %1(%%edi), %%eax\n"       /*  %1 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp3-4)\n"

		"  movw %2(%%edi), %%dx\n"        /*  %2 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %3(%%edi), %%esi\n"       /*  %3 = ch->pos */
		"  addl %4(%%edi), %%esi\n"       /*  %4 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp3:\n"
		"    flds (,%%esi,4)\n"
		"    fmuls scale\n"
		"    fistpl integer\n"
		"    movl integer, %%eax\n"

		"    addl $1234, %%edx\n"
		"edx3:\n"
		"    adcl $1234, %%esi\n"
		"ebp3:\n"
		"    addl %%eax, (%%edi)\n"
		"    addl $4, %%edi\n"
		"  decl %%ebp\n"
		"  jnz lp3\n"

		"done3:\n"
		"  ret\n"

		".data\n"
		"integer: .long 0\n"
		"scale: .float 0.0\n"
		"gscale: .float 64.0\n"
		".previous\n"
		:
		: "m" (((struct mixchannel *)NULL)->vol.voltabs[0]), /*  0  */
		  "m" (((struct mixchannel *)NULL)->step),           /*  1  */
		  "m" (((struct mixchannel *)NULL)->fpos),           /*  2  */
		  "m" (((struct mixchannel *)NULL)->pos),            /*  3  */
		  "m" (((struct mixchannel *)NULL)->samp)            /*  4  */
	);

	__asm__ __volatile__
	(
		"playmonoi:\n"
		"  cmpl $0, render16bit\n"
		"  jnz playmonoir\n"

		"  cmpl $0, play16bit\n"
		"  jnz playmonoi16\n"

		"  cmpl $0, playfloat\n"
		"  jnz playmono32\n"   /* !! */

		"  cmpl $0, %%ebp\n"
		"  je done4\n"

		"  movl mixIntrpolTab, %%eax\n"
		"  movl %%eax, (int04-4)\n"
		"  incl %%eax\n"
		"  movl %%eax, (int14-4)\n"
		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol14-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx4-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp4-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp4:\n"
		"    movl %%edx, %%ecx\n"
		"    shrl $20, %%ecx\n"
		"    movb (%%esi), %%cl\n"
		"    movb 1234(%%ecx,%%ecx), %%bl\n"
		"int04:\n"
		"    movb 1(%%esi), %%cl\n"
		"    addb 1234(%%ecx,%%ecx), %%bl\n"
		"int14:\n"

		"    addl $1234, %%edx\n"
		"edx4:\n"
		"    movl 1234(,%%ebx,4), %%eax\n"
		"vol14:\n"
		"    adcl $1234, %%esi\n"
		"ebp4:\n"
		"    addl %%eax, (%%edi)\n"
		"    addl $4, %%edi\n"
		"  decl %%ebp\n"
		"  jnz lp4\n"

		"done4:\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */
	);

	__asm__ __volatile__
	(
		"playmonoi16:\n"
		"   cmpl $0, %%ebp\n"
		"   je done5\n"

		"  movl mixIntrpolTab, %%eax\n"
		"  movl %%eax, (int05-4)\n"
		"  incl %%eax\n"
		"  movl %%eax, (int15-4)\n"
		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol15-4)\n"
		"  movl %2(%%edi), %%eax\n"
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx5-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp5-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp5:\n"
		"    movl %%edx, %%ecx\n"
		"    shrl $20, %%ecx\n"
		"    movb 1(%%esi,%%esi), %%cl\n"
		"    movb 1234(%%ecx,%%ecx), %%bl\n"
		"int05:\n"
		"    movb 3(%%esi, %%esi), %%cl\n"
		"    addb 1234(%%ecx,%%ecx), %%bl\n"
		"int15:\n"

		"    addl $1234, %%edx\n"
		"edx5:\n"
		"    movl 1234(,%%ebx,4), %%eax\n"
		"vol15:\n"
		"    adcl $1234, %%esi\n"
		"ebp5:\n"
		"    addl %%eax, (%%edi)\n"
		"    addl $4, %%edi\n"
		"  dec %%ebp\n"
		"  jnz lp5\n"

		"done5:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */
	);

	__asm__ __volatile__
	(
		"playmonoir:\n"
		"  cmpl $0, play16bit\n"
		"  jne playmonoi16r\n"

		"  cmpl $0, playfloat\n"
		"  jne playmono32\n" /* !! */

		"  cmpl $0, %%ebp\n"
		"  je done6\n"

		"  movl mixIntrpolTab2, %%eax\n"
		"  movl %%eax, (int06-4)\n"
		"  addl $2, %%eax\n"
		"  movl %%eax, (int16-4)\n"

		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol16-4)\n"
		"  addl $1024, %%eax\n"
		"  movl %%eax, (vol26-4)\n"

		"  movl %0(%%edi),%%eax\n"        /*  %0 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx6-4)\n"
		"  movl %0(%%edi),%%eax\n"        /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp6-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp6:\n"
		"    movl %%edx, %%ecx\n"
		"    shrl $19, %%ecx\n"
		"    movb (%%esi), %%cl\n"
		"    movl 1234(,%%ecx,4), %%ebx\n"
		"int06:\n"
		"    movb 1(%%esi), %%cl\n"
		"    addl 1234(,%%ecx,4), %%ebx\n"
		"int16:\n"
		"    movzbl %%bh, %%ecx\n"
		"    movzbl %%bl, %%ebx\n"

		"    addl $1234, %%edx\n"
		"edx6:\n"
		"    movl 1234(,%%ecx,4), %%eax\n"
		"vol16:\n"
		"    adcl $1234, %%esi\n"
		"ebp6:\n"
		"    addl 1234(,%%ebx,4), %%eax\n"
		"vol26:\n"
		"    addl %%eax, (%%edi)\n"
		"    addl $4, %%edi\n"
		"  dec %%ebp\n"
		"  jnz lp6\n"

		"done6:\n"
		"  ret\n"

		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */
	);

	__asm__ __volatile__
	(
		"playmonoi16r:\n"
		"  cmpl $0, %%ebp\n"
		"  je done7\n"

		"  movl mixIntrpolTab2, %%eax\n"
		"  movl %%eax, (int07-4)\n"
		"  addl $2, %%eax\n"
		"  movl %%eax, (int17-4)\n"

		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol17-4)\n"
		"  addl $1024, %%eax\n"
		"  movl %%eax, (vol27-4)\n"

		"  movl %0(%%edi),%%eax\n"        /*  %0 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx7-4)\n"
		"  movl %0(%%edi),%%eax\n"        /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp7-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch ->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp7:\n"
		"    movl %%edx, %%ecx\n"
		"    shrl $19, %%ecx\n"
		"    movb 1(%%esi,%%esi), %%cl\n"
		"    movl 1234(,%%ecx,4), %%ebx\n"
		"int07:\n"
		"    movb 3(%%esi,%%esi), %%cl\n"
		"    addl 1234(,%%ecx,4), %%ebx\n"
		"int17:\n"
		"    movzbl %%bh, %%ecx\n"
		"    movzbl %%bl, %%ebx\n"

		"    addl $1234, %%edx\n"
		"edx7:\n"
		"    movl 1234(,%%ecx,4), %%eax\n"
		"vol17:\n"
		"    adcl $1234, %%esi\n"
		"ebp7:\n"
		"    addl 1234(,%%ebx,4), %%eax\n"
		"vol27:\n"
		"    addl %%eax, (%%edi)\n"
		"    addl $4, %%edi\n"
		"  decl %%ebp\n"
		"  jnz lp7\n"

		"done7:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */
	);

	__asm__ __volatile__
	(
		"playodd:\n"
		"  cmpl $0, interpolate\n"
		"  jnz playoddi\n"

		"  cmpl $0, play16bit\n"
		"  jnz playodd16\n"

		"  cmpl $0, playfloat\n"
		"  jnz done8\n" /* !! */

		"  cmpl $0, %%ebp\n"
		"  je done8\n"

		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol18-4)\n"
		"  movl voltabs+4, %%eax\n"
		"  movl %%eax, (vol28-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx8-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp8-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi),%%esi\n"        /*  %2 = ch->pos */
		"  addl %3(%%edi),%%esi\n"        /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp8:\n"
		"  movb (%%esi), %%bl\n"
		"  addl $1234, %%edx\n"
		"edx8:\n"
		"  movw 1234(,%%ebx,4), %%ax\n"
		"vol18:\n"
		"  adcl $1234, %%esi\n"
		"ebp8:\n"
		"  addl %%eax, (%%edi)\n"
		"  movw 1234(,%%ebx,4), %%ax\n"
		"vol28:\n"
		"  addl %%eax, 4(%%edi)\n"
		"  addl $8, %%edi\n"
		"  decl %%ebp\n"
		"  jnz lp8\n"

		"done8:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */
	);

	__asm__ __volatile__
	(
		"playodd16:\n"
		"  cmpl $0, %%ebp\n"
		"  je done9\n"

		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol19-4)\n"
		"  movl voltabs+4, %%eax\n"
		"  movl %%eax, (vol29-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx9-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp9-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp9:\n"
		"  movb 1(%%esi,%%esi), %%bl\n"
		"  addl $1234, %%edx\n"
		"edx9:\n"
		"  movl 1234(,%%ebx,4), %%eax\n"
		"vol19:\n"
		"  adcl $1234, %%esi\n"
		"ebp9:\n"
		"  addl %%eax, (%%edi)\n"
		"  movw 1234(,%%ebx,4), %%ax\n"
		"vol29:\n"
		"  addl %%eax, 4(%%edi)\n"
		"  addl $8, %%edi\n"
		"  decl %%ebp\n"
		"  jnz lp9\n"

		"done9:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */

	);

	__asm__ __volatile__
	(
		"playoddi:\n"
		"  cmpl $0, render16bit\n"
		"  jnz playoddir\n"

		"  cmpl $0, play16bit\n"
		"  jnz playoddi16\n"

		"  cmpl $0, playfloat\n"
		"  jnz done10\n"  /* !! */

		"  cmpl $0, %%ebp\n"
		"  je done10\n"

		"  movl mixIntrpolTab, %%eax\n"
		"  movl %%eax, (int010-4)\n"
		"  incl %%eax\n"
		"  movl %%eax, (int110-4)\n"
		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol110-4)\n"
		"  movl voltabs+4, %%eax\n"
		"  movl %%eax, (vol210-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx10-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp10-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp10:\n"
		"  movl %%edx, %%ecx\n"
		"  shrl $20, %%ecx\n"
		"  movb (%%esi), %%cl\n"
		"  movb 1234(%%ecx,%%ecx),%%bl\n"
		"int010:\n"
		"  movb 1(%%esi), %%cl\n"
		"  addb 1234(%%ecx,%%ecx),%%bl\n"
		"int110:\n"

		"  addl $1234, %%edx\n"
		"edx10:\n"
		"  movl 1234(,%%ebx,4), %%eax\n"
		"vol110:\n"
		"  adc $1234, %%esi\n"
		"ebp10:\n"
		"  addl %%eax, (%%edi)\n"
		"  movl 1234(,%%ebx,4), %%eax\n"
		"vol210:\n"
		"  addl %%eax, 4(%%edi)\n"
		"  addl $8, %%edi\n"
		"  decl %%ebp\n"
		"  jnz lp10\n"

		"done10:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */
	);

	__asm__ __volatile__
	(
		"playoddi16:\n"
		"  cmpl $0, %%ebp\n"
		"  je done11\n"

		"  movl mixIntrpolTab, %%eax\n"
		"  movl %%eax, (int011-4)\n"
		"  incl %%eax\n"
		"  movl %%eax, (int111-4)\n"
		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol111-4)\n"
		"  movl voltabs+4, %%eax\n"
		"  movl %%eax, (vol211-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx11-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp11-4)\n"

		"  movw %1(%%edi),%%dx\n"         /*  %1 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp11:\n"
		"  movl %%edx, %%ecx\n"
		"  shrl $20, %%ecx\n"
		"  movb 1(%%esi,%%esi), %%cl\n"
		"  movb 1234(%%ecx, %%ecx), %%bl\n"
		"int011:\n"
		"  movb 3(%%esi,%%esi), %%cl\n"
		"  addb 1234(%%ecx, %%ecx), %%bl\n"
		"int111:\n"

		"  addl $1234, %%edx\n"
		"edx11:\n"
		"  movl 1234(,%%ebx,4), %%eax\n"
		"vol111:\n"
		"  adcl $1234, %%esi\n"
		"ebp11:\n"
		"  addl %%eax, (%%edi)\n"
		"  movl 1234(,%%ebx,4), %%eax\n"
		"vol211:\n"
		"  addl %%eax, 4(%%edi)\n"
		"  addl $8, %%edi\n"
		"  decl %%ebp\n"
		"  jnz lp11\n"

		"done11:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */
	);

	__asm__ __volatile__
	(
		"playoddir:"
		"  cmpl $0, play16bit\n"
		"  jne playoddi16r\n"

		"  cmpl $0, playfloat\n"
		"  jne done12\n"

		"  cmpl $0, %%ebp\n"
		"  je done12\n"

		"  movl mixIntrpolTab2, %%eax\n"
		"  movl %%eax, (int012-4)\n"
		"  addl $2, %%eax\n"
		"  movl %%eax, (int112-4)\n"
		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol112-4)\n"
		"  addl $1024, %%eax\n"
		"  movl %%eax, (vol1b12-4)\n"
		"  movl voltabs+4, %%eax\n"
		"  movl %%eax, (vol212-4)\n"
		"  addl $1024, %%eax\n"
		"  movl %%eax, (vol2b12-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx12-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp12-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->->fpos */
		"  shll $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ecx, %%ebx\n"

		"lp12:"
		"  movl %%edx, %%ecx\n"
		"  shrl $19, %%ecx\n"
		"  movb (%%esi), %%cl\n"
		"  movl 1234(,%%ecx, 4), %%ebx\n"
		"int012:\n"
		"  movb 1(%%esi), %%cl\n"
		"  addl 1234(,%%ecx,4), %%ebx\n"
		"int112:\n"
		"  movzbl %%bh, %%ecx\n"
		"  movzbl %%bl, %%ebx\n"

		"  addl $1234, %%edx\n"
		"edx12:\n"
		"  movl 1234(,%%ecx,4), %%eax\n"
		"vol112:\n"
		"  adcl $1234, %%esi\n"
		"ebp12:\n"
		"  addl 1234(,%%ebx,4),%%eax\n"
		"vol1b12:\n"
		"  addl %%eax, (%%edi)\n"
		"  movl 1234(,%%ecx, 4), %%eax\n"
		"vol212:\n"
		"  addl 1234(,%%ebx,4), %%eax\n"
		"vol2b12:\n"
		"  addl %%eax, 4(%%edi)\n"
		"  addl $8, %%edi\n"
		"  decl %%ebp\n"
		"  jne lp12\n"

		"done12:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0  */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */
	);

	__asm__ __volatile__
	(
		"playoddi16r:\n"
		"  cmp $0, %%ebp\n"
		"  je done13\n"

		"  movl mixIntrpolTab2, %%eax\n"
		"  movl %%eax, (int013-4)\n"
		"  addl $2, %%eax\n"
		"  movl %%eax, (int113-4)\n"
		"  movl voltabs+0, %%eax\n"
		"  movl %%eax, (vol113-4)\n"
		"  addl $1024, %%eax\n"
		"  movl %%eax, (vol1b13-4)\n"
		"  movl voltabs+4, %%eax\n"
		"  movl %%eax, (vol213-4)\n"
		"  addl $1024, %%eax\n"
		"  movl %%eax, (vol2b13-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (edx13-4)\n"
		"  movl %0(%%edi), %%eax\n"       /*  %0 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (ebp13-4)\n"

		"  movw %1(%%edi), %%dx\n"        /*  %1 = ch->fpos */
		"  shl $16, %%edx\n"
		"  movl %2(%%edi), %%esi\n"       /*  %2 = ch->pos */
		"  addl %3(%%edi), %%esi\n"       /*  %3 = ch->samp */
		"  movl %%ecx, %%edi\n"
		"  xorl %%ebx, %%ebx\n"

		"lp13:\n"
		"  movl %%edx, %%ecx\n"
		"  shrl $19, %%ecx\n"
		"  movb 1(%%esi,%%esi), %%cl\n"
		"  movl 1234(,%%ecx,4), %%ebx\n"
		"int013:\n"
		"  movb 3(%%esi,%%esi), %%cl\n"
		"  add 1234(,%%ecx,4), %%ebx\n"
		"int113:\n"
		"  movzbl %%bh, %%ecx\n"
		"  movzbl %%bl, %%ebx\n"

		"  addl $1234, %%edx\n"
		"edx13:\n"
		"  movl 1234(,%%ecx,4), %%eax\n"
		"vol113:\n"
		"  adcl $1234, %%esi\n"
		"ebp13:\n"
		"  addl 1234(,%%ecx,4), %%eax\n"
		"vol1b13:\n"
		"  add %%eax, (%%edi)\n"
		"  movl 1234(,%%ecx,4), %%eax\n"
		"vol213:\n"
		"  addl 1234(,%%ecx,4), %%eax\n"
		"vol2b13:\n"
		"  add %%eax, 4(%%edi)\n"
		"  add $8, %%edi\n"
		"  dec %%ebp\n"
		"  jnz lp13\n"

		"done13:\n"
		"  ret\n"
		:
		: "m" (((struct mixchannel *)NULL)->step), /*  0 */
		  "m" (((struct mixchannel *)NULL)->fpos), /*  1  */
		  "m" (((struct mixchannel *)NULL)->pos),  /*  2  */
		  "m" (((struct mixchannel *)NULL)->samp)  /*  3  */
	);

}

uint32_t mixAddAbs(const struct mixchannel *ch, uint32_t len)
{
	int d0, d1;
#ifdef __PIC__
	int ebx;
#endif
	register unsigned long retval;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"movl %%ebx, %10\n"
#endif
		"  testb $MIX_PLAY16BIT, %3(%%eax)\n"
		                                  /*  %3 = ch->status */
		"  jnz mixAddAbs_16bit\n"
		"  testb $MIX_PLAYFLOAT, %3(%%eax)\n"
		                                  /*  %3 = ch->status */
		"  jnz mixAddAbs_32bit\n"

		"  movl %4(%%eax), %%edx\n"       /*  %4 = ch->replen */
		"  movl %5(%%eax), %%esi\n"       /*  %5 = ch->pos */
		"  addl %6(%%eax), %%esi\n"       /*  %6 = ch->samp */
		"  movl %7(%%eax), %%ebx\n"       /*  %7 = ch->length */
		"  addl %6(%%eax), %%ebx\n"       /*  %6 = ch->samp */
/* eax = ch
 * edi = target len
 *
 * edx = replen
 * esi = current pos in sample (effecient pointer)
 * ebx = end (effecient pointer)
 */
		"  addl %%esi, %%edi\n"
/* edi = ETA end (effecient pointer)
 *
 * eax = ch
 * edx = replen
 * esi = current pos in sample (effecient pointer)
 * ebx = end (effecient pointer)
 */
		"  xorl %%ecx, %%ecx\n"
/* ecx/retval = 0
 *
 * edi = ETA end (effecient pointer)
 * ebx = end (effecient pointer)
 * eax = ch
 * edx = replen
 * esi = current pos in sample (effecient pointer)
 */
		"mixAddAbs_bigloop:\n"

		"    pushl %%edi\n"
/* store edi for later */

		"    cmpl %%ebx, %%edi\n"
		"    ja mixAddAbs_less\n"
/* do we hit end before eta? */
		"      xorl %%edx, %%edx\n"


/* remove replen if so*/
		"    jmp mixAddAbs_loop\n"
		"  mixAddAbs_less:\n"
		"    movl %%ebx, %%edi\n"
/* set eta to end (so we don't go past end) */
		"  jmp mixAddAbs_loop\n"

/* Entering loop:
 *
 * edx = 0 if ETA > end, else edx = replen
 * edi = ETA (pushed up the end if underrun), efficent pointer

 * ecx/retval = 0
 * ebx = end (effecient pointer)
 * eax = ch
 * esi = current pos in sample (effecient pointer)
 */
		"  mixAddAbs_neg:\n"
		"    subl %%eax, %%ecx\n"
		"    cmpl %%edi, %%esi\n"
		"    jae mixAddAbs_loopend\n"
		"  mixAddAbs_loop:\n"
		"      movsbl (%%esi), %%eax\n"
		"      incl %%esi\n"
		"      xorl $0xffffff80, %%eax\n"
		"      js mixAddAbs_neg\n"
		"      addl %%eax, %%ecx\n"
		"    cmpl %%edi, %%esi\n"
		"    jb mixAddAbs_loop\n"
		"  mixAddAbs_loopend:\n"
		"    popl %%edi\n"
		"    cmpl $0, %%edx\n"
		"    je mixAddAbs_exit\n"
		"    subl %%edx, %%edi\n"
		"    subl %%edx, %%esi\n"
		"  jmp mixAddAbs_bigloop\n"

		"mixAddAbs_16bit:\n"
		"  movl %4(%%eax), %%edx\n"       /*  %4 = ch->replen */
		"  movl %5(%%eax), %%esi\n"       /*  %5 = ch->pos */
		"  addl %6(%%eax), %%esi\n"       /*  %6 = ch->samp */
		"  movl %7(%%eax), %%ebx\n"       /*  %7 = ch->length */
		"  addl %6(%%eax), %%ebx\n"       /*  %6 = ch->samp */
		"  addl %%esi, %%edi\n"
		"  xorl %%ecx, %%ecx\n"
		"mixAddAbs_16bigloop:\n"
		"    pushl %%edi\n"
		"    cmpl %%ebx, %%edi\n"
		"    ja mixAddAbs_16less\n"
		"      xorl %%edx, %%edx\n"
		"    jmp mixAddAbs_16loop\n"
		"  mixAddAbs_16less:\n"
		"    movl %%ebx, %%edi\n"
		"  jmp mixAddAbs_16loop\n"

		"  mixAddAbs_16neg:\n"
		"      subl %%eax, %%ecx\n"
		"    cmpl %%edi, %%esi\n"
		"    jae mixAddAbs_16loopend\n"
		"  mixAddAbs_16loop:\n"
		"      movsbl 1(%%esi,%%esi), %%eax\n"
		"      incl %%esi\n"
		"      xorl $0xffffff80, %%eax\n"
		"      js mixAddAbs_16neg\n"
		"      addl %%eax, %%ecx\n"
		"    cmpl %%edi, %%esi\n"
		"    jb mixAddAbs_16loop\n"
		"  mixAddAbs_16loopend:\n"
		"    popl %%edi\n"
		"    cmpl $0, %%edx\n"
		"    je mixAddAbs_exit\n"
		"    subl %%edx, %%edi\n"
		"    subl %%edx, %%esi\n"
		"  jmp mixAddAbs_16bigloop\n"

		"mixAddAbs_32bit:\n"
		"  movl %4(%%eax), %%edx\n"       /*  %4 = ch->replen*/
		"  movl %5(%%eax), %%esi\n"       /*  %5 = ch->pos */
		"  addl %6(%%eax), %%esi\n"       /*  %6 = ch->samp */
		"  movl %7(%%eax), %%ebx\n"       /*  %7 = ch->length */
		"  addl %6(%%eax), %%ebx\n"       /*  %6 = ch->samp */
		"  addl %%esi, %%edi\n"
		"  xorl %%ecx, %%ecx\n"
		"mixAddAbs_32bigloop:\n"
		"    pushl %%edi\n"
		"    cmpl %%ebx, %%edi\n"
		"    ja mixAddAbs_32less\n"
		"      xorl %%edx, %%edx\n"
		"    jmp mixAddAbs_32loop\n"
		"  mixAddAbs_32less:\n"
		"    movl %%ebx, %%edi\n"
		"  jmp mixAddAbs_32loop\n"

		"  mixAddAbs_32neg:\n"
		"    subl %%eax, %%ecx\n"
		"    cmpl %%edi, %%esi\n"
		"    jae mixAddAbs_32loopend\n"
		"  mixAddAbs_32loop:\n"
		"      flds (,%%esi,4)\n"
		"      fistps integer\n"
		"      movl integer, %%eax\n"
		"      incl %%esi\n"
		"      xorl $0xffffff80, %%eax\n"
		"      js mixAddAbs_32neg\n"
		"      addl %%eax, %%ecx\n"
		"    cmpl %%edi, %%esi\n"
		"    jb mixAddAbs_32loop\n"
		"  mixAddAbs_32loopend:\n"
		"    popl %%edi\n"
		"    cmpl $0, %%edx\n"
		"    je mixAddAbs_exit\n"
		"    subl %%edx, %%edi\n"
		"    subl %%edx, %%esi\n"
		"  jmp mixAddAbs_32bigloop\n"

		"mixAddAbs_exit:\n"
#ifdef __PIC__
		"movl %10, %%ebx\n"

#endif
		: "=c"  (retval),                            /*   0  */
		  "=&a" (d0),                                /*   1  */
		  "=&D" (d1)                                 /*   2  */
		: "m" (((struct mixchannel *)NULL)->status), /*   3  */
		  "m" (((struct mixchannel *)NULL)->replen), /*   4  */
		  "m" (((struct mixchannel *)NULL)->pos),    /*   5  */
		  "m" (((struct mixchannel *)NULL)->samp),   /*   6  */
		  "m" (((struct mixchannel *)NULL)->length), /*   7  */
		  "1" (ch),
		  "2" (len)
#ifdef __PIC__
		  ,"m"(ebx)
#endif

		: "memory", "edx", "esi"
	);
	return retval;
}

void mixClip(int16_t *dst, const int32_t *src, uint32_t len, int16_t (*tab)[256], int32_t max)
{ /* does saturation of some sort.. needs to read the code when I'm not sleepy */
	int d0, d1, d2, d3, d4;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"  pushl %%ebx\n" /* EBX is pic register */
#endif
		"  movl %%eax, %%ebx\n"

		"  movl %%ebx, (amp1-4)\n"
		"  addl $512, %%ebx\n"
		"  movl %%ebx, (amp2-4)\n"
		"  addl $512, %%ebx\n"
		"  movl %%ebx, (amp3-4)\n"
		"  subl $1024, %%ebx\n"
		"  movl %%edx, (max-4)\n"
		"  negl %%edx\n"
		"  movl %%edx, (min-4)\n"

		"  xorl %%edx, %%edx\n"
		"  movb (min-4), %%dl\n"
		"  movl (%%ebx,%%edx,2), %%eax\n"
		"  movb (min-3), %%dl\n"
		"  addl 512(%%ebx,%%edx,2), %%eax\n"
		"  movb (min-2), %%dl\n"
		"  addl 1024(%%ebx,%%edx,2), %%eax\n"
		"  movw %%ax, (minv-2)\n"
		"  movb (max-4), %%dl\n"
		"  movl (%%ebx,%%edx,2), %%eax\n"
		"  movb (max-3), %%dl\n"
		"  addl 512(%%ebx,%%edx,2), %%eax\n"
		"  movb (max-2), %%dl\n"
		"  addl 1024(%%ebx,%%edx,2), %%eax\n"
		"  movw %%ax, (maxv-2)\n"
		"  leal (%%edi,%%ecx,2), %%ecx\n"
		"  movl %%ecx,(endp1-4)\n"
		"  movl %%ecx,(endp2-4)\n"
		"  movl %%ecx,(endp3-4)\n"
		"  xorl %%ebx, %%ebx\n"
		"  xorl %%ecx, %%ecx\n"
		"  xorl %%edx, %%edx\n"

		"mixlp:\n"
		"  cmpl $1234, (%%esi)\n"
		"min:\n"
		"  jl low\n"
		"  cmpl $1234, (%%esi)\n"
		"max:\n"
		"  jg high\n"

		"  movb (%%esi), %%bl\n"
		"  movb 1(%%esi), %%cl\n"
		"  movb 2(%%esi), %%dl\n"
		"  movl 1234(,%%ebx,2), %%eax\n"
		"amp1:\n"
		"  addl 1234(,%%ecx,2), %%eax\n"
		"amp2:\n"
		"  addl 1234(,%%edx,2), %%eax\n"
		"amp3:\n"
		"  movw %%ax, (%%edi)\n"
		"  addl $2, %%edi\n"
		"  addl $4, %%esi\n"
		"  cmpl $1234, %%edi\n"
		"endp1:\n"
		"  jb mixlp\n"
		"  jmp done\n" /* done: ret pair used to be close here.. but we might need frame-adjust and shit like that */

		"low:\n"
		"  movw $1234, (%%edi)\n"
		"minv:\n"
		"  addl $2, %%edi\n"
		"  addl $4, %%esi\n"
		"  cmpl $1234, %%edi\n"
		"endp2:\n"
		"  jb mixlp\n"
		"  jmp done\n"

		"high:\n"
		"  movw $1234, (%%edi)\n"
		"maxv:\n"
		"  addl $2, %%edi\n"
		"  addl $4, %%esi\n"
		"  cmpl $1234, %%edi\n"
		"endp3:\n"
		"  jb mixlp\n"
/*
		"  jmp done\n" */

		"done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&D"(d0),
		  "=&S"(d1),
		  "=&c"(d2),
		  "=&a"(d3),
		  "=&d"(d4)
		: "0" (dst),
		  "1" (src),
		  "2" (len),
		  "3" (tab),
		  "4" (max)
#ifdef __PIC__
		: "memory"
#else
		: "memory", "ebx"
#endif

	);
}

void mixasm_remap_stop(void){}

#else

int8_t (*mixIntrpolTab)[256][2];
int16_t (*mixIntrpolTab2)[256][2];

uint32_t mixAddAbs(const struct mixchannel *chan, uint32_t len)
{
	unsigned long retval=0;

	if (chan->status&MIX_PLAY16BIT)
	{
		int replen=chan->replen;
		int16_t *pos=chan->realsamp.fmt16+chan->pos; /* actual position */
		int16_t *end=chan->realsamp.fmt16+chan->length; /* actual EOS */
		int16_t *target=pos+len; /* target EOS */
		while(1)
		{
			int16_t *_temptarget=target;
			if (_temptarget<end)
			{
				replen=0;
			} else {
				_temptarget=end;
			}
			do {
				int16_t sample;
				sample=*pos++;
				if (sample<0)
				{
					sample=0-sample;
				}
				retval+=sample;
			} while (pos<_temptarget);

			if (!replen)
				goto exit;
			target-=replen;
			pos-=replen;
		}

	} else if (chan->status&MIX_PLAYFLOAT)
	{
		int replen=chan->replen;
		float *pos=chan->realsamp.fmtfloat+chan->pos; /* actual position */
		float *end=chan->realsamp.fmtfloat+chan->length; /* actual EOS */
		float *target=pos+len; /* target EOS */
		while(1)
		{
			float *_temptarget=target;
			if (_temptarget<end)
			{
				replen=0;
			} else {
				_temptarget=end;
			}
			do {
				float sample;
				sample=*pos++;
				if (sample<0)
				{
					sample=0-sample;
				}
				retval+=sample;
			} while (pos<_temptarget);
			if (!replen)
				goto exit;
			target-=replen;
			pos-=replen;
		}

	} else { /* 8 bit*/
		int replen=chan->replen;
		int8_t *pos=chan->realsamp.fmt8+chan->pos; /* actual position */
		int8_t *end=chan->realsamp.fmt8+chan->length; /* actual EOS */
		int8_t *target=pos+len; /* target EOS */
		while(1)
		{
			int8_t *_temptarget=target;
			if (_temptarget<end)
			{
				replen=0;
			} else {
				_temptarget=end;
			}
			do {
				int8_t sample;
				sample=*pos++;
				if (sample<0)
				{
					sample=0-sample;
				}
				retval+=sample;
			} while (pos<_temptarget);

			if (!replen)
				goto exit;
			target-=replen;
			pos-=replen;
		}
	}
exit:
	return retval;
}

void mixClip(int16_t *dst, const int32_t *src, uint32_t len, int16_t (*tab)[256], int32_t max)
{
	int16_t *amp1=tab[0];
	int16_t *amp2=tab[1];
	int16_t *amp3=tab[2];
	int32_t min=~max;
	int16_t minv =
		tab[0][min&0xff] +
		tab[1][(min&0xff00)>>8] +
		tab[2][(min&0xff0000)>>16];
	int16_t maxv =
		tab[0][max&0xff] +
		tab[1][(max&0xff00)>>8] +
		tab[2][(max&0xff0000)>>16];
	int16_t *endp=dst+len;

	do
	{
		if ((*src)<min)
		{
			*(dst++)=minv;
		} else if ((*src)>max)
		{
			*(dst++)=maxv;
		} else
		{
			*(dst++)=amp1[(*src)&0xff]+
			         amp2[((*src)&0xff00)>>8]+
			         amp3[((*src)&0xff0000)>>16];
		}
		src++;
	} while (dst<endp);
}

static uint32_t *voltabs[2]={0,0};

static void playmono(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0][*src];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playodd(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0][*src];
		dst++;
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmonoi(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0]
			[
				(uint8_t)(
					mixIntrpolTab[fpos>>12][src[0]][0]+
					mixIntrpolTab[fpos>>12][src[1]][1]
				)
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playoddi(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0]
			[
				(uint8_t)(
					mixIntrpolTab[fpos>>12][src[0]][0]+
					mixIntrpolTab[fpos>>12][src[1]][1]
				)
			];
		dst++;
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmonoir(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		uint16_t vol = mixIntrpolTab2[fpos>>11][src[0]][0]+
			       mixIntrpolTab2[fpos>>11][src[1]][1];
		*(dst++)+=voltabs[0]
			[
				(vol>>8)
			]
			+
			voltabs[0]
			[
				(vol&255)
				+256
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playoddir(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		uint16_t vol = mixIntrpolTab2[fpos>>11][src[0]][0]+
			       mixIntrpolTab2[fpos>>11][src[1]][1];
		*(dst++)+=voltabs[0]
			[
				(vol>>8)
			]
			+
			voltabs[0]
			[
				(vol&255)
				+256
			];
		dst++;
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}
static void playmono16(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0][(*src)>>8];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playodd16(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0][(*src)>>8];
		dst++;
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmonoi16(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;


	while (len)
	{
		*(dst++)+=voltabs[0]
			[
				(uint8_t)(
					mixIntrpolTab[fpos>>12][src[0]>>8][0]+
					mixIntrpolTab[fpos>>12][src[1]>>8][0]
				)
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playoddi16(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;


	while (len)
	{
		*(dst++)+=voltabs[0]
			[
			(uint8_t)(mixIntrpolTab[fpos>>12][src[0]>>8][0]+
			mixIntrpolTab[fpos>>12][src[1]>>8][0])
			];
		dst++;
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmonoi16r(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		uint16_t vol = mixIntrpolTab2[fpos>>11][src[0]>>8][0]+
			       mixIntrpolTab2[fpos>>11][src[1]>>8][1];
		*(dst++)+=voltabs[0]
			[
				(vol>>8)
			]
			+
			voltabs[0]
			[
				(vol&255)
				+256
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playoddi16r(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		uint16_t vol = mixIntrpolTab2[fpos>>11][src[0]>>8][0]+
			       mixIntrpolTab2[fpos>>11][src[1]>>8][1];
		*(dst++)+=voltabs[0]
			[
				(vol>>8)
			]
			+
			voltabs[0]
			[
				(vol&255)
				+256
			];
		dst++;
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmono32(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	float scale = ch->vol.volfs[0] * 64.0;
	float *src = ch->realsamp.fmtfloat+ch->pos;
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;
	while (len)
	{
		*(dst++)+=(int32_t)((*src)*scale);
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playodd32(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	float scale = ch->vol.volfs[0] * 64.0;
	float *src = ch->realsamp.fmtfloat+ch->pos;
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;
	/* write(2, "TODO: playodd32 reached\n", 24); -- does not exists in assembler version */
	while (len)
	{
		*(dst++)+=(int32_t)((*src)*scale);
		dst++;
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}
void mixPlayChannel(int32_t *dst, uint32_t len, struct mixchannel *ch, int stereo)
{
	void (*playrout)(int32_t *dst, uint32_t len, struct mixchannel *ch);

	int interpolate=0;
	int play16bit=0;
	int playfloat=0;
	int render16bit=0;
	int32_t step;
	uint16_t fpos;
	int inloop;
	uint32_t eax;
	uint64_t pos;
	uint32_t mylen;

	if (!(ch->status&MIX_PLAYING))
		return;
	if (ch->status&MIX_INTERPOLATE)
	{
		interpolate=1;
		render16bit=ch->status&MIX_MAX;
	}

	play16bit=(ch->status&MIX_PLAY16BIT);

	playfloat=(ch->status&MIX_PLAYFLOAT);

	if (stereo)
	{
		voltabs[0]=ch->vol.voltabs[0];
		voltabs[1]=ch->vol.voltabs[1];
		if (playfloat)
			playrout=playodd32; /* this overrides all other settings, since we only have one (dummy) 32bit player odd */
		else {
			if (interpolate)
			{
				if (render16bit)
				{
					if (play16bit)
						playrout=playoddi16r;
					else
						playrout=playoddir;
				} else {
					if (play16bit)
						playrout=playoddi16;
					else
						playrout=playoddi;
				}
			} else  { /* no interpolate */
				if (play16bit)
					playrout=playodd16;
				else
					playrout=playodd;
			}
		}
	} else {
		voltabs[0]=ch->vol.voltabs[0];

		if (playfloat)
			playrout=playmono32; /* this overrides all other settings, since we only have one 32bit player mono */
		else {
			if (interpolate)
			{
				if (render16bit)
				{
					if (play16bit)
						playrout=playmonoi16r;
					else
						playrout=playmonoir;
				} else {
					if (play16bit)
						playrout=playmonoi16;
					else
						playrout=playmonoi;
				}
			} else  { /* no interpolate */
				if (play16bit)
					playrout=playmono16;
				else
					playrout=playmono;
			}
		}
	}

	do
	{
/*mixPlayChannel_bigloop:*/
		inloop=0;
		step=ch->step; /* ebx */
		fpos=ch->fpos; /* si */
		mylen=len;
		/* count until end of sample/fold... eax */
		if (!step)
			return;
		if (step>0)
		{
/*mixPlayChannel_forward:*/
			eax=ch->length-ch->pos;
			if ((fpos^=-1))
				eax--;
			if ((ch->status&MIX_LOOPED)&&(ch->pos<ch->loopend))
			{
				eax-=(ch->length-ch->loopend);
				inloop=1;
			}
		} else /*if (step<0)*/
		{
/*'mixP	layChannel_backforward:*/
			step=~step;
			eax=ch->pos;
			if ((ch->status&MIX_LOOPED)&&(ch->pos>=ch->loopstart))
			{
				eax-=ch->loopstart;
				inloop=1;
			}
		}
/*mixPlayChannel_maxplaylen:*/
		pos=((eax<<16)|fpos)+ch->step;
		if ((pos>>32)<=ch->step)
		{/* avoid overflow exception */
			eax=pos/ch->step;
			if ((eax<=mylen)&&inloop)
			{
				ch->status&=(MIX_ALL-MIX_PLAYING);
				len=mylen;
			}
		}

/*mixPlayChannel_playecx:*/
		playrout(dst, mylen, ch);

		dst+=mylen<<(!!stereo);
		len-=mylen;

		pos=((uint64_t)ch->pos<<16)|ch->fpos;
		pos+=(int64_t)mylen*(int64_t)ch->step;

		if (!inloop)
			break;

		eax=ch->pos;

		if (ch->step<0)
		{
			if (eax>=ch->loopstart)
				break;
			if (!(ch->status&MIX_PINGPONGLOOP))
			{
				eax+=ch->replen;
			} else {
/*mixPlayChannel_pong:*/
				ch->step=-ch->step;
				if ((ch->fpos=-ch->fpos))
					eax++;
				eax=-eax;
				eax+=ch->loopstart;
				eax+=ch->loopstart;
			}
		} else {
/*mixPlayChannel_forward2:*/
			if (eax<ch->loopend)
				break;

			if (ch->status&MIX_PINGPONGLOOP)
			{
/*mixPlaychannel_ping:*/
				if ((ch->fpos=-ch->fpos))
					eax++;
				eax=-eax;
				eax+=ch->loopend;
				eax+=ch->loopend;
			} else {
				eax=ch->replen;
			}
		}
/*mixPlayChannel_loopiflen:*/

		ch->pos=eax;
	} while (len);
#if 0
		edi=ch
		"  movl $0, interpolate\n"
		"  movl $0, play16bit\n"
		"  movl $0, playfloat\n"
		"  movl $0, render16bit\n"

		"  testb $MIX_PLAYING, %8(%%edi)\n"
						/*  %8 = ch->status */
		edi=ch

		"  jz pcexit\n"

		"  testb $MIX_INTERPOLATE, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"  jz mixPlayChannel_nointr\n"
		"     movl $1, interpolate\n"
		"     testb $MIX_MAX, %8(%%edi)\n"/*  %8 = ch->status */
		"     jz mixPlayChannel_nointr\n"
		"       movl $1, render16bit\n"
		"mixPlayChannel_nointr:\n"
		"  testb $MIX_PLAY16BIT, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"  jz mixPlayChannel_no16bit\n"
		"    movl $1, play16bit\n"
		"mixPlayChannel_no16bit:\n"
		"  testb $MIX_PLAYFLOAT, %8(%%edi)\n"
		                                  /*  %8= ch->status */
		"  jz mixPlayChannel_no32bit\n"
		"    movl $1, playfloat\n"
		"mixPlayChannel_no32bit:\n"

		"  cmpb $0, %1\n"                 /*  %1 = stereo */
		"  jne mixPlayChannel_pstereo\n"

		"mixPlayChannel_pmono:\n"
		"  movl %12(%%edi), %%eax\n"      /* %12 = ch->vol.voltabs[0] */
		"  movl %%eax, voltabs+0\n"
		"  movl $playmono, playrout\n"
		"  jmp mixPlayChannel_bigloop\n"

		"mixPlayChannel_pstereo:\n"
		"  movl %12(%%edi), %%eax\n"      /* %12 = ch->vol.voltabs[0] */
		"  movl %13(%%edi), %%ebx\n"      /* %13 = ch->vol.voltabs[1] */
		"  movl %%eax, voltabs\n"
		"  movl %%ebx, voltabs+4\n"
		"  movl $playodd, playrout\n"






		"mixPlayChannel_bigloop:\n"
		"  movl %2, %%ecx\n"              /*  %2 = len */
		"  movl %11(%%edi), %%ebx\n"      /* %11 = ch->step */
		"  movl %9(%%edi), %%edx\n"       /*  %9 = ch->pos */
		"  movw %10(%%edi), %%si\n"       /* %10 = ch->->fpos */
		"  movl $0, inloop\n"

		edi=ch

		ecx=len
		ebx=ch->step
		edx=ch->pos
		 si=ch->fpos

		inloop=0;


		%1=stereo
		%2=len
		%3=buf
		%4=->length
		%5=->loopend
		%6=->replen
		%7=->loopstart
		%8=->status
		%9=->pos
		%10=->fpos
		%11=->step
		%12=->vol.voltabs[0]
		%13=->vol.voltabs[1]

		"  cmpl $0, %%ebx\n"


		"  je mixPlayChannel_playecx\n"
		"  jg mixPlayChannel_forward\n"
		"    negl %%ebx\n"
		"    movl %%edx, %%eax\n"

	edi=ch

	ecx=len
	ebx=abs(ch->step)
	edx=ch->pos
		eax=ch->pos
		 si=ch->fpos
		"    testb $MIX_LOOPED, %8(%%edi)\n"
		                                  /*  %8 = ch->->status */
		"    jz mixPlayChannel_maxplaylen\n"
		"    cmpl %7(%%edi), %%edx\n"     /*  %7 = ch->loopstart */
		"    jb mixPlayChannel_maxplaylen\n"
		"    subl %7(%%edi), %%eax\n"     /*  %7 = ch->loopstart */
		"    movl $1, inloop\n"
		"    jmp mixPlayChannel_maxplaylen\n"
		"mixPlayChannel_forward:\n"
		"    movl %4(%%edi), %%eax\n"     /*  %4 = ch->length */
		"    negw %%si\n"
		"    sbbl %%edx, %%eax\n"
		"    testb $MIX_LOOPED, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"    jz mixPlayChannel_maxplaylen\n"
		"    cmpl %5(%%edi), %%edx\n"     /*  %5 = ch->loopend */
		"    jae mixPlayChannel_maxplaylen\n"
		"    subl %4(%%edi), %%eax\n"     /*  %4 = ch->length */
		"    addl %5(%%edi), %%eax\n"     /*  %5 = ch->loopend */
		"    movl $1, inloop\n"

		"mixPlayChannel_maxplaylen:\n"

		edi=ch

		ecx=len
		ebx=abs(ch->step)
		edx=ch->pos
		eax=runlength
		 si=abs(ch->fpos) /* rel step */

		"  xorl %%edx, %%edx\n"

		edx=0

		"  shld $16, %%eax, %%edx\n"

		eax=runlength
		edx=(runlength&0xffff0000)>>16

		"  shll $16, %%esi\n"

		esi=abs(16bit ch->fpos)<<16

		"  shld $16, %%esi, %%eax\n"

		eax=(runlength<<16)|abs(16bit fpos)

		"  addl %%ebx, %%eax\n"

		eax=(runlength<<16)|abs(16bit fpos) + abs(ch->step)

		"  adcl $0, %%edx\n"

		edx=(runlength&0xffff0000)>>16 carry from eax

		"  subl $1, %%eax\n"

		eax--

		"  sbbl $0, %%edx\n"

		edx=(runlength&0xffff0000)>>16 + carry from eax - carry from eax

		"  cmpl %%ebx, %%edx\n"
		"  jae mixPlayChannel_playecx\n"

		/* check that we are not going to throw exception.. this is FUCKING clever */
		if (abs(step) <= (runlength&0xffff0000)>>16 + carry from eax - carry from eax )
			goto mixPlayChannel_playecx

		"  divl %%ebx\n"

		eax=(edx:eax)/ebx
		edx=(edx:eax)%ebx

		eax = how many samples we can get out before EOS/End of fold

		"  cmpl %%eax, %%ecx\n"
		"  jb mixPlayChannel_playecx\n"

		if (ecx<eax) /* ecx is copy of len */
			goto mixPlayChannel_playecx

		"    movl %%eax, %%ecx\n"
		"    cmpl $0, inloop\n"
		"    jnz mixPlayChannel_playecx\n"
		"      andb $254, %8(%%edi)\n"    /*** 254 = MCP_PLAYING^255   no parameters left  ***
		                                   *  %8 = ch->status */
		"      movl %%ecx, %2\n"          /*  %2 = len */

		"mixPlayChannel_playecx:\n"

		eax=trash
		ebx=abs(ch->step)
		ecx=samplecount (dst)
		edx=trash
		ebp=stack frame (internal)
		edi=ch
		esi=trash

		"  pushl %%ebp\n"
		"  pushl %%edi\n"
		"  pushl %%ecx\n"
		"  movl %%ecx, %%eax\n"
		"  movl %3, %%ecx\n"              /*  %3 = buf */
		"  movl %%eax, %%ebp\n"

		"  call *playrout\n"

		call playrout
					ebx=abs(ch->step)
			*ecx=buf
					eax=len
		*edx=trash
			*ebp=len
				*edi=ch
					esi=trash

		"  popl %%ecx\n"
		"  popl %%edi\n"
		"  popl %%ebp\n"

		eax=ignored
		ebx=abs(ch->step)
		ecx=samplecount(dst)
		edx=ignored
		esi=ignored
		edi=ch

		"  movl %%ecx, %%eax\n"

		eax=samplecount(dst)

		"  shll $2, %%eax\n"

		eax=samplecount(dst)*sizeof(uint32_t)

		"  cmpl $0, %1\n"                 /*  %1 = stereo */
		"  je mixPlayChannel_m2\n"
		"  shll $1, %%eax\n"
		"mixPlayChannel_m2:\n"

		if (stereo)
			eax*=2;

		"  addl %%eax, %3\n"              /*  %3 = buf */

		buf+=eax

		"  subl %%ecx, %2\n"              /*  %2 = len */

		len-=len;

		"  movl %11(%%edi), %%eax\n"      /* %11 = ch->step */

		eax=ch->step

		"  imul %%ecx\n"

		edx:eax=ch->step*samplecount /* This should not go above 0x0000:ffff:ffff:ffff */

		"  shld $16, %%eax, %%edx\n"

		edx=n of edx:eax

		"  addw %%ax, %10(%%edi)\n"       /* %10 = ch->->fpos */

		ch->fpos+=ax (fp of edx:eax)

		"  adcl %%edx, %9(%%edi)\n"       /*  %9 = ch->pos */

		edi->pos+=edx (n of edx_eax)+carry from above

		"  movl %9(%%edi), %%eax\n"       /*  %9 = ch->pos */

		eax=ch->pos

		"  cmpl $0, inloop\n"
		"  jz pcexit\n"

		if (!inloop)
			goto pcexit


		"  cmpl $0, %11(%%edi)\n"         /* %11 = ch->step */
		"  jge mixPlayChannel_forward2\n"

		if (ch->step>=0)
			goto mixPlayChannel_forward2;

		"    cmpl %7(%%edi), %%eax\n"     /*  %7 = ch->loopstart */
		"    jge pcexit\n"

		if (ch->pos>=ch->loopstart)
			goto pcexit;

		"    testb $MIX_PINGPONGLOOP, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"    jnz mixPlayChannel_pong\n"

		if (ch->status&MIX_PINGPONGLOOP)
			goto mixPlayChannel_pong;

		"      addl %6(%%edi), %%eax\n"   /*  %6 = ch->replen */

		eax+=ch->replen
		=>
		eax=ch->pos+ch->replen;

		"      jmp mixPlayChannel_loopiflen\n"
		goto mixPlayChannel_loopiflen

		"mixPlayChannel_pong:\n"

mixPlayChannel_pong:

		"      negl %11(%%edi)\n"         /* %11 = ch->step */

		ch->step=-ch->step

		"      negw %10(%%edi)\n"         /* %10 = ch->->fpos */

		ch->fpos=-ch->fpos

		"      adcl $0, %%eax\n"

		if (ch->fpos)
			eax++;

		"      negl %%eax\n"

		eax=-eax;

		"      addl %7(%%edi), %%eax\n"   /*  %7 = ch->->loopstart */
		"      addl %7(%%edi), %%eax\n"   /*  %7 = ch->->loopstart */


		ch->loopstart+=eax
		ch->loopstart+=eax

		"      jmp mixPlayChannel_loopiflen\n"

		goto mixPlayChannel_loopiflen

		"mixPlayChannel_forward2:\n"

mixPlayChannel_forward2:

		eax=ch->pos

		"    cmpl %5(%%edi), %%eax\n"     /*  %5 = ch->->loopend */
		"    jb pcexit\n"

		if (eax<ch->loopend)
				goto pcexit;

		"    testb $MIX_PINGPONGLOOP, %8(%%edi)\n"
		                                  /*  %8 = ch->status */
		"    jnz mixPlayChannel_ping\n"

		if (ch->status&MIX_PINGPONGLOOP)
				goto mixPlayChannel_ping;

		"      sub %6(%%edi), %%eax\n"    /*  %6 = ch->replen */

		eax=ch->pos
		=>
		eax-=ch->replen

		"      jmp mixPlayChannel_loopiflen\n"

		goto mixPlayChannel_loopiflen

		"mixPlayChannel_ping:\n"

mixPlayChannel_ping:

		"      negl %11(%%edi)\n"         /* %11 = ch->step */

		ch->step=-ch->step
		"      negw %10(%%edi)\n"         /* %10 = ch->->fpos */
		"      adcl $0, %%eax\n"

		eax=ch->pos
		=>
		if (ch->fpos=-ch->fpos)
			eax++;

		"      negl %%eax\n"

		eax=-eax;

		"      addl %5(%%edi), %%eax\n"   /*  %5 = ch->->loopend */
		"      addl %5(%%edi), %%eax\n"   /*  %5 = ch->->loopend */

		eax+=ch->loopend
		eax+=ch->loopend

		"mixPlayChannel_loopiflen:\n"

mixPlayChannel_loopiflen:

		"  movl %%eax, %9(%%edi)\n"       /*  %9 = ch->pos */

		ch->pos=eax

		"  cmpl $0, %2\n"                 /*  %2 = len */
		"  jne mixPlayChannel_bigloop\n"

		if (len)
			mixPlayChannel_bigloop

		"pcexit:\n"

pcexit:
#endif
}

#endif
