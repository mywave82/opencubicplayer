/* OpenCP Module Player
 * copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * DOS4GFIX initialisation handlers
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb98????   Tammo Hinrichs <opencp@gmx.net>                 (?)
 *    -some bugfix(?)
 *  -fd981205   Felix Domke <tmbinc@gmx.net>
 *    -KB's "bugfix" doesn't work with Watcom106
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -Redone to use gcc assembler inline and posix libc
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -Made the assembler inlines optimize-safe
 */

#ifndef IMSRTNS__H
#define IMSRTNS__H

/* Assembler optimization per cpu we can wait with.... */

#define memsetb(dst, what, len) memset(dst, what, len*1) /* let the compiler handle this */
#define memcpyb(dst, src, len) memcpy(dst, src, len) /* let the compiler handle this */
#define memcpyf(dst, src, len) memcpy(dst, src, len*sizeof(float)) /* let the compiler handle this.. even this one... not even used yet */
#define memmovel(dst, src, n) memmove(dst, src, (n)*4)

#ifdef I386_ASM

static inline int32_t imuldiv(int32_t a,int32_t b,int32_t c)
{
#ifdef __PIC__
	int d0, d1;
	register int32_t retval;
	__asm__ __volatile__(
		"pushl %%ebx\n"
		"movl %%ecx, %%ebx\n"
		"imul %%edx\n"
		"idiv %%ebx\n"
		"popl %%ebx\n"
		:"=a"(retval), "=&d"(d0), "=&c"(d1)
		:"0"(a), "1"(b), "2"(c)
	);
	return retval;
#else
	int d0, d1;
	register int32_t retval;
	__asm__ __volatile__(
		"imul %%edx\n"
		"idiv %%ebx\n"
		:"=a"(retval), "=&d"(d0), "=&b"(d1)
		:"0"(a), "1"(b), "2"(c)
	);
	return retval;
#endif
}

static inline uint32_t umuldiv(uint32_t a,uint32_t b,uint32_t c)
{
#ifdef __PIC__
	int d0, d1;
	register uint32_t retval;
	__asm__ __volatile__
	(
		"pushl %%ebx\n"
		"movl %%ecx, %%ebx\n"
		"mul %%edx\n"
		"div %%ebx\n"
		"popl %%ebx\n"
		:"=a"(retval), "=&d"(d0), "=&c"(d1)
		:"0"(a), "1"(b), "2"(c)
	);
	return retval;
#else
	int d0, d1;
	register uint32_t retval;
	__asm__ __volatile__
	(
		"mul %%edx\n"
		"div %%ebx\n"
		:"=a"(retval), "=&d"(d0), "=&b"(d1)
		:"0"(a), "1"(b), "2"(c)
	);
	return retval;
#endif
}

static inline int32_t imulshr16(int32_t a,int32_t b)
{
	int d0;
	register int32_t retval;
	__asm__ __volatile__
	(
		"imul %%edx\n"
		"shrd $16,%%edx,%%eax\n"
		:"=a"(retval), "=&d"(d0)
		:"0"(a), "1"(b)
	);
	return retval;
}

static inline uint32_t umulshr16(uint32_t a,uint32_t b)
{
	int d0;
	register uint32_t retval;
	__asm__ __volatile__
	(
		"mul %%edx\n"
		"shrd $16,%%edx,%%eax\n"
		:"=a"(retval), "=&d"(d0)
		:"0"(a), "1"(b)
	);
	return retval;
}

static inline uint32_t umldivrnd(uint32_t a,uint32_t b,uint32_t c)
{
	int d0, d1;
	register uint32_t retval;
	__asm__ __volatile__
	(
		"mul %%edx\n"
		"movl %%ecx,%%ebx\n"
		"shrl $1,%%ebx\n"
		"addl %%ebx,%%eax\n"
		"adc $0,%%edx\n"
		"div %%ecx\n"
		:"=a"(retval), "=&d"(d0), "=&c"(d1)
		:"0"(a), "1"(b), "2"(c)
		:"ebx"
	);
	return retval;
}

static inline void memsetd(void *dst, uint32_t what, uint32_t len)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
		"rep stosl\n"
		: "=&D"(d0), "=&a"(d1), "=&c"(d2)
		: "0" (dst), "1" (what), "2" (len)
		: "memory"
	);
}

static inline void memsetw(void *dst, uint16_t what, uint32_t len)
{
	short d1;
	int d0, d2;
	__asm__ __volatile__
	(
		"rep stosw\n"
		: "=&D"(d0), "=&a"(d1), "=&c"(d2)
		: "0" (dst), "1" (what), "2" (len)
		: "memory"
	);
}

#else

static inline int32_t imuldiv(int32_t a,int32_t b,int32_t c)
{
	int64_t temp = (int64_t)a*(int64_t)b;
	return temp/c;
}

static inline uint32_t umuldiv(uint32_t a,uint32_t b,uint32_t c)
{
	uint64_t temp = (uint64_t)a*(uint64_t)b;
	return temp/c;
}

static inline int32_t imulshr16(int32_t a,int32_t b)
{
	int64_t temp = (int64_t)a*(int64_t)b;
	return temp>>16;
}

static inline uint32_t umulshr16(uint32_t a,uint32_t b)
{
	uint64_t temp = (uint64_t)a*(uint64_t)b;
	return temp>>16;
}

#define umldivrnd(mul1, mul2, divby) umuldiv(mul1, mul2, divby) /* dirty */

#define memsetd(dst, what, len) {int i;uint32_t *tmp_dst=(uint32_t *)dst;for(i=len;i;i--)*tmp_dst++=(uint32_t)what;} while(0)
#define memsetw(dst, what, len) {int i;uint16_t *tmp_dst=(uint16_t *)dst;for(i=len;i;i--)*tmp_dst++=(uint16_t)what;} while(0)

#endif


#endif
