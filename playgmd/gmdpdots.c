/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "types.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "gmdplay.h"
#include "gmdpdots.h"

OCP_INTERNAL int gmdGetDots (struct cpifaceSessionAPI_t *cpifaceSession, struct notedotsdata *d, int max)
{
	int pos=0;
	int i;
	/* mod.channum == cpifaceSession->LogicalChannelCount */
	for (i=0; i<mod.channum; i++)
	{
		struct chaninfo ci;
		int vl,vr;

		if (!mpGetChanStatus (cpifaceSession, i))
			continue;

		mpGetChanInfo(i, &ci);

		mpGetRealVolume (cpifaceSession, i, &vl, &vr);
		if (!vl&&!vr&&!ci.vol)
			continue;

		if (pos>=max)
			break;
		d[pos].voll=vl;
		d[pos].volr=vr;
		d[pos].chan=i;
		d[pos].note=mpGetRealNote(cpifaceSession, i);
		d[pos].col=32+(ci.ins&15);/* sustain */
		pos++;
	}
	return pos;
}
