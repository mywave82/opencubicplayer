/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIFace file type dectection routines for the file selector
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 *  -fd981119   Felix Domke <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT'
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "cpiptype.h"
#include "filesel/mdb.h"

static int cpiReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *file, const char *buf, size_t len)
{
	if (!memcmp(buf, "CPANI\x1A\x00\x00", 8))
	{
		strncpy(m->title,(char *)buf+8,31);
		if (!m->title[0])
		{
			strcpy(m->title, "wuerfel mode animation");
		}
		m->modtype.integer.i=MODULETYPE("ANI");
		return 1;
	}
	return 0;
}

struct mdbreadinforegstruct cpiReadInfoReg = {"ANI", cpiReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
