/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMDPlay note dots routines
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
#include <stdio.h> /* FILE is defined here */
#include "types.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "gmdplay.h"
#include "gmdpdots.h"

/*extern unsigned short plNLChan;*/

int __attribute__ ((visibility ("internal"))) gmdGetDots(struct notedotsdata *d, int max)
{
	int pos=0;
	int i;
	for (i=0; i<plNLChan; i++)
	{
		struct chaninfo ci;
		int vl,vr;

		if (!mpGetChanStatus(i))
			continue;

		mpGetChanInfo(i, &ci);

		mpGetRealVolume(i, &vl, &vr);
		if (!vl&&!vr&&!ci.vol)
			continue;

		if (pos>=max)
			break;
		d[pos].voll=vl;
		d[pos].volr=vr;
		d[pos].chan=i;
		d[pos].note=mpGetRealNote(i);
		d[pos].col=32+(ci.ins&15);/* sustain */
		pos++;
	}
	return pos;
}
