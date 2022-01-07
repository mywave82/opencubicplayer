/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Assembler/C for the sample processing routines (compression, mixer
 * preparation etc)
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
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -first release (splitted out from smpman.cpp)
 *    -rewrote assembler to gcc
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -made assembler optimitize safe
 */

#ifdef I386_ASM

static inline unsigned long getpitch(void *ptr, unsigned long len)
{
	int d0, d1;
	register unsigned long ret;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  xorl %%edx, %%edx\n"
		"  xorl %%ebx, %%ebx\n"
		"  xorl %%eax, %%eax\n"
		"1:\n"
		"  movb (%%esi), %%dl\n"
		"  movb 1(%%esi), %%dh\n"
		"  xorl $0x8080, %%edx\n"
		"  subb %%dh, %%dl\n"
		"  sbbb %%dh, %%dh\n"
		"  incb %%dh\n"
		"  movw (%%ecx, %%edx, 2), %%bx\n"
		"  addl %%ebx, %%eax\n"
		"  incl %%esi\n"
		"  decl %%edi\n"
		"jnz 1b\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=a" (ret), "=&S"(d0), "=&D"(d1)
		: "1" (ptr), "c" (abstab[0]), "2" (len)
#ifdef __PIC__
		: "edx"
#else
		: "edx", "ebx"
#endif
	);
	return ret;
}

static inline unsigned long getpitch16(void *ptr, unsigned long len)
{
	int d0, d1;
	register unsigned long ret;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  xorl %%edx, %%edx\n"
		"  xorl %%ebx, %%ebx\n"
		"  xorl %%eax, %%eax\n"
		"1:\n"
		"  movb 1(%%esi), %%dl\n"
		"  movb 3(%%esi), %%dh\n"
		"  xorl $0x8080, %%edx\n"
		"  subb %%dh, %%dl\n"
		"  sbbb %%dh, %%dh\n"
		"  incb %%dh\n"
		"  movw (%%ecx, %%edx, 2), %%bx\n"
		"  addl %%ebx, %%eax\n"
		"  addl $2, %%esi\n"
		"  decl %%edi\n"
		"  jnz 1b\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=a" (ret), "=&S"(d0), "=&D"(d1)
		: "1" (ptr), "c" (abstab[0]), "2" (len)
#ifdef __PIC__
		: "edx"
#else
		: "edx", "ebx"
#endif
	);
	return ret;
}

#else

static uint32_t getpitch16(const void *ptr, unsigned long len)
{
	uint32_t retval=0;
	do {
		uint8_t dl, dh;
		dl=((uint8_t *)ptr)[1]^0x80;
		dh=((uint8_t *)ptr)[3]^0x80;
		if ((dh>dl))
		{
			dl-=dh;
			dh=0;
		} else {
			dl-=dh;
			dh=1;
		}
		retval+=abstab[(dh<<8)+dl];
		ptr=((uint8_t *)ptr)+2;
	} while (--len);
	return retval;
}

static uint32_t getpitch(const void *ptr, unsigned long len)
{
	uint32_t retval=0;
	do {
		uint8_t dl, dh;
		dl=((int8_t *)ptr)[0]^0x80;
		dh=((int8_t *)ptr)[1]^0x80;
		if ((dh>dl))
		{
			dl-=dh;
			dh=0;
		} else {
			dl-=dh;
			dh=1;
		}
		retval+=abstab[(dh<<8)+dl];
		ptr=((int8_t *)ptr)+1;
	} while (--len);
	return retval;
}

#endif
