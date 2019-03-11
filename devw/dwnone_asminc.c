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
		" movl %c4(%%edi), %%ebx\n"    /*  4 = ->step */
		" movl %c5(%%edi), %%edx\n"    /*  5 = ->pos */
		" movw %c6(%%edi), %%si\n"     /*  6 = ->fpos */
		" movb $0, %3\n"              /*  3 = inloop */
		" cmpl $0, %%ebx\n"
		" je 5f\n" /* playecx */
		" jg 2f\n" /* forward */
		" negl %%ebx\n"
		" movl %%edx, %%eax\n"
		" testb $MIX_LOOPED, %c7(%%edi)\n" /*  7 = ->status */
		" jz 4f\n" /* maxplaylen */
		" cmpl %c8(%%edi), %%edx\n"    /*  8 = ->loopstart */
		" jb 4f\n" /* maxplaylen */
		" sub %c8(%%edi), %%eax\n"     /*  8 = ->loopstart */
		" movb $1, %3\n"
		" jmp 4f\n" /* maxplaylen */
		"2:\n" /* forward */
		" movl %c9(%%edi), %%eax\n"    /*  9 = ->length */
		" negw %%si\n"
		" sbbl %%edx, %%eax\n"
		" testb $MIX_LOOPED, %c7(%%edi)\n"
		" jz 4f\n" /* maxplaylen */
		" cmpl %%edx, %c10(%%edi)\n"   /* 10 = ->loopend */
		" jae 4f\n" /* maxplaylen */
		" subl %c9(%%edi), %%eax\n"    /*  9 = ->length */
		" addl %c10(%%edi), %%eax\n"   /* 10 = ->loopend */
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
		"     andb $0xfe, %c7(%%edi)\n" /* 0xfe is NOT MIX_PLAYING */
		"     movl %%ecx,%2\n"

		"5:\n"
		" subl %%ecx, %2\n"
		" movl %c4(%%edi), %%eax\n"    /*  4 = ->step */
		" imul %%ecx\n"
		" shld $16, %%eax, %%edx\n"
		" addw %%ax, %c6(%%edi)\n"
		" adcl %%edx, %c5(%%edi)\n"
		" movl %c5(%%edi), %%eax\n"

		" cmpl $0, %3\n"
		" jz 9f\n" /* exit */

		" cmpl $0, %c4(%%edi)\n"
		" jge 3f\n" /* forward2 */
		"   cmpl %c8(%%edi), %%eax\n"
		"   jge 9f\n" /* exit */
		"   testb $MIX_PINGPONGLOOP,%c7(%%edi)\n"
		"   jnz 6f\n" /* pong */
		"     addl %c11(%%edi), %%eax\n"
		"     jmp 8f\n" /* loopiflen */
		"6:\n" /* pong */
		"     negl %c4(%%edi)\n"
		"     negl %c4(%%edi)\n"
		"     adcl $0, %%eax\n"
		"     negl %%eax\n"
		"     add  %c8(%%edi), %%eax\n"
		"     add  %c8(%%edi), %%eax\n"
		"     jmp 8f\n" /* loopiflen */
		"3:\n"
		"   cmpl %c10(%%edi), %%eax\n"
		"   jb 9f\n" /* exit */
		"   testb $MIX_PINGPONGLOOP, %c7(%%edi)\n"
		"   jnz 7f\n" /* ping */
		"     subl %c11(%%edi), %%eax\n"
		"     jmp 8f\n" /* loopiflen */
		"7:\n" /* ping */
		"     negl %c4(%%edi)\n"
		"     negw %c6(%%edi)\n"
		"     adcl $0, %%eax\n"
		"     negl %%eax\n"
		"     addl %c10(%%edi), %%eax\n"
		"     addl %c10(%%edi), %%eax\n"

		"8:\n" /* loopiflen */
		" movl %%eax, %c5(%%edi)\n"
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
		  "n" (&(((struct channel *)0)->step)),        /*   4  */
		  "n" (&(((struct channel *)0)->pos)),         /*   5  */
		  "n" (&(((struct channel *)0)->fpos)),        /*   6  */
		  "n" (&(((struct channel *)0)->status)),      /*   7  */
		  "n" (&(((struct channel *)0)->loopstart)),   /*   8  */
		  "n" (&(((struct channel *)0)->length)),      /*   9  */
		  "n" (&(((struct channel *)0)->loopend)),     /*  10  */
		  "n" (&(((struct channel *)0)->replen))       /*  11  */
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
