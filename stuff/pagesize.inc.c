/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * pagesize() call used from several plugins. Different unix variants provides
 * pagesize info in different ways.. sysconf, header-file, not mentioned
 * (4096 default fallback).
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

#include <limits.h>
#include <unistd.h>
#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

static long pagesize(void) __attribute__((const));
static long pagesize(void)
{
	static long retval=0;

	if (retval)
		return retval;
#ifdef HAVE_SYSCONF
#ifdef _SC_PAGESIZE
	retval = sysconf(_SC_PAGESIZE);
	if (retval>0)
		return retval;
	if (retval<0)
		perror("[compat] sysconf(_SC_PAGESIZE)");
#endif
#endif
	retval = PAGESIZE;
	return retval;
}

