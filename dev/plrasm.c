/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * aux assembler routines for player devices system
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
 *    -partially ported assembler to gcc
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -made the partially assembler optimize safe
 */


#include "config.h"
#include "types.h"
#include "plrasm.h"

#ifdef I386_ASM

void plrClearBuf(void *buf, int len, int unsign)
{
	int d0, d1;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  cmpl $0, %%ecx\n"
		"  je plrClearBuf_done\n"
		"  cmpl $0, %%eax\n"
		"  je plrClearBuf_signed\n"
		"  mov $0x80008000, %%eax\n"
		"plrClearBuf_signed:\n"
		"  testl $2, %%ebx\n"
		"  jz plrClearBuf_ok1\n"
		"  stosw\n"
		"  decl %%ecx\n"
		"plrClearBuf_ok1:\n"
		"  shrl $1, %%ecx\n"
		"  jnc plrClearBuf_ok2\n"
		"  movw %%ax, (%%edi,%%ecx,4)\n"
		"plrClearBuf_ok2:\n"
		"  rep\n"
		"  stosl\n"
		"plrClearBuf_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&c"(d0), "=&a"(d1)
		: "D" ((long)buf), "0" (len), "1" (unsign)
#ifdef __PIC__
		: "memory"
#else
		: "memory", "ebx"
#endif
	);
}

#else

void plrClearBuf(void *buf, int len, int unsign)
{
	uint32_t fill;
	if (unsign)
		fill=0;
	else
		fill=0x80008000;
	while (len>1)
	{
		*(uint32_t *)buf=fill;
		buf=(char *)buf+sizeof(fill);
		len-=2;
	}
	if (len)
		*(uint16_t *)buf=(uint16_t)fill;
}

#endif

void plr16to8(uint8_t *dst, const uint16_t *src, unsigned long len)
{
	while (len)
	{
		*dst=(*src)>>8;
		len--;
	}
}
/*
blockbeg2 macro exp
blockexp=exp
blocksize=(1 shl exp)
  push ecx
  and ecx,blocksize-1
  sub ecx,blocksize
  neg ecx
  sub esi,ecx
  sub esi,ecx
  sub edi,ecx
  mov eax,(@@blockend-@@block) shr blockexp
  mul cl
  add eax,offset @@block
  pop ecx
  shr ecx,blockexp
  inc ecx
  jmp eax
@@block:
endm

blockend2 macro
@@blockend:
  add esi,blocksize*2
  add edi,blocksize
  dec ecx
  jnz @@block
endm

public plr16to8_
plr16to8_ proc ;//esi=buf16, edi=buf8, ecx=len
  blockbeg2 6
    i=0
    rept blocksize
      db 8ah, 46h, 2*i+1     ;mov al,ds:[esi+2*i+1]
      db 88h, 47h, i         ;mov ds:[edi+i],al
      i=i+1
    endm
  blockend2
  ret
endp

end
*/
