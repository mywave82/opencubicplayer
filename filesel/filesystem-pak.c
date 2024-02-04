/* OpenCP Module Player
 * copyright (c) 2022-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decode PAK (Westwood and Quake) file archives
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "adbmeta.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-pak.h"

#if defined(PAK_DEBUG) || defined(PAK_VERBOSE)
static int do_pak_debug_print=1;
#endif

#ifdef PAK_DEBUG
#define DEBUG_PRINT(...) do { if (do_pak_debug_print) { fprintf(stderr, "[PAK] " __VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#ifdef PAK_VERBOSE
#define VERBOSE_PRINT(...) do { if (do_pak_debug_print) { fprintf(stderr, "[PAK] " __VA_ARGS__); } } while (0)
#else
#define VERBOSE_PRINT(...) {}
#endif

struct pak_instance_t;

struct pak_instance_dir_t
{
	struct ocpdir_t        head;
	struct pak_instance_t *owner;
	uint32_t               dir_parent; /* used for making blob */
	uint32_t               dir_next;
	uint32_t               dir_child;
	uint32_t               file_child;
	char                  *orig_full_dirpath; /* if encoding deviates from UTF-8 */
};

struct pak_instance_file_t
{
	struct ocpfile_t           head;
	struct pak_instance_t     *owner;
	uint32_t                   dir_parent; /* used for making blob */
	uint32_t                   file_next;
	uint32_t                   filesize;
	uint32_t                   fileoffset; /* offset into the pak file */
	char                      *orig_full_filepath; /* if encoding deviates from UTF-8, this can be used to recode on the fly too */
};

struct pak_instance_filehandle_t
{
	struct ocpfilehandle_t      head;
	struct pak_instance_file_t *file;
	int                         error;

	uint64_t                    filepos;
};

struct pak_instance_t
{
	struct pak_instance_t       *next;
	int                          ready; /* a complete scan has been performed, use cache instead of file-read */

	struct pak_instance_dir_t  **dirs;
	struct pak_instance_dir_t    dir0;
	int                          dir_fill;
	int                          dir_size;
	struct pak_instance_file_t **files;
        int                          file_fill;
	int                          file_size;

	struct ocpfile_t            *archive_file;
	struct ocpfilehandle_t      *archive_filehandle;
#if 0
	iconv_t                     *iconv_handle;
	char                        *charset_override;  /* either NULL, or an override string */
#endif
	int                          refcount;
	int                          iorefcount;
};

static struct pak_instance_t *pak_root;

static void pak_io_ref (struct pak_instance_t *self); /* count if we need I/O from the .PAK file */
static void pak_io_unref (struct pak_instance_t *self);

static void pak_dir_ref (struct ocpdir_t *);
static void pak_dir_unref (struct ocpdir_t *);

static ocpdirhandle_pt pak_dir_readdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                 void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
static ocpdirhandle_pt pak_dir_readflatdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *), void *token);
static void pak_dir_readdir_cancel (ocpdirhandle_pt);
static int pak_dir_readdir_iterate (ocpdirhandle_pt);
static struct ocpdir_t *pak_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref);
static struct ocpfile_t *pak_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref);

static void pak_file_ref (struct ocpfile_t *);
static void pak_file_unref (struct ocpfile_t *);
static struct ocpfilehandle_t *pak_file_open (struct ocpfile_t *);
static uint64_t pak_file_filesize (struct ocpfile_t *);
static int pak_file_filesize_ready (struct ocpfile_t *);

static void pak_filehandle_ref (struct ocpfilehandle_t *);
static void pak_filehandle_unref (struct ocpfilehandle_t *);
static int pak_filehandle_seek_set (struct ocpfilehandle_t *, int64_t pos);
static uint64_t pak_filehandle_getpos (struct ocpfilehandle_t *);
static int pak_filehandle_eof (struct ocpfilehandle_t *);
static int pak_filehandle_error (struct ocpfilehandle_t *);
static int pak_filehandle_read (struct ocpfilehandle_t *, void *dst, int len);
static uint64_t pak_filehandle_filesize (struct ocpfilehandle_t *);
static int pak_filehandle_filesize_ready (struct ocpfilehandle_t *);

static void pak_scan (struct pak_instance_t *iter);
static void pak_scan_quake (struct pak_instance_t *iter);
static void pak_scan_westwood (struct pak_instance_t *iter);

#if 0
static void pak_get_default_string (struct ocpdir_t *self, const char **label, const char **key);
static const char *pak_get_byuser_string (struct ocpdir_t *self);
static void pak_set_byuser_string (struct ocpdir_t *self, const char *byuser);
static char **pak_get_test_strings(struct ocpdir_t *self);

static void pak_translate_prepare (struct pak_instance_t *self);
static void pak_translate (struct pak_instance_t *self, char *src, char **buffer, int *buffersize);
static void pak_translate_complete (struct pak_instance_t *self);

static const struct ocpdir_charset_override_API_t pak_charset_API =
{
	pak_get_default_string,
	pak_get_byuser_string,
	pak_set_byuser_string,
	pak_get_test_strings
};
#endif

static uint32_t pak_instance_add (struct pak_instance_t *self,
                                  char           *Filename,
                                  const uint32_t  filesize,
                                  const uint32_t  fileoffset);

/* in the blob, we will switch / into \0 temporary as we parse them */
static void pak_instance_decode_blob (struct pak_instance_t *self, uint8_t *blob, size_t blobsize)
{

	uint8_t *eos;

	eos = memchr (blob, 0, blobsize);
	if (!eos)
	{
		return;
	}
#if 0
	if (eos != blob)
	{
		self->charset_override = strdup ((char *)blob);
	} else {
		self->charset_override = NULL;
	}
#endif
	eos++;
	blobsize -= eos - blob;
	blob = eos;

#if 0
	pak_translate_prepare (self);
#endif

	while (blobsize >= 10)
	{
		uint64_t filesize =   ((uint64_t)(blob[ 3]) << 24) |
		                      ((uint64_t)(blob[ 2]) << 16) |
		                      ((uint64_t)(blob[ 1]) <<  8) |
		                      ((uint64_t)(blob[ 0])      );
		uint64_t fileoffset = ((uint64_t)(blob[ 7]) << 24) |
		                      ((uint64_t)(blob[ 6]) << 16) |
		                      ((uint64_t)(blob[ 5]) <<  8) |
		                      ((uint64_t)(blob[ 4])      );
		blob += 8;
		blobsize -= 8;
		eos = memchr (blob, 0, blobsize);
		if (!eos)
		{
			break;
		}

		pak_instance_add (self, (char *)blob, filesize, fileoffset);

		eos++;
		blobsize -= eos - blob;
		blob = eos;
	}

#if 0
	pak_translate_complete (self);
#endif
}

static void pak_instance_encode_blob (struct pak_instance_t *self, uint8_t **blob, size_t *blobfill)
{
	uint32_t counter;
	uint32_t blobsize = 0;

	*blobfill = 0;
	*blob = 0;

#if 0
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
#else
	{
		blobsize = 1 + 1024;
		*blob = malloc (blobsize);
		if (!*blob)
		{
			return;
		}
		(*blob)[0] = 0;
		*blobfill = 1;
	}
#endif

	for (counter=0; counter < self->file_fill; counter++)
	{
		int filenamesize = strlen (self->files[counter]->orig_full_filepath);
		if ((filenamesize + 1 + 8 + *blobfill) > blobsize)
		{
			uint32_t newsize = *blobfill + filenamesize + 1 + 8 + 1024;
			uint8_t *temp = realloc (*blob, newsize);
			if (!temp)
			{
				break;
			}
			*blob = temp;
			blobsize = newsize;
		}

		(*blob)[(*blobfill) + 3] = self->files[counter]->filesize >> 24;
		(*blob)[(*blobfill) + 2] = self->files[counter]->filesize >> 16;
		(*blob)[(*blobfill) + 1] = self->files[counter]->filesize >> 8;
		(*blob)[(*blobfill) + 0] = self->files[counter]->filesize;
		(*blob)[(*blobfill) + 7] = self->files[counter]->fileoffset >> 24;
		(*blob)[(*blobfill) + 6] = self->files[counter]->fileoffset >> 16;
		(*blob)[(*blobfill) + 5] = self->files[counter]->fileoffset >> 8;
		(*blob)[(*blobfill) + 4] = self->files[counter]->fileoffset;

		strcpy ((char *)(*blob) + 8 + (*blobfill), self->files[counter]->orig_full_filepath);
		*blobfill += 8 + filenamesize + 1;
	}
}

static void pak_io_ref (struct pak_instance_t *self)
{
	DEBUG_PRINT ( " pak_io_ref (old count = %d)\n", self->iorefcount);
	if (!self->iorefcount)
	{
		self->archive_filehandle = self->archive_file->open (self->archive_file);
	}
	self->iorefcount++;
}

static void pak_io_unref (struct pak_instance_t *self)
{
	DEBUG_PRINT (" pak_io_unref (old count = %d)\n", self->iorefcount);

	self->iorefcount--;
	if (self->iorefcount)
	{
		return;
	}

	if (self->archive_filehandle)
	{
		DEBUG_PRINT (" pak_io_unref => RELEASE archive_filehandle\n");
		self->archive_filehandle->unref (self->archive_filehandle);
		self->archive_filehandle = 0;
	}
}

static uint32_t pak_instance_add_create_dir (struct pak_instance_t *self,
                                             const uint32_t         dir_parent,
                                             const char            *Dirpath,
                                             const char            *Dirname)
{
	uint32_t *prev, iter;
	uint32_t dirdb_ref;

	DEBUG_PRINT ("[PAK] create_dir: %s %s\n", Dirpath, Dirname);

#if 0
	{
		char *temp = 0;
		int templen = 0;
		pak_translate (self, Filename, &temp, &templen);
		dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, temp ? temp : "???", dirdb_use_dir);
		free (temp); temp = 0;
	}
#else
	dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, Dirname, dirdb_use_dir);
#endif

	if (self->dir_fill == self->dir_size)
	{
		int size = self->dir_size + 16;
		struct pak_instance_dir_t **dirs = realloc (self->dirs, size * sizeof (self->dirs[0]));

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
	                pak_dir_ref,
	                pak_dir_unref,
	               &self->dirs[dir_parent]->head,
	                pak_dir_readdir_start,
	                pak_dir_readflatdir_start,
	                pak_dir_readdir_cancel,
	                pak_dir_readdir_iterate,
	                pak_dir_readdir_dir,
	                pak_dir_readdir_file,
	                0,
	                dirdb_ref,
	                0, /* refcount */
	                1, /* is_archive */
	                0, /* is_playlist */
	                self->archive_file->compression);

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

static uint32_t pak_instance_add_file (struct pak_instance_t *self,
                                       const uint32_t         dir_parent,
                                       const char            *Filepath,
                                       const char            *Filename,
                                       const uint32_t         filesize,
                                       const uint32_t         fileoffset)
{
	uint32_t *prev, iter;
	uint32_t dirdb_ref;

	DEBUG_PRINT ("[PAK] add_file: %s %s\n", Filepath, Filename);

	if (self->file_fill == self->file_size)
	{
		int size = self->file_size + 64;
		struct pak_instance_file_t **files = realloc (self->files, size * sizeof (self->files[0]));

		if (!files)
		{ /* out of memory */
			return UINT32_MAX;
		}

		self->files = files;
		self->file_size = size;
	}

#if 0
	{
		char *temp = 0;
		int templen = 0;

		pak_translate (self, Filename, &temp, &templen);
		dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, temp ? temp : "???", dirdb_use_file);
		free (temp); temp = 0;
	}
#else
	dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, Filename, dirdb_use_file);
#endif

	self->files[self->file_fill] = malloc (sizeof (*self->files[self->file_fill]));
	if (!self->files[self->file_fill])
	{ /* out of memory */
		dirdbUnref (dirdb_ref, dirdb_use_file);
		return UINT32_MAX;
	}

	ocpfile_t_fill (&self->files[self->file_fill]->head,
	                 pak_file_ref,
	                 pak_file_unref,
	                &self->dirs[dir_parent]->head,
	                 pak_file_open,
	                 pak_file_filesize,
	                 pak_file_filesize_ready,
	                 0, /* filename_override */
	                 dirdb_ref,
	                 0, /* refcount */
	                 0, /* is_nodetect */
	                 COMPRESSION_ADD_STORE (self->archive_file->compression));

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

static uint32_t pak_instance_add (struct pak_instance_t *self,
                                  char                  *Filepath,
                                  const uint32_t         filesize,
                                  const uint32_t         fileoffset)
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
					if (!strcmp (self->dirs[search]->orig_full_dirpath, Filepath))
					{
						*slash = '/';
						ptr = slash + 1;
						iter = search;
						goto again; /* we need a break + continue; */
					}
				}

				/* no hit, create one */
				iter = pak_instance_add_create_dir (self, iter, Filepath, ptr);
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
				return pak_instance_add_file (self, iter, Filepath, ptr, filesize, fileoffset);
			}
			return UINT32_MAX; /* out of memory.... */
		}
	}
}

struct ocpdir_t *pak_check (const struct ocpdirdecompressor_t *self, struct ocpfile_t *file, const char *filetype)
{
	struct pak_instance_t *iter;
	if (strcasecmp (filetype, ".pak"))
	{
		return 0;
	}

	DEBUG_PRINT ("[PAK] filetype (%s) matches .pak\n", filetype);

	/* Check the cache for an active instance */
	for (iter = pak_root; iter; iter = iter->next)
	{
		if (iter->dirs[0]->head.dirdb_ref == file->dirdb_ref)
		{
			DEBUG_PRINT ("[PAK] found a cached entry for the given dirdb_ref => refcount the ROOT entry\n");
			iter->dirs[0]->head.ref (&iter->dirs[0]->head);
			return &iter->dirs[0]->head;
		}
	}

	iter = calloc (sizeof (*iter), 1);
	iter->dir_size = 16;
	iter->dirs = malloc (iter->dir_size * sizeof (iter->dirs[0]));

	DEBUG_PRINT( "[PAK] creating a DIR using the same parent dirdb_ref\n");

	dirdbRef (file->dirdb_ref, dirdb_use_dir);
	iter->dirs[0] = &iter->dir0;
	ocpdir_t_fill (&iter->dirs[0]->head,
	                pak_dir_ref,
	                pak_dir_unref,
	                file->parent,
	                pak_dir_readdir_start,
	                pak_dir_readflatdir_start,
	                pak_dir_readdir_cancel,
	                pak_dir_readdir_iterate,
	                pak_dir_readdir_dir,
	                pak_dir_readdir_file,
#if 0
	                &pak_charset_API,
#else
			0,
#endif
	                file->dirdb_ref,
	                0, /* refcount */
	                1, /* is_archive */
	                0, /* is_playlist */
	                file->compression);

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

	iter->next = pak_root;
#if 0
	iter->iconv_handle = (iconv_t *)-1;
#endif
	pak_root = iter;

	if (iter->archive_file->filesize_ready (iter->archive_file))
	{
		const char *filename = 0;
		uint8_t *metadata = 0;
		size_t metadatasize = 0;

		dirdbGetName_internalstr (iter->archive_file->dirdb_ref, &filename);
		if (!adbMetaGet (filename, iter->archive_file->filesize (iter->archive_file), "PAK", &metadata, &metadatasize))
		{
			DEBUG_PRINT ("[PAK] We found adbmeta cache\n");
			pak_instance_decode_blob (iter, metadata, metadatasize);
			free (metadata);
			iter->ready = 1;
		}
	}

	if (!iter->ready)
	{
#if 0
		pak_translate_prepare (iter);
#endif
		pak_scan (iter);
		iter->ready = 1;
#if 0
		pak_translate_complete (iter);
#endif
		if (iter->file_fill)
		{
			uint8_t *metadata = 0;
			size_t metadatasize = 0;
			const char *filename = 0;

			pak_instance_encode_blob (iter, &metadata, &metadatasize);
			dirdbGetName_internalstr (iter->archive_file->dirdb_ref, &filename);
			adbMetaAdd (filename, iter->archive_file->filesize (iter->archive_file), "PAK", metadata, metadatasize);
			free (metadata);
		}
	}

	DEBUG_PRINT ("[PAK] finished creating the datastructures, refcount the ROOT entry\n");
	iter->dirs[0]->head.ref (&iter->dirs[0]->head);
	return &iter->dirs[0]->head;
}

static void pak_instance_ref (struct pak_instance_t *self)
{
	DEBUG_PRINT (" PAK_INSTANCE_REF (old count = %d)\n", self->refcount);
	self->refcount++;
}

static void pak_instance_unref (struct pak_instance_t *self)
{
	uint32_t counter;
	struct pak_instance_t **prev, *iter;

	DEBUG_PRINT (" PAK_INSTANCE_UNREF (old count = %d)\n", self->refcount);

	self->refcount--;
	if (self->refcount)
	{
		return;
	}

	DEBUG_PRINT (" KILL THE INSTANCE!!! (iocount = %d)\n", self->iorefcount);

#if 0
	pak_translate_complete (self);
#endif

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

	prev = &pak_root;
	for (iter = pak_root; iter; iter = iter->next)
	{
		if (iter == self)
		{
			*prev = self->next;
			break;
		}
		prev = &iter->next;
	}

#if 0
	free (self->charset_override);
#endif
	free (self);
}

static struct ocpdirdecompressor_t pakdecompressor =
{
	"pak",
	"Westwood and Quake II PAK fileformats",
	pak_check
};

void filesystem_pak_register (void)
{
	register_dirdecompressor (&pakdecompressor);
}

static void pak_dir_ref (struct ocpdir_t *_self)
{
	struct pak_instance_dir_t *self = (struct pak_instance_dir_t *)_self;
	DEBUG_PRINT ("PAK_DIR_REF (old count = %d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		pak_instance_ref (self->owner);
	}
	self->head.refcount++;
}

static void pak_dir_unref (struct ocpdir_t *_self)
{
	struct pak_instance_dir_t *self = (struct pak_instance_dir_t *)_self;
	DEBUG_PRINT ("PAK_DIR_UNREF (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		pak_instance_unref (self->owner);
	}
}

struct pak_instance_ocpdirhandle_t
{
	struct pak_instance_dir_t *dir;

	void(*callback_file)(void *token, struct ocpfile_t *);
        void(*callback_dir )(void *token, struct ocpdir_t *);
	void *token;

	int flatdir;
#if 0
	int fastmode; /* if this is false, we did io-ref */
#endif

	/* fast-mode */
	uint32_t nextdir;
	uint32_t nextfile;
#if 0
	/* scanmode */
	uint64_t nextheader_offset;
	char *LongLink;
#endif
};

static ocpdirhandle_pt pak_dir_readdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                      void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct pak_instance_dir_t *self = (struct pak_instance_dir_t *)_self;
	struct pak_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	DEBUG_PRINT ("pak_dir_readdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->callback_dir = callback_dir;
	retval->token = token;

	retval->flatdir = 0;
#if 0
	retval->fastmode = self->owner->ready;
	if (!self->owner->ready)
	{
		pak_io_ref (self->owner);
	}
#else
	assert (self->owner->ready);
#endif

	retval->nextfile = self->file_child;
	retval->nextdir = self->dir_child;

#if 0
	retval->nextheader_offset = 0;

	retval->LongLink = 0;
	DEBUG_PRINT ("\n");
#endif

	return retval;
}

static ocpdirhandle_pt pak_dir_readflatdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *), void *token)
{
	struct pak_instance_dir_t *self = (struct pak_instance_dir_t *)_self;
	struct pak_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	DEBUG_PRINT ("pak_dir_readflatdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->callback_dir = 0;
	retval->token = token;

	retval->flatdir = 1;
#if 0
	retval->fastmode = self->owner->ready;
	if (!self->owner->ready)
	{
		pak_io_ref (self->owner);
	}
#else
	assert (self->owner->ready);
#endif

	retval->nextfile = 0;
#if 0
	retval->nextheader_offset = 0;

	retval->LongLink = 0;
	DEBUG_PRINT ("\n");
#endif
	return retval;
}

static void pak_dir_readdir_cancel (ocpdirhandle_pt _self)
{
	struct pak_instance_ocpdirhandle_t *self = (struct pak_instance_ocpdirhandle_t *)_self;

	DEBUG_PRINT ("pak_dir_readdir_cancel, we need to UNREF\n");

	self->dir->head.unref (&self->dir->head);

#if 0
	if (!self->fastmode)
	{
		pak_io_unref (self->dir->owner);
	}

	if (self->LongLink)
	{
		free (self->LongLink);
		self->LongLink = 0;
	}
#endif

	free (self);

#if 0
	DEBUG_PRINT ("\n");
#endif
}

static int pak_dir_readdir_iterate (ocpdirhandle_pt _self)
{
	struct pak_instance_ocpdirhandle_t *self = (struct pak_instance_ocpdirhandle_t *)_self;

#if 0
	if (self->fastmode)
#endif
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
	}
}

static struct ocpdir_t *pak_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct pak_instance_dir_t *self = (struct pak_instance_dir_t *)_self;
	int i;

#if 0
	pak_force_ready (self);
#endif

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

static struct ocpfile_t *pak_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct pak_instance_dir_t *self = (struct pak_instance_dir_t *)_self;
	int i;

#if 0
	pak_force_ready (self);
#endif

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

static int pak_filehandle_read (struct ocpfilehandle_t *_self, void *dst, int len)
{
	struct pak_instance_filehandle_t *self = (struct pak_instance_filehandle_t *)_self;
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

static void pak_file_ref (struct ocpfile_t *_self)
{
	struct pak_instance_file_t *self = (struct pak_instance_file_t *)_self;
	DEBUG_PRINT ("pak_file_ref (old value=%d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		pak_instance_ref (self->owner);
	}
	self->head.refcount++;
	DEBUG_PRINT ("\n");
}

static void pak_file_unref (struct ocpfile_t *_self)
{
	struct pak_instance_file_t *self = (struct pak_instance_file_t *)_self;
	DEBUG_PRINT ("pak_file_unref (old value=%d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		pak_instance_unref (self->owner);
	}
	DEBUG_PRINT ("\n");
}

static struct ocpfilehandle_t *pak_file_open (struct ocpfile_t *_self)
{
	struct pak_instance_file_t *self = (struct pak_instance_file_t *)_self;
	struct pak_instance_filehandle_t *retval;

	retval = calloc (sizeof (*retval), 1);
	ocpfilehandle_t_fill (&retval->head,
	                       pak_filehandle_ref,
	                       pak_filehandle_unref,
	                       _self,
	                       pak_filehandle_seek_set,
	                       pak_filehandle_getpos,
	                       pak_filehandle_eof,
	                       pak_filehandle_error,
	                       pak_filehandle_read,
	                       0, /* ioctl */
	                       pak_filehandle_filesize,
	                       pak_filehandle_filesize_ready,
	                       0, /* filename_override */
	                       dirdbRef (self->head.dirdb_ref, dirdb_use_filehandle),
	                       1 /* refcount */
	);

	retval->file = self;

	DEBUG_PRINT ("We just created a PAK handle, ref the source\n");
	pak_instance_ref (self->owner);
	pak_io_ref (self->owner);

	return &retval->head;
}

static uint64_t pak_file_filesize (struct ocpfile_t *_self)
{
	struct pak_instance_file_t *self = (struct pak_instance_file_t *)_self;
	return self->filesize;
}

static int pak_file_filesize_ready (struct ocpfile_t *_self)
{
	return 1;
}

static void pak_filehandle_ref (struct ocpfilehandle_t *_self)
{
	struct pak_instance_filehandle_t *self = (struct pak_instance_filehandle_t *)_self;

	DEBUG_PRINT ("pak_filehandle_ref (old count = %d)\n", self->head.refcount);
	self->head.refcount++;
}

static void pak_filehandle_unref (struct ocpfilehandle_t *_self)
{
	struct pak_instance_filehandle_t *self = (struct pak_instance_filehandle_t *)_self;

	DEBUG_PRINT ("pak_filehandle_unref (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		dirdbUnref (self->head.dirdb_ref, dirdb_use_filehandle);
		pak_io_unref (self->file->owner);
		pak_instance_unref (self->file->owner);
		free (self);
	}
	DEBUG_PRINT ("\n");
}

static int pak_filehandle_seek_set (struct ocpfilehandle_t *_self, int64_t pos)
{
	struct pak_instance_filehandle_t *self = (struct pak_instance_filehandle_t *)_self;

	if (pos < 0) return -1;

	if (pos > self->file->filesize) return -1;

	self->filepos = pos;
	self->error = 0;

	return 0;
}

static uint64_t pak_filehandle_getpos (struct ocpfilehandle_t *_self)
{
	struct pak_instance_filehandle_t *self = (struct pak_instance_filehandle_t *)_self;
	return self->filepos;
}

static int pak_filehandle_eof (struct ocpfilehandle_t *_self)
{
	struct pak_instance_filehandle_t *self = (struct pak_instance_filehandle_t *)_self;
	return self->filepos >= self->file->filesize;
}

static int pak_filehandle_error (struct ocpfilehandle_t *_self)
{
	struct pak_instance_filehandle_t *self = (struct pak_instance_filehandle_t *)_self;
	return self->error;
}

static uint64_t pak_filehandle_filesize (struct ocpfilehandle_t *_self)
{
	struct pak_instance_filehandle_t *self = (struct pak_instance_filehandle_t *)_self;
	return self->file->filesize;
}

static int pak_filehandle_filesize_ready (struct ocpfilehandle_t *_self)
{
	return 1;
}

#if 0
static void pak_get_default_string (struct ocpdir_t *_self, const char **label, const char **key)
{
	//struct pak_instance_dir_t *self = (struct pak_instance_dir_t *)_self;
	*label = "UTF-8";
	*key = "UTF-8";
}

static const char *pak_get_byuser_string (struct ocpdir_t *_self)
{
	struct pak_instance_dir_t *self = (struct pak_instance_dir_t *)_self;
	return self->owner->charset_override;
}

static void pak_translate_prepare (struct pak_instance_t *self)
{
	const char *pakget;
	char *temp;
	pakget = self->charset_override ? self->charset_override : "UTF-8";

	DEBUG_PRINT ("pak_translate_prepare %s\n", self->charset_override ? self->charset_override : "(NULL) UTF-8");

	if (self->iconv_handle != (iconv_t)-1)
	{
		iconv_close (self->iconv_handle);
		self->iconv_handle = (iconv_t)-1;
	}

	temp = malloc (strlen (pakget) + 11);
	if (temp)
	{
		sprintf (temp, "%s//TRANSLIT", pakget);
		self->iconv_handle = iconv_open ("UTF-8", temp);
		free (temp);
		temp = 0;
	}
	if (self->iconv_handle == (iconv_t)-1)
	{
		self->iconv_handle = iconv_open ("UTF-8", pakget);
	}
}

static void pak_translate_complete (struct pak_instance_t *self)
{
	DEBUG_PRINT ("pak_translate_complete\n");

	if (self->iconv_handle != (iconv_t)-1)
	{
		iconv_close (self->iconv_handle);
		self->iconv_handle = (iconv_t)-1;
	}
}

static void pak_translate (struct pak_instance_t *self, char *src, char **buffer, int *buffersize)
{
	char *temp;
	size_t srclen;

	char *dst = *buffer;
	size_t dstlen = *buffersize;

	DEBUG_PRINT ("pak_translate %s =>", src);

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
				fprintf (stderr, "pak_translate: out of memory\n");
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
			fprintf (stderr, "pak_translate: out of memory\n");
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

static void pak_set_byuser_string (struct ocpdir_t *_self, const char *byuser)
{
	struct pak_instance_dir_t *self = (struct pak_instance_dir_t *)_self;
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

		pak_instance_encode_blob (self->owner, &metadata, &metadatasize);
		dirdbGetName_internalstr (self->owner->archive_file->dirdb_ref, &filename);
		adbMetaAdd (filename, self->owner->archive_file->filesize (self->owner->archive_file), "PAK", metadata, metadatasize);
		free (metadata);
	}

	pak_translate_prepare (self->owner);

	temp = 0;
	templen = 0;

	for (i=1; i < self->owner->dir_fill; i++)
	{
		pak_translate (self->owner, self->owner->dirs[i]->orig_full_dirpath, &temp, &templen);
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
		pak_translate (self->owner, self->owner->files[i]->orig_full_filepath, &temp, &templen);
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

	pak_translate_complete (self->owner);
}

static char **pak_get_test_strings(struct ocpdir_t *__self)
{
	struct pak_instance_dir_t *_self = (struct pak_instance_dir_t *)__self;
	struct pak_instance_t *self = _self->owner;
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
#endif

struct PAK_quake_header_t
{
	char id[4];
	int offset;
	int size;
};

struct PAK_quake_file_t
{
	char filename[56];
	int offset;
	int size;
};

#warning TODO, directory maybe has \ instead of / ?
static void pak_scan_quake (struct pak_instance_t *self)
{
	struct PAK_quake_header_t pak_header;
	struct PAK_quake_file_t pak_file;
	int num_files;
	int i;
	uint64_t filesize;

	DEBUG_PRINT ("pak_scan_quake()\n");

	if (self->archive_filehandle->read (self->archive_filehandle, &pak_header, sizeof (pak_header)) != sizeof (pak_header))
	{
		VERBOSE_PRINT ("pak_scan_quake(): Failed to fetch header\n");
		return;
	}

	if (memcmp(pak_header.id, "PACK", 4))
	{
		VERBOSE_PRINT ("pak_scan_quake(): Header signature is incorrect\n");
		return;
	}

	pak_header.offset = uint32_little (pak_header.offset);
	pak_header.size = uint32_little (pak_header.size);
	num_files = pak_header.size / sizeof(struct PAK_quake_file_t);

	DEBUG_PRINT ("pak_scan_quake(): pak_header.offset=%"PRIu32"\n", pak_header.offset);
	DEBUG_PRINT ("pak_scan_quake(): pak_header.size=%"PRIu32"\n", pak_header.size);
	DEBUG_PRINT ("pak_scan_quake(): pak_header.offset=%d\n", num_files);

	if (self->archive_filehandle->seek_set (self->archive_filehandle, pak_header.offset))
	{
		VERBOSE_PRINT ("pak_scan_quake(): Failed to seek to records\n");
		return;
	}

	filesize = self->archive_filehandle->filesize (self->archive_filehandle);

	for (i = 0; i < num_files; i++)
	{
		if (self->archive_filehandle->read (self->archive_filehandle, &pak_file, sizeof (pak_file)) != sizeof (pak_file))
		{
			VERBOSE_PRINT ("pak_scan_quake(): Failed to read record %d\n", i + 1);
			return;
		}

		pak_file.offset = uint32_little (pak_file.offset);
		pak_file.size = uint32_little (pak_file.size);

		if (!memchr (pak_file.filename, 0, sizeof (pak_file.filename)))
		{
			VERBOSE_PRINT ("pak_scan_quake(): Filename not terminated for record %d\n", i + 1);
			return;
		}
		if (pak_file.offset > filesize)
		{
			VERBOSE_PRINT ("pak_scan_quake(): Offset out of range for record %d\n", i + 1);
			continue;
		}
		if (((uint64_t)pak_file.offset + pak_file.size) > filesize)
		{
			VERBOSE_PRINT ("pak_scan_quake(): Size out of range for record %d\n", i + 1);
			continue;
		}
		pak_instance_add (self, pak_file.filename, pak_file.size, pak_file.offset);
	}
}

static void pak_scan_westwood (struct pak_instance_t *self)
{
	uint32_t prevoffset = 0;
	int records_n = 0;
	uint64_t filesize = self->archive_filehandle->filesize (self->archive_filehandle);
	int superbreak = 1;
	char filename[64]; /* normally should not exceed 8.3, and no directories */

	DEBUG_PRINT ("pak_scan_westwood()\n");

	while (superbreak)
	{
		int namelen;
		uint32_t nextoffset;

		if (ocpfilehandle_read_uint32_le (self->archive_filehandle, &nextoffset))
		{
			VERBOSE_PRINT ("pak_scan_westwood(): Failed to fetch offset at index %d\n", records_n);
			superbreak = 0;
			nextoffset = 0;
		} else if (nextoffset == 0)
		{
			VERBOSE_PRINT ("pak_scan_westwood(): PAK Version 2/3 list termination encountered\n");
			superbreak = 0;
			//nextoffset = 0;
		} else if ((self->archive_filehandle->getpos (self->archive_filehandle) - 1) == nextoffset)
		{
			VERBOSE_PRINT ("pak_scan_westwood(): PAK Version 1 list termination encountered\n");
			superbreak = 0;
			nextoffset = 0;
		} else if (records_n && (nextoffset < prevoffset))
		{
			VERBOSE_PRINT ("pak_scan_westwood(): PAK Version 1 decrease termination encountered\n");
			superbreak = 0;
			nextoffset = 0;
			break;
		} else if (nextoffset > filesize)
		{
			VERBOSE_PRINT ("pak_scan_westwood(): PAK Version 1 garbage termination encountered\n");
			superbreak = 0;
			nextoffset = 0;
		} else {
			DEBUG_PRINT ("pak_scan_westwood() fetch offset %"PRIu32"\n", nextoffset);
		}

		if (records_n)
		{
			uint32_t thisfilesize = (nextoffset ? nextoffset : filesize) - prevoffset;
			pak_instance_add (self, filename, thisfilesize, prevoffset);
		}

		for (namelen = 0;;namelen++)
		{

			char c;
			if (self->archive_filehandle->read(self->archive_filehandle, &c, 1) != 1)
			{
				VERBOSE_PRINT ("pak_scan_westwood(): Failed to fetch filename character at index %d.%d\n", records_n, namelen);
				superbreak = 0;
				break;
			}
			if ((!namelen) && (!c))
			{
				VERBOSE_PRINT ("pak_scan_westwood(): Filename too short for index %d\n", records_n);
				superbreak = 0;
				break;
			}
			if (((namelen+1) == sizeof (filename)) && c)
			{
				VERBOSE_PRINT ("pak_scan_westwood(): Filename too long for index %d\n", records_n);
				superbreak = 0;
				break;
			}
			if (isalnum(c) || (c=='.') || (c == 0))
			{
				filename[namelen] = c;
				if (c == 0)
				{
					break;
				}
			} else {
				VERBOSE_PRINT ("pak_scan_westwood(): Filename has invalid character at index %d\n", records_n);
				superbreak = 0;
				break;
			}
		}
		prevoffset = nextoffset;
		records_n++;
	}
}

static void pak_scan (struct pak_instance_t *self)
{
	char buffer[4];
	DEBUG_PRINT ("pak_scan()\n");
	pak_io_ref (self);
	if (!self->archive_filehandle)
	{
		pak_io_unref (self);
		VERBOSE_PRINT ("pak_scan() Failed to open archive\n");
		return;
	}

	self->archive_filehandle->seek_set (self->archive_filehandle, 0);
	if (self->archive_filehandle->read (self->archive_filehandle, buffer, 4) != 4)
	{
		VERBOSE_PRINT ("pak_scan() Failed to read signature\n");
		return;
	}
	self->archive_filehandle->seek_set (self->archive_filehandle, 0);

	if (memcmp (buffer, "PACK", 4))
	{
		pak_scan_westwood (self);
	} else {
		pak_scan_quake (self);
	}

	pak_io_unref (self);
}
