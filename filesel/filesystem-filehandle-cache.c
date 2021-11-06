/* OpenCP Module Player
 * copyright (c) 2020 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "types.h"
#include "filesystem.h"
#include "filesystem-filehandle-cache.h"

#ifndef CACHE_LINE_SIZE
# define CACHE_LINE_SIZE 65536 /* default size */
#endif

#define CACHE_LINES 4

#ifdef FILEHANDLE_CACHE_DEBUG
#define DEBUG_PRINT(...) do { if (do_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#define DUMP_SELF(S) do { if (do_debug_print) { dump_self(s); } } while (0)
#else
#define DEBUG_PRINT(...) do {} while (0)
#define DUMP_SELF(S) do {} while (0)
#endif

struct cache_line_t
{
	char *data;
	size_t offset;
	size_t fill;
	size_t size;
};

struct cache_ocpfilehandle_t
{
	struct ocpfilehandle_t  head;
	struct ocpfile_t       *owner;
	struct ocpfilehandle_t *parent;

	int      filesize_pending;
	uint64_t filesize;

	uint64_t handle_pos;
	uint64_t pos;
	int error;

	struct cache_line_t cache_line[CACHE_LINES];
/* 0 = head
   1 = moving window
   2 = tail
   3 = post-tail, when trying read past the current known EOF
 */
};

static void cache_filehandle_ref (struct ocpfilehandle_t *_s);

static void cache_filehandle_unref (struct ocpfilehandle_t *_s);

static int cache_filehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos);

static int cache_filehandle_seek_cur (struct ocpfilehandle_t *_s, int64_t pos);

static int cache_filehandle_seek_end (struct ocpfilehandle_t *_s, int64_t pos);

static uint64_t cache_filehandle_getpos (struct ocpfilehandle_t *_s);

static int cache_filehandle_eof (struct ocpfilehandle_t *_s);

static int cache_filehandle_error (struct ocpfilehandle_t *_s);

static int cache_filehandle_read (struct ocpfilehandle_t *_s, void *dst, int len);

static uint64_t cache_filehandle_filesize (struct ocpfilehandle_t *);

static int cache_filehandle_filesize_ready (struct ocpfilehandle_t *);

static int cache_filehandle_ioctl (struct ocpfilehandle_t *, const char *cmd, void *ptr);

struct ocpfilehandle_t *cache_filehandle_open_pre (struct ocpfile_t *owner, char *headptr, uint32_t headlen, char *tailptr, uint32_t taillen)
{
	struct cache_ocpfilehandle_t *retval = calloc (1, sizeof (*retval));
	ocpfilehandle_t_fill
	(
		&retval->head,
		cache_filehandle_ref,
		cache_filehandle_unref,
		cache_filehandle_seek_set,
		cache_filehandle_seek_cur,
		cache_filehandle_seek_end,
		cache_filehandle_getpos,
		cache_filehandle_eof,
		cache_filehandle_error,
		cache_filehandle_read,
		cache_filehandle_ioctl,
		cache_filehandle_filesize,
		cache_filehandle_filesize_ready,
		owner->dirdb_ref // we do not dirdb_ref()/dirdb_unref(), since we ref the owner instead
	);
	retval->owner = owner;
	retval->owner->ref (retval->owner);

	retval->head.refcount = 1;
	retval->filesize_pending = 0;
	retval->filesize = owner->filesize(owner); // if calling open_pre, size tailbuf is the tail, so the filesize should be known directly
	retval->cache_line[0].data = headptr;
	retval->cache_line[0].fill = retval->cache_line[0].size = headlen;

	retval->cache_line[2].data = tailptr;
	retval->cache_line[2].fill = retval->cache_line[2].size = taillen;

	return &retval->head;
}

/* for general cached version, we go directly for an open handle */
struct ocpfilehandle_t *cache_filehandle_open (struct ocpfilehandle_t *parent)
{
	struct cache_ocpfilehandle_t *retval = calloc (1, sizeof (*retval));
	ocpfilehandle_t_fill
	(
		&retval->head,
		cache_filehandle_ref,
		cache_filehandle_unref,
		cache_filehandle_seek_set,
		cache_filehandle_seek_cur,
		cache_filehandle_seek_end,
		cache_filehandle_getpos,
		cache_filehandle_eof,
		cache_filehandle_error,
		cache_filehandle_read,
		cache_filehandle_ioctl,
		cache_filehandle_filesize,
		cache_filehandle_filesize_ready,
		parent->dirdb_ref // we do not dirdb_ref()/dirdb_unref(), since we ref the owner instead
	);

	retval->parent = parent;
	retval->parent->ref (retval->parent);
	if (parent->filesize_ready (parent))
	{
		retval->filesize_pending = 0;
		retval->filesize = parent->filesize (parent);
	} else {
		retval->filesize_pending = 1;
		retval->filesize = 0;//UINT64_C(0xffffffffffffffff);
	}

	retval->head.refcount = 1;

	return &retval->head;
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

	if (s->owner)
	{
		s->owner->unref (s->owner);
		s->owner = 0;
	}

	if (s->parent)
	{
		s->parent->unref (s->parent);
		s->parent = 0;
	}

	free (s);
}

static int cache_filehandle_filesize_unpend (struct cache_ocpfilehandle_t *s)
{
	uint64_t filesize = FILESIZE_ERROR;
	if (s->parent)
	{
		filesize = s->parent->filesize (s->parent);
	} else if (s->owner)
	{
		filesize = s->owner->filesize (s->owner);
	}
	if (filesize == FILESIZE_ERROR)
	{
		return -1;
	}
	s->filesize = filesize;
	s->filesize_pending = 0;

	return 0;
}

static int cache_filehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	if (pos < 0) return -1;

	if ((s->filesize_pending) && (pos > s->filesize))
	{
		if (cache_filehandle_filesize_unpend (s))
		{
			return -1;
		}
	}

	if (pos > s->filesize) return -1;

	s->pos = pos;
	s->error = 0;

	return 0;
}

static int cache_filehandle_seek_cur (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	if (pos < 0)
	{
		if (pos == INT64_MIN) return -1; /* we never have files this size */
		if ((-pos) > s->pos) return -1;
		s->pos += pos;
	} else {
		/* check for overflow */
		if ((int64_t)(pos + s->pos) < 0) return -1;

		if ((s->filesize_pending) && ((pos + s->pos) > s->filesize))
		{
			if (cache_filehandle_filesize_unpend (s))
			{
				return -1;
			}
		}

		if ((pos + s->pos) > s->filesize) return -1;
		s->pos += pos;
	}

	s->error = 0;

	return 0;
}

static int cache_filehandle_seek_end (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	if (pos > 0) return -1;

	if (pos == INT64_MIN) return -1; /* we never have files this size */

	if (s->filesize_pending)
	{
		if (cache_filehandle_filesize_unpend (s))
		{
			return -1;
		}
	}

	if (pos < -(int64_t)(s->filesize)) return -1;
	s->pos = s->filesize + pos;

	s->error = 0;

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

	if (!s->filesize_pending)
	{
		return s->pos == s->filesize;
	}
	return 0;
}

static int cache_filehandle_error (struct ocpfilehandle_t *_s)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	return s->error;
}

/* try to fill data from the middle buffers out to the HEAD and TAIL buffers */
static void cache_steal_buffers (struct cache_ocpfilehandle_t *s)
{
	/* Do we have a POST tail buffer ? */
	if ( (!s->cache_line[3].fill) )
	{
		/* if TAIL is empty, or TAIL is smaller than we want, swap TAIL and POST-tail */
		if ((!s->cache_line[2].fill) || (s->cache_line[2].size < CACHE_LINE_SIZE))
		{
			struct cache_line_t temp = s->cache_line[2];
			s->cache_line[2] = s->cache_line[3];
			s->cache_line[3] = temp;
			/* free POST-tail is size is smaller than we want */
			if (s->cache_line[3].size < CACHE_LINE_SIZE)
			{
				free (s->cache_line[3].data);
				s->cache_line[3].data = 0;
				s->cache_line[3].size = 0;
				s->cache_line[3].fill = 0;
			}
		/* if MOVING-WINDOW is empty, rotate buffers MOVING-WINDOW, TAIL and POST-TAIL  left */
		} else if (!s->cache_line[1].fill)
		{
			struct cache_line_t temp = s->cache_line[1];
			s->cache_line[1] = s->cache_line[2];
			s->cache_line[2] = s->cache_line[3];
			s->cache_line[3] = temp;
			/* free POST-tail is size is smaller than we want */
			if (s->cache_line[3].size < CACHE_LINE_SIZE)
			{
				free (s->cache_line[3].data);
				s->cache_line[3].data = 0;
				s->cache_line[3].size = 0;
			}
		} else { /* (s->cache_line[2].size >= CACHE_LINE_SIZE) */
			/* Merge POST-TAIL into TAIL, keep as much as possibly */

			/* any free space in TAIL? */
			int tail_fit = s->cache_line[2].size - s->cache_line[2].fill;

			/* how many bytes will we stuff into that free space in TAIL */
			int request_underflow = (s->cache_line[3].fill > tail_fit) ? tail_fit : s->cache_line[3].fill;

			/* how many bytes extra do we need to push out */
			int request_overflow = s->cache_line[3].fill - request_underflow;

			memmove (s->cache_line[2].data, s->cache_line[2].data + request_overflow, s->cache_line[2].fill - request_overflow);
			s->cache_line[2].offset += request_overflow;
			s->cache_line[2].fill -= request_overflow;

			memcpy (s->cache_line[2].data + s->cache_line[2].fill, s->cache_line[3].data, request_overflow);
			s->cache_line[2].fill += request_overflow;

			memcpy (s->cache_line[2].data + s->cache_line[2].fill, s->cache_line[3].data + request_overflow, request_underflow);
			s->cache_line[2].fill += request_underflow;

			s->cache_line[3].fill = 0;
		}
	}

	// is the head missing, but middle buffer can be used instead?
	if ( (!s->cache_line[0].fill) &&
	      (s->cache_line[1].fill) &&
	      (s->cache_line[1].offset == 0))
	{
		DEBUG_PRINT ("(MIDDLE BECOMES HEAD)\n");

		free (s->cache_line[0].data);
		s->cache_line[0] = s->cache_line[1];

		s->cache_line[1].fill = 0;
		s->cache_line[1].size = 0;
		s->cache_line[1].data = 0;

		DUMP_SELF (s);
	}

	// could we steal from the middle buffer and use it in the head?
	if ((s->cache_line[0].fill < CACHE_LINE_SIZE) &&
	    (s->cache_line[1].fill) &&
	    (s->cache_line[1].offset == s->cache_line[0].fill))
	{
		int hitlen = CACHE_LINE_SIZE - s->cache_line[0].fill;
		if (hitlen >= s->cache_line[1].fill)
		{
			hitlen = s->cache_line[1].fill;
		}

		if ((s->cache_line[0].fill + hitlen) > s->cache_line[0].size)
		{ /* grow the head if needed */
			void *data;
			data = realloc (s->cache_line[0].data, s->cache_line[0].fill + hitlen);
			if (!data)
			{
				return;
			}
			s->cache_line[0].data = data;
			s->cache_line[0].size = s->cache_line[0].fill + hitlen;
		}

		DEBUG_PRINT ("(HEAD IS STEALING FROM MIDDLE: HITLEN=%d)\n", (int)hitlen);

		memcpy  (s->cache_line[0].data + s->cache_line[0].fill, s->cache_line[1].data, hitlen);
		memmove (s->cache_line[1].data, s->cache_line[1].data + hitlen, s->cache_line[1].fill - hitlen);
		s->cache_line[0].fill += hitlen;
		s->cache_line[1].fill -= hitlen;
		if (!s->cache_line[1].fill)
		{
			free (s->cache_line[1].data);
			s->cache_line[1].fill = 0;
			s->cache_line[1].size = 0;
			s->cache_line[1].data = 0;
		}

		DUMP_SELF (s);
	}

	// if the last data-read caused us to read EOF, move middle-buffer into last-buffer
	if ( (!s->cache_line[2].fill) &&
	      (s->cache_line[1].fill) &&
	     ((s->cache_line[1].offset + s->cache_line[1].fill) == s->filesize) )
	{

		DEBUG_PRINT ("(MIDDLE BECOMES TAIL)\n");

		free (s->cache_line[2].data); // just in case it is pre-allocated
		s->cache_line[2] = s->cache_line[1];
		s->cache_line[1].fill = 0;
		s->cache_line[1].size = 0;
		s->cache_line[1].data = 0;

		DUMP_SELF (s);
	}

	// if middle data has space in the last-buffer, move it
	if ( s->cache_line[1].fill &&
	     (CACHE_LINE_SIZE > s->cache_line[2].fill) &&
	     ((s->cache_line[1].offset + s->cache_line[1].fill) == s->cache_line[2].offset) )
	{
		int hitlen = CACHE_LINE_SIZE - s->cache_line[2].fill;
		if (hitlen >= s->cache_line[1].fill)
		{
			hitlen = s->cache_line[1].fill;
		}

		if ((s->cache_line[2].fill + hitlen) < s->cache_line[2].size)
		{
			void *data = realloc (s->cache_line[2].data, s->cache_line[2].fill + hitlen);
			if (!data)
			{
				return;
			}
			s->cache_line[2].data = data;
			s->cache_line[2].size = s->cache_line[2].fill + hitlen;
		}

		DEBUG_PRINT ("(TAIL IS STEALING FROM MIDDLE: HITLEN=%d)\n", (int)hitlen);

		memmove (s->cache_line[2].data + hitlen, s->cache_line[2].data, s->cache_line[2].fill);
		memcpy  (s->cache_line[2].data, s->cache_line[1].data + s->cache_line[1].fill - hitlen, hitlen);
		s->cache_line[2].offset -= hitlen;
		s->cache_line[2].fill += hitlen;
		s->cache_line[1].fill -= hitlen;
		if (!s->cache_line[1].fill)
		{
			free (s->cache_line[1].data);
			s->cache_line[1].fill = 0;
			s->cache_line[1].size = 0;
			s->cache_line[1].data = 0;
		}

		DUMP_SELF (s);
	}
}

static int cache_filehandle_seek_and_read (struct cache_ocpfilehandle_t *s, uint64_t pos, void *dst, int len)
{
	int readresult;

	if (s->handle_pos != pos)
	{
		if (s->parent->seek_set (s->parent, pos))
		{
			s->error = 1;
			memset (dst, 0, len);
			return 0;
		}
		s->handle_pos = pos;
	}

	readresult = s->parent->read (s->parent, dst, len);
	s->handle_pos += readresult;

#if 0
	if (s->filesize_pending)
	{
		uint64_t temp2 = s->pos + readresult;
		if (temp2 > s->filesize)
		{
			s->filesize = temp2;
		}
		if (s->parent->eof (s->parent))
		{
			s->filesize_pending = 0;
		}
	}
#else
	{
		uint64_t temp2 = s->pos + readresult;
		if (temp2 > s->filesize)
		{ /* should never happen if s->filesize_pending, but does not hurt performing this task */
			s->filesize = temp2;
		}
		if (s->parent->eof (s->parent))
		{
			s->filesize_pending = 0;
		}
	}
#endif

	if (readresult != len)
	{
		s->error = s->parent->error (s->parent);
	}

	return readresult;
}

static int cache_filehandle_read (struct ocpfilehandle_t *_s, void *dst, int len)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;
	int readresult;

	int returnvalue = 0;

	if (s->error)
	{
		return 0;
	}

	if (len < 0)
	{
		return 0;
	}

	if (s->error)
	{
		//memset (dst, 0, len);
		return 0;
	}

	if (!s->filesize_pending)
	{
		if ((s->pos + len) > s->filesize)
		{
			len = s->filesize - len;
		}
	}

	if (!len)
	{
		return 0;
	}

	/* do we have data-hit in the HEAD-buffer that starts from position zero?
	 *
	 * |----FILE-DATA-ON-DISK------------------------------------|
	 * |                                                         |
	 * |-HEAD-CACHE-]   [-MOVING-WINDOW-CACHE-]     [-TAIL-CACHE-|
	 *
	 *       ^
	 *       | (is this start point within HEAD-CACHE ?
	 *       |
	 *       [-READ-REQUEST-]
	 *
	 * If so, we can partially and/or fully complete the request
	 **/

	if (s->pos < s->cache_line[0].fill)
	{
		int hitlen = (s->cache_line[0].fill - s->pos);

		if (hitlen >= len)
		{
			hitlen = len;
		}

		DEBUG_PRINT ("CACHE HEAD: POS=%d LEN=%d\n", (int)s->pos, hitlen);

		memcpy (dst, s->cache_line[0].data + s->pos, hitlen);
		returnvalue += hitlen;
		dst = ((char *)dst) + hitlen;
		len -= hitlen;
		s->pos += hitlen;

		DUMP_SELF (s);
	}

	/* Is the request fully replied yet? */
	while (len)
	{
		int iterlen = len; /* we temporary store len into a variable, since we might temporary adjust the size to avoid the MOVING-WINDOW-CACHE to grow into TAIL-CACHE */

		/* do we have data-hit in the TAIL-buffer?
		 *
		 * |----FILE-DATA-ON-DISK------------------------------------|
		 * |                                                         |
		 * |-HEAD-CACHE-]   [-MOVING-WINDOW-CACHE-]     [-TAIL-CACHE-|
		 *
		 *                                                       ^
		 *     is the end of the request within the TAIL-CACHE ? |
		 *                                                       |
		 *                                        [-READ-REQUEST-]
		 *
		 * If the entire read-request is withing the TAIL-CACHE we can complete the request now
		 *
		 * If only parts of the read-request is within the TAIL-CACHE, we temporary make it look smaller, so MOVING-WINDOW-CACHE do not cross into TAIL-CACHE
		 **/

		if ( s->cache_line[2].fill &&
		     ((s->pos + iterlen) > s->cache_line[2].offset) )
		{
			/* maybe the entire thing is in this BUFFER */
			if (s->pos >= s->cache_line[2].offset)
			{
				if (s->pos < (s->cache_line[2].offset + s->cache_line[2].fill))
				{
					if (s->pos + iterlen > s->filesize)
					{
						iterlen = s->filesize - s->pos;
					}

					DEBUG_PRINT ("CACHE TAIL: POS=%d LEN=%d\n", (int)s->pos, (int)iterlen);

					memcpy (dst, s->cache_line[2].data + s->pos - s->cache_line[2].offset, iterlen);
					returnvalue += iterlen;
					len -= iterlen;
					dst = (char *)dst + iterlen;
					s->pos = s->pos + iterlen;
					DUMP_SELF (s);

					if (!len)
					{
						return returnvalue;
					}
				}

				/* we want to read past EOF...
				 *
				 * |----FILE-DATA-ON-DISK------------------------------------|
				 * |                                                         |
				 * |-HEAD-CACHE-]   [-MOVING-WINDOW-CACHE-]     [-TAIL-CACHE-]
				 *                                                            [-POST-TAIL-CACHE-]
				 */
				/* make sure that the POST TAIL buffer is initialized */
				if (!s->cache_line[3].size)
				{
					s->cache_line[3].data = calloc (1, CACHE_LINE_SIZE);
					if (!s->cache_line[3].data)
					{
						s->error = 1;
						//memset (dst, 0, len);
						return returnvalue;
					}
					s->cache_line[3].size = CACHE_LINE_SIZE;
				}
				s->cache_line[3].offset = s->pos;
				s->cache_line[3].fill = 0;
				if (iterlen > s->cache_line[3].size)
				{
					iterlen = s->cache_line[3].size;
				}
				readresult = cache_filehandle_seek_and_read (s, s->pos, s->cache_line[3].data, iterlen);
				s->cache_line[3].fill = readresult;
				DEBUG_PRINT ("CACHE POST TAIL: POS=%d LEN=%d RESULT=%d\n", (int)s->pos, (int)iterlen, readresult);

				if (readresult)
				{
					memcpy (dst, s->cache_line[3].data, readresult);

					returnvalue += readresult;
					len -= readresult;
					dst = (char *)dst + readresult;
					s->pos = s->pos + readresult;

					cache_steal_buffers (s);

					DUMP_SELF (s);

					if (len)
					{
						continue;
					}
				}
				return returnvalue;
			} else {
				/* limit the length of this iteration to run upto the TAIL buffer */
				if (s->pos + len >= s->cache_line[2].offset)
				{
					iterlen = s->cache_line[2].offset - s->pos;
				}
			}
		}

		/* At this point, we need I/O to be available for sure, since we are going to play with the middle buffer */
		if (!s->parent)
		{
			s->parent = s->owner->open (s->owner);
			if (!s->parent)
			{
				s->error = 1;
				//memset (dst, 0, len);
				return returnvalue;
			}
		}

		/* make sure that the middle buffer is initialized */
		if (!s->cache_line[1].size)
		{
			s->cache_line[1].data = calloc (1, CACHE_LINE_SIZE);
			if (!s->cache_line[1].data)
			{
				s->error = 1;
				//memset (dst, 0, len);
				return returnvalue;
			}
			s->cache_line[1].offset = s->cache_line[0].fill;
			s->cache_line[1].fill = 0;
			s->cache_line[1].size = CACHE_LINE_SIZE;
		}


		/* Is the head-data needed missing?
		 *
		 * |----FILE-DATA-ON-DISK------------------------------------|
		 * |                                                         |
		 * |-HEAD-CACHE-]    .  [-MOVING-WINDOW-CACHE-] [-TAIL-CACHE-|
		 *
		 *                   ^
		 *                   | Is the start of the request infront of the MOVING-WINDOW-CACHE
		 *                   |
		 *                   [-READ-REQUEST-]
		 *
		 * If the entire read-request is withing the TAIL-CACHE we can complete the request now
		 *
		 * If only parts of the read-request is within the TAIL-CACHE, we temporary make it look smaller, so MOVING-WINDOW-CACHE do not cross into TAIL-CACHE
		 **/
		if (s->pos < s->cache_line[1].offset)
		{
			size_t premiss = s->cache_line[1].offset - s->pos;
			if (premiss < s->cache_line[1].size)
			{
				/* can we actually keep any data? */
				size_t keep = s->cache_line[1].size - premiss;
				if (keep > s->cache_line[1].fill)
				{
					keep = s->cache_line[1].fill;
				}

				DEBUG_PRINT ("CACHE MIDDLE PARTIALLY KEEP=%d RESTART POS=%d PREMISSLEN=%d\n", (int)keep, (int)s->pos, (int)premiss);

				memmove (s->cache_line[1].data + premiss,
				         s->cache_line[1].data,
				         keep);
				s->cache_line[1].fill = keep + premiss;

				s->cache_line[1].offset = s->pos;

				readresult = cache_filehandle_seek_and_read (s, s->pos, s->cache_line[1].data, premiss);
				if (readresult != premiss)
				{
					s->error = 1; /* this should not happen */
					s->cache_line[1].fill = 0;
					return returnvalue;
				}
				DUMP_SELF (s);
			} else {
				/* no data can be kept */
				s->cache_line[1].offset = s->pos;
				if (iterlen > s->cache_line[1].size)
				{
					iterlen = s->cache_line[1].size;
				}
				s->cache_line[1].fill = iterlen;

				DEBUG_PRINT ("CACHE MIDDLE RESTART POS=%d LEN=%d\n", (int)s->pos, (int)s->cache_line[1].fill);

				readresult = cache_filehandle_seek_and_read (s, s->pos, s->cache_line[1].data, s->cache_line[1].fill);
				if (readresult != s->cache_line[1].fill)
				{
					s->error = 1; /* this should not happen */
					s->cache_line[1].fill = 0;
					return returnvalue;
				}
				DUMP_SELF (s);
			}
#if 0 //This will be caught by the next test

			if (iterlen > s->cache_line[1].fill)
			{
				iterlen = s->cache_line[1].fill;
			}
			s->pos += iterlen;
			memcpy (dst, s->cache_line[1].data, iterlen);
			returnvalue += iterlen;
			dst = (char *)dst + iterlen;
			len -= iterlen;

			cache_steal_buffers (s);

			continue;
#endif
		}

		/* is there any data in the cache-line that can be used? Due to the block above, we know that pos must be equal to or bigger than MOVING-WINDOW-CACHE
		 *
		 * |----FILE-DATA-ON-DISK------------------------------------|
		 * |                                                         |
		 * |-HEAD-CACHE-]    [-MOVING-WINDOW-CACHE-]    [-TAIL-CACHE-|
		 *
		 *                     ^
		 *                     | Is the start of the request inside the MOVING-WINDOW-CACHE. We know it can not be infront of it!
		 *                     |
		 *                     [-READ-REQUEST-]
		 **/
redo_middle_cache:
		if (/*(s->cache_line[1].fill) &&*/
		    ((s->cache_line[1].offset + s->cache_line[1].fill) > s->pos) )
		{
			int hitlen = s->cache_line[1].offset + s->cache_line[1].fill - s->pos;
			if (iterlen > hitlen)
			{
				iterlen = hitlen;
			}

			DEBUG_PRINT ("CACHE MIDDLE REUSE POS=%d LEN=%d\n", (int)s->pos, iterlen);

			memcpy (dst, s->cache_line[1].data + (s->pos - s->cache_line[1].offset), iterlen);
			s->pos += iterlen;
			returnvalue += iterlen;
			dst = (char *)dst + iterlen;
			len -= iterlen;

			DUMP_SELF (s);

			cache_steal_buffers (s);

			continue;
		}

		/* is there more space in the cache-line that can be utilized
		 * due to "Is the head-data needed missing?", we know that
		 * s->pos >= s->cache_line[1].offset
		 *
		 * |----FILE-DATA-ON-DISK------------------------------------|
		 * |                                                         |
		 * |-HEAD-CACHE-] [-MOVING-WINDOW-CACHE-|---]   [-TAIL-CACHE-|
		 *
		 *                                        ^
		 *                                        | Is the start of the request inside the unused space of MOVING-WINDOW-CACHE ?
		 *                                        |
		 *                                        [-READ-REQUEST-]
		 **/
redo_topup:
		if ( (s->cache_line[1].fill < s->cache_line[1].size) /* do we have free space */ &&
		     ((s->cache_line[1].offset + s->cache_line[1].size) > s->pos) )
		{
			size_t middle_end_pos = s->cache_line[1].offset + s->cache_line[1].fill;
			size_t available_len = s->cache_line[1].size - s->cache_line[1].fill;
			size_t needed_len = (s->pos + iterlen) - middle_end_pos;
			size_t add_len;
			if (available_len > needed_len)
			{
				add_len = needed_len;
			} else {
				add_len = available_len;
			}

			DEBUG_PRINT ("CACHE MIDDLE TOPUP LEN=%d\n", (int)add_len);

			readresult = cache_filehandle_seek_and_read (s, middle_end_pos, s->cache_line[1].data + s->cache_line[1].fill, add_len);

			if (readresult != add_len)
			{
				s->error = 1;
				return returnvalue;
			}

			s->cache_line[1].fill += add_len;

			DUMP_SELF (s);

			goto redo_middle_cache; // continue
		}


		/* can we keep anything in the middle buffer.....the read request is behind it
		 *
		 * |----FILE-DATA-ON-DISK------------------------------------|
		 * |                                                         |
		 * |-HEAD-CACHE-] [-MOVING-WINDOW-CACHE-]       [-TAIL-CACHE-|
                 *                       [-WINDOW-CACHE-|------] 
		 *
		 *                                         ^
		 *                                         |
		 *                                         |
		 *                                         [-READ-REQUEST-]
		 **/
		if ((s->pos + iterlen) < (s->cache_line[1].offset + s->cache_line[1].fill + s->cache_line[1].size))
		{
			size_t keep = (s->cache_line[1].offset + s->cache_line[1].fill + s->cache_line[1].size) - (s->pos + iterlen);
                                                    

			DEBUG_PRINT ("CACHE MIDDLE, ONLY KEEP LAST LEN=%d\n", (int)keep);

			memmove (s->cache_line[1].data,
			         s->cache_line[1].data + s->cache_line[1].fill - keep,
			         keep);
			s->cache_line[1].offset += s->cache_line[1].fill - keep; 
			s->cache_line[1].fill = keep;

			DUMP_SELF (s);

			goto redo_topup;
		}


		/* our read-request is totally missing MOVING-WINDOW-CACHE, so reset it
		 *
		 * |----FILE-DATA-ON-DISK----------------------------------------------------------|
		 * |                                                                               |
		 * |-HEAD-CACHE-] [-MOVING-WINDOW-CACHE-]                             [-TAIL-CACHE-|
                 *                    
		 *
		 *                                                              ^
		 *                                                              |
		 *                                                              |
		 *                                                              [-READ-REQUEST-]
		 **/
		if (1)
		{
			if (iterlen > s->cache_line[1].size)
			{
				iterlen = s->cache_line[1].size;
			}
			s->cache_line[1].fill = iterlen;
			s->cache_line[1].offset = s->pos;

			DEBUG_PRINT ("CACHE MIDDLE, RESET POS=%d LEN=%d\n", (int)s->pos, (int)iterlen);

			readresult = cache_filehandle_seek_and_read (s, s->pos, s->cache_line[1].data, iterlen);
			if (readresult != iterlen)
			{
				s->error = 1;
				return returnvalue;
			}
			memcpy (dst, s->cache_line[1].data, iterlen);
			returnvalue += iterlen;
			s->pos += iterlen;
			dst = (char *)dst + iterlen;
			len -= iterlen;

			DUMP_SELF (s);

			cache_steal_buffers (s);

			continue;
		}
	}

	return returnvalue;
}

static int cache_filehandle_ioctl (struct ocpfilehandle_t *_s, const char *cmd, void *ptr)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	return s->parent->ioctl (s->parent, cmd, ptr);
}

static uint64_t cache_filehandle_filesize (struct ocpfilehandle_t * _s)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	return s->filesize;
}

static int cache_filehandle_filesize_ready (struct ocpfilehandle_t *_s)
{
	struct cache_ocpfilehandle_t *s = (struct cache_ocpfilehandle_t *)_s;

	return !s->filesize_pending;
}
