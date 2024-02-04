/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress BZIP2 files using libbz2
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
#include <bzlib.h>
#include "types.h"
#include "adbmeta.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-bzip2.h"
#include "stuff/framelock.h"

#ifndef INPUTBUFFERSIZE
# define INPUTBUFFERSIZE  65536
#endif

#ifndef OUTPUTBUFFERSIZE
# define OUTPUTBUFFERSIZE 65536
#endif

#if defined(BZIP2_DEBUG) || defined(BZIP2_VERBOSE)
static int do_bzip2_debug_print=1;
#endif

#ifdef BZIP2_DEBUG
#define DEBUG_PRINT(...) do { if (do_bzip2_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#ifdef BZIP2_VERBOSE
#define VERBOSE_PRINT(...) do { if (do_bzip2_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define VERBOSE_PRINT(...) {}
#endif

struct bzip2_ocpfilehandle_t
{
	struct ocpfilehandle_t head;

	struct ocpfilehandle_t *compressedfilehandle;

	bz_stream strm;
	int eofhit;
	uint8_t inputbuffer[INPUTBUFFERSIZE];
	uint8_t outputbuffer[OUTPUTBUFFERSIZE];

	uint8_t *outputbuffer_pos;
	int      outputbuffer_fill;

	struct bzip2_ocpfile_t *owner;

	uint64_t realpos;
	uint64_t pos;

	int need_deinit;
	int error;
};

struct bzip2_ocpfile_t /* head->parent always point to a bzip2_ocpdir_t */
{
	struct ocpfile_t      head;
	struct ocpfile_t     *compressedfile;

	int                   filesize_pending;
	uint64_t uncompressed_filesize;
};

struct bzip2_ocpdir_t
{
	struct ocpdir_t        head;
	struct bzip2_ocpfile_t child;
};

static int bzip2_ocpfilehandle_compressInit (struct bzip2_ocpfilehandle_t *s)
{
	int retval;

	if (s->need_deinit)
	{
		BZ2_bzDecompressEnd (&s->strm);
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

	s->strm.next_in = (char *)s->inputbuffer;
	retval = s->compressedfilehandle->read (s->compressedfilehandle, s->inputbuffer, INPUTBUFFERSIZE);
	if (retval <= 0)
	{
		s->error = 1;
		return -1;
	}
	s->strm.avail_in = retval;

	if (BZ2_bzDecompressInit (&s->strm, 0 /* no verbosity */, 0 /* do not use the small decompression routine */) != BZ_OK)
	{
		s->error = 1;
		return -1;
	}

	s->need_deinit = 1;

	return 0;
}

static void bzip2_ocpfilehandle_ref (struct ocpfilehandle_t *_s)
{
	struct bzip2_ocpfilehandle_t *s = (struct bzip2_ocpfilehandle_t *)_s;

	s->head.refcount++;
}

static void bzip2_ocpfilehandle_unref (struct ocpfilehandle_t *_s)
{
	struct bzip2_ocpfilehandle_t *s = (struct bzip2_ocpfilehandle_t *)_s;

	s->head.refcount--;
	if (s->head.refcount)
	{
		return;
	}

	if (s->need_deinit)
	{
		BZ2_bzDecompressEnd (&s->strm);
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

static int bzip2_ocpfilehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct bzip2_ocpfilehandle_t *s = (struct bzip2_ocpfilehandle_t *)_s;

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

static uint64_t bzip2_ocpfilehandle_getpos (struct ocpfilehandle_t *_s)
{
	struct bzip2_ocpfilehandle_t *s = (struct bzip2_ocpfilehandle_t *)_s;

	return s->pos;
}

static int bzip2_ocpfilehandle_eof (struct ocpfilehandle_t *_s)
{
	struct bzip2_ocpfilehandle_t *s = (struct bzip2_ocpfilehandle_t *)_s;

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

static int bzip2_ocpfilehandle_error (struct ocpfilehandle_t *_s)
{
	struct bzip2_ocpfilehandle_t *s = (struct bzip2_ocpfilehandle_t *)_s;

	return s->error;
}

static int bzip2_ocpfilehandle_read (struct ocpfilehandle_t *_s, void *dst, int len)
{
	struct bzip2_ocpfilehandle_t *s = (struct bzip2_ocpfilehandle_t *)_s;
	int retval = 0;
	int recall = 0;

	/* do we need to reverse? */
	if ((s->pos < s->realpos) || (!s->need_deinit))
	{
		if (bzip2_ocpfilehandle_compressInit (s))
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
			s->strm.next_in = (char *)s->inputbuffer;
			s->strm.avail_in = s->compressedfilehandle->read (s->compressedfilehandle, s->inputbuffer, INPUTBUFFERSIZE);
			if (s->compressedfilehandle->error (s->compressedfilehandle))
			{
				s->error = 1;
				return -1;
			}
		}

		s->strm.next_out = (char *)s->outputbuffer;
		s->strm.avail_out = OUTPUTBUFFERSIZE;
		s->outputbuffer_pos = s->outputbuffer;

		inputsize = s->strm.avail_in;
		ret = BZ2_bzDecompress (&s->strm);

		switch (ret)
		{
			default:
			case BZ_PARAM_ERROR:
			case BZ_DATA_ERROR:
			case BZ_DATA_ERROR_MAGIC:
			case BZ_MEM_ERROR:
				/* should not happen */
				s->error = 1;
				return -1;
			case BZ_STREAM_END:
				s->eofhit = 1;
			case BZ_OK:
				break;
		}
		s->outputbuffer_fill = OUTPUTBUFFERSIZE - s->strm.avail_out;
		if ((s->outputbuffer_fill == 0) && ((ret == BZ_STREAM_END) || (inputsize == 0)))
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
			s->strm.next_in = (char *)s->inputbuffer;
			s->strm.avail_in = s->compressedfilehandle->read (s->compressedfilehandle, s->inputbuffer, INPUTBUFFERSIZE);
			if (s->compressedfilehandle->error (s->compressedfilehandle))
			{
				s->error = 1;
				return -1;
			}
		}

		s->strm.next_out = (char *)s->outputbuffer;
		s->strm.avail_out = OUTPUTBUFFERSIZE;
		s->outputbuffer_pos = s->outputbuffer;

		inputsize = s->strm.avail_in;
		ret = BZ2_bzDecompress (&s->strm);

		switch (ret)
		{
			default:
			case BZ_PARAM_ERROR:
			case BZ_DATA_ERROR:
			case BZ_DATA_ERROR_MAGIC:
			case BZ_MEM_ERROR:
				/* should not happen */
				s->error = 1;
				return -1;
			case BZ_STREAM_END:
				s->eofhit = 1;
			case BZ_OK:
				break;
		}
		s->outputbuffer_fill = OUTPUTBUFFERSIZE - s->strm.avail_out;
#if 0
		if (ret == BZ_STREAM_END)
#else
		if ((s->outputbuffer_fill == 0) && ((ret == BZ_STREAM_END) || (inputsize == 0)))
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

				DEBUG_PRINT ("[BZIP2 filehandle_read EOF] adbMetaAdd(%s, %"PRId64", BZIP2, [%02x %02x %02x %02x %02x %02x %02x %02x] (%"PRIu64" + %d => %"PRIu64")\n", filename, compressedfile_size, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], s->realpos, s->outputbuffer_fill, filesize);
				adbMetaAdd (filename, compressedfile_size, "BZIP2", buffer, 8);
			}

			if (!s->outputbuffer_fill)
			{
				return retval;
			}
		}
	}

	return retval;
}

static uint64_t bzip2_ocpfilehandle_filesize (struct ocpfilehandle_t *_s)
{
	struct bzip2_ocpfilehandle_t *s = (struct bzip2_ocpfilehandle_t *)_s;
	return s->owner->head.filesize (&s->owner->head);
}

static int bzip2_ocpfilehandle_filesize_ready (struct ocpfilehandle_t *_s)
{
	struct bzip2_ocpfilehandle_t *s = (struct bzip2_ocpfilehandle_t *)_s;

	return !s->owner->filesize_pending;
}

static void bzip2_ocpfile_ref (struct ocpfile_t *s)
{
	s->parent->ref (s->parent);
}

static void bzip2_ocpfile_unref (struct ocpfile_t *s)
{
	s->parent->unref (s->parent);
}

static struct ocpfilehandle_t *bzip2_ocpfile_open (struct ocpfile_t *_s)
{
	struct bzip2_ocpfile_t *s = (struct bzip2_ocpfile_t *)_s;
	struct bzip2_ocpfilehandle_t *retval = calloc (1, sizeof (*retval));

	if (!retval)
	{
		return 0;
	}

	ocpfilehandle_t_fill (&retval->head,
	                       bzip2_ocpfilehandle_ref,
	                       bzip2_ocpfilehandle_unref,
	                      &s->head,
	                       bzip2_ocpfilehandle_seek_set,
	                       bzip2_ocpfilehandle_getpos,
	                       bzip2_ocpfilehandle_eof,
	                       bzip2_ocpfilehandle_error,
	                       bzip2_ocpfilehandle_read,
	                       0, /* ioctl */
	                       bzip2_ocpfilehandle_filesize,
	                       bzip2_ocpfilehandle_filesize_ready,
	                       0, /* filename_override */
	                       dirdbRef (s->head.dirdb_ref, dirdb_use_filehandle),
	                       1 /* refcount */);

	retval->owner = s;
	s->head.ref (&s->head);

	retval->compressedfilehandle = s->compressedfile->open (s->compressedfile);

	if (!retval->compressedfilehandle)
	{
		dirdbUnref (s->head.dirdb_ref, dirdb_use_filehandle);
		free (retval);
		return 0;
	}

	return &retval->head;
}

static int bzip2_ocpfile_filesize_ready (struct ocpfile_t *_s)
{
	struct bzip2_ocpfile_t *s = (struct bzip2_ocpfile_t *)_s;

	return !s->filesize_pending;
}

static uint64_t bzip2_ocpfile_filesize (struct ocpfile_t *_s)
{
	struct bzip2_ocpfile_t *s = (struct bzip2_ocpfile_t *)_s;
	struct ocpfilehandle_t *h = 0;
	uint64_t compressedfile_size = 0;
	bz_stream strm = {0};
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

		if (!adbMetaGet (filename, compressedfile_size, "BZIP2", &metadata, &metadatasize))
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

				DEBUG_PRINT ("[BZIP2 ocpfile_filesize]: got metadatasize=0x%08lx %02x %02x %02x %02x %02x %02x %02x %02x => %"PRIu64"\n", metadatasize, metadata[0], metadata[1], metadata[2], metadata[3], metadata[4], metadata[5], metadata[6], metadata[7], s->uncompressed_filesize);

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
	outputbuffer = malloc (OUTPUTBUFFERSIZE);
	if (!outputbuffer)
	{
		h->unref (h);
		h = 0;
		free (inputbuffer);
		return FILESIZE_ERROR;
	}

	strm.next_in = (char *)inputbuffer;
	strm.avail_in = h->read (h, inputbuffer, INPUTBUFFERSIZE);

	if (BZ2_bzDecompressInit (&strm, 0 /* no verbosity */, 0 /* do not use the small decompression routine */) != BZ_OK)
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
			strm.next_in = (char *)inputbuffer;
			strm.avail_in = h->read (h, inputbuffer, INPUTBUFFERSIZE);
			if (h->error (h))
			{
				BZ2_bzDecompressEnd (&strm);
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
			strm.next_out = (char *)outputbuffer;
			strm.avail_out = OUTPUTBUFFERSIZE;

			ret = BZ2_bzDecompress (&strm);

			switch (ret)
			{
				default:
				case BZ_PARAM_ERROR:
				case BZ_DATA_ERROR:
				case BZ_DATA_ERROR_MAGIC:
				case BZ_MEM_ERROR:
				/* should not happen */
					BZ2_bzDecompressEnd (&strm);
					free (inputbuffer);
					free (outputbuffer);
					h->unref (h);
					h = 0;
					return FILESIZE_ERROR;
				case BZ_STREAM_END:
				case BZ_OK:
					break;
			}
			filesize += OUTPUTBUFFERSIZE - strm.avail_out;
		 } while ((strm.avail_in != 0) && (ret != BZ_STREAM_END));
	} while (ret != BZ_STREAM_END);

	BZ2_bzDecompressEnd (&strm);
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

	DEBUG_PRINT ("[BZIP2 file_filesize] adbMetaAdd(%s, %"PRIu64", BZIP2, [%02x %02x %02x %02x %02x %02x %02x %02x]\n", filename, compressedfile_size, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
	adbMetaAdd (filename, compressedfile_size, "BZIP2", buffer, 8);

	return s->uncompressed_filesize;
}


static void bzip2_dir_ref (struct ocpdir_t *self)
{
	struct bzip2_ocpdir_t *s = (struct bzip2_ocpdir_t *)self;
	s->head.refcount++;
}

static void bzip2_dir_unref (struct ocpdir_t *self)
{
	struct bzip2_ocpdir_t *s = (struct bzip2_ocpdir_t *)self;
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

struct bzip2_dir_readdir_handle_t
{
	struct bzip2_ocpdir_t *self;
	void(*callback_file)(void *token, struct ocpfile_t *);
	void *token;
};

static ocpdirhandle_pt bzip2_dir_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct bzip2_dir_readdir_handle_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{ /* out of memory */
		return 0;
	}
	retval->self = (struct bzip2_ocpdir_t *)self;
	retval->callback_file = callback_file;
	retval->token = token;
	return retval;
}

static ocpdirhandle_pt bzip2_dir_readflatdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void *token)
{
	struct bzip2_dir_readdir_handle_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{ /* out of memory */
		return 0;
	}
	retval->self = (struct bzip2_ocpdir_t *)self;
	retval->callback_file = callback_file;
	retval->token = token;
	return retval;
}

static void bzip2_dir_readdir_cancel (ocpdirhandle_pt handle)
{
	free (handle);
}

static int bzip2_dir_readdir_iterate (ocpdirhandle_pt handle)
{
	struct bzip2_dir_readdir_handle_t *h = (struct bzip2_dir_readdir_handle_t *)handle;
	h->callback_file (h->token, &h->self->child.head);
	return 0;
}

static struct ocpdir_t *bzip2_dir_readdir_dir (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	return 0;
}

static struct ocpfile_t *bzip2_dir_readdir_file (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	struct bzip2_ocpdir_t *s = (struct bzip2_ocpdir_t *)self;
	if (s->child.head.dirdb_ref == dirdb_ref)
	{
		s->child.head.ref (&s->child.head);
		return &s->child.head;
	}
	return 0;
}

/* steals the dirdb_ref */
static struct ocpdir_t *bzip2_check_steal (struct ocpfile_t *s, const uint32_t dirdb_ref)
{
	struct bzip2_ocpdir_t *retval = calloc (1, sizeof (*retval));

	ocpdir_t_fill (&retval->head,
	                bzip2_dir_ref,
	                bzip2_dir_unref,
	                s->parent,
	                bzip2_dir_readdir_start,
	                bzip2_dir_readflatdir_start,
	                bzip2_dir_readdir_cancel,
	                bzip2_dir_readdir_iterate,
	                bzip2_dir_readdir_dir,
	                bzip2_dir_readdir_file,
	                0,
	                s->dirdb_ref,
	                1, /* refcount */
	                1, /* is_archive */
	                0, /* is_playlist */
	                s->compression);

	s->parent->ref (s->parent);
	dirdbRef (s->dirdb_ref, dirdb_use_dir);

	ocpfile_t_fill (&retval->child.head,
	                 bzip2_ocpfile_ref,
	                 bzip2_ocpfile_unref,
	                &retval->head,
	                 bzip2_ocpfile_open,
	                 bzip2_ocpfile_filesize,
	                 bzip2_ocpfile_filesize_ready,
	                 0, /* filename_override */
	                 dirdb_ref,
	                 0, /* refcount */
	                 0, /* is_nodetect */
	                 COMPRESSION_ADD_STREAM(s->compression));

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

		if (!adbMetaGet (filename, retval->child.compressedfile->filesize (s), "BZIP2", &metadata, &metadatasize))
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

				DEBUG_PRINT ("[BZIP2 bzip2_check_steal]: got metadatasize=0x%08lx %02x %02x %02x %02x %02x %02x %02x %02x => %"PRIu64"\n", metadatasize, metadata[0], metadata[1], metadata[2], metadata[3], metadata[4], metadata[5], metadata[6], metadata[7], retval->child.uncompressed_filesize);

			}
			free (metadata);
			metadata = 0;
		}
	}

	return &retval->head;
}

static struct ocpdir_t *bzip2_check (const struct ocpdirdecompressor_t *ref, struct ocpfile_t *s, const char *filetype)
{
	struct ocpdir_t *retval;
	char *newname;
	int l;

	if (!strcasecmp (filetype, ".bz"))
	{
		dirdbGetName_malloc (s->dirdb_ref, &newname);
		l = strlen (newname);
		newname[l-3] = 0;
		retval = bzip2_check_steal (s, dirdbFindAndRef (s->dirdb_ref, newname, dirdb_use_file));
		free (newname);
		return retval;
	}

	if (!strcasecmp (filetype, ".bz2"))
	{
		dirdbGetName_malloc (s->dirdb_ref, &newname);
		l = strlen (newname);
		newname[l-4] = 0;
		retval = bzip2_check_steal (s, dirdbFindAndRef (s->dirdb_ref, newname, dirdb_use_file));
		free (newname);
		return retval;
	}


	if (!strcasecmp (filetype, ".tbz"))
	{
		dirdbGetName_malloc (s->dirdb_ref, &newname);
		l = strlen (newname);
		strcpy (newname + l - 4, ".tar");
		retval = bzip2_check_steal (s, dirdbFindAndRef (s->dirdb_ref, newname, dirdb_use_file));
		free (newname);
		return retval;
	}

	if (!strcasecmp (filetype, ".tbz2"))
	{
		dirdbGetName_malloc (s->dirdb_ref, &newname);
		l = strlen (newname);
		strcpy (newname + l - 5, ".tar");
		retval = bzip2_check_steal (s, dirdbFindAndRef (s->dirdb_ref, newname, dirdb_use_file));
		free (newname);
		return retval;
	}

	return 0;
}

static struct ocpdirdecompressor_t bzip2dirdecompressor =
{
	"bzip2",
	"BZip2 and compress fileformats (using libbz2)",
	bzip2_check
};

void filesystem_bzip2_register (void)
{
	register_dirdecompressor (&bzip2dirdecompressor);
}
