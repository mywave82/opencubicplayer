/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay Initialisiation (reads GUS patches etc)
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
 *  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
 *    -changed path searching for ULTRASND.INI and patch files
 *    -corrected some obviously wrong allocations
 *  -sss050411 Stian Skjelstad <stian@nixia.no>
 *    -splitet up sourcecode to it's logical pieces
 */

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "gmiplay.h"
#include "dev/mcp.h"
#include "boot/psetting.h"
#include "stuff/compat.h"

void __attribute__ ((visibility ("internal")))
	(*_midClose)(void) = 0;

void __attribute__ ((visibility ("internal"))) midClose(void)
{
	int i;
	for (i=0; i<256; i++)
		midInstrumentNames[i][0]=0;
	if (_midClose)
	{
		_midClose();
		_midClose=0;
	}
}

int __attribute__ ((visibility ("internal"))) midInit(void)
{
	const char *midUsePref=cfGetProfileString("midi", "use", 0);
	if (midUsePref)
	{
		if ((!strcmp(midUsePref, "ultradir"))||(!strcmp(midUsePref, "ultrasnd"))||(!strcmp(midUsePref, "ultra")))
			return midInitUltra();
		if (!strcmp(midUsePref, "fff"))
			return midInitFFF();
		if (!strcmp(midUsePref, "freepats"))
			return midInitFreePats();
		if (!strcmp(midUsePref, "timidity"))
			return midInitTimidity();
		fprintf(stderr, "Invalid use= in [midi] section of ocp.ini\n");
		return 0;
	}
	if (midInitFreePats())
		return 1;
	if (midInitFFF())
		return 1;
	if (midInitUltra())
		return 1;
	if (midInitTimidity())
		return 1;
	fprintf(stderr, "No midi font loaded\n");
	return 0;
}
