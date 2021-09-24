/* OpenCP Module Player
 * copyright (c) 2020-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Base code to handle filesystems
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
#include "types.h"
#include "filesystem.h"

#define MAX_FILEDECOMPRESSORS 16
#define MAX_DIRDECOMPRESSORS 16

const struct ocpdirdecompressor_t *ocpdirdecompressor[MAX_DIRDECOMPRESSORS];
int ocpdirdecompressors;

void register_dirdecompressor(const struct ocpdirdecompressor_t *e)
{
	int i;

	if (ocpdirdecompressors >= MAX_DIRDECOMPRESSORS)
	{
		fprintf (stderr, "[filesystem] Too many dirdecompressors, unable to add %s\n", e->name);
		return;
	}

	for (i=0; i < ocpdirdecompressors; i++)
	{
		if (ocpdirdecompressor[i] == e)
		{
			return;
		}
	}
	ocpdirdecompressor[ocpdirdecompressors++] = e;
}

struct ocpdir_t *ocpdirdecompressor_check (struct ocpfile_t *f, const char *filetype)
{
	int i;
	for (i=0; i < ocpdirdecompressors; i++)
	{
		struct ocpdir_t *r = ocpdirdecompressor[i]->check(ocpdirdecompressor[i], f, filetype);
		if (r)
		{
			return r;
		}
	}
	return 0;
}

/* returns 0 or the number of bytes read */
int ocpfilehandle_read_uint8 (struct ocpfilehandle_t *s, uint8_t *dst)
{
	if (s->read (s, dst, 1) != 1) return -1;
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint16_be (struct ocpfilehandle_t *s, uint16_t *dst)
{
	if (s->read (s, dst, 2) != 2) return -1;
	*dst = uint16_big (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint32_be (struct ocpfilehandle_t *s, uint32_t *dst)
{
	if (s->read (s, dst, 4) != 4) return -1;
	*dst = uint32_big (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint64_be (struct ocpfilehandle_t *s, uint64_t *dst)
{
	if (s->read (s, dst, 8) != 8) return -1;
	*dst = uint64_big (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint16_le (struct ocpfilehandle_t *s, uint16_t *dst)
{
	if (s->read (s, dst, 2) != 2) return -1;
	*dst = uint16_little (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint32_le (struct ocpfilehandle_t *s, uint32_t *dst)
{
	if (s->read (s, dst, 4) != 4) return -1;
	*dst = uint32_little (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint64_le (struct ocpfilehandle_t *s, uint64_t *dst)
{
	if (s->read (s, dst, 8) != 8) return -1;
	*dst = uint64_little (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint24_be (struct ocpfilehandle_t *s, uint32_t *dst)
{
	uint8_t t[3];
	if (s->read (s, t, 3) != 3) return -1;
	*dst = (t[0] << 16) | (t[1]<<8) | t[2];
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint24_le (struct ocpfilehandle_t *s, uint32_t *dst)
{
	uint8_t t[3];
	if (s->read (s, t, 3) != 3) return -1;
	*dst = (t[2] << 16) | (t[1]<<8) | t[0];
	return 0;
}

int ocpfilehandle_t_fill_default_ioctl (struct ocpfilehandle_t *s, const char *cmd, void *ptr)
{
	return -1;
}
