/* OpenCP Module Player
 * copyright (c) 2021-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress UNIX Z compressed files
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
#include "types.h"
#include "adbmeta.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-z.h"
#include "stuff/framelock.h"

#ifndef INPUTBUFFERSIZE
# define INPUTBUFFERSIZE  65536
#endif

#if defined(Z_DEBUG) || defined(Z_VERBOSE)
static int do_Z_debug_print=1;
#endif

#ifdef Z_DEBUG
#define DEBUG_PRINT(...) do { if (do_Z_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#ifdef Z_VERBOSE
#define VERBOSE_PRINT(...) do { if (do_Z_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define VERBOSE_PRINT(...) {}
#endif

#include "z-unlzw.c"

struct Z_ocpfilehandle_t
{
	struct ocpfilehandle_t head;

	struct ocpfilehandle_t *compressedfilehandle;

	uint8_t inputbuffer[INPUTBUFFERSIZE];
	uint8_t *input_next;
	int      input_len;

	struct lzw_handle_t handle;
	int initialized;

	struct Z_ocpfile_t *owner;

	uint64_t realpos;
	uint64_t pos;

	int error;
};

struct Z_ocpfile_t /* head->parent always point to a Z_ocpdir_t */
{
	struct ocpfile_t      head;
	struct ocpfile_t     *compressedfile;

	int                   filesize_pending;
	uint64_t uncompressed_filesize;
};

struct Z_ocpdir_t
{
	struct ocpdir_t        head;
	struct Z_ocpfile_t child;
};

static int Z_ocpfilehandle_compressInit (struct Z_ocpfilehandle_t *s)
{
	int retval;

	unlzw_init (&s->handle);

	s->initialized = 1;

	s->error = 0;
	s->realpos = 0;

	if (s->compressedfilehandle->seek_set (s->compressedfilehandle, 0) < 0)
	{
		s->error = 1;
		return -1;
	}

	retval = s->compressedfilehandle->read (s->compressedfilehandle, s->inputbuffer, INPUTBUFFERSIZE);
	if (retval <= 0)
	{
		s->error = 1;
		return -1;
	}
	s->input_next = s->inputbuffer;
	s->input_len = retval;

	if (s->input_len <= 2)
	{
		s->error = 1;
		return -1;
	}
	if (memcmp (s->input_next, LZW_MAGIC, 2))
	{
		s->error = 1;
		return -1;
	}
	s->input_next += 2;
	s->input_len -= 2;

	return 0;
}

static void Z_ocpfilehandle_ref (struct ocpfilehandle_t *_s)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;

	s->head.refcount++;
}

static void Z_ocpfilehandle_unref (struct ocpfilehandle_t *_s)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;

	s->head.refcount--;
	if (s->head.refcount)
	{
		return;
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

static int Z_ocpfilehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;

	if (pos < 0) return -1;

	if (s->owner->filesize_pending)
	{
		if (pos > s->pos)
		{
			if (_s->filesize (_s) == FILESIZE_ERROR) /* force the size to be calculated */
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

static int Z_ocpfilehandle_seek_cur (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;

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

static int Z_ocpfilehandle_seek_end (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;

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


static uint64_t Z_ocpfilehandle_getpos (struct ocpfilehandle_t *_s)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;

	return s->pos;
}

static int Z_ocpfilehandle_eof (struct ocpfilehandle_t *_s)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;

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

static int Z_ocpfilehandle_error (struct ocpfilehandle_t *_s)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;

	return s->error;
}

static int Z_ocpfilehandle_read (struct ocpfilehandle_t *_s, void *dst, int len)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;
	int retval = 0;
	int recall = 0;
	int eofhit = 0;
	int eofhit2 = 0;

	/* do we need to reverse? */
	if ((s->pos < s->realpos) || (!s->initialized))
	{
		if (Z_ocpfilehandle_compressInit (s))
		{
			s->error = 1;
			return -1;
		}
	}

	while (len)
	{
		int ret;

		if (s->handle.outlen)
		{
			if (s->pos > s->realpos)
			{
				uint64_t targetskip = s->pos - s->realpos;
				int skip = s->handle.outlen;
				if (targetskip < (uint64_t)skip)
				{
					skip = targetskip;
				}
				s->handle.outlen -= skip;
				s->handle.outpos += skip;
				s->realpos += skip;
				continue;
			} else {
				int copy = len;
				if (copy > s->handle.outlen)
				{
					copy = s->handle.outlen;
				}
				memcpy (dst, s->handle.outbuf + s->handle.outpos, copy);
				s->handle.outpos += copy;
				s->handle.outlen -= copy;
				retval += copy;
				len -= copy;
				dst += copy;
				s->pos += copy;
				s->realpos += copy;
				continue;
			}
		}

		ret = unlzw_digest (&s->handle);
		if (ret < 0)
		{
			s->error = 1;
			return -1;
		} if (ret > 0)
		{
			continue;
		}
		eofhit2 = eofhit;

		if (!s->input_len)
		{
			s->input_next = s->inputbuffer;
			s->input_len = s->compressedfilehandle->read (s->compressedfilehandle, s->inputbuffer, INPUTBUFFERSIZE);
			if (s->compressedfilehandle->error (s->compressedfilehandle))
			{
				s->error = 1;
				return -1;
			}
			if (!s->input_len)
			{
				eofhit = 1;
				break;
			}
		}

		if (eofhit)
		{
			unlzw_flush (&s->handle);
		} else {
			ret = unlzw_feed (&s->handle, *s->input_next);
			s->input_next++;
			s->input_len--;
			if (ret < 0)
			{
				s->error = 1;
				return -1;
			}
		}

		if (!recall) /* yield ? */
		{
			preemptive_framelock();
			recall = 100000;
		}
		recall--;
	}

	if (eofhit && eofhit2 && (s->handle.bufferfill < s->handle.n_bits))
	{
		uint64_t filesize = s->realpos;

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

			DEBUG_PRINT ("[Z filehandle_read EOF] adbMetaAdd(%s, %"PRId64", Z, [%02x %02x %02x %02x %02x %02x %02x %02x] %"PRIu64"\n", filename, compressedfile_size, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], filesize);
			adbMetaAdd (filename, compressedfile_size, "Z", buffer, 8);
		}
	}

	return retval;
}

static uint64_t Z_ocpfilehandle_filesize (struct ocpfilehandle_t *_s)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;
	return s->owner->head.filesize (&s->owner->head);
}

static int Z_ocpfilehandle_filesize_ready (struct ocpfilehandle_t *_s)
{
	struct Z_ocpfilehandle_t *s = (struct Z_ocpfilehandle_t *)_s;

	return !s->owner->filesize_pending;
}

static void Z_ocpfile_ref (struct ocpfile_t *s)
{
	s->parent->ref (s->parent);
}

static void Z_ocpfile_unref (struct ocpfile_t *s)
{
	s->parent->unref (s->parent);
}

static struct ocpfilehandle_t *Z_ocpfile_open (struct ocpfile_t *_s)
{
	struct Z_ocpfile_t *s = (struct Z_ocpfile_t *)_s;
	struct Z_ocpfilehandle_t *retval = calloc (1, sizeof (*retval));

	if (!retval)
	{
		return 0;
	}

	ocpfilehandle_t_fill (&retval->head,
	                       Z_ocpfilehandle_ref,
	                       Z_ocpfilehandle_unref,
	                       _s,
	                       Z_ocpfilehandle_seek_set,
	                       Z_ocpfilehandle_seek_cur,
	                       Z_ocpfilehandle_seek_end,
	                       Z_ocpfilehandle_getpos,
	                       Z_ocpfilehandle_eof,
	                       Z_ocpfilehandle_error,
	                       Z_ocpfilehandle_read,
	                       0, /* ioctl */
	                       Z_ocpfilehandle_filesize,
	                       Z_ocpfilehandle_filesize_ready,
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

static int Z_ocpfile_filesize_ready (struct ocpfile_t *_s)
{
	struct Z_ocpfile_t *s = (struct Z_ocpfile_t *)_s;

	return !s->filesize_pending;
}

static uint64_t Z_ocpfile_filesize (struct ocpfile_t *_s)
{
	struct Z_ocpfile_t *s = (struct Z_ocpfile_t *)_s;
	struct ocpfilehandle_t *h = 0;
	uint64_t compressedfile_size = 0;

	struct lzw_handle_t handle;
	uint8_t *inputbuffer;
	uint8_t *input_next;
	int      input_len;
	uint64_t filesize = 0;
	uint8_t buffer[8];
	int ret; // for zlib
	const char *filename = 0;
	int eof = 0;

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

		if (!adbMetaGet (filename, compressedfile_size, "Z", &metadata, &metadatasize))
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

				DEBUG_PRINT ("[Z ocpfile_filesize]: got metadatasize=0x%08lx %02x %02x %02x %02x %02x %02x %02x %02x => %"PRIu64"\n", metadatasize, metadata[0], metadata[1], metadata[2], metadata[3], metadata[4], metadata[5], metadata[6], metadata[7], s->uncompressed_filesize);

				return s->uncompressed_filesize;
			}
			free (metadata); /* wrong size???... */
			metadata = 0;
		}
	}

/* Second, we decompress the wole thing... */
	h = s->compressedfile->open (s->compressedfile);
	if (!h)
	{
		return FILESIZE_ERROR;
	}

	inputbuffer = malloc (INPUTBUFFERSIZE);
	if (!inputbuffer)
	{
		h->unref (h);
		h = 0;
		return FILESIZE_ERROR;
	}
	input_next = inputbuffer;
	input_len = h->read (h, inputbuffer, INPUTBUFFERSIZE);

	if (input_len < 2)
	{
		free (inputbuffer);
		h->unref (h);
		h = 0;
		return FILESIZE_ERROR;
	}

	if (memcmp (input_next, LZW_MAGIC, 2))
	{
		free (inputbuffer);
		h->unref (h);
		h = 0;
		return FILESIZE_ERROR;
	}

	input_len -= 2;
	input_next += 2;

	unlzw_init (&handle);

	while (!eof)
	{
		if (!input_len) // this is false for the first iteration
		{
			input_next = inputbuffer;
			input_len = h->read (h, inputbuffer, INPUTBUFFERSIZE);

			if (h->error (h))
			{
				free (inputbuffer);
				h->unref (h);
				h = 0;
				return FILESIZE_ERROR;
			}

			if (!input_len) // this is false for the first iteration
			{
				eof = 1;
			}
		}

		if (eof)
		{
			unlzw_flush (&handle);
		} else {
			ret = unlzw_feed (&handle, *input_next);
			input_next++;
			input_len--;
			if (ret < 0)
			{
				free (inputbuffer);
				h->unref (h);
				h = 0;
				return FILESIZE_ERROR;
			}
		}

		while (1)
		{
			ret = unlzw_digest (&handle);
			if (ret < 0)
			{
				free (inputbuffer);
				h->unref (h);
				h = 0;
				return FILESIZE_ERROR;
			} else if (ret == 0)
			{
				break;
			} else {
				filesize +=  handle.outlen;
				handle.outlen = 0;
			}
		 }
	}

	free (inputbuffer);
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

	DEBUG_PRINT ("[Z file_filesize] adbMetaAdd(%s, %"PRIu64", Z, [%02x %02x %02x %02x %02x %02x %02x %02x]\n", filename, compressedfile_size, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
	adbMetaAdd (filename, compressedfile_size, "Z", buffer, 8);

	return s->uncompressed_filesize;
}

static void Z_dir_ref (struct ocpdir_t *self)
{
	struct Z_ocpdir_t *s = (struct Z_ocpdir_t *)self;
	s->head.refcount++;
}

static void Z_dir_unref (struct ocpdir_t *self)
{
	struct Z_ocpdir_t *s = (struct Z_ocpdir_t *)self;
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

struct Z_dir_readdir_handle_t
{
	struct Z_ocpdir_t *self;
	void(*callback_file)(void *token, struct ocpfile_t *);
	void *token;
};

static ocpdirhandle_pt Z_dir_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct Z_dir_readdir_handle_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{ /* out of memory */
		return 0;
	}
	retval->self = (struct Z_ocpdir_t *)self;
	retval->callback_file = callback_file;
	retval->token = token;
	return retval;
}

static ocpdirhandle_pt Z_dir_readflatdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void *token)
{
	struct Z_dir_readdir_handle_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{ /* out of memory */
		return 0;
	}
	retval->self = (struct Z_ocpdir_t *)self;
	retval->callback_file = callback_file;
	retval->token = token;
	return retval;
}

static void Z_dir_readdir_cancel (ocpdirhandle_pt handle)
{
	free (handle);
}

static int Z_dir_readdir_iterate (ocpdirhandle_pt handle)
{
	struct Z_dir_readdir_handle_t *h = (struct Z_dir_readdir_handle_t *)handle;
	h->callback_file (h->token, &h->self->child.head);
	return 0;
}

static struct ocpdir_t *Z_dir_readdir_dir (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	return 0;
}

static struct ocpfile_t *Z_dir_readdir_file (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	struct Z_ocpdir_t *s = (struct Z_ocpdir_t *)self;
	if (s->child.head.dirdb_ref == dirdb_ref)
	{
		s->child.head.ref (&s->child.head);
		return &s->child.head;
	}
	return 0;
}

/* steals the dirdb_ref */
static struct ocpdir_t *Z_check_steal (struct ocpfile_t *s, const uint32_t dirdb_ref)
{
	struct Z_ocpdir_t *retval = calloc (1, sizeof (*retval));

	ocpdir_t_fill (&retval->head,
	                Z_dir_ref,
	                Z_dir_unref,
	                s->parent,
	                Z_dir_readdir_start,
	                Z_dir_readflatdir_start,
	                Z_dir_readdir_cancel,
	                Z_dir_readdir_iterate,
	                Z_dir_readdir_dir,
	                Z_dir_readdir_file,
	                0,
	                s->dirdb_ref,
	                1, /* refcount */
	                1, /* is_archive */
	                0  /* is_playlist */);

	s->parent->ref (s->parent);
	dirdbRef (s->dirdb_ref, dirdb_use_dir);

	ocpfile_t_fill (&retval->child.head,
	                 Z_ocpfile_ref,
	                 Z_ocpfile_unref,
	                &retval->head,
	                 Z_ocpfile_open,
	                 Z_ocpfile_filesize,
	                 Z_ocpfile_filesize_ready,
	                 0, /* filename_override */
	                 dirdb_ref,
	                 0, /* refcount */
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

		if (!adbMetaGet (filename, retval->child.compressedfile->filesize (s), "Z", &metadata, &metadatasize))
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

				DEBUG_PRINT ("[Z Z_check_steal]: got metadatasize=0x%08lx %02x %02x %02x %02x %02x %02x %02x %02x => %"PRIu64"\n", metadatasize, metadata[0], metadata[1], metadata[2], metadata[3], metadata[4], metadata[5], metadata[6], metadata[7], retval->child.uncompressed_filesize);

			}
			free (metadata);
			metadata = 0;
		}
	}

	return &retval->head;
}

static struct ocpdir_t *Z_check (const struct ocpdirdecompressor_t *ref, struct ocpfile_t *s, const char *filetype)
{
	struct ocpdir_t *retval;
	char *newname;
	int l;

	if (!strcasecmp (filetype, ".Z"))
	{
		dirdbGetName_malloc (s->dirdb_ref, &newname);
		l = strlen (newname);
		newname[l-2] = 0;
		retval = Z_check_steal (s, dirdbFindAndRef (s->dirdb_ref, newname, dirdb_use_file));
		free (newname);
		return retval;
	}

	return 0;
}

static struct ocpdirdecompressor_t zdirdecompressor =
{
	"z",
	"Unix compress fileformat",
	Z_check
};

void filesystem_Z_register (void)
{
	register_dirdecompressor (&zdirdecompressor);
}
