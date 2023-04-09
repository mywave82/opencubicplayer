/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decode TAR file archives
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
#include "filesystem-tar.h"

#if defined(TAR_DEBUG) || defined(TAR_VERBOSE)
static int do_tar_debug_print=1;
#endif

#ifdef TAR_DEBUG
#define DEBUG_PRINT(...) do { if (do_tar_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#ifdef TAR_VERBOSE
#define VERBOSE_PRINT(...) do { if (do_tar_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define VERBOSE_PRINT(...) {}
#endif

#define MAX_FILENAME_SIZE 4096

struct tar_instance_t;

struct tar_instance_dir_t
{
	struct ocpdir_t        head;
	struct tar_instance_t *owner;
	uint32_t               dir_parent; /* used for making blob */
	uint32_t               dir_next;
	uint32_t               dir_child;
	uint32_t               file_child;

	char                  *orig_full_dirpath; /* if encoding deviates from UTF-8 */
};

struct tar_instance_file_t
{
	struct ocpfile_t           head;
	struct tar_instance_t     *owner;
	uint32_t                   dir_parent; /* used for making blob */
	uint32_t                   file_next;
	uint64_t                   filesize;
	uint64_t                   fileoffset; /* offset into the tar file */

	char                      *orig_full_filepath; /* if encoding deviates from UTF-8 */
};

struct tar_instance_filehandle_t
{
	struct ocpfilehandle_t      head;
	struct tar_instance_file_t *file;
	int                         error;

	uint64_t                    filepos;
};

struct tar_instance_t
{
	struct tar_instance_t       *next;
	int                          ready; /* a complete scan has been performed, use cache instead of file-read */

	struct tar_instance_dir_t  **dirs;
	struct tar_instance_dir_t    dir0;
	int                          dir_fill;
	int                          dir_size;
	struct tar_instance_file_t **files;
        int                          file_fill;
	int                          file_size;

	struct ocpfile_t            *archive_file;
	struct ocpfilehandle_t      *archive_filehandle;

	iconv_t                     *iconv_handle;
	char                        *charset_override;  /* either NULL, or an override string */

	int                          refcount;
	int                          iorefcount;
};

static struct tar_instance_t *tar_root;

static void tar_io_ref (struct tar_instance_t *self); /* count if we need I/O from the .TAR file */
static void tar_io_unref (struct tar_instance_t *self);

static void tar_dir_ref (struct ocpdir_t *);
static void tar_dir_unref (struct ocpdir_t *);

static ocpdirhandle_pt tar_dir_readdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                 void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
static ocpdirhandle_pt tar_dir_readflatdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *), void *token);
static void tar_dir_readdir_cancel (ocpdirhandle_pt);
static int tar_dir_readdir_iterate (ocpdirhandle_pt);
static struct ocpdir_t *tar_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref);
static struct ocpfile_t *tar_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref);

static void tar_file_ref (struct ocpfile_t *);
static void tar_file_unref (struct ocpfile_t *);
static struct ocpfilehandle_t *tar_file_open (struct ocpfile_t *);
static uint64_t tar_file_filesize (struct ocpfile_t *);
static int tar_file_filesize_ready (struct ocpfile_t *);

static void tar_filehandle_ref (struct ocpfilehandle_t *);
static void tar_filehandle_unref (struct ocpfilehandle_t *);
static int tar_filehandle_seek_set (struct ocpfilehandle_t *, int64_t pos);
static int tar_filehandle_seek_cur (struct ocpfilehandle_t *, int64_t pos);
static int tar_filehandle_seek_end (struct ocpfilehandle_t *, int64_t pos);
static uint64_t tar_filehandle_getpos (struct ocpfilehandle_t *);
static int tar_filehandle_eof (struct ocpfilehandle_t *);
static int tar_filehandle_error (struct ocpfilehandle_t *);
static int tar_filehandle_read (struct ocpfilehandle_t *, void *dst, int len);
static uint64_t tar_filehandle_filesize (struct ocpfilehandle_t *);
static int tar_filehandle_filesize_ready (struct ocpfilehandle_t *);

static void tar_get_default_string (struct ocpdir_t *self, const char **label, const char **key);
static const char *tar_get_byuser_string (struct ocpdir_t *self);
static void tar_set_byuser_string (struct ocpdir_t *self, const char *byuser);
static char **tar_get_test_strings(struct ocpdir_t *self);

static void tar_translate_prepare (struct tar_instance_t *self);
static void tar_translate (struct tar_instance_t *self, char *src, char **buffer, int *buffersize);
static void tar_translate_complete (struct tar_instance_t *self);

static const struct ocpdir_charset_override_API_t tar_charset_API =
{
	tar_get_default_string,
	tar_get_byuser_string,
	tar_set_byuser_string,
	tar_get_test_strings
};

static uint32_t tar_instance_add (struct tar_instance_t *self,
                                  char           *Filename,
                                  const uint64_t  filesize,
                                  const uint64_t  fileoffset);

/* in the blob, we will switch / into \0 temporary as we parse them */
static void tar_instance_decode_blob (struct tar_instance_t *self, uint8_t *blob, size_t blobsize)
{

	uint8_t *eos;

	eos = memchr (blob, 0, blobsize);
	if (!eos)
	{
		return;
	}
	if (eos != blob)
	{
		self->charset_override = strdup ((char *)blob);
	} else {
		self->charset_override = NULL;
	}

	eos++;
	blobsize -= eos - blob;
	blob = eos;

	tar_translate_prepare (self);

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

		tar_instance_add (self, (char *)blob, filesize, fileoffset);

		eos++;
		blobsize -= eos - blob;
		blob = eos;
	}

	tar_translate_complete (self);
}

static void tar_instance_encode_blob (struct tar_instance_t *self, uint8_t **blob, size_t *blobfill)
{
	uint32_t counter;
	uint32_t blobsize = 0;

	*blobfill = 0;
	*blob = 0;

	{
		uint32_t newsize = *blobfill + (self->charset_override?strlen(self->charset_override):0) + 1 + 1024;
		uint8_t *temp = realloc (*blob, newsize);
		if (!temp)
		{
			return;
		}
		*blob = temp;
		blobsize = newsize;

		if (self->charset_override)
		{
			strcpy ((char *)*blob + *blobfill, self->charset_override);
			*blobfill += strlen(self->charset_override) + 1;
		} else {
			(*blob)[*blobfill] = 0;
			*blobfill += 1;
		}
	}

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


static void tar_io_ref (struct tar_instance_t *self)
{
	DEBUG_PRINT ( " tar_io_ref (old count = %d)\n", self->iorefcount);
	if (!self->iorefcount)
	{
		self->archive_filehandle = self->archive_file->open (self->archive_file);
	}
	self->iorefcount++;
}

static void tar_io_unref (struct tar_instance_t *self)
{
	DEBUG_PRINT (" tar_io_unref (old count = %d)\n", self->iorefcount);

	self->iorefcount--;
	if (self->iorefcount)
	{
		return;
	}

	if (self->archive_filehandle)
	{
		DEBUG_PRINT (" tar_io_unref => RELEASE archive_filehandle\n");
		self->archive_filehandle->unref (self->archive_filehandle);
		self->archive_filehandle = 0;
	}
}

static uint32_t tar_instance_add_create_dir (struct tar_instance_t *self,
                                             const uint32_t         dir_parent,
                                             char                  *Dirpath,
                                             char                  *Dirname)
{
	uint32_t *prev, iter;
	uint32_t dirdb_ref;

	DEBUG_PRINT ("[TAR] create_dir: \"%s\" \"%s\"\n", Dirpath, Dirname);

	{
		char *temp = 0;
		int templen = 0;

		tar_translate (self, Dirname, &temp, &templen);
		dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, temp ? temp : "???", dirdb_use_dir);
		free (temp); temp = 0;
	}

	if (self->dir_fill == self->dir_size)
	{
		int size = self->dir_size + 16;
		struct tar_instance_dir_t **dirs = realloc (self->dirs, size * sizeof (self->dirs[0]));

		if (!dirs)
		{ /* out of memory */
			dirdbUnref (dirdb_ref, dirdb_use_dir);
			return 0;
		}

		self->dirs = dirs;
		self->dir_size = size;
	}

	self->dirs[self->dir_fill] = malloc (sizeof(self->dirs[self->dir_fill][0]));
	if (!self->dirs[self->dir_fill])
	{ /* out of memory */
		dirdbUnref (dirdb_ref, dirdb_use_dir);
		return 0;
	}

	ocpdir_t_fill (&self->dirs[self->dir_fill]->head,
	                tar_dir_ref,
	                tar_dir_unref,
	               &self->dirs[dir_parent]->head,
	                tar_dir_readdir_start,
	                tar_dir_readflatdir_start,
	                tar_dir_readdir_cancel,
	                tar_dir_readdir_iterate,
	                tar_dir_readdir_dir,
	                tar_dir_readdir_file,
	                0,
	                dirdb_ref,
	                0, /* refcount */
	                1, /* is_archive */
	                0  /* is_playlist */);

	self->dirs[self->dir_fill]->owner = self;
	self->dirs[self->dir_fill]->dir_parent = dir_parent;
	self->dirs[self->dir_fill]->dir_next = UINT32_MAX;
	self->dirs[self->dir_fill]->dir_child = UINT32_MAX;
	self->dirs[self->dir_fill]->file_child = UINT32_MAX;
	self->dirs[self->dir_fill]->orig_full_dirpath = strdup (Dirpath);

	prev = &self->dirs[dir_parent]->dir_child;
	for (iter = *prev; iter != UINT32_MAX; iter = self->dirs[iter]->dir_next)
	{
		prev = &self->dirs[iter]->dir_next;
	};
	*prev = self->dir_fill;

	self->dir_fill++;

	return *prev;
}

static uint32_t tar_instance_add_file (struct tar_instance_t *self,
                                       const uint32_t         dir_parent,
                                       char                  *Filepath,
                                       char                  *Filename,
                                       const uint64_t         filesize,
                                       const uint64_t         fileoffset)
{
	uint32_t *prev, iter;
	uint32_t dirdb_ref;

	DEBUG_PRINT ("[TAR] add_file: %s %s\n", Filepath, Filename);

	if (self->file_fill == self->file_size)
	{
		int size = self->file_size + 64;
		struct tar_instance_file_t **files = realloc (self->files, size * sizeof (self->files[0]));

		if (!files)
		{ /* out of memory */
			return UINT32_MAX;
		}

		self->files = files;
		self->file_size = size;
	}

	{
		char *temp = 0;
		int templen = 0;

		tar_translate (self, Filename, &temp, &templen);
		dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, temp ? temp : "???", dirdb_use_file);
		free (temp); temp = 0;
	}

	self->files[self->file_fill] = malloc (sizeof (*self->files[self->file_fill]));
	if (!self->files[self->file_fill])
	{ /* out of memory */
		dirdbUnref (dirdb_ref, dirdb_use_file);
		return UINT32_MAX;
	}

	ocpfile_t_fill (&self->files[self->file_fill]->head,
	                 tar_file_ref,
	                 tar_file_unref,
	                &self->dirs[dir_parent]->head,
	                 tar_file_open,
	                 tar_file_filesize,
	                 tar_file_filesize_ready,
	                 0, /* filename_override */
	                 dirdb_ref,
	                 0, /* refcount */
	                 0  /* is_nodetect */);

	self->files[self->file_fill]->owner      = self;
	self->files[self->file_fill]->dir_parent = dir_parent;
	self->files[self->file_fill]->file_next  = UINT32_MAX;
	self->files[self->file_fill]->filesize   = filesize;
	self->files[self->file_fill]->fileoffset = fileoffset;
	self->files[self->file_fill]->orig_full_filepath = strdup (Filepath);

	prev = &self->dirs[dir_parent]->file_child;
	for (iter = *prev; iter != UINT32_MAX; iter = self->files[iter]->file_next)
	{
		prev = &self->files[iter]->file_next;
	};
	*prev = self->file_fill;

	self->file_fill++;

	return *prev;
}

static uint32_t tar_instance_add (struct tar_instance_t *self,
                                  char                  *Filepath,
                                  const uint64_t         filesize,
                                  const uint64_t         fileoffset)
{
	uint32_t iter = 0;
	char *ptr = Filepath;
	while (1)
	{
		char *slash;
		uint32_t search;
again:
		if (*ptr == '/')
		{
			ptr++;
			continue;
		}
		if (*ptr == 0)
		{
			return UINT32_MAX; /* not a file... */
		}
		slash = strchr (ptr, '/');
		if (slash)
		{
			*slash = 0;
			if (strcmp (ptr, ".") && strcmp (ptr, "..") && strlen (ptr)) /* we ignore these entries */
			{
				/* check if we already have this node */
				for (search = 1; search < self->dir_fill; search++)
				{
					if ( (self->dirs[search]->dir_parent == iter) &&
					     (!strcmp (self->dirs[search]->orig_full_dirpath, Filepath)) )
					{
						*slash = '/';
						ptr = slash + 1;
						iter = search;
						goto again; /* we need a break + continue; */
					}
				}

				/* no hit, create one */
				iter = tar_instance_add_create_dir (self, iter, Filepath, ptr);
				*slash = '/';
				ptr = slash + 1;
				if (iter == 0)
				{
					return UINT32_MAX; /* out of memory.... */
				}
			} else {
				*slash = '/';
				ptr = slash + 1;
			}
		} else {
			if (strcmp (ptr, ".") && strcmp (ptr, "..") && strlen (ptr)) /* we ignore these entries */
			{
				return tar_instance_add_file (self, iter, Filepath, ptr, filesize, fileoffset);
			}
			return UINT32_MAX; /* out of memory.... */
		}
	}
}

struct ocpdir_t *tar_check (const struct ocpdirdecompressor_t *self, struct ocpfile_t *file, const char *filetype)
{
	struct tar_instance_t *iter;
	if (strcasecmp (filetype, ".tar"))
	{
		return 0;
	}

	DEBUG_PRINT ("[TAR] filetype (%s) matches .tar\n", filetype);

	/* Check the cache for an active instance */
	for (iter = tar_root; iter; iter = iter->next)
	{
		if (iter->dirs[0]->head.dirdb_ref == file->dirdb_ref)
		{
			DEBUG_PRINT ("[TAR] found a cached entry for the given dirdb_ref => refcount the ROOT entry\n");
			iter->dirs[0]->head.ref (&iter->dirs[0]->head);
			return &iter->dirs[0]->head;
		}
	}

	iter = calloc (sizeof (*iter), 1);
	iter->dir_size = 16;
	iter->dirs = malloc (iter->dir_size * sizeof (iter->dirs[0]));

	DEBUG_PRINT( "[TAR] creating a DIR using the same parent dirdb_ref\n");

	dirdbRef (file->dirdb_ref, dirdb_use_dir);
	iter->dirs[0] = &iter->dir0;
	ocpdir_t_fill (&iter->dirs[0]->head,
	                tar_dir_ref,
	                tar_dir_unref,
	                file->parent,
	                tar_dir_readdir_start,
	                tar_dir_readflatdir_start,
	                tar_dir_readdir_cancel,
	                tar_dir_readdir_iterate,
	                tar_dir_readdir_dir,
	                tar_dir_readdir_file,
	                &tar_charset_API,
	                file->dirdb_ref,
	                0, /* refcount */
	                1, /* is_archive */
	                0  /* is_playlist */);

	file->parent->ref (file->parent);
	iter->dirs[0]->owner = iter;
	iter->dirs[0]->dir_parent = UINT32_MAX;
	iter->dirs[0]->dir_next = UINT32_MAX;
	iter->dirs[0]->dir_child = UINT32_MAX;
	iter->dirs[0]->file_child = UINT32_MAX;
	iter->dirs[0]->orig_full_dirpath = 0;
	iter->dir_fill = 1;

	file->ref (file);
	iter->archive_file = file;

	iter->next = tar_root;
	iter->iconv_handle = (iconv_t *)-1;
	tar_root = iter;

	if (iter->archive_file->filesize_ready (iter->archive_file))
	{
		const char *filename = 0;
		uint8_t *metadata = 0;
		size_t metadatasize = 0;

		dirdbGetName_internalstr (iter->archive_file->dirdb_ref, &filename);
		if (!adbMetaGet (filename, iter->archive_file->filesize (iter->archive_file), "TAR", &metadata, &metadatasize))
		{
			DEBUG_PRINT ("[TAR] We found adbmeta cache\n");
			tar_instance_decode_blob (iter, metadata, metadatasize);
			free (metadata);
			iter->ready = 1;
		}
	}

	if (!iter->ready)
	{
		tar_translate_prepare (iter);
		//tar_scan (iter);
		//tar_translate_complete (iter);
	}


	DEBUG_PRINT ("[TAR] finished creating the datastructures, refcount the ROOT entry\n");
	iter->dirs[0]->head.ref (&iter->dirs[0]->head);
	return &iter->dirs[0]->head;
}

static void tar_instance_ref (struct tar_instance_t *self)
{
	DEBUG_PRINT (" TAR_INSTANCE_REF (old count = %d)\n", self->refcount);
	self->refcount++;
}

static void tar_instance_unref (struct tar_instance_t *self)
{
	uint32_t counter;
	struct tar_instance_t **prev, *iter;

	DEBUG_PRINT (" TAR_INSTANCE_UNREF (old count = %d)\n", self->refcount);

	self->refcount--;
	if (self->refcount)
	{
		return;
	}

	DEBUG_PRINT (" KILL THE INSTANCE!!! (iocount = %d)\n", self->iorefcount);

	tar_translate_complete (self);

	/* dirs[0] is special */
	self->dirs[0]->head.parent->unref (self->dirs[0]->head.parent);
	self->dirs[0]->head.parent = 0;

	dirdbUnref (self->dirs[0]->head.dirdb_ref, dirdb_use_dir);
	for (counter = 1; counter < self->dir_fill; counter++)
	{
		dirdbUnref (self->dirs[counter]->head.dirdb_ref, dirdb_use_dir);
		free (self->dirs[counter]->orig_full_dirpath);
		free (self->dirs[counter]);
	}

	for (counter = 0; counter < self->file_fill; counter++)
	{
		dirdbUnref (self->files[counter]->head.dirdb_ref, dirdb_use_file);
		free (self->files[counter]->orig_full_filepath);
		free (self->files[counter]);
	}

	free (self->dirs);
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

	prev = &tar_root;
	for (iter = tar_root; iter; iter = iter->next)
	{
		if (iter == self)
		{
			*prev = self->next;
			break;
		}
		prev = &iter->next;
	}

	free (self->charset_override);
	free (self);
}

static struct ocpdirdecompressor_t tardecompressor =
{
	"tar",
	"GNU / star tar archive fileformats",
	tar_check
};

void filesystem_tar_register (void)
{
	register_dirdecompressor (&tardecompressor);
}

static void tar_dir_ref (struct ocpdir_t *_self)
{
	struct tar_instance_dir_t *self = (struct tar_instance_dir_t *)_self;
	DEBUG_PRINT ("TAR_DIR_REF (old count = %d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		tar_instance_ref (self->owner);
	}
	self->head.refcount++;
}

static void tar_dir_unref (struct ocpdir_t *_self)
{
	struct tar_instance_dir_t *self = (struct tar_instance_dir_t *)_self;
	DEBUG_PRINT ("TAR_DIR_UNREF (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		tar_instance_unref (self->owner);
	}
}

struct tar_instance_ocpdirhandle_t
{
	struct tar_instance_dir_t *dir;

	void(*callback_file)(void *token, struct ocpfile_t *);
        void(*callback_dir )(void *token, struct ocpdir_t *);
	void *token;

	int flatdir;
	int fastmode; /* if this is false, we did io-ref */

	/* fast-mode */
	uint32_t nextdir;
	uint32_t nextfile;
	/* scanmode */
	uint64_t nextheader_offset;
	char *LongLink;
};


static ocpdirhandle_pt tar_dir_readdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                      void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct tar_instance_dir_t *self = (struct tar_instance_dir_t *)_self;
	struct tar_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	DEBUG_PRINT ("tar_dir_readdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->callback_dir = callback_dir;
	retval->token = token;

	retval->flatdir = 0;
	retval->fastmode = self->owner->ready;
	if (!self->owner->ready)
	{
		tar_io_ref (self->owner);
	}

	retval->nextfile = self->file_child;
	retval->nextdir = self->dir_child;

	retval->nextheader_offset = 0;

	retval->LongLink = 0;
	DEBUG_PRINT ("\n");

	return retval;
}

static ocpdirhandle_pt tar_dir_readflatdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *), void *token)
{
	struct tar_instance_dir_t *self = (struct tar_instance_dir_t *)_self;
	struct tar_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	DEBUG_PRINT ("tar_dir_readflatdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->callback_dir = 0;
	retval->token = token;

	retval->flatdir = 1;
	retval->fastmode = self->owner->ready;
	if (!self->owner->ready)
	{
		tar_io_ref (self->owner);
	}

	retval->nextfile = 0;
	retval->nextheader_offset = 0;

	retval->LongLink = 0;
	DEBUG_PRINT ("\n");

	return retval;
}

static void tar_dir_readdir_cancel (ocpdirhandle_pt _self)
{
	struct tar_instance_ocpdirhandle_t *self = (struct tar_instance_ocpdirhandle_t *)_self;

	DEBUG_PRINT ("tar_dir_readdir_cancel, we need to UNREF\n");

	self->dir->head.unref (&self->dir->head);

	if (!self->fastmode)
	{
		tar_io_unref (self->dir->owner);
	}

	if (self->LongLink)
	{
		free (self->LongLink);
		self->LongLink = 0;
	}


	free (self);

	DEBUG_PRINT ("\n");
}

static unsigned long long char12tolonglong(char src[12])
{
	unsigned long long retval;
	char tmp[13];
	strncpy(tmp, src, 12);
	tmp[12]=0;
	retval=strtoull(tmp, 0, 8);
	return retval;
}

static int tar_dir_readdir_iterate (ocpdirhandle_pt _self)
{
	struct tar_instance_ocpdirhandle_t *self = (struct tar_instance_ocpdirhandle_t *)_self;

	if (self->fastmode)
	{
		if (self->flatdir)
		{
			if (self->nextfile >= self->dir->owner->file_fill)
			{
				return 0;
			}
			self->callback_file (self->token, &self->dir->owner->files[self->nextfile++]->head);
			return 1;
		} else {
			if (self->nextdir != UINT32_MAX)
			{
				self->callback_dir (self->token, &self->dir->owner->dirs[self->nextdir]->head);
				self->nextdir = self->dir->owner->dirs[self->nextdir]->dir_next;
				return 1;
			}
			if (self->nextfile != UINT32_MAX)
			{
				self->callback_file (self->token, &self->dir->owner->files[self->nextfile]->head);
				self->nextfile = self->dir->owner->files[self->nextfile]->file_next;
				return 1;
			}
			return 0;
		}
	} else {
		uint8_t header[512];
		uint64_t filesize;

		if (!self->dir->owner->archive_filehandle)
		{
			return 0; /* failed to open during init? */
		}

		self->dir->owner->archive_filehandle->seek_set (self->dir->owner->archive_filehandle, self->nextheader_offset);
		if ((self->dir->owner->archive_filehandle->read (self->dir->owner->archive_filehandle, header, 512) != 512) ||
		    (memcmp (header + 257, "ustar", 5)))
		{
			const char *filename;
			uint8_t *metadata = 0;
			size_t metadatasize = 0;

			self->dir->owner->ready = 1;
			// tar_translate_complete (iter); /* in theory, two instances might scan at the same time, so we only clean-up then in the destructor
			tar_instance_encode_blob (self->dir->owner, &metadata, &metadatasize);
			if (metadata)
			{
				dirdbGetName_internalstr (self->dir->owner->archive_filehandle->dirdb_ref, &filename);
				adbMetaAdd (filename, self->dir->owner->archive_filehandle->filesize (self->dir->owner->archive_filehandle), "TAR", metadata, metadatasize);
				free (metadata);
			}

			if (!self->flatdir)
			{
				uint32_t iter;
				for (iter = self->dir->owner->dirs[0]->dir_child; iter != UINT32_MAX; iter = self->dir->owner->dirs[iter]->dir_next)
				{
					self->callback_dir (self->token, &self->dir->owner->dirs[iter]->head);
				}
			}

			return 0;
		}

		filesize = char12tolonglong ((char *)(header + 124));

		if ((header[156] == 'L') && (!strncmp ((char *)header, "././@LongLink", 14)))
		{
			if (self->LongLink)
			{
				free (self->LongLink);
				self->LongLink = 0;
			}

			if (filesize && (filesize < MAX_FILENAME_SIZE))
			{
				self->LongLink = malloc (filesize + 1);
				self->LongLink[filesize] = 0;
				if (self->dir->owner->archive_filehandle->read (self->dir->owner->archive_filehandle, self->LongLink, filesize) != filesize)
				{
					free (self->LongLink);
					self->LongLink = 0;
				}
			}
		}
		if (header[0] && ((header[156] == '0')/* || (header[156] == '5')  we ignore directories */ || (header[156] == '7')))
		{
			char filename[256];
			uint32_t fileid;

			/* please extract the filename */
			if (!memcmp (header + 263, "ustar""\0""00", 8))
			{
				strncpy (filename, (char *)(header + 345), 155);
				filename[155] = 0;
				strncat (filename, (char *)header, 100);
				filename[255] = 0;
			} else {
				strncpy (filename, (char *)header, 100);
				filename[100] = 0;
			}

			fileid = tar_instance_add (self->dir->owner, self->LongLink ? self->LongLink : filename, filesize, self->nextheader_offset + 512);

			if (fileid != UINT32_MAX)
			{
				if (self->flatdir)
				{
					self->callback_file (self->token, &self->dir->owner->files[fileid]->head);
				} else {
					if (self->dir->owner->dirs[self->dir->owner->files[fileid]->dir_parent] == self->dir)
					{
						self->callback_file (self->token, &self->dir->owner->files[fileid]->head);
					}
				}
			}
		}
		if ((header[156] >= '0') && (header[156] <= '7') && self->LongLink)
		{
			free (self->LongLink); self->LongLink = 0;
		}
		self->nextheader_offset += ((512 + filesize) + 511 )& ~511; /* round up to nearest 512 block */

		return 1;
	}
}

static void tar_dir_readdir_forcescan_file (void *token, struct ocpfile_t *file)
{
}

static void tar_dir_readdir_forcescan_dir (void *token, struct ocpdir_t *dir)
{
}

static void tar_force_ready (struct tar_instance_dir_t *self)
{
	ocpdirhandle_pt handle;

	if (self->owner->ready)
	{
		return;
	}

	handle = self->head.readdir_start (&self->head, tar_dir_readdir_forcescan_file, tar_dir_readdir_forcescan_dir, 0);
	if (!handle)
	{
		fprintf (stderr, "tar_force_ready: out of memory?\n");
		return;
	}
	while (self->head.readdir_iterate (handle))
	{
	}
	self->head.readdir_cancel (handle);
}

static struct ocpdir_t *tar_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct tar_instance_dir_t *self = (struct tar_instance_dir_t *)_self;
	int i;

	tar_force_ready (self);

	for (i=0; i < self->owner->dir_fill; i++)
	{
		if (self->owner->dirs[i]->head.dirdb_ref == dirdb_ref)
		{
			self->owner->dirs[i]->head.ref (&self->owner->dirs[i]->head);
			return &self->owner->dirs[i]->head;
		}
	}

	return 0;
}

static struct ocpfile_t *tar_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct tar_instance_dir_t *self = (struct tar_instance_dir_t *)_self;
	int i;

	tar_force_ready (self);

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

static int tar_filehandle_read (struct ocpfilehandle_t *_self, void *dst, int len)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;
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

static void tar_file_ref (struct ocpfile_t *_self)
{
	struct tar_instance_file_t *self = (struct tar_instance_file_t *)_self;
	DEBUG_PRINT ("tar_file_ref (old value=%d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		tar_instance_ref (self->owner);
	}
	self->head.refcount++;
	DEBUG_PRINT ("\n");
}

static void tar_file_unref (struct ocpfile_t *_self)
{
	struct tar_instance_file_t *self = (struct tar_instance_file_t *)_self;
	DEBUG_PRINT ("tar_file_unref (old value=%d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		tar_instance_unref (self->owner);
	}
	DEBUG_PRINT ("\n");
}

static struct ocpfilehandle_t *tar_file_open (struct ocpfile_t *_self)
{
	struct tar_instance_file_t *self = (struct tar_instance_file_t *)_self;
	struct tar_instance_filehandle_t *retval;

	retval = calloc (sizeof (*retval), 1);
	ocpfilehandle_t_fill (&retval->head,
	                       tar_filehandle_ref,
	                       tar_filehandle_unref,
	                       _self,
	                       tar_filehandle_seek_set,
	                       tar_filehandle_seek_cur,
	                       tar_filehandle_seek_end,
	                       tar_filehandle_getpos,
	                       tar_filehandle_eof,
	                       tar_filehandle_error,
	                       tar_filehandle_read,
	                       0, /* ioctl */
	                       tar_filehandle_filesize,
	                       tar_filehandle_filesize_ready,
	                       0, /* filename_override */
	                       dirdbRef (self->head.dirdb_ref, dirdb_use_filehandle));

	retval->file = self;

	DEBUG_PRINT ("We just created a TAR handle, REF it\n");
	retval->head.ref (&retval->head);
	DEBUG_PRINT ("\n");

	tar_io_ref (self->owner);

	return &retval->head;
}

static uint64_t tar_file_filesize (struct ocpfile_t *_self)
{
	struct tar_instance_file_t *self = (struct tar_instance_file_t *)_self;
	return self->filesize;
}

static int tar_file_filesize_ready (struct ocpfile_t *_self)
{
	return 1;
}

static void tar_filehandle_ref (struct ocpfilehandle_t *_self)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;

	DEBUG_PRINT ("tar_filehandle_ref (old count = %d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		tar_instance_ref (self->file->owner);
	}
	self->head.refcount++;
	DEBUG_PRINT ("\n");
}

static void tar_filehandle_unref (struct ocpfilehandle_t *_self)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;

	DEBUG_PRINT ("tar_filehandle_unref (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		dirdbUnref (self->head.dirdb_ref, dirdb_use_filehandle);
		tar_io_unref (self->file->owner);
		tar_instance_unref (self->file->owner);
		free (self);
	}
	DEBUG_PRINT ("\n");
}

static int tar_filehandle_seek_set (struct ocpfilehandle_t *_self, int64_t pos)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;

	if (pos < 0) return -1;

	if (pos > self->file->filesize) return -1;

	self->filepos = pos;
	self->error = 0;

	return 0;
}

static int tar_filehandle_seek_cur (struct ocpfilehandle_t *_self, int64_t pos)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;

	if (pos <= 0)
	{
		if (pos == INT64_MIN) return -1; /* we never have files this size */
		if ((-pos) > self->filepos) return -1;
		self->filepos += pos;
	} else {
		/* check for overflow */
		if ((int64_t)(pos + self->filepos) < 0) return -1;

		if ((pos + self->filepos) > self->file->filesize) return -1;
		self->filepos += pos;
	}

	self->error = 0;

	return 0;
}

static int tar_filehandle_seek_end (struct ocpfilehandle_t *_self, int64_t pos)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;

	if (pos > 0) return -1;

	if (pos == INT64_MIN) return -1; /* we never have files this size */

	if (pos < -(int64_t)(self->file->filesize)) return -1;

	self->filepos = self->file->filesize + pos;

	self->error = 0;

	return 0;
}

static uint64_t tar_filehandle_getpos (struct ocpfilehandle_t *_self)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;
	return self->filepos;
}

static int tar_filehandle_eof (struct ocpfilehandle_t *_self)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;
	return self->filepos >= self->file->filesize;
}

static int tar_filehandle_error (struct ocpfilehandle_t *_self)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;
	return self->error;
}

static uint64_t tar_filehandle_filesize (struct ocpfilehandle_t *_self)
{
	struct tar_instance_filehandle_t *self = (struct tar_instance_filehandle_t *)_self;
	return self->file->filesize;
}

static int tar_filehandle_filesize_ready (struct ocpfilehandle_t *_self)
{
	return 1;
}

static void tar_get_default_string (struct ocpdir_t *_self, const char **label, const char **key)
{
	//struct tar_instance_dir_t *self = (struct tar_instance_dir_t *)_self;
	*label = "UTF-8";
	*key = "UTF-8";
}

static const char *tar_get_byuser_string (struct ocpdir_t *_self)
{
	struct tar_instance_dir_t *self = (struct tar_instance_dir_t *)_self;
	return self->owner->charset_override;
}

static void tar_translate_prepare (struct tar_instance_t *self)
{
	const char *target;
	char *temp;
	target = self->charset_override ? self->charset_override : "UTF-8";

	DEBUG_PRINT ("tar_translate_prepare %s\n", self->charset_override ? self->charset_override : "(NULL) UTF-8");

	if (self->iconv_handle != (iconv_t)-1)
	{
		iconv_close (self->iconv_handle);
		self->iconv_handle = (iconv_t)-1;
	}

	temp = malloc (strlen (target) + 11);
	if (temp)
	{
		sprintf (temp, "%s//TRANSLIT", target);
		self->iconv_handle = iconv_open ("UTF-8", temp);
		free (temp);
		temp = 0;
	}
	if (self->iconv_handle == (iconv_t)-1)
	{
		self->iconv_handle = iconv_open ("UTF-8", target);
	}
}

static void tar_translate_complete (struct tar_instance_t *self)
{
	DEBUG_PRINT ("tar_translate_complete\n");

	if (self->iconv_handle != (iconv_t)-1)
	{
		iconv_close (self->iconv_handle);
		self->iconv_handle = (iconv_t)-1;
	}
}

static void tar_translate (struct tar_instance_t *self, char *src, char **buffer, int *buffersize)
{
	char *temp;
	size_t srclen;

	char *dst = *buffer;
	size_t dstlen = *buffersize;

	DEBUG_PRINT ("tar_translate %s =>", src);

	temp = strrchr (src, '/');
	if (temp)
	{
		src = temp + 1;
	}
	srclen = strlen (src);

	if (!self->iconv_handle)
	{
		*buffer = strdup (src);
		*buffersize = *buffer ? strlen (*buffer) : 0;
		return;
	}

	iconv (self->iconv_handle, 0, 0, 0, 0);

	while (srclen)
	{
		if (dstlen <= 10)
		{
			int oldofs = dst - (*buffer);
			*buffersize += 32;
			char *temp = realloc (*buffer, *buffersize);
			if (!temp)
			{
				*buffersize -= 32;
				fprintf (stderr, "tar_translate: out of memory\n");
				free (*buffer);
				*buffer = 0;
				*buffersize = 0;
				return;
			}
			*buffer = temp;
			dst = temp + oldofs;
			dstlen += 32;
		}

		if (iconv (self->iconv_handle, &src, &srclen, &dst, &dstlen) == (size_t)-1)
		{
			if (errno != E2BIG)
			{
				src++;
				srclen--;
			}
		}
	}

	if (dstlen <= 10)
	{
		int oldofs = dst - (*buffer);
		*buffersize += 32;
		char *temp = realloc (*buffer, *buffersize);
		if (!temp)
		{
			*buffersize -= 32;
			fprintf (stderr, "tar_translate: out of memory\n");
			free (*buffer);
			*buffer = 0;
			*buffersize = 0;
			return;
		}
		*buffer = temp;
		dst = temp + oldofs;
		dstlen += 32;
	}

	*dst = 0;

	DEBUG_PRINT ("%s\n", *buffer);
}


static void tar_set_byuser_string (struct ocpdir_t *_self, const char *byuser)
{
	struct tar_instance_dir_t *self = (struct tar_instance_dir_t *)_self;
	char *temp;
	int i;
	int templen;

	if (!strcmp (byuser?byuser:"", self->owner->charset_override ? self->owner->charset_override : ""))
	{
		return;
	}

	free (self->owner->charset_override);
	if (byuser)
	{
		self->owner->charset_override = strdup (byuser);
	} else {
		self->owner->charset_override = 0;
	}

	/* update adbMeta */
	{
		uint8_t *metadata = 0;
		size_t metadatasize = 0;
		const char *filename = 0;

		tar_instance_encode_blob (self->owner, &metadata, &metadatasize);
		dirdbGetName_internalstr (self->owner->archive_file->dirdb_ref, &filename);
		adbMetaAdd (filename, self->owner->archive_file->filesize (self->owner->archive_file), "TAR", metadata, metadatasize);
		free (metadata);
	}

	tar_translate_prepare (self->owner);

	temp = 0;
	templen = 0;

	for (i=1; i < self->owner->dir_fill; i++)
	{
		tar_translate (self->owner, self->owner->dirs[i]->orig_full_dirpath, &temp, &templen);
		if (temp)
		{
			dirdbUnref (self->owner->dirs[i]->head.dirdb_ref, dirdb_use_dir);
			self->owner->dirs[i]->head.dirdb_ref = dirdbFindAndRef (
				self->owner->dirs
				[
					self->owner->dirs[i]->dir_parent
				]->head.dirdb_ref, temp, dirdb_use_dir);
		}
	}

	for (i=0; i < self->owner->file_fill; i++)
	{
		tar_translate (self->owner, self->owner->files[i]->orig_full_filepath, &temp, &templen);
		if (temp)
		{
			dirdbUnref (self->owner->files[i]->head.dirdb_ref, dirdb_use_file);
			self->owner->files[i]->head.dirdb_ref = dirdbFindAndRef (
				self->owner->dirs
				[
					self->owner->files[i]->dir_parent
				]->head.dirdb_ref, temp, dirdb_use_file);
		}
	}

	free (temp);

	tar_translate_complete (self->owner);
}

static char **tar_get_test_strings(struct ocpdir_t *__self)
{
	struct tar_instance_dir_t *_self = (struct tar_instance_dir_t *)__self;
	struct tar_instance_t *self = _self->owner;
	char **retval;

	int count = self->dir_fill + self->file_fill - 1, i;
	retval = calloc (count + 1, sizeof (char *));
	if (!retval)
	{
		return 0;
	}
	count = 0;
	for (i=1; i < self->dir_fill; i++)
	{
		retval[count] = strdup (self->dirs[i]->orig_full_dirpath);
		if (!retval[count])
		{
			return retval;
		}
		count++;
	}
	for (i=0; i < self->file_fill; i++)
	{
		retval[count] = strdup (self->files[i]->orig_full_filepath);
		if (!retval[count])
		{
			return retval;
		}
		count++;
	}
	return retval;
}
