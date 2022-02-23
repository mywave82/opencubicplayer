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

#define memsetd(dst, what, len) {int i;uint32_t *tmp_dst=(uint32_t *)(dst);for(i=(len);i;i--)*tmp_dst++=(uint32_t)(what);} while(0)
#define memsetw(dst, what, len) {int i;uint16_t *tmp_dst=(uint16_t *)(dst);for(i=(len);i;i--)*tmp_dst++=(uint16_t)(what);} while(0)

#endif
