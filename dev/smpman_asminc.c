/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
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

/* included from smpman.c */

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
