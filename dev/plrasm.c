/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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

void plrMono16ToStereo16(int16_t *buf, int len)
{
	int i;
	for (i = len; i >= 0; i--)
	{
		buf[i<<1] = buf[(i<<1)+1] = buf[i];
	}
}

void plrConvertBufferFromStereo16BitSigned (void *dstbuf, int16_t *srcbuf, int samples, int to16bit, int tosigned, int tostereo, int revstereo)
{
	int16_t left, right;

	while (samples)
	{
		if (revstereo)
		{
			left =  srcbuf[1];
			right = srcbuf[0];
		} else {
			left =  srcbuf[0];
			right = srcbuf[1];
		}
		srcbuf+=2;

		if (!tostereo)
		{
			left = ((int)left + right)/2;
			if (!tosigned)
			{
				left ^= 0x8000;
			}
			if (to16bit)
			{
				((int16_t *)dstbuf)[0] = left;
				dstbuf = (int16_t *)dstbuf + 1;
			} else {
				((uint8_t *)dstbuf)[0] = ((uint16_t )left) >> 8;
				dstbuf = (uint8_t *)dstbuf + 1;
			}
		} else {
			if (!tosigned)
			{
				left ^= 0x8000;
				right ^= 0x8000;
			}
			if (to16bit)
			{
				((int16_t *)dstbuf)[0] = left;
				((int16_t *)dstbuf)[1] = right;
				dstbuf = (int16_t *)dstbuf + 2;
			} else {
				((uint8_t *)dstbuf)[0] = ((uint16_t )left) >> 8;
				((uint8_t *)dstbuf)[1] = ((uint16_t )right) >> 8;
				dstbuf = (uint8_t *)dstbuf + 2;
			}
		}
		samples--;
	}
}
