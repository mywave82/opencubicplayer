/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay note dots routines
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
 */

#include "config.h"
#include "types.h"

#include "dev/mcp.h"
#include "gmiplay.h"
#include "cpiface/cpiface.h"

int __attribute__ ((visibility ("internal"))) gmiGetDots(struct notedotsdata *d, int max)
{
	int i,j;
	int pos=0;
	for (i=0; i<plNLChan; i++)
	{
		struct mchaninfo2 ci;

		if (pos>=max)
			break;
		midGetRealNoteVol(i, &ci);
		for (j=0; j<ci.notenum; j++)
		{
			uint16_t vl, vr;

			if (pos>=max)
				break;
			vl=ci.voll[j];
			vr=ci.volr[j];

			if (!vl&&!vr&&!ci.opt[j])
				continue;

			d[pos].voll=vl<<1;
			d[pos].volr=vr<<1;
			d[pos].chan=i;
			d[pos].note=ci.note[j]+12*256;
			d[pos].col=(ci.ins[j]&15)+(ci.opt[j]?32:16);
			pos++;
		}
	}
	return pos;
}
