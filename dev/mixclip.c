/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * assembler routines for amplifying/clipping
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
 *    -translated assembler to gcc
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -made assembler optimize-safe for gcc
 */

#include "config.h"
#include "types.h"
#include "mixclip.h"
#include "boot/plinkman.h"

#ifdef I386_ASM

/* assembler versions uses the first 512 entries as 32bit pointers */

void mixCalcClipTab(uint16_t *ct, int32_t amp)
{
	signed long i,j,a,b;

	a=-amp;
	for (i=0; i<256; i++)
		ct[i+768]=(a+=amp)>>16;

	for (i=0; i<256; i++)
		ct[i+1024]=0;

	b=0x800000-(amp<<7);
	for (i=0; i<256; i++)
	{
		if (b<0x000000)
		{
			if ((b+amp)<0x000000)
			{
				((unsigned short **)ct)[i]=ct+1024;
				ct[i+512]=0x0000;
			} else {
				a=0;
				for (j=0; j<256; j++)
				{
					ct[j+1280]=(((a>>8)+b)<0x000000)?0x0000:(((a>>8)+b)>>8);
					a+=amp;
				}
				((unsigned short **)ct)[i]=ct+1280;
				ct[i+512]=0x0000;
			}
		} else if ((b+amp)>0xFFFFFF)
		{
			if (b>0xFFFFFF)
			{
				((unsigned short **)ct)[i]=ct+1024;
				ct[i+512]=0xFFFF;
			} else {
				a=0;
				for (j=0; j<256; j++)
				{
					ct[j+1536]=(((a>>8)+b)>0xFFFFFF)?0x0000:((((a>>8)+b)>>8)+1);
					a+=amp;
				}
				((unsigned short **)ct)[i]=ct+1536;
				ct[i+512]=0xFFFF;
			}
		} else {
			((unsigned short **)ct)[i]=ct+768;
			ct[i+512]=b>>8;
		}
		b+=amp;
	}
}

void mixClipAlt(uint16_t *dst, const uint16_t *src, uint32_t len, const uint16_t *tab)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl  %%edx, %%ebx\n"
#endif
		"  pushl %%ebp\n"
		"  xorl %%edx, %%edx\n"

		"  pushl %%ecx\n" /* macro, exp=4 , blocksize = (1 shl exp) = 16, blockexp=exp*/
		"  andl $15, %%ecx\n" /* blocksize-1 */
		"  subl $16, %%ecx\n" /* blocksize */
		"  negl %%ecx\n"
		"  subl %%ecx, %%esi\n"
		"  subl %%ecx, %%esi\n"
		"  subl %%ecx, %%edi\n"
		"  subl %%ecx, %%edi\n"

		"  movl $blockend, %%eax\n"
		"  subl $block, %%eax\n"
		"  shrl $4, %%eax\n"

		"  mulb %%cl\n"
		"  addl $block, %%eax\n"
		"  popl %%ecx\n"
		"  shrl $4, %%ecx\n"
		"  incl %%ecx\n"
		"  jmp *%%eax\n"

		"block:\n"

/*
		"  nop\n"
		"  nop\n" */
		".macro dummy from=0, to=30\n"
		"  movb 1+\\from(%%esi), %%dl\n"
		"  movl (%%ebx,%%edx,4), %%ebp\n"
		"  movl 1024(%%ebx,%%edx,2), %%eax\n"
		".byte 0x8a\n.byte 0x56\n.byte \\from\n"  /* since some gcc/binutils patct-sets optimze away 0 offset, we do this  "movb \\from(%%esi), %%dl\n" */
		"  addl (%%ebp,%%edx,2), %%eax\n"
		".byte 0x66\n.byte 0x89\n.byte 0x47\n.byte \\from\n" /* since some gcc/binutils patct-sets optimze away 0 offset, we do this "movw %%ax, \\from(%%edi)\n" */
		".if \\to-\\from\n"
		" dummy \"(\\from+2)\",\\to\n"
		".endif\n"
		".endm\n"
		" dummy\n"

		"blockend:\n"
		"  addl $32, %%esi\n" /* 128 ??? */
		"  addl $32, %%edi\n" /* 128 ??? */
		"  decl %%ecx\n"
		"  jnz block\n"

		"  popl %%ebp\n"
#ifdef __PIC__
		"movl  %%ebx, %%edx\n"
		"popl %%ebx\n"
		: "=&S"(d0), "=&D"(d1), "=&c"(d2)
		: "0" (src), "1" (dst), "d" (tab), "2" (len)
		: "memory", "eax"
#else
		: "=&S"(d0), "=&D"(d1), "=&c"(d2)
		: "0" (src), "1" (dst), "b" (tab), "2" (len)
		: "memory", "eax", "edx"
#endif

	);
}


void mixClipAlt2(uint16_t *dst, const uint16_t *src, uint32_t len, const uint16_t *tab)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl  %%edx, %%ebx\n"
#endif
		"  pushl %%ebp\n"

		"  xorl %%edx, %%edx\n"

		"  pushl %%ecx\n" /* macro, exp=4 , blocksize = (1 shl exp) = 16, blockexp=exp*/
		"  andl $15, %%ecx\n" /* blocksize-1 */
		"  subl $16, %%ecx\n" /* blocksize */
		"  negl %%ecx\n"
		"  subl %%ecx, %%esi\n"
		"  subl %%ecx, %%esi\n"
		"  subl %%ecx, %%esi\n"
		"  subl %%ecx, %%esi\n"
		"  subl %%ecx, %%edi\n"
		"  subl %%ecx, %%edi\n"
		"  subl %%ecx, %%edi\n"
		"  subl %%ecx, %%edi\n"

		"  movl $blockend2, %%eax\n"
		"  subl $block2, %%eax\n"
		"  shrl $4, %%eax\n"

		"  mulb %%cl\n"
		"  addl $block2, %%eax\n"
		"  popl %%ecx\n"
		"  shrl $4, %%ecx\n"
		"  incl %%ecx\n"
		"  jmp *%%eax\n"

		"block2:\n"
/*
		"  nop\n"
		"  nop\n" */
		".macro dummy2 from=0, to=60\n"
		"  movb 1+\\from(%%esi), %%dl\n"
		"  movl (%%ebx,%%edx,4), %%ebp\n"
		"  movl 1024(%%ebx,%%edx,2), %%eax\n"
		".byte 0x8A\n.byte 0x56\n.byte \\from\n" /* since some gcc/binutils patch-sets optimze away 0 offset, we do this "  movb \\from(%%esi), %%dl\n"*/
		"  addl (%%ebp,%%edx,2), %%eax\n"
		".byte 0x66\n.byte 0x89\n.byte 0x47\n.byte \\from\n" /* since some gcc/binutils patct-sets optimze away 0 offset, we do this "movw %%ax, \\from(%%edi)\n" */
		".if \\to-\\from\n"
		" dummy2 \"(\\from+4)\",\\to\n"
		".endif\n"
		".endm\n"
		" dummy2\n"

		"blockend2:\n"
		"  addl $64, %%esi\n"
		"  addl $64, %%edi\n"
		"  decl %%ecx\n"
		"  jnz block2\n"

		"  popl %%ebp\n"
#ifdef __PIC__
		"movl  %%ebx, %%edx\n"
		"popl %%ebx\n"
		: "=&S"(d0), "=&D"(d1), "=&c"(d2)
		: "0" (src), "1" (dst), "d" (tab), "2" (len)
		: "memory", "eax"
#else
		: "=&S"(d0), "=&D"(d1), "=&c"(d2)
		: "0" (src), "1" (dst), "b" (tab), "2" (len)
		: "memory", "eax", "edx"
#endif

	);
}

#else

void mixCalcClipTab(uint16_t *ct, int32_t amp)
{
	signed long i,j,a,b;

	a=-amp;
	for (i=0; i<256; i++)
		ct[i+768]=(a+=amp)>>16;

	for (i=0; i<256; i++)
		ct[i+1024]=0;

	b=0x800000-(amp<<7);
	for (i=0; i<256; i++)
	{
		if (b<0x000000)
		{
			if ((b+amp)<0x000000)
			{
				ct[i]=1024;
				ct[i+512]=0x0000;
			} else {
				a=0;
				for (j=0; j<256; j++)
				{
					ct[j+1280]=(((a>>8)+b)<0x000000)?0x0000:(((a>>8)+b)>>8);
					a+=amp;
				}
				ct[i]=1280;
				ct[i+512]=0x0000;
			}
		} else if ((b+amp)>0xFFFFFF)
		{
			if (b>0xFFFFFF)
			{
				ct[i]=1024;
				ct[i+512]=0xFFFF;
			} else {
				a=0;
				for (j=0; j<256; j++)
				{
					ct[j+1536]=(((a>>8)+b)>0xFFFFFF)?0x0000:((((a>>8)+b)>>8)+1);
					a+=amp;
				}
				ct[i]=1536;
				ct[i+512]=0xFFFF;
			}
		} else {
			ct[i]=768;
			ct[i+512]=b>>8;
		}
		b+=amp;
	}
}

void mixClipAlt(uint16_t *dst, const uint16_t *src, uint32_t len, const uint16_t *tab)
{
	while (len)
	{
		const uint16_t *tabfine=tab+tab[(*src)>>8];
		*dst=tab[512+((*src)>>8)]+tabfine[(*src)&0xff];
		dst++;
		src++;
		len--;
	}
}

void mixClipAlt2(uint16_t *dst, const uint16_t *src, uint32_t len, const uint16_t *tab)
{
	while (len)
	{
		const uint16_t *tabfine=tab+tab[(*src)>>8];
		*dst=tab[512+((*src)>>8)]+tabfine[(*src)&0xff];
		dst+=2;
		src+=2;
		len--;
	}
}

#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "mixclip", .desc = "OpenCP common clipper for various streams (c) 1994-10 Niklas Beisert, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
