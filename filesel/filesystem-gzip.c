/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress GZIP files using zLib
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "types.h"
#include "adbmeta.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-gzip.h"
#include "stuff/framelock.h"

#ifndef INPUTBUFFERSIZE
# define INPUTBUFFERSIZE  65536
#endif

#ifndef OUTPUTBUFFERSIZE
# define OUTPUTBUFFERSIZE 65536
#endif

#define LARGEST_THEORETICALLY_32BIT_SIZE 0x3f80fe // based on TAIL-32bit original size information is wrapping for LARGE objects, and theoretically largest compression ratio is 1032:1 =>  0xffffffff / 1032

#if defined(GZIP_DEBUG) || defined(GZIP_VERBOSE)
static int do_gzip_debug_print=1;
#endif

#ifdef GZIP_DEBUG
#define DEBUG_PRINT(...) do { if (do_gzip_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#ifdef GZIP_VERBOSE
#define VERBOSE_PRINT(...) do { if (do_gzip_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define VERBOSE_PRINT(...) {}
#endif

struct gzip_ocpfilehandle_t
{
	struct ocpfilehandle_t head;

	struct ocpfilehandle_t *compressedfilehandle;

	z_stream strm;
	int eofhit;
	uint8_t inputbuffer[INPUTBUFFERSIZE];
	uint8_t outputbuffer[OUTPUTBUFFERSIZE];

	uint8_t *outputbuffer_pos;
	int      outputbuffer_fill;

	struct gzip_ocpfile_t *owner;

	uint64_t realpos;
	uint64_t pos;

	int need_deinit;
	int error;
};

struct gzip_ocpfile_t
{
	struct ocpfile_t      head;
	struct ocpfile_t     *compressedfile;

	int                   filesize_pending;
	uint64_t uncompressed_filesize;
};

struct gzip_ocpdir_t
{
	struct ocpdir_t       head;
	struct gzip_ocpfile_t child;
};

static int gzip_ocpfilehandle_inflateInit (struct gzip_ocpfilehandle_t *s)
{
	int retval;

	if (s->need_deinit)
	{
		inflateEnd (&s->strm);
		s->need_deinit = 0;
	}

	s->error = 0;
	s->realpos = 0;

	s->outputbuffer_pos = 0;
	s->outputbuffer_fill = 0;

	s->eofhit = 0;

	if (s->compressedfilehandle->seek_set (s->compressedfilehandle, 0) < 0)
	{
		s->error = 1;
		return -1;
	}

	memset (&s->strm, 0, sizeof (s->strm));

	s->strm.next_in = s->inputbuffer;
	retval = s->compressedfilehandle->read (s->compressedfilehandle, s->inputbuffer, INPUTBUFFERSIZE);
	if (retval <= 0)
	{
		s->error = 1;
		return -1;
	}
	s->strm.avail_in = retval;

	if (inflateInit2(&s->strm, (16+MAX_WBITS)) != Z_OK)
	{
		s->error = 1;
		return -1;
	}

	s->need_deinit = 1;

	return 0;
}

static void gzip_ocpfilehandle_ref (struct ocpfilehandle_t *_s)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;

	s->head.refcount++;
}

static void gzip_ocpfilehandle_unref (struct ocpfilehandle_t *_s)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;

	s->head.refcount--;
	if (s->head.refcount)
	{
		return;
	}

	if (s->need_deinit)
	{
		inflateEnd (&s->strm);
		s->need_deinit = 0;
	}

	dirdbUnref (s->head.dirdb_ref, dirdb_use_filehandle);

	if (s->compressedfilehandle)
	{
		s->compressedfilehandle->unref (s->compressedfilehandle);
		s->compressedfilehandle = 0;
	}

	if (s->owner)
	{
		s->owner->head.unref (&s->owner->head);
	}

	free (s);
}

static int gzip_ocpfilehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;

	if (pos < 0) return -1;

	if (s->owner->filesize_pending)
	{
		if (pos > s->pos)
		{
			if (s->owner->head.filesize (&s->owner->head) == FILESIZE_ERROR) /* force filesize to be ready */
			{
				s->error = 1;
				return -1;
			}
		}
	} else {
		if (pos > s->owner->uncompressed_filesize) return -1;
	}

	s->pos = pos;
	s->error = 0;

	return 0;
}

static int gzip_ocpfilehandle_seek_cur (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;

	if (pos <= 0)
	{
		if (pos == INT64_MIN) return -1; /* we never have files this size */
		if ((-pos) > s->pos) return -1;
		s->pos += pos;
	} else {
		/* check for overflow */
		if ((int64_t)(pos + s->pos) < 0) return -1;

		if (s->owner->filesize_pending)
		{
			if (_s->filesize (_s) == FILESIZE_ERROR) /* force the size to be calculated */
			{
				s->error = 1;
				return -1;
			}
		}

		if ((pos + s->pos) > s->owner->uncompressed_filesize) return -1;
		s->pos += pos;
	}

	s->error = 0;

	return 0;
}

static int gzip_ocpfilehandle_seek_end (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;

	if (pos > 0) return -1;

	if (pos == INT64_MIN) return -1; /* we never have files this size */

	if (s->owner->filesize_pending)
	{
		if (_s->filesize (_s) == FILESIZE_ERROR) /* force the size to be calculated */
		{
			s->error = 1;
			return -1;
		}
	}

	if (pos < -(int64_t)(s->owner->uncompressed_filesize)) return -1;

	s->pos = s->owner->uncompressed_filesize + pos;

	s->error = 0;

	return 0;
}


static uint64_t gzip_ocpfilehandle_getpos (struct ocpfilehandle_t *_s)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;

	return s->pos;
}

static int gzip_ocpfilehandle_eof (struct ocpfilehandle_t *_s)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;

	if (!s->owner->filesize_pending)
	{
		if (_s->filesize (_s) == FILESIZE_ERROR) /* force the size to be calculated */
		{
			s->error = 1;
			return -1;
		}
	}
	return s->pos == s->owner->uncompressed_filesize;
}

static int gzip_ocpfilehandle_error (struct ocpfilehandle_t *_s)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;

	return s->error;
}

static int gzip_ocpfilehandle_read (struct ocpfilehandle_t *_s, void *dst, int len)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;
	int retval = 0;
	int recall = 0;

	/* do we need to reverse? */
	if ((s->pos < s->realpos) || (!s->need_deinit))
	{
		if (gzip_ocpfilehandle_inflateInit (s))
		{
			s->error = 1;
			return -1;
		}
	}

	/* we need for fast-forward */
	while (s->pos > s->realpos)
	{
		uint64_t targetskip = s->pos - s->realpos;
		int ret, inputsize;

		if (s->outputbuffer_fill)
		{
			int skip = s->outputbuffer_fill;
			if (targetskip < (uint64_t)skip)
			{
				skip = targetskip;
			}
			s->outputbuffer_fill -= skip;
			s->outputbuffer_pos += skip;
			s->realpos += skip;
			continue;
		}
		if (s->eofhit)
		{
			s->error = 1;
			return -1;
		}

		if (!s->strm.avail_in)
		{
			s->strm.next_in = s->inputbuffer;
			s->strm.avail_in = s->compressedfilehandle->read (s->compressedfilehandle, s->inputbuffer, INPUTBUFFERSIZE);
			if (s->compressedfilehandle->error (s->compressedfilehandle))
			{
				s->error = 1;
				return -1;
			}
		}

		s->strm.next_out = s->outputbuffer;
		s->strm.avail_out = OUTPUTBUFFERSIZE;
		s->outputbuffer_pos = s->outputbuffer;

		inputsize = s->strm.avail_in;
		ret = inflate (&s->strm, Z_NO_FLUSH);

		switch (ret)
		{
			default:
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				/* should not happen */
				s->error = 1;
				return -1;
			case Z_STREAM_END:
				s->eofhit = 1;
			case Z_OK:
				break;
		}
		s->outputbuffer_fill = OUTPUTBUFFERSIZE - s->strm.avail_out;
		if ((s->outputbuffer_fill == 0) && ((ret == Z_STREAM_END) || (inputsize == 0)))
		{
			/* should not happen when we are fast-forwarding... */
			s->error = 1;
			return -1;
		}

		if (!recall) /* yield ? */
		{
			preemptive_framelock();
			recall = 20;
		}
		recall--;
	}

	while (len)
	{
		int ret, inputsize;

		if (s->outputbuffer_fill)
		{
			int copy = len;
			if (copy > s->outputbuffer_fill)
			{
				copy = s->outputbuffer_fill;
			}
			memcpy (dst, s->outputbuffer_pos, copy);
			s->outputbuffer_pos += copy;
			s->outputbuffer_fill -= copy;
			retval += copy;
			len -= copy;
			dst += copy;
			s->pos += copy;
			s->realpos += copy;
			continue;
		}

		if (s->eofhit)
		{
			break;
		}

		if (!s->strm.avail_in)
		{
			s->strm.next_in = s->inputbuffer;
			s->strm.avail_in = s->compressedfilehandle->read (s->compressedfilehandle, s->inputbuffer, INPUTBUFFERSIZE);
			if (s->compressedfilehandle->error (s->compressedfilehandle))
			{
				s->error = 1;
				return -1;
			}
		}

		s->strm.next_out = s->outputbuffer;
		s->strm.avail_out = OUTPUTBUFFERSIZE;
		s->outputbuffer_pos = s->outputbuffer;

		inputsize = s->strm.avail_in;
		ret = inflate (&s->strm, Z_NO_FLUSH);

		switch (ret)
		{
			default:
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				/* should not happen */
				s->error = 1;
				return -1;
			case Z_STREAM_END:
				s->eofhit = 1;
			case Z_OK:
				break;
		}
		s->outputbuffer_fill = OUTPUTBUFFERSIZE - s->strm.avail_out;
#if 0
		if (ret == Z_STREAM_END)
#else
		if ((s->outputbuffer_fill == 0) && ((ret == Z_STREAM_END) || (inputsize == 0)))
#endif
		{
			uint64_t filesize = s->realpos + s->outputbuffer_fill;

			if ((s->owner->filesize_pending) || (s->owner->uncompressed_filesize != filesize))
			{
				uint8_t buffer[8];
				const char *filename = 0;
				uint64_t compressedfile_size = s->compressedfilehandle->filesize (s->compressedfilehandle);


				s->owner->filesize_pending = 0;
				s->owner->uncompressed_filesize = filesize;

				buffer[7] = filesize >> 56;
				buffer[6] = filesize >> 48;
				buffer[5] = filesize >> 40;
				buffer[4] = filesize >> 32;
				buffer[3] = filesize >> 24;
				buffer[2] = filesize >> 16;
				buffer[1] = filesize >> 8;
				buffer[0] = filesize;

				dirdbGetName_internalstr (s->compressedfilehandle->dirdb_ref, &filename);

				DEBUG_PRINT ("[GZIP filehandle_read EOF] adbMetaAdd(%s, %"PRId64", GZIP, [%02x %02x %02x %02x %02x %02x %02x %02x] (%"PRIu64" + %d => %"PRIu64")\n", filename, compressedfile_size, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], s->realpos, s->outputbuffer_fill, filesize);
				adbMetaAdd (filename, compressedfile_size, "GZIP", buffer, 8);
			}

			if (!s->outputbuffer_fill)
			{
				return retval;
			}
		}
	}

	return retval;
}

static uint64_t gzip_ocpfilehandle_filesize (struct ocpfilehandle_t *_s)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;
	return s->owner->head.filesize (&s->owner->head);
}

static int gzip_ocpfilehandle_filesize_ready (struct ocpfilehandle_t *_s)
{
	struct gzip_ocpfilehandle_t *s = (struct gzip_ocpfilehandle_t *)_s;

	return !s->owner->filesize_pending;
}

static void gzip_ocpfile_ref (struct ocpfile_t *s)
{
	s->parent->ref (s->parent);
}

static void gzip_ocpfile_unref (struct ocpfile_t *s)
{
	s->parent->unref (s->parent);
}

static struct ocpfilehandle_t *gzip_ocpfile_open (struct ocpfile_t *_s)
{
	struct gzip_ocpfile_t *s = (struct gzip_ocpfile_t *)_s;
	struct gzip_ocpfilehandle_t *retval = calloc (1, sizeof (*retval));

	if (!retval)
	{
		return 0;
	}

	ocpfilehandle_t_fill (&retval->head,
	                       gzip_ocpfilehandle_ref,
	                       gzip_ocpfilehandle_unref,
	                      &s->head,
	                       gzip_ocpfilehandle_seek_set,
	                       gzip_ocpfilehandle_seek_cur,
	                       gzip_ocpfilehandle_seek_end,
	                       gzip_ocpfilehandle_getpos,
	                       gzip_ocpfilehandle_eof,
	                       gzip_ocpfilehandle_error,
	                       gzip_ocpfilehandle_read,
	                       0, /* ioctl */
	                       gzip_ocpfilehandle_filesize,
	                       gzip_ocpfilehandle_filesize_ready,
	                       0, /* filename_override */
	                       dirdbRef (s->head.dirdb_ref, dirdb_use_filehandle));

	retval->owner = s;
	s->head.ref (&s->head);

	retval->compressedfilehandle = s->compressedfile->open (s->compressedfile);

	if (!retval->compressedfilehandle)
	{
		dirdbUnref (s->head.dirdb_ref, dirdb_use_filehandle);
		free (retval);
		return 0;
	}

	retval->head.refcount = 1;

	return &retval->head;
}

static int gzip_ocpfile_filesize_ready (struct ocpfile_t *_s)
{
	struct gzip_ocpfile_t *s = (struct gzip_ocpfile_t *)_s;

	return !s->filesize_pending;
}

static uint64_t gzip_ocpfile_filesize (struct ocpfile_t *_s)
{
	struct gzip_ocpfile_t *s = (struct gzip_ocpfile_t *)_s;
	struct ocpfilehandle_t *h = 0;
	uint64_t compressedfile_size = 0;
	z_stream strm = {0};
	uint8_t *inputbuffer;
	uint8_t *outputbuffer;
	uint64_t filesize = 0;
	uint8_t buffer[8];
	int ret; // for zlib
	const char *filename = 0;

	if (!s->filesize_pending)
	{
		return s->uncompressed_filesize;
	}

/* First we re-check the CACHE */
	if (s->compressedfile->filesize_ready (s->compressedfile))
	{
		compressedfile_size = s->compressedfile->filesize (s->compressedfile);

		uint8_t *metadata = 0;
		size_t metadatasize = 0;

		if ((compressedfile_size < 12) || (compressedfile_size == FILESIZE_ERROR) || (compressedfile_size == FILESIZE_STREAM))
		{
			return FILESIZE_ERROR;
		}

		dirdbGetName_internalstr (s->compressedfile->dirdb_ref, &filename);

		if (!adbMetaGet (filename, compressedfile_size, "GZIP", &metadata, &metadatasize))
		{
			if (metadatasize == 8)
			{
				s->filesize_pending = 0;
				s->uncompressed_filesize = ((uint64_t)(metadata[7]) << 56) |
				                           ((uint64_t)(metadata[6]) << 48) |
				                           ((uint64_t)(metadata[5]) << 40) |
				                           ((uint64_t)(metadata[4]) << 32) |
				                           ((uint64_t)(metadata[3]) << 24) |
				                           ((uint64_t)(metadata[2]) << 16) |
				                           ((uint64_t)(metadata[1]) << 8) |
				                           ((uint64_t)(metadata[0]));
				free (metadata);

				DEBUG_PRINT ("[GZIP ocpfile_filesize]: got metadatasize=0x%08lx %02x %02x %02x %02x %02x %02x %02x %02x => %"PRIu64"\n", metadatasize, metadata[0], metadata[1], metadata[2], metadata[3], metadata[4], metadata[5], metadata[6], metadata[7], s->uncompressed_filesize);

				return s->uncompressed_filesize;
			}
			free (metadata); /* wrong size???... */
			metadata = 0;
		}

/* Second we check the TAIL of the GZIP... */
		if (compressedfile_size > LARGEST_THEORETICALLY_32BIT_SIZE)
		{
			goto UseZlib;
		}

		h = s->compressedfile->open (s->compressedfile);
		if (!h)
		{
			return FILESIZE_ERROR;
		}

		/* Is this file a standard GZIP? */
		if (h->read (h, buffer, 3) != 3)
		{
			h->unref (h); h = 0;
			return FILESIZE_ERROR;
		}
		if ((buffer[0] != 0x1f) /* MAGIC_ID1 */ ||
		    (buffer[1] != 0x8b) /* MAGIC_ID2 */ ||
		    (buffer[2] != 0x08) /* CompressionMethod == Deflate */)
		{
			if (h->seek_set (h, 0) < 0)
			{
				return FILESIZE_ERROR;
			}
			goto UseZlib;
		}

		/* If so, the original size (for the last member....) is stored at the end of the file... as 32bit, hence the test for LARGEST_THEORETICALLY_32BIT_SIZE */
		if (h->seek_end (h, -4) < 0)
		{
			h->unref (h); h = 0;
			return FILESIZE_ERROR;
		}
		if (h->read (h, buffer, 4) != 4)
		{
			h->unref (h); h = 0;
			return FILESIZE_ERROR;
		}
		h->unref (h); h = 0;
		s->uncompressed_filesize = (buffer[3] << 24) |
		                           (buffer[2] << 16) |
		                           (buffer[1] << 8) |
		                           (buffer[0]);
		s->filesize_pending = 0;

		buffer[4] = 0;
		buffer[5] = 0;
		buffer[6] = 0;
		buffer[7] = 0;;
		adbMetaAdd (filename, compressedfile_size, "GZIP", buffer,  8);
		return s->uncompressed_filesize;
	}

/* Third, we decompress the wole thing... */
UseZlib:
	if (!h)
	{
		h = s->compressedfile->open (s->compressedfile);
		if (!h)
		{
			return FILESIZE_ERROR;
		}
	}

	inputbuffer = malloc (INPUTBUFFERSIZE);
	if (!inputbuffer)
	{
		h->unref (h);
		h = 0;
		return FILESIZE_ERROR;
	}
	outputbuffer = malloc (OUTPUTBUFFERSIZE);
	if (!outputbuffer)
	{
		h->unref (h);
		h = 0;
		free (inputbuffer);
		return FILESIZE_ERROR;
	}

	strm.next_in = inputbuffer;
	strm.avail_in = h->read (h, inputbuffer, INPUTBUFFERSIZE);

	if (inflateInit2(&strm, (16+MAX_WBITS)) != Z_OK)
	{
		free (outputbuffer);
		h->unref (h);
		h = 0;
		return FILESIZE_ERROR;
	}

	do
	{
		if (!strm.avail_in) // this is false for the first iteration
		{
			strm.next_in = inputbuffer;
			strm.avail_in = h->read (h, inputbuffer, INPUTBUFFERSIZE);
			if (h->error (h))
			{
				inflateEnd (&strm);
				free (inputbuffer);
				free (outputbuffer);
				h->unref (h);
				h = 0;
				return FILESIZE_ERROR;
			}
		}
		if (strm.avail_in == 0) // EOF hit, without file-error
		{
			break;
		}

		do
		{
			strm.next_out = outputbuffer;
			strm.avail_out = OUTPUTBUFFERSIZE;

			ret = inflate (&strm, Z_NO_FLUSH);

			switch (ret)
			{
				default:
				case Z_NEED_DICT:
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					inflateEnd (&strm);
					free (inputbuffer);
					free (outputbuffer);
					h->unref (h);
					h = 0;
					return FILESIZE_ERROR;
				case Z_STREAM_END:
				case Z_OK:
					break;
			}
			filesize += OUTPUTBUFFERSIZE - strm.avail_out;
		 } while ((strm.avail_in != 0) && (ret != Z_STREAM_END));
	} while (ret != Z_STREAM_END);

	inflateEnd (&strm);
	free (inputbuffer);
	free (outputbuffer);
	h->unref (h);
	h = 0;

	s->filesize_pending = 0;
	s->uncompressed_filesize = filesize;

	buffer[7] = filesize >> 56;
	buffer[6] = filesize >> 48;
	buffer[5] = filesize >> 40;
	buffer[4] = filesize >> 32;
	buffer[3] = filesize >> 24;
	buffer[2] = filesize >> 16;
	buffer[1] = filesize >> 8;
	buffer[0] = filesize;

	if (!filename)
	{
		dirdbGetName_internalstr (s->compressedfile->dirdb_ref, &filename);
	}

	compressedfile_size = s->compressedfile->filesize (s->compressedfile);

	DEBUG_PRINT ("[GZIP file_filesize] adbMetaAdd(%s, %"PRIu64", GZIP, [%02x %02x %02x %02x %02x %02x %02x %02x]\n", filename, compressedfile_size, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
	adbMetaAdd (filename, compressedfile_size, "GZIP", buffer, 8);

	return s->uncompressed_filesize;
}


static void gzip_dir_ref (struct ocpdir_t *self)
{
	struct gzip_ocpdir_t *s = (struct gzip_ocpdir_t *)self;
	s->head.refcount++;
}

static void gzip_dir_unref (struct ocpdir_t *self)
{
	struct gzip_ocpdir_t *s = (struct gzip_ocpdir_t *)self;
	s->head.refcount--;

	if (s->head.refcount)
	{
		return;
	}

	if (s->child.compressedfile)
	{
		s->child.compressedfile->unref (s->child.compressedfile);
		s->child.compressedfile = 0;
	}

	s->head.parent->unref (s->head.parent);
	s->head.parent = 0;

	dirdbUnref (s->head.dirdb_ref, dirdb_use_dir);
	dirdbUnref (s->child.head.dirdb_ref, dirdb_use_file);

	free (s);
}

struct gzip_dir_readdir_handle_t
{
	struct gzip_ocpdir_t *self;
	void(*callback_file)(void *token, struct ocpfile_t *);
	void *token;
};

static ocpdirhandle_pt gzip_dir_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct gzip_dir_readdir_handle_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{ /* out of memory */
		return 0;
	}
	retval->self = (struct gzip_ocpdir_t *)self;
	retval->callback_file = callback_file;
	retval->token = token;
	return retval;
}

static ocpdirhandle_pt gzip_dir_readflatdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void *token)
{
	struct gzip_dir_readdir_handle_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{ /* out of memory */
		return 0;
	}
	retval->self = (struct gzip_ocpdir_t *)self;
	retval->callback_file = callback_file;
	retval->token = token;
	return retval;
}

static void gzip_dir_readdir_cancel (ocpdirhandle_pt handle)
{
	free (handle);
}

static int gzip_dir_readdir_iterate (ocpdirhandle_pt handle)
{
	struct gzip_dir_readdir_handle_t *h = (struct gzip_dir_readdir_handle_t *)handle;
	h->callback_file (h->token, &h->self->child.head);
	return 0;
}

static struct ocpdir_t *gzip_dir_readdir_dir (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	return 0;
}

static struct ocpfile_t *gzip_dir_readdir_file (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	struct gzip_ocpdir_t *s = (struct gzip_ocpdir_t *)self;
	if (s->child.head.dirdb_ref == dirdb_ref)
	{
		s->child.head.ref (&s->child.head);
		return &s->child.head;
	}
	return 0;
}

/* steals the dirdb_ref */
static struct ocpdir_t *gzip_check_steal (struct ocpfile_t *s, const uint32_t dirdb_ref)
{
	struct gzip_ocpdir_t *retval = calloc (1, sizeof (*retval));

	ocpdir_t_fill (&retval->head,
	                gzip_dir_ref,
	                gzip_dir_unref,
	                s->parent,
	                gzip_dir_readdir_start,
	                gzip_dir_readflatdir_start,
	                gzip_dir_readdir_cancel,
	                gzip_dir_readdir_iterate,
	                gzip_dir_readdir_dir,
	                gzip_dir_readdir_file,
	                0,
	                s->dirdb_ref,
	                1, /* refcount */
	                1, /* is_archive */
	                0  /* is_playlist */);

	s->parent->ref (s->parent);
	dirdbRef (s->dirdb_ref, dirdb_use_dir);

	ocpfile_t_fill (&retval->child.head,
	                 gzip_ocpfile_ref,
	                 gzip_ocpfile_unref,
	                &retval->head,
	                 gzip_ocpfile_open,
	                 gzip_ocpfile_filesize,
	                 gzip_ocpfile_filesize_ready,
	                 0, /* filename_override */
	                 dirdb_ref,
	                 1, /* refcount */
	                 0  /* is_nodetect */);

	retval->child.filesize_pending = 1;
	retval->child.uncompressed_filesize = 0;

	retval->child.compressedfile = s;
	s->ref (s);

	if (s->filesize_ready (s))
	{
		unsigned char *metadata = 0;
		size_t metadatasize = 0;
		const char *filename = 0;

		dirdbGetName_internalstr (retval->child.compressedfile->dirdb_ref, &filename);

		if (!adbMetaGet (filename, retval->child.compressedfile->filesize (s), "GZIP", &metadata, &metadatasize))
		{
			if (metadatasize == 8)
			{
				retval->child.filesize_pending = 0;
				retval->child.uncompressed_filesize = ((uint64_t)(metadata[7]) << 56) |
				                                      ((uint64_t)(metadata[6]) << 48) |
				                                      ((uint64_t)(metadata[5]) << 40) |
				                                      ((uint64_t)(metadata[4]) << 32) |
				                                      ((uint64_t)(metadata[3]) << 24) |
				                                      ((uint64_t)(metadata[2]) << 16) |
				                                      ((uint64_t)(metadata[1]) << 8) |
				                                      ((uint64_t)(metadata[0]));

				DEBUG_PRINT ("[GZIP gzip_check_steal]: got metadatasize=0x%08lx %02x %02x %02x %02x %02x %02x %02x %02x => %"PRIu64"\n", metadatasize, metadata[0], metadata[1], metadata[2], metadata[3], metadata[4], metadata[5], metadata[6], metadata[7], retval->child.uncompressed_filesize);

			}
			free (metadata);
			metadata = 0;
		}
	}

	return &retval->head;
}

static struct ocpdir_t *gzip_check (const struct ocpdirdecompressor_t *ref, struct ocpfile_t *s, const char *filetype)
{
	struct ocpdir_t *retval;
	char *newname;
	int l;

	if (!strcasecmp (filetype, ".gz"))
	{
		dirdbGetName_malloc (s->dirdb_ref, &newname);
		l = strlen (newname);
		newname[l-3] = 0;
		retval = gzip_check_steal (s, dirdbFindAndRef (s->dirdb_ref, newname, dirdb_use_file));
		free (newname);
		return retval;
	}

	if (!strcasecmp (filetype, ".tgz"))
	{
		dirdbGetName_malloc (s->dirdb_ref, &newname);
		l = strlen (newname);
		strcpy (newname + l - 4, ".tar");
		retval = gzip_check_steal (s, dirdbFindAndRef (s->dirdb_ref, newname, dirdb_use_file));
		free (newname);
		return retval;
	}

	if (!strcasecmp (filetype, ".vgz"))
	{
		dirdbGetName_malloc (s->dirdb_ref, &newname);
		l = strlen (newname);
		strcpy (newname + l - 4, ".vgm");
		retval = gzip_check_steal (s, dirdbFindAndRef (s->dirdb_ref, newname, dirdb_use_file));
		free (newname);
		return retval;
	}

	return 0;
}

static struct ocpdirdecompressor_t gzipdirdecompressor =
{
	"gzip",
	"GZip and compress fileformats (using zlib)",
	gzip_check
};

void filesystem_gzip_register (void)
{
	register_dirdecompressor (&gzipdirdecompressor);
}
