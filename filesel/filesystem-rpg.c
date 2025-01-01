/* OpenCP Module Player
 * copyright (c) 2023-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decode RPG file archives (ohrrpgce)
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
#include <errno.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "adbmeta.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-rpg.h"
#include "stuff/cp437.h"

#if defined(RPG_DEBUG) || defined(RPG_VERBOSE)
static int do_rpg_debug_print=1;
#endif

#ifdef RPG_DEBUG
#define DEBUG_PRINT(...) do { if (do_rpg_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#ifdef RPG_VERBOSE
#define VERBOSE_PRINT(...) do { if (do_rpg_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define VERBOSE_PRINT(...) {}
#endif

#define MAX_FILENAME_SIZE 50

struct rpg_instance_t;

struct rpg_instance_dir_t
{
	struct ocpdir_t        head;
	struct rpg_instance_t *owner;
}; /* we only have root directory */


struct rpg_instance_file_t
{
	struct ocpfile_t           head;
	struct rpg_instance_t     *owner;
	uint64_t                   filesize;
	uint64_t                   fileoffset; /* offset into the rpg file */
	char                       orig_full_filepath[51];
};

struct rpg_instance_filehandle_t
{
	struct ocpfilehandle_t      head;
	struct rpg_instance_file_t *file;
	int                         error;
	uint64_t                    filepos;
};

struct rpg_instance_t
{
	struct rpg_instance_t       *next;
	int                          ready; /* a complete scan has been performed, use cache instead of file-read */

	struct rpg_instance_dir_t    dir0;

	struct rpg_instance_file_t **files;
        int                          file_fill;
	int                          file_size;

	struct ocpfile_t            *archive_file;
	struct ocpfilehandle_t      *archive_filehandle;

	int                          refcount;
	int                          iorefcount;
};

static struct rpg_instance_t *rpg_root;

static void rpg_io_ref (struct rpg_instance_t *self); /* count if we need I/O from the .RPG file */
static void rpg_io_unref (struct rpg_instance_t *self);

static void rpg_dir_ref (struct ocpdir_t *);
static void rpg_dir_unref (struct ocpdir_t *);

static ocpdirhandle_pt rpg_dir_readdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                 void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
static ocpdirhandle_pt rpg_dir_readflatdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *), void *token);
static void rpg_dir_readdir_cancel (ocpdirhandle_pt);
static int rpg_dir_readdir_iterate (ocpdirhandle_pt);
static struct ocpdir_t *rpg_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref);
static struct ocpfile_t *rpg_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref);

static void rpg_file_ref (struct ocpfile_t *);
static void rpg_file_unref (struct ocpfile_t *);
static struct ocpfilehandle_t *rpg_file_open (struct ocpfile_t *);
static uint64_t rpg_file_filesize (struct ocpfile_t *);
static int rpg_file_filesize_ready (struct ocpfile_t *);

static void rpg_filehandle_ref (struct ocpfilehandle_t *);
static void rpg_filehandle_unref (struct ocpfilehandle_t *);
static int rpg_filehandle_seek_set (struct ocpfilehandle_t *, int64_t pos);
static uint64_t rpg_filehandle_getpos (struct ocpfilehandle_t *);
static int rpg_filehandle_eof (struct ocpfilehandle_t *);
static int rpg_filehandle_error (struct ocpfilehandle_t *);
static int rpg_filehandle_read (struct ocpfilehandle_t *, void *dst, int len);
static uint64_t rpg_filehandle_filesize (struct ocpfilehandle_t *);
static int rpg_filehandle_filesize_ready (struct ocpfilehandle_t *);

static uint32_t rpg_instance_add_file (struct rpg_instance_t *self,
                                       char           *Filename,
                                       const uint64_t  filesize,
                                       const uint64_t  fileoffset);

/* in the blob, we will switch / into \0 temporary as we parse them */
static void rpg_instance_decode_blob (struct rpg_instance_t *self, uint8_t *blob, uint32_t blobsize)
{
	uint8_t *eos;

	/* block should start with a zero, reserve space for a string */
	if ((!blobsize) || (blob[0] != 0))
	{
		return;
	}
	blob++;
	blobsize--;

	while (blobsize >= 18)
	{
		uint64_t filesize =   ((uint64_t)(blob[ 7]) << 56) |
		                      ((uint64_t)(blob[ 6]) << 48) |
		                      ((uint64_t)(blob[ 5]) << 40) |
		                      ((uint64_t)(blob[ 4]) << 32) |
		                      ((uint64_t)(blob[ 3]) << 24) |
		                      ((uint64_t)(blob[ 2]) << 16) |
		                      ((uint64_t)(blob[ 1]) <<  8) |
		                      ((uint64_t)(blob[ 0])      );
		uint64_t fileoffset = ((uint64_t)(blob[15]) << 56) |
		                      ((uint64_t)(blob[14]) << 48) |
		                      ((uint64_t)(blob[13]) << 40) |
		                      ((uint64_t)(blob[12]) << 32) |
		                      ((uint64_t)(blob[11]) << 24) |
		                      ((uint64_t)(blob[10]) << 16) |
		                      ((uint64_t)(blob[ 9]) <<  8) |
		                      ((uint64_t)(blob[ 8])      );
		blob += 16;
		blobsize -= 16;
		eos = memchr (blob, 0, blobsize);
		if (!eos)
		{
			break;
		}

		rpg_instance_add_file (self, (char *)blob, filesize, fileoffset);

		eos++;
		blobsize -= eos - blob;
		blob = eos;
	}
}

static void rpg_instance_encode_blob (struct rpg_instance_t *self, uint8_t **blob, uint32_t *blobfill)
{
	uint32_t counter;
	uint32_t blobsize = 0;

	/* we start with a zero-byte, reserve space for string in future versions */
	*blobfill = 0;
	*blob = calloc (1, 1);
	if (!*blob)
	{
		return;
	}
	*blobfill = 1;

	for (counter=0; counter < self->file_fill; counter++)
	{
		int filenamesize = strlen (self->files[counter]->orig_full_filepath);

		if ((filenamesize + 1 + 16 + *blobfill) > blobsize)
		{
			uint32_t newsize = *blobfill + filenamesize + 1 + 16 + 1024;
			uint8_t *temp = realloc (*blob, newsize);
			if (!temp)
			{
				break;
			}
			*blob = temp;
			blobsize = newsize;
		}

		(*blob)[(*blobfill) +  7] = self->files[counter]->filesize >> 56;
		(*blob)[(*blobfill) +  6] = self->files[counter]->filesize >> 48;
		(*blob)[(*blobfill) +  5] = self->files[counter]->filesize >> 40;
		(*blob)[(*blobfill) +  4] = self->files[counter]->filesize >> 32;
		(*blob)[(*blobfill) +  3] = self->files[counter]->filesize >> 24;
		(*blob)[(*blobfill) +  2] = self->files[counter]->filesize >> 16;
		(*blob)[(*blobfill) +  1] = self->files[counter]->filesize >> 8;
		(*blob)[(*blobfill) +  0] = self->files[counter]->filesize;
		(*blob)[(*blobfill) + 15] = self->files[counter]->fileoffset >> 56;
		(*blob)[(*blobfill) + 14] = self->files[counter]->fileoffset >> 48;
		(*blob)[(*blobfill) + 13] = self->files[counter]->fileoffset >> 40;
		(*blob)[(*blobfill) + 12] = self->files[counter]->fileoffset >> 32;
		(*blob)[(*blobfill) + 11] = self->files[counter]->fileoffset >> 24;
		(*blob)[(*blobfill) + 10] = self->files[counter]->fileoffset >> 16;
		(*blob)[(*blobfill) +  9] = self->files[counter]->fileoffset >> 8;
		(*blob)[(*blobfill) +  8] = self->files[counter]->fileoffset;

		strcpy ((char *)(*blob) + 16 + (*blobfill), self->files[counter]->orig_full_filepath);
		*blobfill += 16 + filenamesize + 1;
	}
}


static void rpg_io_ref (struct rpg_instance_t *self)
{
	DEBUG_PRINT ( " rpg_io_ref (old count = %d)\n", self->iorefcount);
	if (!self->iorefcount)
	{
		self->archive_filehandle = self->archive_file->open (self->archive_file);
	}
	self->iorefcount++;
}

static void rpg_io_unref (struct rpg_instance_t *self)
{
	DEBUG_PRINT (" rpg_io_unref (old count = %d)\n", self->iorefcount);

	self->iorefcount--;
	if (self->iorefcount)
	{
		return;
	}

	if (self->archive_filehandle)
	{
		DEBUG_PRINT (" rpg_io_unref => RELEASE archive_filehandle\n");
		self->archive_filehandle->unref (self->archive_filehandle);
		self->archive_filehandle = 0;
	}
}

static uint32_t rpg_instance_add_file (struct rpg_instance_t *self,
                                       char                  *Filename,
                                       const uint64_t         filesize,
                                       const uint64_t         fileoffset)
{
	uint32_t dirdb_ref;

	DEBUG_PRINT ("[RPG] add_file: %s\n", Filename);
	if (strlen (Filename) > 50)
	{
		VERBOSE_PRINT ("[RPG]: filename too long\n");
		return UINT32_MAX;
	}

	if (self->file_fill == self->file_size)
	{
		int size = self->file_size + 64;
		struct rpg_instance_file_t **files = realloc (self->files, size * sizeof (self->files[0]));

		if (!files)
		{ /* out of memory */
			return UINT32_MAX;
		}

		self->files = files;
		self->file_size = size;
	}

	{
		char temp[50*3+1];

		cp437_f_to_utf8_z (Filename, strlen (Filename), temp, sizeof (temp));
		dirdb_ref = dirdbFindAndRef (self->dir0.head.dirdb_ref, temp, dirdb_use_file);
	}

	self->files[self->file_fill] = malloc (sizeof (*self->files[self->file_fill]));
	if (!self->files[self->file_fill])
	{ /* out of memory */
		dirdbUnref (dirdb_ref, dirdb_use_file);
		return UINT32_MAX;
	}

	ocpfile_t_fill (&self->files[self->file_fill]->head,
	                 rpg_file_ref,
	                 rpg_file_unref,
	                &self->dir0.head,
	                 rpg_file_open,
	                 rpg_file_filesize,
	                 rpg_file_filesize_ready,
	                 0, /* filename_override */
	                 dirdb_ref,
	                 0, /* refcount */
	                 0, /* is_nodetect */
	                 COMPRESSION_ADD_STORE (self->archive_file->compression));

	self->files[self->file_fill]->owner      = self;
	self->files[self->file_fill]->filesize   = filesize;
	self->files[self->file_fill]->fileoffset = fileoffset;
	strcpy (self->files[self->file_fill]->orig_full_filepath, Filename);

	self->file_fill++;

	return self->file_fill - 1;
}

struct ocpdir_t *rpg_check (const struct ocpdirdecompressor_t *self, struct ocpfile_t *file, const char *filetype)
{
	struct rpg_instance_t *iter;
	if (strcasecmp (filetype, ".rpg"))
	{
		return 0;
	}

	DEBUG_PRINT ("[RPG] filetype (%s) matches .rpg\n", filetype);

	/* Check the cache for an active instance */
	for (iter = rpg_root; iter; iter = iter->next)
	{
		if (iter->dir0.head.dirdb_ref == file->dirdb_ref)
		{
			DEBUG_PRINT ("[RPG] found a cached entry for the given dirdb_ref => refcount the ROOT entry\n");
			iter->dir0.head.ref (&iter->dir0.head);
			return &iter->dir0.head;
		}
	}

	iter = calloc (sizeof (*iter), 1);

	DEBUG_PRINT( "[RPG] creating a DIR using the same parent dirdb_ref\n");

	dirdbRef (file->dirdb_ref, dirdb_use_dir);
	ocpdir_t_fill (&iter->dir0.head,
	                rpg_dir_ref,
	                rpg_dir_unref,
	                file->parent,
	                rpg_dir_readdir_start,
	                rpg_dir_readflatdir_start,
	                rpg_dir_readdir_cancel,
	                rpg_dir_readdir_iterate,
	                rpg_dir_readdir_dir,
	                rpg_dir_readdir_file,
	                0, /* charset_API */
	                file->dirdb_ref,
	                0, /* refcount */
	                1, /* is_archive */
	                0, /* is_playlist */
	                file->compression);

	file->parent->ref (file->parent);
	iter->dir0.owner = iter;

	file->ref (file);
	iter->archive_file = file;

	iter->next = rpg_root;
	rpg_root = iter;

	if (iter->archive_file->filesize_ready (iter->archive_file))
	{
		const char *filename = 0;
		uint8_t *metadata = 0;
		uint32_t metadatasize = 0;

		dirdbGetName_internalstr (iter->archive_file->dirdb_ref, &filename);
		if (!adbMetaGet (filename, iter->archive_file->filesize (iter->archive_file), "RPG", &metadata, &metadatasize))
		{
			DEBUG_PRINT ("[RPG] We found adbmeta cache\n");
			rpg_instance_decode_blob (iter, metadata, metadatasize);
			free (metadata);
			iter->ready = 1;
		}
	}

	DEBUG_PRINT ("[RPG] finished creating the datastructures, refcount the ROOT entry\n");
	iter->dir0.head.ref (&iter->dir0.head);
	return &iter->dir0.head;
}

static void rpg_instance_ref (struct rpg_instance_t *self)
{
	DEBUG_PRINT (" RPG_INSTANCE_REF (old count = %d)\n", self->refcount);
	self->refcount++;
}

static void rpg_instance_unref (struct rpg_instance_t *self)
{
	uint32_t counter;
	struct rpg_instance_t **prev, *iter;

	DEBUG_PRINT (" RPG_INSTANCE_UNREF (old count = %d)\n", self->refcount);

	self->refcount--;
	if (self->refcount)
	{
		return;
	}

	DEBUG_PRINT (" KILL THE INSTANCE!!! (iocount = %d)\n", self->iorefcount);

	self->dir0.head.parent->unref (self->dir0.head.parent);
	self->dir0.head.parent = 0;

	dirdbUnref (self->dir0.head.dirdb_ref, dirdb_use_dir);

	for (counter = 0; counter < self->file_fill; counter++)
	{
		dirdbUnref (self->files[counter]->head.dirdb_ref, dirdb_use_file);
		free (self->files[counter]);
	}

	free (self->files);

	if (self->archive_file)
	{
		self->archive_file->unref (self->archive_file);
		self->archive_file = 0;
	}
	if (self->archive_filehandle)
	{
		self->archive_filehandle->unref (self->archive_filehandle);
		self->archive_filehandle = 0;
	}

	prev = &rpg_root;
	for (iter = rpg_root; iter; iter = iter->next)
	{
		if (iter == self)
		{
			*prev = self->next;
			break;
		}
		prev = &iter->next;
	}

	free (self);
}

static struct ocpdirdecompressor_t rpgdecompressor =
{
	"rpg",
	"ohrrpgce/RPG archive format",
	rpg_check
};

void filesystem_rpg_register (void)
{
	register_dirdecompressor (&rpgdecompressor);
}

static void rpg_dir_ref (struct ocpdir_t *_self)
{
	struct rpg_instance_dir_t *self = (struct rpg_instance_dir_t *)_self;
	DEBUG_PRINT ("RPG_DIR_REF (old count = %d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		rpg_instance_ref (self->owner);
	}
	self->head.refcount++;
}

static void rpg_dir_unref (struct ocpdir_t *_self)
{
	struct rpg_instance_dir_t *self = (struct rpg_instance_dir_t *)_self;
	DEBUG_PRINT ("RPG_DIR_UNREF (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		rpg_instance_unref (self->owner);
	}
}

struct rpg_instance_ocpdirhandle_t
{
	struct rpg_instance_dir_t *dir;

	void(*callback_file)(void *token, struct ocpfile_t *);
	void *token;

	int flatdir;
	int fastmode; /* if this is false, we did io-ref */

	/* fast-mode */
	uint32_t nextfile;
	/* scanmode */
	uint64_t nextheader_offset;
};

static ocpdirhandle_pt rpg_dir_readdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                      void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct rpg_instance_dir_t *self = (struct rpg_instance_dir_t *)_self;
	struct rpg_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	DEBUG_PRINT ("rpg_dir_readdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->token = token;

	retval->flatdir = 0;
	retval->fastmode = self->owner->ready;
	if (!self->owner->ready)
	{
		rpg_io_ref (self->owner);
	}

	retval->nextfile = 0;

	retval->nextheader_offset = 0;

	DEBUG_PRINT ("\n");

	return retval;
}

static ocpdirhandle_pt rpg_dir_readflatdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *), void *token)
{
	struct rpg_instance_dir_t *self = (struct rpg_instance_dir_t *)_self;
	struct rpg_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	DEBUG_PRINT ("rpg_dir_readflatdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->token = token;

	retval->flatdir = 1;
	retval->fastmode = self->owner->ready;
	if (!self->owner->ready)
	{
		rpg_io_ref (self->owner);
	}

	retval->nextfile = 0;
	retval->nextheader_offset = 0;

	DEBUG_PRINT ("\n");

	return retval;
}

static void rpg_dir_readdir_cancel (ocpdirhandle_pt _self)
{
	struct rpg_instance_ocpdirhandle_t *self = (struct rpg_instance_ocpdirhandle_t *)_self;

	DEBUG_PRINT ("rpg_dir_readdir_cancel, we need to UNREF\n");

	self->dir->head.unref (&self->dir->head);

	if (!self->fastmode)
	{
		rpg_io_unref (self->dir->owner);
	}

	free (self);

	DEBUG_PRINT ("\n");
}

static int rpg_dir_readdir_iterate (ocpdirhandle_pt _self)
{
	struct rpg_instance_ocpdirhandle_t *self = (struct rpg_instance_ocpdirhandle_t *)_self;

	if (self->fastmode)
	{
		if (self->nextfile >= self->dir->owner->file_fill)
		{
			return 0;
		}
		self->callback_file (self->token, &self->dir->owner->files[self->nextfile++]->head);
		return 1;
	} else {
		uint8_t *eos;
		uint8_t  header[51+4];
		uint32_t filesize;
		uint32_t fileid;

		if (!self->dir->owner->archive_filehandle)
		{
			return 0; /* failed to open during init? */
		}

		memset (header, 0, sizeof (header));

		self->dir->owner->archive_filehandle->seek_set (self->dir->owner->archive_filehandle, self->nextheader_offset);
		if (self->dir->owner->archive_filehandle->read (self->dir->owner->archive_filehandle, header, sizeof (header)) < 6)
		{
			const char *filename;
			uint8_t *metadata = 0;
			uint32_t metadatasize = 0;
finished:
			self->dir->owner->ready = 1;
			rpg_instance_encode_blob (self->dir->owner, &metadata, &metadatasize);
			if (metadata)
			{
				dirdbGetName_internalstr (self->dir->owner->archive_filehandle->dirdb_ref, &filename);
				adbMetaAdd (filename, self->dir->owner->archive_filehandle->filesize (self->dir->owner->archive_filehandle), "RPG", metadata, metadatasize);
				free (metadata);
			}

			return 0;
		}
		eos = memchr (header, 0, 51);
		if (!eos)
		{
			goto finished;
		}
		filesize = (eos[2] << 24) |
		           (eos[1] << 16) |
		           (eos[4] <<  8) |
		            eos[3];

		fileid = rpg_instance_add_file (self->dir->owner, (char *)header, filesize, self->nextheader_offset + (eos - header) + 1 + 4);
		if (fileid != UINT32_MAX)
		{
			self->callback_file (self->token, &self->dir->owner->files[fileid]->head);
		}

		self->nextheader_offset += filesize + (eos - header) + 1 + 4;

		return 1;
	}
}

static void rpg_dir_readdir_forcescan_file (void *token, struct ocpfile_t *file)
{
}

static void rpg_dir_readdir_forcescan_dir (void *token, struct ocpdir_t *dir)
{
}

static void rpg_force_ready (struct rpg_instance_dir_t *self)
{
	ocpdirhandle_pt handle;

	if (self->owner->ready)
	{
		return;
	}

	handle = self->head.readdir_start (&self->head, rpg_dir_readdir_forcescan_file, rpg_dir_readdir_forcescan_dir, 0);
	if (!handle)
	{
		fprintf (stderr, "rpg_force_ready: out of memory?\n");
		return;
	}
	while (self->head.readdir_iterate (handle))
	{
	}
	self->head.readdir_cancel (handle);
}

static struct ocpdir_t *rpg_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	return 0;
}

static struct ocpfile_t *rpg_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct rpg_instance_dir_t *self = (struct rpg_instance_dir_t *)_self;
	int i;

	rpg_force_ready (self);

	for (i=0; i < self->owner->file_fill; i++)
	{
		if (self->owner->files[i]->head.dirdb_ref == dirdb_ref)
		{
			self->owner->files[i]->head.ref (&self->owner->files[i]->head);
			return &self->owner->files[i]->head;
		}
	}

	return 0;
}

static int rpg_filehandle_read (struct ocpfilehandle_t *_self, void *dst, int len)
{
	struct rpg_instance_filehandle_t *self = (struct rpg_instance_filehandle_t *)_self;
	int retval;
	struct ocpfilehandle_t *filehandle;

	if (self->error)
	{
		return 0;
	}

	filehandle = self->file->owner->archive_filehandle; /* cache */
	if (!filehandle)
	{
		self->error = 1;
		return 0;
	}
	if (filehandle->seek_set (filehandle, self->file->fileoffset + self->filepos) < 0)
	{
		self->error = 1;
		return 0;
	}
	retval = filehandle->read (filehandle, dst, len);
	self->filepos += retval;
	self->error = filehandle->error (filehandle);

	return retval;
}

static void rpg_file_ref (struct ocpfile_t *_self)
{
	struct rpg_instance_file_t *self = (struct rpg_instance_file_t *)_self;
	DEBUG_PRINT ("rpg_file_ref (old value=%d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		rpg_instance_ref (self->owner);
	}
	self->head.refcount++;
	DEBUG_PRINT ("\n");
}

static void rpg_file_unref (struct ocpfile_t *_self)
{
	struct rpg_instance_file_t *self = (struct rpg_instance_file_t *)_self;
	DEBUG_PRINT ("rpg_file_unref (old value=%d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		rpg_instance_unref (self->owner);
	}
	DEBUG_PRINT ("\n");
}

static struct ocpfilehandle_t *rpg_file_open (struct ocpfile_t *_self)
{
	struct rpg_instance_file_t *self = (struct rpg_instance_file_t *)_self;
	struct rpg_instance_filehandle_t *retval;

	retval = calloc (sizeof (*retval), 1);
	ocpfilehandle_t_fill (&retval->head,
	                       rpg_filehandle_ref,
	                       rpg_filehandle_unref,
	                       _self,
	                       rpg_filehandle_seek_set,
	                       rpg_filehandle_getpos,
	                       rpg_filehandle_eof,
	                       rpg_filehandle_error,
	                       rpg_filehandle_read,
	                       0, /* ioctl */
	                       rpg_filehandle_filesize,
	                       rpg_filehandle_filesize_ready,
	                       0, /* filename_override */
	                       dirdbRef (self->head.dirdb_ref, dirdb_use_filehandle),
	                       1
	);

	retval->file = self;

	DEBUG_PRINT ("We just created a RPG handle, REF the source\n");
	rpg_instance_ref (self->owner);
	rpg_io_ref (self->owner);

	return &retval->head;
}

static uint64_t rpg_file_filesize (struct ocpfile_t *_self)
{
	struct rpg_instance_file_t *self = (struct rpg_instance_file_t *)_self;
	return self->filesize;
}

static int rpg_file_filesize_ready (struct ocpfile_t *_self)
{
	return 1;
}

static void rpg_filehandle_ref (struct ocpfilehandle_t *_self)
{
	struct rpg_instance_filehandle_t *self = (struct rpg_instance_filehandle_t *)_self;

	DEBUG_PRINT ("rpg_filehandle_ref (old count = %d)\n", self->head.refcount);
	self->head.refcount++;
}

static void rpg_filehandle_unref (struct ocpfilehandle_t *_self)
{
	struct rpg_instance_filehandle_t *self = (struct rpg_instance_filehandle_t *)_self;

	DEBUG_PRINT ("rpg_filehandle_unref (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		dirdbUnref (self->head.dirdb_ref, dirdb_use_filehandle);
		rpg_io_unref (self->file->owner);
		rpg_instance_unref (self->file->owner);
		free (self);
	}
	DEBUG_PRINT ("\n");
}

static int rpg_filehandle_seek_set (struct ocpfilehandle_t *_self, int64_t pos)
{
	struct rpg_instance_filehandle_t *self = (struct rpg_instance_filehandle_t *)_self;

	if (pos < 0) return -1;

	if (pos > self->file->filesize) return -1;

	self->filepos = pos;
	self->error = 0;

	return 0;
}

static uint64_t rpg_filehandle_getpos (struct ocpfilehandle_t *_self)
{
	struct rpg_instance_filehandle_t *self = (struct rpg_instance_filehandle_t *)_self;
	return self->filepos;
}

static int rpg_filehandle_eof (struct ocpfilehandle_t *_self)
{
	struct rpg_instance_filehandle_t *self = (struct rpg_instance_filehandle_t *)_self;
	return self->filepos >= self->file->filesize;
}

static int rpg_filehandle_error (struct ocpfilehandle_t *_self)
{
	struct rpg_instance_filehandle_t *self = (struct rpg_instance_filehandle_t *)_self;
	return self->error;
}

static uint64_t rpg_filehandle_filesize (struct ocpfilehandle_t *_self)
{
	struct rpg_instance_filehandle_t *self = (struct rpg_instance_filehandle_t *)_self;
	return self->file->filesize;
}

static int rpg_filehandle_filesize_ready (struct ocpfilehandle_t *_self)
{
	return 1;
}
