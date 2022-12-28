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

static inline int32_t imulshr24(int32_t a,int32_t b)
{
	int64_t temp = (int64_t)a*(int64_t)b;
	return temp>>24;
}

static inline int32_t imulshr32(int32_t a,int32_t b)
{
	int64_t temp = (int64_t)a*(int64_t)b;
	return temp>>32;
}

static inline uint32_t umulshr16(uint32_t a,uint32_t b)
{
	uint64_t temp = (uint64_t)a*(uint64_t)b;
	return temp>>16;
}

#define saturate(value,minvalue,maxvalue) (((value) < (minvalue)) ? (minvalue) : ((value) > (maxvalue)) ? (maxvalue) : (value))

#define umldivrnd(mul1, mul2, divby) umuldiv(mul1, mul2, divby) /* dirty */

#define memsetw(dst, what, len) {int i;uint16_t *tmp_dst=(uint16_t *)(dst);for(i=(len);i;i--)*tmp_dst++=(uint16_t)(what);} while(0)

#endif
