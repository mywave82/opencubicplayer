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

uint32_t mixAddAbs16M(const void *ch, uint32_t len)
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

uint32_t mixAddAbs16MS(const void *ch, uint32_t len)
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

uint32_t mixAddAbs16S(const void *ch, uint32_t len)
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

uint32_t mixAddAbs16SS(const void *ch, uint32_t len)
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

uint32_t mixAddAbs8M(const void *ch, uint32_t len)
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

uint32_t mixAddAbs8MS(const void *ch, uint32_t len)
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

uint32_t mixAddAbs8S(const void *ch, uint32_t len)
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

uint32_t mixAddAbs8SS(const void *ch, uint32_t len)
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

/********************************************************************/

void mixGetMasterSampleMS8M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleMU8M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleMS8S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleMU8S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleSS8M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleSU8M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleSS8S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleSU8S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleMS16M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleMU16M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleMS16S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleMU16S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleSS16M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleSU16M(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleSS16S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

void mixGetMasterSampleSU16S(int16_t *_dst, const void *_src, uint32_t len, uint32_t step)
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

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "mchasm", .desc = "OpenCP Player Auxiliary Routines (c) 1994-'22 Niklas Beisert, Tammo Hinrichs", .ver = DLLVERSION, .size = 0};
