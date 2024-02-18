/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Player system variables / auxiliary routines
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
 *  -ss040907   Stian Skjelstad <stian@nixia.no>
 *    -minor buffer cleanups.. Let drivers allocate their own memory
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "mchasm.h"
#include "mcp.h"
#include "player.h"
#include "stuff/imsrtns.h"

void plrGetRealMasterVolume(int *l, int *r)
{
	unsigned long v;
#define fn mixAddAbs16SS
	int16_t *buf1, *buf2;
	unsigned int length1, length2;

	plrDevAPI->PeekBuffer ((void **)&buf1, &length1, (void **)&buf2, &length2);

	if (!(length1 + length2))
	{
		*l = *r = 0;
		return;
	}

	v=fn(buf1, length1);
	if (length2)
		v+=fn(buf2, length2);

	v=v*128/((length1+length2)*16384);
	*l=(v>255)?255:v;

	v=fn(buf1+1, length1);
	if (length2)
		v+=fn(buf2+1, length2);

	v=v*128/((length1+length2)*16384);
	*r=(v>255)?255:v;
#undef fn
}

void plrGetMasterSample(int16_t *buf, uint32_t len, uint32_t rate, int opt)
{
	uint32_t step=umuldiv(plrDevAPI->GetRate(), 0x10000, rate);
	int stereoout;
	int16_t *buf1, *buf2;
	unsigned int length1, length2;
	unsigned int maxlen;
	signed int pass2;

	if (step<0x1000)
		step=0x1000;
	if (step>0x800000)
		step=0x800000;

	plrDevAPI->PeekBuffer ((void **)&buf1, &length1, (void **)&buf2, &length2);
	stereoout=(opt&mcpGetSampleStereo)?1:0;

	/* length1, length2 and len are all in sample space, while mixGetMasterSampleSS16S()
	 * and mixGetMasterSampleSS16M() are from time where shared audio-buffer was
	 * stereo/mono/8bit/16bit agnostic and step is multiplied by 2 in order to get stereo.
	 * So we have to compensate: */
	length1 >>= 1;
	length2 >>= 1;

	maxlen = imuldiv((length1 + length2), 0x10000, step); /* step goes with twice the speed on stereo */
	if (len > maxlen) /* not enough data? zero-fill and limit */
	{
		memset (buf + maxlen, 0, (len - maxlen) << (1 /* bit16 */ + stereoout));
		len = maxlen;
	}
	pass2 = (signed int)len - (imuldiv (length1, 0x10000, step)); /* pass2 goes negative if length1 can provide more than 256 samples... and maxlen protects both passes */

	if (stereoout)
	{
		if (pass2 > 0)
		{
			mixGetMasterSampleSS16S (buf, buf1, len-pass2, step);
			mixGetMasterSampleSS16S (buf, buf2, pass2, step);
		} else {
			mixGetMasterSampleSS16S (buf, buf1, len, step);
		}
	} else {
		if (pass2 > 0)
		{
			mixGetMasterSampleSS16M (buf, buf1, len - pass2, step);
			mixGetMasterSampleSS16M (buf, buf2, pass2, step);
		} else {
			mixGetMasterSampleSS16M (buf, buf1, len, step);
		}
	}
}
