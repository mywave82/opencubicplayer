/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to cache filehandles I/O
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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#include "types.h"
#include "filesystem.h"
#include "filesystem-filehandle-cache.h"

#ifndef CACHE_LINE_SIZE
# define CACHE_LINE_SIZE 65536 /* default size */
#endif

#ifndef CACHE_LINES
# define CACHE_LINES 8
#endif

#if CACHE_LINES < 4
# error CACHE_LINES must be atleast 4 (start, end, and two runners)
#endif

struct cache_line_t
{
	uint64_t offset;
	uint_fast32_t points;
	uint_fast32_t fill;
	char *data;
};

struct cache_ocpfilehandle_t
{
	struct ocpfilehandle_t  head;
	struct ocpfilehandle_t *parent;

	uint64_t pos;
	uint64_t maxpos; /* max position seen on the origin at any given time */
	uint64_t lastpage; /* last known page */

	uint64_t filesize_cache;
	int filesize_ready_cache;

	struct cache_line_t cache_line[CACHE_LINES];
/* 0 = head
   1..n = floating windows
 */
};

static void cache_filehandle_ref (struct ocpfilehandle_t *_s);

static void cache_filehandle_unref (struct ocpfilehandle_t *_s);

static int cache_filehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos);

static uint64_t cache_filehandle_getpos (struct ocpfilehandle_t *_s);

static int cache_filehandle_eof (struct ocpfilehandle_t *_s);

static int cache_filehandle_error (struct ocpfilehandle_t *_s);

static int cache_filehandle_read (struct ocpfilehandle_t *_s, void *dst, int len);

static uint64_t cache_filehandle_filesize (struct ocpfilehandle_t *);

static int cache_filehandle_filesize_ready (struct ocpfilehandle_t *);

static int cache_filehandle_ioctl (struct ocpfilehandle_t *, const char *cmd, void *ptr);

/* for general cached version, we go directly for an open handle */
struct ocpfilehandle_t *cache_filehandle_open (struct ocpfilehandle_t *parent)
{
	uint_fast32_t fill;
	struct cache_ocpfilehandle_t *s = calloc (1, sizeof (*s));
	ocpfilehandle_t_fill
	(
		&s->head,
		cache_filehandle_ref,
		cache_filehandle_unref,
		parent->origin,
		cache_filehandle_seek_set,
		cache_filehandle_getpos,
		cache_filehandle_eof,
		cache_filehandle_error,
		cache_filehandle_read,
		cache_filehandle_ioctl,
		cache_filehandle_filesize,
		cache_filehandle_filesize_ready,
		0, /* filename_override */
		parent->dirdb_ref, /* we do not dirdb_ref()/dirdb_unref(), since we ref the origin instead */
		1 /* refcount */
	);

	parent->origin->ref (parent->origin);

	s->cache_line[0].data = calloc (1, CACHE_LINE_SIZE);
	if (!s->cache_line[0].data)
	{
		fprintf (stderr, "cache_filehandle_open, failed to allocate cache line 0\n");
		free (s);
		return 0;
	}

	s->parent = parent;
	s->parent->ref (s->parent);

	/* prefill cache-line 0 which is dedicated for the start of the file */

	parent->seek_set (parent, 0);
	fill = parent->read (parent, s->cache_line[0].data, CACHE_LINE_SIZE);
	s->cache_line[0].fill   = fill;
	s->cache_line[0].points = CACHE_LINE_SIZE;
	s->maxpos               = fill;

	return &s->head;
}

static void cache_filehandle_ref (struct ocpfilehandle_t *_s)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;
	s->head.refcount++;
}

static void cache_filehandle_unref (struct ocpfilehandle_t *_s)
{
	int i;
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	s->head.refcount--;

	if (s->head.refcount)
	{
		return;
	}

	for (i=0; i < CACHE_LINES; i++)
	{
		free (s->cache_line[i].data);
		s->cache_line[i].data = 0;
	}

	if (s->parent)
	{
		s->parent->unref (s->parent);
		s->parent = 0;
	}

	if (s->head.origin)
	{
		s->head.origin->unref (s->head.origin);
		s->head.origin = 0;
	}

	free (s);
}

static int cache_filehandle_fill_pagedata (struct cache_ocpfilehandle_t *s, uint64_t pageaddr)
{
	int i;
	int worstpage_i = -1;
	uint_fast32_t worstscore = 0xffffffff;

#ifdef FILEHANDLE_CACHE_DEBUG
	fprintf (stderr, "  cache_filehandle_fill_pagedata pageaddr=0x%08" PRIx64 " pageaddr_segment=0x%08" PRIx64 "\n", pageaddr, pageaddr & (CACHE_LINE_SIZE-1));
#endif
	assert (!(pageaddr & (CACHE_LINE_SIZE-1)));

	/* search cache for our data, and find a potential page to kill */

	for (i=0; i < CACHE_LINES; i++)
	{
		if (s->cache_line[i].offset == pageaddr)
		{
			s->cache_line[i].points++;
#ifdef FILEHANDLE_CACHE_DEBUG
			fprintf (stderr, "   hit in cache-line %d\n", i);
#endif
			return i;
		}
		if (!s->cache_line[i].offset && i)
		{
			worstpage_i = i;
#ifdef FILEHANDLE_CACHE_DEBUG
			fprintf (stderr, "   cache not full, first empty is cache-line %d\n", i);
#endif
			goto fillpage; /* no hits, and we found a free-page, use it for fresh-data collection */
		}
		if (!i)
		{ /* first page is sacred */
#ifdef FILEHANDLE_CACHE_DEBUG
			fprintf (stderr, "Do not sacriface page 0, it is special\n");
#endif
			continue;
		}
		if (s->cache_line[i].offset == s->lastpage)
		{ /* last known page is sacred */
#ifdef FILEHANDLE_CACHE_DEBUG
			fprintf (stderr, "Do not sacriface page %d, it is currently the last page\n", i);
#endif
			continue;
		}
		if (worstscore > s->cache_line[i].points)
		{
			worstscore = s->cache_line[i].points;
			worstpage_i = i;
		}
	}

	for (i=0; i < CACHE_LINES; i++)
	{
		s->cache_line[i].points >>= 1;
	}

fillpage:
#ifdef FILEHANDLE_CACHE_DEBUG
	fprintf (stderr, "   fill cache-line %d\n", worstpage_i);
#endif
	assert (worstpage_i >= 0);
	i = worstpage_i;
	s->cache_line[i].offset = pageaddr;
	if (!s->cache_line[i].data)
	{
		s->cache_line[i].data = malloc (CACHE_LINE_SIZE);
		if (!s->cache_line[i].data)
		{
			fprintf (stderr, "cache_filehandle_fill_pagedata: malloc() failed\n");
			goto errorout;
		}
	}

	if (s->parent->seek_set (s->parent, pageaddr))
	{ /* we probably hit EOF earlier */
		goto errorout;
	}

	s->cache_line[i].fill = s->parent->read (s->parent, s->cache_line[i].data, CACHE_LINE_SIZE);
	if (!s->cache_line[i].fill)
	{ /* we probably hit EOF in the previous read (page-exact hit */
		goto errorout;
	}

	if (pageaddr > s->lastpage)
	{
		s->lastpage = pageaddr;
	}
	if (pageaddr + s->cache_line[i].fill > s->maxpos)
	{
		s->maxpos = pageaddr + s->cache_line[i].fill;
	}
	s->cache_line[i].points = CACHE_LINE_SIZE;

	return i;

errorout:
	s->cache_line[i].points = 0;
	s->cache_line[i].offset = 0;
	s->cache_line[i].fill = 0;
	if (!pageaddr)
	{
		return 0; /* special case, zero-length file */
	}
	return -1;
}

/* we use spool for files that have unknown size, that we want to keep in the cache */
static void cache_filehandle_spool_from_and_upto (struct cache_ocpfilehandle_t *s, uint64_t frompos, uint64_t topos)
{
	frompos = frompos                       & ~(CACHE_LINE_SIZE-1); /* round down to nearest page */
	topos   = (topos + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE-1); /* round up to nearest page */

	while (frompos < topos)
	{
		if (cache_filehandle_fill_pagedata (s, frompos) < 0)
		{
			return;
		}
		frompos += CACHE_LINE_SIZE;
	}
}

static int cache_filehandle_filesize_ready (struct ocpfilehandle_t *_s)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	if (!s->filesize_ready_cache)
	{
		s->filesize_ready_cache = s->head.origin->filesize_ready (s->head.origin);
		if (s->filesize_ready_cache)
		{
			s->filesize_cache = s->head.origin->filesize (s->head.origin);
		}
	}

	return s->filesize_ready_cache;
}

static uint64_t cache_filehandle_filesize (struct ocpfilehandle_t * _s)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	if ((!cache_filehandle_filesize_ready (_s)) || (_s->origin->compression >= COMPRESSION_STREAM))
	{
		if (!(s->maxpos & (CACHE_LINE_SIZE - 1)))
		{
			cache_filehandle_spool_from_and_upto (s, s->maxpos, 0x4000000000000000ll);
		}
	}

	s->filesize_ready_cache = 1;
	s->filesize_cache = s->head.origin->filesize (s->head.origin);
	return s->filesize_cache;
}

static int cache_filehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	if (pos < 0) return -1;

	if (pos <= s->maxpos)
	{
		s->pos = pos;
		return 0; /* this is valid range */
	}

	/* is filesize known yet or not? */
	if (cache_filehandle_filesize_ready (_s))
	{
		if (pos > s->filesize_cache)
		{
			return -1;
		}
	} else {
		cache_filehandle_spool_from_and_upto (s, s->maxpos, pos);
		if (pos > s->maxpos)
		{
			return -1;
		};
	}

	s->pos = pos;
	return 0;
}

static uint64_t cache_filehandle_getpos (struct ocpfilehandle_t *_s)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	return s->pos;
}

static int cache_filehandle_eof (struct ocpfilehandle_t *_s)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;
	uint64_t oldpos;

	if (s->pos < s->maxpos)
	{
		return 0;
	}

	oldpos = s->pos;
	/* attempt to pull in more data */
	cache_filehandle_seek_set (_s, s->maxpos + 1);
	cache_filehandle_seek_set (_s, oldpos);

	if (s->pos < s->maxpos)
	{
		return 0;
	}

	return 1;
}

static int cache_filehandle_error (struct ocpfilehandle_t *_s)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	return s->parent->error (s->parent);
}

static int cache_filehandle_ioctl (struct ocpfilehandle_t *_s, const char *cmd, void *ptr)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	return s->parent->ioctl (s->parent, cmd, ptr);
}

static int cache_filehandle_read (struct ocpfilehandle_t *_s, void *dst, int len)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;
	int returnvalue = 0;
	cache_filehandle_filesize_ready (_s);

	if ((s->pos >= s->maxpos) &&
	    s->filesize_ready_cache &&
	    (s->filesize_cache <= (CACHE_LINES * CACHE_LINE_SIZE))) /* read in the entire file at once, it can fit */
	{
		uint64_t frompos =  s->maxpos                     & ~(CACHE_LINE_SIZE-1); /* round down to nearest page */
		uint64_t topos   = (s->pos + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE-1); /* round up to nearest page */
		cache_filehandle_spool_from_and_upto (s, frompos, topos);
	}

	while (len)
	{
		uint64_t pageaddr;
		uint32_t offset;
		uint32_t runlen;
		int i;

		if (s->filesize_ready_cache && (s->pos >= s->filesize_cache))
		{
			break;
		}

		pageaddr = s->pos & ~(CACHE_LINE_SIZE-1);
		offset = s->pos & (CACHE_LINE_SIZE-1);

#ifdef FILEHANDLE_CACHE_DEBUG
		fprintf (stderr, "  cache_filehandle_read.len=%d pos=0x%08" PRIx64 " pageaddr=0x%08" PRIx64 " offset=0x%08" PRIx32 " mask_down=0x%08x mask_offset=0x%08x\n", len, s->pos, pageaddr, offset, ~(CACHE_LINE_SIZE-1), CACHE_LINE_SIZE-1);
#endif

		i = cache_filehandle_fill_pagedata (s, pageaddr);
		if (i < 0)
		{
			break;
		}

		if (offset >= s->cache_line[i].fill)
		{
			return returnvalue;
		}

		if ((offset + len) > (s->cache_line[i].fill))
		{
			runlen = s->cache_line[i].fill - offset;
		} else {
			runlen = len;
		}

		memcpy (dst, s->cache_line[i].data + offset, runlen);
		s->cache_line[i].points += runlen;

		len -= runlen;
		dst += runlen;
		s->pos += runlen;
		returnvalue += runlen;

		if (s->cache_line[i].fill != CACHE_LINE_SIZE)
		{ /* not more data is available if the current cache page is incomplete */
			break;
		}
	}
	return returnvalue;
}
