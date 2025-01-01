/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
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

#if 0
This was used when amplification was configured - non of the players in Linux/Unix used this

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

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "mixclip", .desc = "OpenCP common clipper for various streams (c) 1994-'25 Niklas Beisert, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 10};
