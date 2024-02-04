/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to give a virtual static memory-stored file
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
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-file-mem.h"

struct mem_ocpfile_t
{
	struct ocpfile_t  head;

	uint32_t filesize;

	char *ptr;
};

struct mem_ocpfilehandle_t
{
	struct ocpfilehandle_t  head;
	struct mem_ocpfile_t   *owner; // can be NULL for standalone

	uint32_t filesize;
	uint64_t pos;
	int error;

	char *ptr;
};

static void mem_filehandle_ref (struct ocpfilehandle_t *_s)
{
	struct mem_ocpfilehandle_t *s = (struct mem_ocpfilehandle_t *)_s;
	s->head.refcount++;
}

static void mem_filehandle_unref (struct ocpfilehandle_t *_s)
{
	struct mem_ocpfilehandle_t *s = (struct mem_ocpfilehandle_t *)_s;
	s->head.refcount--;

	if (!s->head.refcount)
	{
		dirdbUnref (s->head.dirdb_ref, dirdb_use_filehandle);
		if (s->owner)
		{
			s->owner->head.unref (&s->owner->head);
			s->owner = 0;
		} else {
			free (s->ptr);
		}
		free (s);
	}
}

/* returns 0 for OK, and -1 on error, should use positive numbers */
static int mem_filehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct mem_ocpfilehandle_t *s = (struct mem_ocpfilehandle_t *)_s;
	if (pos < 0) return -1;
	if (pos > (int64_t)s->filesize) return -1;

	s->pos = pos;
	s->error = 0;

	return 0;
}

static uint64_t mem_filehandle_getpos (struct ocpfilehandle_t *_s)
{
	struct mem_ocpfilehandle_t *s = (struct mem_ocpfilehandle_t *)_s;

	return s->pos;
}

/* 0 = mere data, 1 = EOF, -1 = error - probably tried to read beyond EOF */
static int mem_filehandle_eof (struct ocpfilehandle_t *_s)
{
	struct mem_ocpfilehandle_t *s = (struct mem_ocpfilehandle_t *)_s;

	return s->pos == s->filesize;
}

static int mem_filehandle_error (struct ocpfilehandle_t *_s)
{
	struct mem_ocpfilehandle_t *s = (struct mem_ocpfilehandle_t *)_s;

	return s->error;
}

/* returns 0 or the number of bytes read - short reads only happens if EOF is hit! */
static int mem_filehandle_read (struct ocpfilehandle_t *_s, void *dst, int len)
{
	struct mem_ocpfilehandle_t *s = (struct mem_ocpfilehandle_t *)_s;
	int iterlen;

	if (len < 0)
	{
		return -1;
	}

	iterlen = len;
	if (iterlen > (s->filesize - s->pos))
	{
		iterlen = s->filesize - s->pos;
	}
	memcpy (dst, s->ptr + s->pos, iterlen);
	dst = (char *)dst + iterlen;
	len -= iterlen;
	s->pos += iterlen;

	if (len)
	{
		memset (dst, 0, len);
	}

	return iterlen;
}

static uint64_t mem_filehandle_filesize (struct ocpfilehandle_t *_s)
{
	struct mem_ocpfilehandle_t *s = (struct mem_ocpfilehandle_t *)_s;

	return s->filesize;
}

static int mem_filehandle_filesize_ready (struct ocpfilehandle_t *_s)
{
	return 1;
}

static struct ocpfilehandle_t *mem_filehandle_open_real (struct mem_ocpfile_t *owner, int dirdb_ref, char *ptr, uint32_t len)
{
	struct mem_ocpfilehandle_t *s = calloc (1, sizeof (*s));

	ocpfilehandle_t_fill
	(
		&s->head,
		mem_filehandle_ref,
		mem_filehandle_unref,
		&owner->head,
		mem_filehandle_seek_set,
		mem_filehandle_getpos,
		mem_filehandle_eof,
		mem_filehandle_error,
		mem_filehandle_read,
		0, /* ioctl */
		mem_filehandle_filesize,
		mem_filehandle_filesize_ready,
		0, /* filename_override */
		dirdbRef (dirdb_ref, dirdb_use_filehandle),
		1 /* refcount */
	);

	s->owner = owner;
	if (s->owner)
	{
		s->owner->head.ref (&s->owner->head);
	}
	s->filesize = len;
	s->ptr = ptr;

	return &s->head;
}

struct ocpfilehandle_t *mem_filehandle_open (int dirdb_ref, char *ptr, uint32_t len)
{
	return mem_filehandle_open_real (0, dirdb_ref, ptr, len);
}

static void mem_file_ref (struct ocpfile_t *_s)
{
	struct mem_ocpfile_t *s = (struct mem_ocpfile_t *)_s;
	s->head.refcount++;
}

static void mem_file_unref (struct ocpfile_t *_s)
{
	struct mem_ocpfile_t *s = (struct mem_ocpfile_t *)_s;
	s->head.refcount--;

	if (!s->head.refcount)
	{
		dirdbUnref (s->head.dirdb_ref, dirdb_use_file);
		free (s->ptr);

		s->head.parent->unref (s->head.parent);
		free (s);
	}
}

static struct ocpfilehandle_t *_mem_file_open (struct ocpfile_t *_s)
{
	struct mem_ocpfile_t *s = (struct mem_ocpfile_t *)_s;

	return mem_filehandle_open_real (s, s->head.dirdb_ref, s->ptr, s->filesize);
}

static uint64_t mem_filesize (struct ocpfile_t *_s)
{
	struct mem_ocpfile_t *s = (struct mem_ocpfile_t *)_s;

	return s->filesize;
}

static int mem_filesize_ready (struct ocpfile_t *_s)
{
	return 1;
}

struct ocpfile_t *mem_file_open (struct ocpdir_t *parent, int dirdb_ref, char *ptr, uint32_t len)
{
	struct mem_ocpfile_t *s = calloc (1, sizeof (*s));

	ocpfile_t_fill
	(
		&s->head,
		mem_file_ref,
		mem_file_unref,
		parent,
		_mem_file_open,
		mem_filesize,
		mem_filesize_ready,
		0, /* filename_override */
		dirdbRef (dirdb_ref, dirdb_use_file),
		1, /* refcount */
		0, /* is_nodetect */
		COMPRESSION_NONE
	);

	parent->ref (parent);

	s->filesize = len;
	s->ptr = ptr;

	return &s->head;
}
