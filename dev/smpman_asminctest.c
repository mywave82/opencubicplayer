/* OpenCP Module Player
 * copyright (c) '04-'10 Stian Skjelstad <stian@nixia.no>
 *
 * Unit-test for "smpman_asminc.c"
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

#include <stdio.h>

static unsigned short abstab[0x200];

#include "smpman_asminc.c"

int main(int argc, char *argv[])
{
	char *test="1982---suuuuuuuper test just for all the c00l guys out here";
	int i;
	for (i=-0x100; i<0x100; i++)
		abstab[i+0x100]=i*i/16;

	fprintf(stderr, "getpitch(): ");
	if ((i=getpitch(test, 50))!=6347)
	{
		fprintf(stderr, "Failed (%d vs %d)\n", i, 8263);
		return -1;
	}
	fprintf(stderr, "ok\n");

	fprintf(stderr, "getpitch16(): ");
	if ((i=getpitch16(test, 25))!=1886)
	{
		fprintf(stderr, "Failed (%d bs %d)\n", i, 1886);
		return -1;
	}
	fprintf(stderr, "ok\n");

	return 0;
}
