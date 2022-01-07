/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "types.h"
#include "boot/plinkman.h"
#include "mchasm.h"

#ifdef I386_ASM
uint32_t RP2 mixAddAbs16M(const void *ch, uint32_t len) /* EAX, EDX */
{
	int d0;
	register uint32_t retval;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl %%eax, %%esi      \n"
		"  shll $1, %%edx         \n"
		"  xorl %%eax, %%eax      \n"
		"  addl %%esi, %%edx      \n"
		"  jmp mixAddAbs16M_loop  \n"
		"mixAddAbs16M_neg:        \n"
		"    subl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jae mixAddAbs16M_loopend\n"
		"mixAddAbs16M_loop:       \n"
		"    movswl (%%esi), %%ebx\n"
		"    addl $2, %%esi       \n"
		"    xorl $0xffff8000, %%ebx\n"
		"    js mixAddAbs16M_neg  \n"
		"    addl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jb mixAddAbs16M_loop   \n"
		"mixAddAbs16M_loopend:    \n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=a" (retval), "=&d"(d0)
		: "0"(ch), "1"(len)
#ifdef __PIC__
		: "memory", "esi"
#else
		: "memory", "ebx", "esi"
#endif
	);
	return retval;
}

uint32_t RP2 mixAddAbs16MS(const void *ch, uint32_t len) /* EAX, EDX */
{
	int d0;
	register uint32_t retval;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl %%eax, %%esi      \n"
		"  shll $1, %%edx         \n"
		"  xorl %%eax, %%eax      \n"
		"  addl %%esi, %%edx      \n"
		"  jmp mixAddAbs16MS_loop  \n"
		"mixAddAbs16MS_neg:        \n"
		"    subl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jae mixAddAbs16MS_loopend\n"
		"mixAddAbs16MS_loop:       \n"
		"    movswl (%%esi), %%ebx\n"
		"    addl $2, %%esi       \n"
		"    testl $0x80000000, %%ebx\n"
		"    js mixAddAbs16MS_neg  \n"
		"    addl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jb mixAddAbs16MS_loop   \n"
		"mixAddAbs16MS_loopend:    \n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=a" (retval), "=&d"(d0)
		: "0" (ch), "1" (len)
#ifdef __PIC__
		: "memory", "esi"
#else
		: "memory", "ebx", "esi"
#endif
	);
	return retval;
}

uint32_t RP2 mixAddAbs16SS(const void *ch, uint32_t len) /* EAX, EDX */
{
	int d0;
	register uint32_t retval;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl %%eax, %%esi      \n"
		"  shll $2, %%edx         \n"
		"  xorl %%eax, %%eax      \n"
		"  addl %%esi, %%edx      \n"
		"  jmp mixAddAbs16SS_loop  \n"
		"mixAddAbs16SS_neg:        \n"
		"    subl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jae mixAddAbs16SS_loopend\n"
		"mixAddAbs16SS_loop:       \n"
		"    movswl (%%esi), %%ebx\n"
		"    addl $4, %%esi       \n"
		"    testl $0x80000000, %%ebx\n"
		"    js mixAddAbs16SS_neg  \n"
		"    addl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jb mixAddAbs16SS_loop   \n"
		"mixAddAbs16SS_loopend:    \n"
#ifdef __PIC__
		"popl  %%ebx\n"
#endif
		: "=a" (retval), "=&d"(d0)
		: "0" (ch), "1" (len)
#ifdef __PIC__
		: "memory", "esi"
#else
		: "memory", "ebx", "esi"
#endif
	);
	return retval;
}

uint32_t RP2 mixAddAbs16S(const void *ch, uint32_t len) /* EAX, EDX */
{
	int d0;
	register uint32_t retval;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl %%eax, %%esi      \n"
		"  shll $2, %%edx         \n"
		"  xorl %%eax, %%eax      \n"
		"  addl %%esi, %%edx      \n"
		"  jmp mixAddAbs16S_loop  \n"
		"mixAddAbs16S_neg:        \n"
		"    subl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jae mixAddAbs16MS_loopend\n"
		"mixAddAbs16S_loop:       \n"
		"    movswl (%%esi), %%ebx\n"
		"    addl $4, %%esi       \n"
		"    xorl $0xffff8000, %%ebx\n"
		"    js mixAddAbs16S_neg  \n"
		"    addl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jb mixAddAbs16S_loop   \n"
		"mixAddAbs16S_loopend:    \n"
#ifdef __PIC__
		"popl  %%ebx\n"
#endif
		: "=a" (retval), "=&d"(d0)
		: "0" (ch), "1" (len)
#ifdef __PIC__
		: "memory", "esi"
#else
		: "memory", "ebx", "esi"
#endif
	);
	return retval;
}

uint32_t RP2 mixAddAbs8M(const void *ch, uint32_t len) /* EAX, EDX */
{
	int d0;
	register uint32_t retval;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl %%eax, %%esi      \n"
		"  xorl %%eax, %%eax      \n"
		"  addl %%esi, %%edx      \n"
		"  jmp mixAddAbs8M_loop  \n"
		"mixAddAbs8M_neg:        \n"
		"    subl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jae mixAddAbs8M_loopend\n"
		"mixAddAbs8M_loop:       \n"
		"    movsbl (%%esi), %%ebx\n"
		"    incl %%esi         \n"
		"    xorl $0xffffff80, %%ebx\n"
		"    js mixAddAbs8M_neg  \n"
		"    addl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jb mixAddAbs8M_loop   \n"
		"mixAddAbs8M_loopend:    \n"
#ifdef __PIC__
		"popl  %%ebx\n"
#endif
		: "=a" (retval), "=&D"(d0)
		: "0" (ch), "1" (len)
#ifdef __PIC__
		: "memory", "esi"
#else
		: "memory", "ebx", "esi"
#endif
	);
	return retval;
}

uint32_t RP2 mixAddAbs8MS(const void *ch, uint32_t len) /* EAX, EDX */
{
	int d0;
	register uint32_t retval;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl %%eax, %%esi      \n"
		"  xorl %%eax, %%eax      \n"
		"  addl %%esi, %%edx      \n"
		"  jmp mixAddAbs8MS_loop  \n"
		"mixAddAbs8MS_neg:        \n"
		"    subl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jae mixAddAbs8MS_loopend\n"
		"mixAddAbs8MS_loop:       \n"
		"    movsbl (%%esi), %%ebx\n"
		"    incl %%esi         \n"
		"    testl $0x80000000, %%ebx\n"
		"    js mixAddAbs8MS_neg  \n"
		"    addl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jb mixAddAbs8MS_loop   \n"
		"mixAddAbs8MS_loopend:    \n"
#ifdef __PIC__
		"popl  %%ebx\n"
#endif
		: "=a" (retval), "=&d"(d0)
		: "0" (ch), "1" (len)
#ifdef __PIC__
		: "memory", "esi"
#else
		: "memory", "ebx", "esi"
#endif
	);
	return retval;
}

uint32_t RP2 mixAddAbs8S(const void *ch, uint32_t len) /* EAX, EDX */
{
	int d0;
	register uint32_t retval;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl %%eax, %%esi      \n"
		"  shl $1, %%edx          \n"
		"  xorl %%eax, %%eax      \n"
		"  addl %%esi, %%edx      \n"
		"  jmp mixAddAbs8S_loop  \n"
		"mixAddAbs8S_neg:        \n"
		"    subl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jae mixAddAbs8S_loopend\n"
		"mixAddAbs8S_loop:       \n"
		"    movsbl (%%esi), %%ebx\n"
		"    addl $2, %%esi       \n"
		"    xorl $0xffffff80, %%ebx\n"
		"    js mixAddAbs8S_neg  \n"
		"    addl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jb mixAddAbs8S_loop   \n"
		"mixAddAbs8S_loopend:    \n"
#ifdef __PIC__
		"popl  %%ebx\n"
#endif
		: "=a" (retval), "=&d"(d0)
		: "0" (ch), "1" (len)
#ifdef __PIC__
		: "memory", "esi"
#else
		: "memory", "ebx", "esi"
#endif
	);
	return retval;
}

uint32_t RP2 mixAddAbs8SS(const void *ch, uint32_t len) /* EAX, EDX */
{
	int d0;
	register uint32_t retval;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl %%eax, %%esi      \n"
		"  shll $1, %%edx         \n"
		"  xorl %%eax, %%eax      \n"
		"  addl %%esi, %%edx      \n"
		"  jmp mixAddAbs8SS_loop  \n"
		"mixAddAbs8SS_neg:        \n"
		"    subl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jae mixAddAbs8SS_loopend\n"
		"mixAddAbs8SS_loop:       \n"
		"    movsbl (%%esi), %%ebx\n"
		"    addl $2, %%esi       \n"
		"    testl $0x80000000, %%ebx\n"
		"    js mixAddAbs8SS_neg  \n"
		"    addl %%ebx, %%eax    \n"
		"  cmpl %%edx, %%esi      \n"
		"  jb mixAddAbs8SS_loop   \n"
		"mixAddAbs8SS_loopend:    \n"
#ifdef __PIC__
		"popl  %%ebx\n"
#endif
		: "=a" (retval), "=&d"(d0)
		: "0" (ch), "1" (len)
#ifdef __PIC__
		: "memory", "esi"
#else
		: "memory", "ebx", "esi"
#endif
	);
	return retval;
}

#else /* I386_ASM */

uint32_t RP2 mixAddAbs16M(const void *ch, uint32_t len)
{ /* Mono, 16bit, unsigned */
	uint32_t retval=0;
	const uint16_t *ref=ch;

	while (len)
	{
		if (*ref<0x8000)
			retval+=0x8000-*ref;
		else
			retval+=-0x8000+*ref;
		ref++;
		len--;
	}
	return retval;
}

uint32_t RP2 mixAddAbs16MS(const void *ch, uint32_t len)
{ /* Mono, 16bit, signed */
	uint32_t retval=0;
	const int16_t *ref=ch;

	while (len)
	{
		if (*ref<0)
			retval-=*ref;
		else
			retval+=*ref;
		ref++;
		len--;
	}
	return retval;
}

uint32_t RP2 mixAddAbs16S(const void *ch, uint32_t len)
{ /* Stereo, 16bit, unsigned */
	uint32_t retval=0;
	const uint16_t *ref=ch;

	while (len)
	{
		if (*ref<0x8000)
			retval+=0x8000-*ref;
		else
			retval+=-0x8000+*ref;
		ref+=2;
		len--;
	}
	return retval;
}

uint32_t RP2 mixAddAbs16SS(const void *ch, uint32_t len)
{ /* Stereo, 16bit, signed */
	uint32_t retval=0;
	const int16_t *ref=ch;

	while (len)
	{
		if (*ref<0)
			retval-=*ref;
		else
			retval+=*ref;
		ref+=2;
		len--;
	}
	return retval;
}

uint32_t RP2 mixAddAbs8M(const void *ch, uint32_t len)
{ /* Mono, 8bit, unsigned */
	uint32_t retval=0;
	const uint8_t *ref=ch;

	while (len)
	{
		if (*ref<0x80)
			retval+=0x80-*ref;
		else
			retval+=-0x80+*ref;
		ref++;
		len--;
	}
	return retval;
}

uint32_t RP2 mixAddAbs8MS(const void *ch, uint32_t len)
{ /* Mono, 8bit, signed */
	uint32_t retval=0;
	const int8_t *ref=ch;

	while (len)
	{
		if (*ref<0)
			retval-=*ref;
		else
			retval+=*ref;
		ref++;
		len--;
	}
	return retval;
}

uint32_t RP2 mixAddAbs8S(const void *ch, uint32_t len)
{ /* Stereo, 8bit, unsigned */
	uint32_t retval=0;
	const uint8_t *ref=ch;

	while (len)
	{
		if (*ref<0x80)
			retval+=0x80-*ref;
		else
			retval+=-0x80+*ref;
		ref+=2;
		len--;
	}
	return retval;
}

uint32_t RP2 mixAddAbs8SS(const void *ch, uint32_t len)
{ /* Stereo, 8bit, signed */
	uint32_t retval=0;
	const int8_t *ref=ch;

	while (len)
	{
		if (*ref<0)
			retval-=*ref;
		else
			retval+=*ref;
		ref+=2;
		len--;
	}
	return retval;
}
#endif

/********************************************************************/


#ifdef I386_ASM
void RP3 mixGetMasterSampleMS8M(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz mixGetMasterSampleMS8M_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  xorb %%dl, %%dl\n"

		"mixGetMasterSampleMS8M_lp:\n"
		"    movb (%%esi), %%dh\n"
		"    addl %%ebp, %%edi\n"
		"    movw %%dx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    addl $2, %%eax\n"
		"  dec %%ecx\n"
		"  jnz mixGetMasterSampleMS8M_lp\n"
		"  pop %%ebp\n"
		"mixGetMasterSampleMS8M_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleMU8M(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz mixGetMasterSampleMU8M_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  xorb %%dl, %%dl\n"

		"mixGetMasterSampleMU8M_lp:\n"
		"    movb (%%esi), %%dh\n"
		"    addl %%ebp, %%edi\n"
		"    adcl %%ebx, %%esi\n"
		"    xorb $0x80, %%dh\n"
		"    movw %%dx, (%%eax)\n"
		"    addl $2, %%eax\n"
		"  dec %%ecx\n"
		"  jnz mixGetMasterSampleMU8M_lp\n"
		"  pop %%ebp\n"
		"mixGetMasterSampleMU8M_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleMS8S(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz mixGetMasterSampleMS8S_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  xorb %%dl, %%dl\n"

		"mixGetMasterSampleMS8S_lp:\n"
		"    addl %%ebp, %%edi\n"
		"    movb (%%esi), %%dh\n"
		"    adcl %%ebx, %%esi\n"
		"    movw %%dx, (%%eax)\n"
		"    movw %%dx, 2(%%eax)\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz mixGetMasterSampleMS8S_lp\n"
		"  pop %%ebp\n"
		"mixGetMasterSampleMS8S_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleMU8S(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz mixGetMasterSampleMU8S_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  xorb %%dl, %%dl\n"

		"mixGetMasterSampleMU8S_lp:\n"
		"    addl %%ebp, %%edi\n"
		"    movb (%%esi), %%dh\n"
		"    adcl %%ebx, %%esi\n"
		"    xorb $0x80, %%dh\n"
		"    movw %%dx, (%%eax)\n"
		"    movw %%dx, 2(%%eax)\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz mixGetMasterSampleMU8S_lp\n"
		"  pop %%ebp\n"
		"mixGetMasterSampleMU8S_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSS8M(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SS8M_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  shrl $1, %%esi\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"SS8M_lp:\n"
		"    xorl %%edx, %%edx\n"
		"    movb (%%esi,%%esi), %%dh\n"
		"    addb 1(%%esi,%%esi), %%dh\n"
		"    clc\n"
		"    jnl SS8M_o\n"
		"      stc\n"
		"SS8M_o:\n"
		"    rcrw %%dx\n"
		"    addl %%ebp, %%edi\n"
		"    adcl %%ebx, %%esi\n"
		"    movw %%dx, (%%eax)\n"
		"    addl $2, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SS8M_lp\n"
		"  pop %%ebp\n"
		"SS8M_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSU8M(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SU8M_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  shrl $1, %%esi\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"SU8M_lp:\n"
		"    xorl %%edx, %%edx\n"
		"    movb (%%esi,%%esi), %%dh\n"
		"    addb 1(%%esi,%%esi), %%dh\n"
		"    clc\n"
		"    jnl SU8M_o\n"
		"      stc\n"
		"SU8M_o:\n"
		"    rcrw %%dx\n"
		"    xorb $0x80, %%dh\n"
		"    addl %%ebp, %%edi\n"
		"    adcl %%ebx, %%esi\n"
		"    movw %%dx, (%%eax)\n"
		"    addl $2, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SU8M_lp\n"
		"  pop %%ebp\n"
		"SU8M_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSS8S(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SS8S_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  shrl $1, %%esi\n"
		"  xorl %%edi, %%edi\n"
		"  xorl %%edx, %%edx\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"SS8S_lp:\n"
		"    movb 1(%%esi,%%esi), %%dh\n"
		"    roll $16, %%edx\n"
		"    movb (%%esi,%%esi), %%dh\n"
		"    addl %%ebp, %%edi\n"
		"    movl %%edx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SS8S_lp\n"
		"  pop %%ebp\n"
		"SS8S_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSU8S(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SU8S_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  shrl $1, %%esi\n"
		"  xorl %%edi, %%edi\n"
		"  xorl %%edx, %%edx\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"SU8S_lp:\n"
		"    movb 1(%%esi,%%esi), %%dh\n"
		"    roll $16, %%edx\n"
		"    movb (%%esi,%%esi), %%dh\n"
		"    xorl $0x80008000, %%edx\n"
		"    addl %%ebp, %%edi\n"
		"    movl %%edx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SU8S_lp\n"
		"  pop %%ebp\n"
		"SU8S_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSS8SR(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SS8SR_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  shrl $1, %%esi\n"
		"  xorl %%edi, %%edi\n"
		"  xorl %%edx, %%edx\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"SS8SR_lp:\n"
		"    movb (%%esi,%%esi), %%dh\n"
		"    roll $16, %%edx\n"
		"    movb 1(%%esi,%%esi), %%dh\n"
		"    addl %%ebp, %%edi\n"
		"    movl %%edx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SS8SR_lp\n"
		"  pop %%ebp\n"
		"SS8SR_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSU8SR(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SU8SR_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  shrl $1, %%esi\n"
		"  xorl %%edi, %%edi\n"
		"  xorl %%edx, %%edx\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"SU8SR_lp:\n"
		"    movb (%%esi,%%esi), %%dh\n"
		"    roll $16, %%edx\n"
		"    movb 1(%%esi,%%esi), %%dh\n"
		"    xorl $0x80008000, %%edx\n"
		"    addl %%ebp, %%edi\n"
		"    movl %%edx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SU8SR_lp\n"
		"  pop %%ebp\n"
		"SU8SR_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleMS16M(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz mixGetMasterSampleMS16M_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  shrl $1, %%esi\n"

		"mixGetMasterSampleMS16M_lp:\n"
		"    movl (%%esi,%%esi), %%edx\n"
		"    addl %%ebp, %%edi\n"
		"    movw %%dx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    addl $2, %%eax\n"
		"  dec %%ecx\n"
		"  jnz mixGetMasterSampleMS16M_lp\n"
		"  pop %%ebp\n"
		"mixGetMasterSampleMS16M_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleMU16M(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz mixGetMasterSampleMU16M_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  shrl $1, %%esi\n"

		"mixGetMasterSampleMU16M_lp:\n"
		"    addl %%ebp, %%edi\n"
		"    movl (%%esi,%%esi), %%edx\n"
		"    adcl %%ebx, %%esi\n"
		"    xorb $0x80, %%dh\n"
		"    movw %%dx, (%%eax)\n"
		"    addl $2, %%eax\n"
		"  dec %%ecx\n"
		"  jnz mixGetMasterSampleMU16M_lp\n"
		"  pop %%ebp\n"
		"mixGetMasterSampleMU16M_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleMS16S(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz mixGetMasterSampleMS16S_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  shrl $1, %%esi\n"

		"mixGetMasterSampleMS16S_lp:\n"
		"    movl (%%esi,%%esi), %%edx\n"
		"    addl %%ebp, %%edi\n"
		"    movw %%dx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    movw %%dx, 2(%%eax)\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz mixGetMasterSampleMS16S_lp\n"
		"  pop %%ebp\n"
		"mixGetMasterSampleMS16S_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleMU16S(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz mixGetMasterSampleMU16S_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  shrl $1, %%esi\n"

		"mixGetMasterSampleMU16S_lp:\n"
		"    addl %%ebp, %%edi\n"
		"    movl (%%esi,%%esi), %%edx\n"
		"    adcl %%ebx, %%esi\n"
		"    xorb $0x80, %%dh\n"
		"    movw %%dx, (%%eax)\n"
		"    movw %%dx, 2(%%eax)\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz mixGetMasterSampleMU16S_lp\n"
		"  pop %%ebp\n"
		"mixGetMasterSampleMU16S_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSS16M(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SS16M_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  shrl $2, %%esi\n"

		"SS16M_lp:\n"
		"    movl -2(,%%esi,4), %%edx\n"
		"    addl (,%%esi,4), %%edx\n"
		"    clc\n"
		"    jnl SS16M_o\n"
		"      stc\n"
		"SS16M_o:\n"
		"    rcrl $17, %%edx\n"
		"    addl %%ebp, %%edi\n"
		"    movw %%dx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    addl $2, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SS16M_lp\n"
		"  pop %%ebp\n"
		"SS16M_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSU16M(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SU16M_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  xorl %%edx, %%edx\n"
		"  shrl $2, %%esi\n"

		"SU16M_lp:\n"
		"    movw (,%%esi,4), %%dx\n"
		"    addw 2(,%%esi,4), %%dx\n"
		"    rcrw $1, %%dx\n"
		"    addl %%ebp, %%edi\n"
		"    adcl %%ebx, %%esi\n"
		"    xorb $0x80, %%dh\n"
		"    movw %%dx, (%%eax)\n"
		"    addl $2, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SU16M_lp\n"
		"  pop %%ebp\n"
		"SU16M_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSS16S(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SS16S_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  shrl $2, %%esi\n"

		"SS16S_lp:\n"
		"    movl (,%%esi,4), %%edx\n"
		"    addl %%ebp, %%edi\n"
		"    movl %%edx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SS16S_lp\n"
		"  pop %%ebp\n"
		"SS16S_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSU16S(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SU16S_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  shrl $2, %%esi\n"

		"SU16S_lp:\n"
		"    movl (,%%esi,4), %%edx\n"
		"    addl %%ebp, %%edi\n"
		"    adcl %%ebx, %%esi\n"
		"    xorl $0x80008000, %%edx\n"
		"    movl %%edx, (%%eax)\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SU16S_lp\n"
		"  pop %%ebp\n"
		"SU16S_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSS16SR(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SS16SR_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  shrl $2, %%esi\n"

		"SS16SR_lp:\n"
		"    movl (,%%esi,4), %%edx\n"
		"    roll $16, %%edx\n"
		"    addl %%ebp, %%edi\n"
		"    movl %%edx, (%%eax)\n"
		"    adcl %%ebx, %%esi\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SS16SR_lp\n"
		"  pop %%ebp\n"
		"SS16SR_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

void RP3 mixGetMasterSampleSU16SR(int16_t *dst, const void *src, uint32_t len, uint32_t step)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%esi, %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  jz SU16SR_done\n"
		"  pushl %%ebp\n"
		"  movl %%edx, %%esi\n"
		"  movl %%ebx, %%ebp\n"
		"  xorl %%edi, %%edi\n"
		"  shll $16, %%ebp\n"
		"  shrl $16, %%ebx\n"
		"  shrl $2, %%esi\n"

		"SU16SR_lp:\n"
		"    movl (,%%esi,4), %%edx\n"
		"    addl %%ebp, %%edi\n"
		"    adcl %%ebx, %%esi\n"
		"    xorl $0x80008000, %%edx\n"
		"    roll $16, %%edx\n"
		"    movl %%edx, (%%eax)\n"
		"    addl $4, %%eax\n"
		"  dec %%ecx\n"
		"  jnz SU16SR_lp\n"
		"  pop %%ebp\n"
		"SU16SR_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
		: "=&a"(d0), "=&c"(d1), "=&S"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi"
#else
		: "=&a"(d0), "=&c"(d1), "=&b"(d2)
		: "0" (dst), "d" (src), "1" (len), "2" (step)
		: "memory", "edi", "esi"
#endif
	);
}

#else /* I386_ASM */

void RP3 mixGetMasterSampleMS8M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=step>>16;
	do {
		*dst=(*src)<<8;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src++;
		}
		dst++;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleMU8M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	uint16_t *dst=(uint16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=step>>16;
	do {
		*dst=((*src)<<8)-0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src++;
		}
		dst++;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleMS8S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=step>>16;
	do {
		dst[1]=dst[0]=(*src)<<8;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src++;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleMU8S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	uint16_t *dst=(uint16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=step>>16;
	do {
		dst[1]=dst[0]=((*src)<<8)+0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src++;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSS8M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		*dst=(src[0]+src[1])<<7;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst++;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSU8M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	uint16_t *dst=(uint16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		*dst=((src[0]+src[1])<<7)+0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst++;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSS8S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		dst[0]=src[0]<<8;
		dst[1]=src[1]<<8;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSU8S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	uint16_t *dst=(uint16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		dst[0]=(src[0]<<8)+0x8000;
		dst[1]=(src[1]<<8)+0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSS8SR(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		dst[0]=src[1]<<8;
		dst[1]=src[0]<<8;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSU8SR(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int8_t *src=(int8_t *)_src;
	uint16_t *dst=(uint16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		dst[0]=(src[1]<<8)+0x8000;
		dst[1]=(src[0]<<8)+0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleMS16M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int16_t *src=(int16_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=step>>16;
	do {
		*dst=*src;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src++;
		}
		dst+=1;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleMU16M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int16_t *src=(int16_t *)_src;
	uint16_t *dst=(uint16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=step>>16;
	do {
		*dst=*src+0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src++;
		}
		dst+=1;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleMS16S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int16_t *src=(int16_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=step>>16;
	do {
		dst[0]=dst[1]=*src;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src++;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleMU16S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int16_t *src=(int16_t *)_src;
	uint16_t *dst=(uint16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=step>>16;
	do {
		dst[0]=dst[1]=*src-0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src++;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSS16M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int16_t *src=(int16_t *)_src;
	int16_t *dst=(int16_t *)_dst;

	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		*dst=(src[0]+src[1])>>1;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=1;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSU16M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	uint16_t *src=(uint16_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		*dst=((src[0]+src[1])>>1)+0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=1;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSS16S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int16_t *src=(int16_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		dst[0]=src[0];
		dst[1]=src[1];
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSU16S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int16_t *src=(int16_t *)_src;
	uint16_t *dst=(uint16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		dst[0]=src[0]+0x8000;
		dst[1]=src[1]+0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSS16SR(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int16_t *src=(int16_t *)_src;
	int16_t *dst=(int16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {

		dst[0]=src[1];
		dst[1]=src[0];
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=2;
		len--;
	} while(len);
}

void RP3 mixGetMasterSampleSU16SR(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
{
	uint32_t addfixed;
	uint32_t addfloat;
	uint32_t addfloatcounter;
	int16_t *src=(int16_t *)_src;
	uint16_t *dst=(uint16_t *)_dst;
	if (!len)
		return;
	addfloatcounter=0;
	addfloat=step&0xffff;
	addfixed=(step>>15)&0xfffe;
	do {
		dst[0]=src[1]+0x8000;
		dst[1]=src[0]+0x8000;
		src+=addfixed;
		if ((addfloatcounter+=addfloat)&0xffff0000)
		{
			addfloatcounter&=0xffff;
			src+=2;
		}
		dst+=2;
		len--;
	} while(len);
}

#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "mchasm", .desc = "OpenCP Player/Sampler Auxiliary Routines (c) 1994-'22 Niklas Beisert, Tammo Hinrichs", .ver = DLLVERSION, .size = 0};
