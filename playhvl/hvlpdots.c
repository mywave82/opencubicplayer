/* OpenCP Module Player
 * copyright (c) 2019-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * HVLPlay note dots routines
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
 */

#include "config.h"
#include "types.h"
#include "cpiface/cpiface.h"
#include "hvlplay.h"
#include "hvlpdots.h"
#include "player.h"

OCP_INTERNAL int hvlGetDots (struct cpifaceSessionAPI_t *cpifaceSession, struct notedotsdata *d, int max)
{
	int pos=0;
	int i;
	for (i=0; i<ht->ht_Channels; i++)
	{
		struct hvl_chaninfo ci;

		hvlGetChanInfo (i, &ci);

		if (!ci.vol)
			continue;

		if (pos>=max)
			break;

		d[pos].voll=(ci.vol * 255-ci.pan) / 256;
		d[pos].volr=(ci.vol * ci.pan) / 256;
		d[pos].chan=i;
		d[pos].note=0x00800000 / ci.noteperiod;
		d[pos].col=32+(ci.ins&15);/* sustain */
		pos++;
	}
	return pos;
}
