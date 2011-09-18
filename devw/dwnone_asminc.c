/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * auxiliary assembler routines for no sound wavetable device
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
 *    -assembler rewritten to gcc
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -minor changes to make gcc able to compile, even with VERY high optimize level
 *  -doj040914  Dirk Jagdmann  <doj@cubic.org>
 *    -push/pop %ebp, and don't flag is as dirty in nonePlayChannel
 */

/* Butt ugly, and no usage of the fpos ... pure 32 bit for now */

#ifdef I386_ASM

static void nonePlayChannel(unsigned long len, struct channel *ch)
{
	int d0;
	int inloop;
#ifdef __PIC__
	int ebx_save;
#endif
	__asm__ __volatile__
	(
#ifdef __PIC__
		"movl %%ebx, %12\n"
#endif
		" movl $0, %3\n"              /*  3 = inloop */

		"1:\n"   /* bigloop */
		" movl %2, %%ecx\n"           /*  2 = len */
		" movl %4(%%edi), %%ebx\n"    /*  4 = ->step */
		" movl %5(%%edi), %%edx\n"    /*  5 = ->pos */
		" movw %6(%%edi), %%si\n"     /*  6 = ->fpos */
		" movb $0, %3\n"              /*  3 = inloop */
		" cmpl $0, %%ebx\n"
		" je 5f\n" /* playecx */
		" jg 2f\n" /* forward */
		" negl %%ebx\n"
		" movl %%edx, %%eax\n"
		" testb $MIX_LOOPED, %7(%%edi)\n" /*  7 = ->status */
		" jz 4f\n" /* maxplaylen */
		" cmpl %8(%%edi), %%edx\n"    /*  8 = ->loopstart */
		" jb 4f\n" /* maxplaylen */
		" sub %8(%%edi), %%eax\n"     /*  8 = ->loopstart */
		" movb $1, %3\n"
		" jmp 4f\n" /* maxplaylen */
		"2:\n" /* forward */
		" movl %9(%%edi), %%eax\n"    /*  9 = ->length */
		" negw %%si\n"
		" sbbl %%edx, %%eax\n"
		" testb $MIX_LOOPED, %7(%%edi)\n"
		" jz 4f\n" /* maxplaylen */
		" cmpl %%edx, %10(%%edi)\n"   /* 10 = ->loopend */
		" jae 4f\n" /* maxplaylen */
		" subl %9(%%edi), %%eax\n"    /*  9 = ->length */
		" addl %10(%%edi), %%eax\n"   /* 10 = ->loopend */
		" movb $1, %3\n"

		"4:\n" /* maxplaylen */
		" xorl %%edx, %%edx\n"
		" shld $16, %%eax, %%edx\n"
		" shll $16, %%esi\n"
		" shld $16, %%esi, %%eax\n"
		" addl %%edx, %%eax\n"
		" adcl $0, %%eax\n"
		" subl $1, %%eax\n"
		" sbbl $0, %%edx\n"
		" cmpl %%ebx, %%edx\n"
		" jae 5f\n" /* playecx */
		" divl %%ebx\n"
		" cmpl %%eax, %%ecx\n"
		" jb 5f\n" /* playecx */
		"  movl %%eax, %%ecx\n"
		"  cmpb $0, %3\n"
		"  jnz 5f\n" /* playecx */
		"     andb $0xfe, %7(%%edi)\n" /* 0xfe is NOT MIX_PLAYING */
		"     movl %%ecx,%2\n"

		"5:\n"
		" subl %%ecx, %2\n"
		" movl %4(%%edi), %%eax\n"    /*  4 = ->step */
		" imul %%ecx\n"
		" shld $16, %%eax, %%edx\n"
		" addw %%ax, %6(%%edi)\n"
		" adcl %%edx, %5(%%edi)\n"
		" movl %5(%%edi), %%eax\n"

		" cmpl $0, %3\n"
		" jz 9f\n" /* exit */

		" cmpl $0, %4(%%edi)\n"
		" jge 3f\n" /* forward2 */
		"   cmpl %8(%%edi), %%eax\n"
		"   jge 9f\n" /* exit */
		"   testb $MIX_PINGPONGLOOP,%7(%%edi)\n"
		"   jnz 6f\n" /* pong */
		"     addl %11(%%edi), %%eax\n"
		"     jmp 8f\n" /* loopiflen */
		"6:\n" /* pong */
		"     negl %4(%%edi)\n"
		"     negl %4(%%edi)\n"
		"     adcl $0, %%eax\n"
		"     negl %%eax\n"
		"     add  %8(%%edi), %%eax\n"
		"     add  %8(%%edi), %%eax\n"
		"     jmp 8f\n" /* loopiflen */
		"3:\n"
		"   cmpl %10(%%edi), %%eax\n"
		"   jb 9f\n" /* exit */
		"   testb $MIX_PINGPONGLOOP, %7(%%edi)\n"
		"   jnz 7f\n" /* ping */
		"     subl %11(%%edi), %%eax\n"
		"     jmp 8f\n" /* loopiflen */
		"7:\n" /* ping */
		"     negl %4(%%edi)\n"
		"     negw %6(%%edi)\n"
		"     adcl $0, %%eax\n"
		"     negl %%eax\n"
		"     addl %10(%%edi), %%eax\n"
		"     addl %10(%%edi), %%eax\n"

		"8:\n" /* loopiflen */
		" movl %%eax, %5(%%edi)\n"
		" cmpl $0, %0\n"
		" jne 1b\n"

		"9:\n"
#ifdef __PIC__
		"movl %12, %%ebx\n"
#endif
		: "=&D"(d0)                                 /*   0  */
		: "0" (ch),                                 /*   1  */
		  "m" (len),                                /*   2  */
		  "m" (inloop),                             /*   3  */
		  "m" (((struct channel *)0)->step),        /*   4  */
		  "m" (((struct channel *)0)->pos),         /*   5  */
		  "m" (((struct channel *)0)->fpos),        /*   6  */
		  "m" (((struct channel *)0)->status),      /*   7  */
		  "m" (((struct channel *)0)->loopstart),   /*   8  */
		  "m" (((struct channel *)0)->length),      /*   9  */
		  "m" (((struct channel *)0)->loopend),     /*  10  */
		  "m" (((struct channel *)0)->replen)       /*  11  */
#ifdef __PIC__
		,
		  "m"(ebx_save)                             /*  12  */
		: "memory", "eax", "ecx", "edx", "esi"
#else
		: "memory", "eax", "ebx", "ecx", "edx", "esi"
#endif
	);
}

#else

void nonePlayChannel(unsigned long len, struct channel *ch)
{
	unsigned int r;

	if (!(ch->status&MIX_PLAYING))
		return;
	if ((!ch->step)||(!len))
		return; /* if no length, or pitch is zero.. we don't need to simulate shit*/
	while(len)
	{
		if (ch->step<0)
		{
			int32_t t = -ch->step;
			unsigned int tmp=ch->fpos;
			r = t>>16;
			tmp-=t&0xffff;
			if (tmp>0xffff)
				r++;
			ch->fpos=(uint16_t)tmp;
		} else {
			unsigned int tmp=ch->fpos;
			r = ch->step>>16;
			tmp+=ch->step&0xffff;
			if (tmp>0xffff)
				r++;
			ch->fpos=(uint16_t)tmp;
		}

		while (r)
		{
			if (ch->step<0)
			{ /* ! forward */
				if (ch->pos-r<ch->loopstart)
				{
					r-=ch->pos-ch->loopstart;
					ch->pos=ch->loopstart;
					ch->step*=-1;
				} else {
					ch->pos-=r;
					r=0;
				}
			} else {
				if (ch->status&MIX_LOOPED)
				{
					if (ch->pos+r>ch->loopend)
					{
						r-=ch->loopend-ch->pos;
						if (ch->status&MIX_PINGPONGLOOP)
						{
							ch->pos=ch->loopend;
							ch->step*=-1;
						} else {
							ch->pos=ch->loopstart;
						}
					} else {
						ch->pos+=r;
						r=0;
					}
				} else {
					if (ch->pos+r>ch->length)
					{
						ch->pos=0;
						ch->fpos=0;
						ch->step=0;
						r=0;
						len=1;
					} else {
						ch->pos+=r;
						r=0;
					}
				}
			}
		}
		len--;
	}
}
#endif
