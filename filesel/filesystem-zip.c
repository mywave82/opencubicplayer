/* OpenCP Module Player
 * copyright (c) 2020-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress ZIP files zLib for deflate. Other decompression
 * methods have been made from scratch.
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
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "adbmeta.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-zip.h"

#if defined(ZIP_DEBUG) || defined(ZIP_VERBOSE)
static int do_zip_debug_print=1;
#endif

#ifdef ZIP_DEBUG
#define DEBUG_PRINT(...) do { if (do_zip_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#ifdef ZIP_VERBOSE
#define VERBOSE_PRINT(...) do { if (do_zip_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define VERBOSE_PRINT(...) {}
#endif

#include "filesystem-zip-headers.c"
#include "zip-bzip2.c"
#include "zip-expand.c"
#include "zip-explode.c"
#include "zip-inflate.c"
#include "zip-unshrink.c"

#define MAX_SIZE_OF_CENTRAL_DIRECTORY (16*1024*1024)
#define MAX_NUMBER_OF_DISKS 1000

struct zip_instance_t;

struct zip_instance_dir_t
{
	struct ocpdir_t              head;
	struct zip_instance_t       *owner;
	uint32_t                     dir_parent; /* used for making blob */
	uint32_t                     dir_next;
	uint32_t                     dir_child;
	uint32_t                     file_child;

	char                        *orig_full_dirpath; /* if encoding deviates from UTF-8 */
	int                          orig_FlaggedUTF8;
};

struct zip_instance_file_t
{
	struct ocpfile_t             head;
	struct zip_instance_t       *owner;
	uint32_t                     dir_parent; /* used for making blob */
	uint32_t                     file_next;

	uint64_t                     uncompressed_filesize;

	uint64_t                     compressed_filesize;
	uint64_t                     compressed_fileoffset_startdisk; /* points to the PK header!! */
	uint32_t                     compressed_startdisk;

	char                        *orig_full_filepath; /* if encoding deviates from UTF-8, this can be used to recode on the fly too */
	int                          orig_FlaggedUTF8;

	uint32_t                     LocalHeaderSize;

/* Not needed - above points to the PK header which contains this information when needed
	uint16_t                     method;
	uint16_t                     flags;
*/
};

struct zip_instance_filehandle_t
{
	struct ocpfilehandle_t       head;
	struct zip_instance_file_t  *file;
	struct zip_instance_t       *owner;
	int                          error;

	uint64_t                     filepos; /* user-pos */
	uint64_t                     curpos;  /* last known position... if missmatch we need to either skip and/or reset */
	uint8_t                     *in_buffer;
	uint32_t                     in_buffer_size;
	uint32_t                     in_buffer_fill;
	uint8_t                     *in_buffer_readnext;
	uint32_t                     in_buffer_diskpos;

	/* The current location where in_buffer_diskpos points at */
	uint32_t                     CurrentDisk;
	uint64_t                     CurrentDiskOffset;

	struct zip_bzip2_t          *bzip2_io;
	struct zip_unshrink_t       *unshrink_io;
	/* struct zip_expand_t      *expand_io; */
	struct zip_explode_t        *explode_io;
	struct zip_inflate_t        *inflate_io;
};

struct zip_instance_t
{
	struct zip_instance_t       *next;
	int                          ready; /* a complete scan has been performed, use cache instead of file-read */

	struct zip_instance_dir_t  **dirs;
	struct zip_instance_dir_t    dir0;
	int                          dir_fill;
	int                          dir_size;
	struct zip_instance_file_t  *files; /* realloc doesn't break on this, since we do a complete scan before we make the nodes public */
        int                          file_fill;
	int                          file_size;

	struct ocpfile_t            *archive_file;
	struct ocpfilehandle_t      *archive_filehandle;

	iconv_t                     *iconv_handle;
	char                        *charset_override;  /* either NULL, or an override string */

	int                          refcount;
	int                          iorefcount;

	int                          Disks_Ready;     /* has the parent directory been scanned yet? */
	char                        *DiskBaseName;    /* used during parent directory scan */
	int                          DiskBaseNameLen; /* used during parent directory scan */
	uint32_t                     Number_of_this_disk;
	uint32_t                     Total_number_of_disks;
	struct ocpfile_t            *DiskReference[MAX_NUMBER_OF_DISKS];
};

static struct zip_instance_t *zip_root;

static void zip_io_ref (struct zip_instance_t *self); /* count if we need I/O from the .ZIP file */
static void zip_io_unref (struct zip_instance_t *self);

static void zip_dir_ref (struct ocpdir_t *);
static void zip_dir_unref (struct ocpdir_t *);
static ocpdirhandle_pt zip_dir_readdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                 void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
static ocpdirhandle_pt zip_dir_readflatdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *), void *token);
static void zip_dir_readdir_cancel (ocpdirhandle_pt);
static int zip_dir_readdir_iterate (ocpdirhandle_pt);
static struct ocpdir_t *zip_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref);
static struct ocpfile_t *zip_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref);

static void zip_file_ref (struct ocpfile_t *);
static void zip_file_unref (struct ocpfile_t *);
static struct ocpfilehandle_t *zip_file_open (struct ocpfile_t *);
static uint64_t zip_file_filesize (struct ocpfile_t *);
static int zip_file_filesize_ready (struct ocpfile_t *);

static void zip_filehandle_ref (struct ocpfilehandle_t *);
static void zip_filehandle_unref (struct ocpfilehandle_t *);
static int zip_filehandle_seek_set (struct ocpfilehandle_t *, int64_t pos);
static uint64_t zip_filehandle_getpos (struct ocpfilehandle_t *);
static int zip_filehandle_eof (struct ocpfilehandle_t *);
static int zip_filehandle_error (struct ocpfilehandle_t *);
static uint64_t zip_filehandle_filesize (struct ocpfilehandle_t *);
static int zip_filehandle_filesize_ready (struct ocpfilehandle_t *);

static void zip_get_default_string (struct ocpdir_t *self, const char **label, const char **key);
static const char *zip_get_byuser_string (struct ocpdir_t *self);
static void zip_set_byuser_string (struct ocpdir_t *self, const char *byuser);
static char **zip_get_test_strings(struct ocpdir_t *self);

static void zip_translate_prepare (struct zip_instance_t *self);
static void zip_translate (struct zip_instance_t *self, char *src, char **buffer, int *buffersize);
static void zip_translate_complete (struct zip_instance_t *self);

static const struct ocpdir_charset_override_API_t zip_charset_API =
{
	zip_get_default_string,
	zip_get_byuser_string,
	zip_set_byuser_string,
	zip_get_test_strings
};

static uint32_t zip_instance_add (struct zip_instance_t *self,
                                  char                  *Filename,
                                  const int              Filename_FlaggedUTF8,
                                  const uint64_t         CompressedSize,
                                  const uint64_t         UncompressedSize,
                                  const uint32_t         DiskNumber,
                                  const uint64_t         OffsetLocalHeader);

static void zip_instance_decode_blob (struct zip_instance_t *self, uint8_t *blob, uint32_t blobsize)
{
	uint8_t *eos;

	if (blobsize < 4)
	{
		return;
	}
	self->Total_number_of_disks =
		              ((uint64_t)(blob[ 3]) << 24) |
		              ((uint64_t)(blob[ 2]) << 16) |
		              ((uint64_t)(blob[ 1]) << 8) |
		              ((uint64_t)(blob[ 0]));
	blob += 4;
	blobsize -= 4;

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

	zip_translate_prepare (self);

	while (blobsize >= 31)
	{
		uint64_t uncompressed_filesize =
		              ((uint64_t)(blob[ 7]) << 56) |
		              ((uint64_t)(blob[ 6]) << 48) |
		              ((uint64_t)(blob[ 5]) << 40) |
		              ((uint64_t)(blob[ 4]) << 32) |
		              ((uint64_t)(blob[ 3]) << 24) |
		              ((uint64_t)(blob[ 2]) << 16) |
		              ((uint64_t)(blob[ 1]) <<  8) |
		              ((uint64_t)(blob[ 0])      );
		uint64_t compressed_filesize =
		              ((uint64_t)(blob[15]) << 56) |
		              ((uint64_t)(blob[14]) << 48) |
		              ((uint64_t)(blob[13]) << 40) |
		              ((uint64_t)(blob[12]) << 32) |
		              ((uint64_t)(blob[11]) << 24) |
		              ((uint64_t)(blob[10]) << 16) |
		              ((uint64_t)(blob[ 9]) <<  8) |
		              ((uint64_t)(blob[ 8])      );
		uint64_t compressed_fileoffset_startdisk =
		              ((uint64_t)(blob[23]) << 56) |
		              ((uint64_t)(blob[22]) << 48) |
		              ((uint64_t)(blob[21]) << 40) |
		              ((uint64_t)(blob[20]) << 32) |
		              ((uint64_t)(blob[19]) << 24) |
		              ((uint64_t)(blob[18]) << 16) |
		              ((uint64_t)(blob[17]) <<  8) |
		              ((uint64_t)(blob[16])      );
		uint64_t compressed_startdisk =
		              ((uint64_t)(blob[27]) << 24) |
		              ((uint64_t)(blob[26]) << 16) |
		              ((uint64_t)(blob[25]) <<  8) |
		              ((uint64_t)(blob[24])      );
		int orig_FlaggedUTF8 =    blob[28] & 1;

		blob += 29;
		blobsize -= 29;
		eos = memchr (blob, 0, blobsize);
		if (!eos)
		{
			break;
		}

		zip_instance_add (self, (char *)blob, orig_FlaggedUTF8, compressed_filesize, uncompressed_filesize, compressed_startdisk, compressed_fileoffset_startdisk);

		eos++;
		blobsize -= eos - blob;
		blob = eos;
	}
	zip_translate_complete (self);
}

static void zip_instance_encode_blob (struct zip_instance_t *self, uint8_t **blob, uint32_t *blobfill)
{
	uint32_t counter;
	uint32_t blobsize = 0;

	*blobfill = 0;
	*blob = 0;

	{
		uint32_t newsize = *blobfill + 4 + (self->charset_override?strlen(self->charset_override):0) + 1 + 1024;
		uint8_t *temp = realloc (*blob, newsize);
		if (!temp)
		{
			return;
		}
		*blob = temp;
		blobsize = newsize;

		(*blob)[(*blobfill) +  3] = self->Total_number_of_disks >> 24;
		(*blob)[(*blobfill) +  2] = self->Total_number_of_disks >> 16;
		(*blob)[(*blobfill) +  1] = self->Total_number_of_disks >> 8;
		(*blob)[(*blobfill) +  0] = self->Total_number_of_disks;
		*blobfill += 4;

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
		int filenamesize = strlen (self->files[counter].orig_full_filepath);
		/*
		 * sizeof (uncompressed_filesize)           => 8
		 * sizeof (compressed_filesize)             => 8
		 * sizeof (compressed_fileoffset_startdisk) => 8
		 * sizeof (compressed_startdisk)            => 4
		 * filenamesize + zero-termination          => filenamesize + 1
		 * flags                                    => 1
		 */
		if ((8 + 8 + 8 + 4 + filenamesize + 1 + 1 + *blobfill) > blobsize)
		{
			uint32_t newsize = *blobfill + 8 + 8 + 8 + 4 + filenamesize + 1 + 1 + 1024;
			uint8_t *temp = realloc (*blob, newsize);
			if (!temp)
			{
				break;
			}
			*blob = temp;
			blobsize = newsize;
		}

		(*blob)[(*blobfill) +  7] = self->files[counter].uncompressed_filesize >> 56;
		(*blob)[(*blobfill) +  6] = self->files[counter].uncompressed_filesize >> 48;
		(*blob)[(*blobfill) +  5] = self->files[counter].uncompressed_filesize >> 40;
		(*blob)[(*blobfill) +  4] = self->files[counter].uncompressed_filesize >> 32;
		(*blob)[(*blobfill) +  3] = self->files[counter].uncompressed_filesize >> 24;
		(*blob)[(*blobfill) +  2] = self->files[counter].uncompressed_filesize >> 16;
		(*blob)[(*blobfill) +  1] = self->files[counter].uncompressed_filesize >> 8;
		(*blob)[(*blobfill) +  0] = self->files[counter].uncompressed_filesize;

		(*blob)[(*blobfill) + 15] = self->files[counter].compressed_filesize >> 56;
		(*blob)[(*blobfill) + 14] = self->files[counter].compressed_filesize >> 48;
		(*blob)[(*blobfill) + 13] = self->files[counter].compressed_filesize >> 40;
		(*blob)[(*blobfill) + 12] = self->files[counter].compressed_filesize >> 32;
		(*blob)[(*blobfill) + 11] = self->files[counter].compressed_filesize >> 24;
		(*blob)[(*blobfill) + 10] = self->files[counter].compressed_filesize >> 16;
		(*blob)[(*blobfill) +  9] = self->files[counter].compressed_filesize >> 8;
		(*blob)[(*blobfill) +  8] = self->files[counter].compressed_filesize;

		(*blob)[(*blobfill) + 23] = self->files[counter].compressed_fileoffset_startdisk >> 56;
		(*blob)[(*blobfill) + 22] = self->files[counter].compressed_fileoffset_startdisk >> 48;
		(*blob)[(*blobfill) + 21] = self->files[counter].compressed_fileoffset_startdisk >> 40;
		(*blob)[(*blobfill) + 20] = self->files[counter].compressed_fileoffset_startdisk >> 32;
		(*blob)[(*blobfill) + 19] = self->files[counter].compressed_fileoffset_startdisk >> 24;
		(*blob)[(*blobfill) + 18] = self->files[counter].compressed_fileoffset_startdisk >> 16;
		(*blob)[(*blobfill) + 17] = self->files[counter].compressed_fileoffset_startdisk >> 8;
		(*blob)[(*blobfill) + 16] = self->files[counter].compressed_fileoffset_startdisk;

		(*blob)[(*blobfill) + 27] = self->files[counter].compressed_startdisk >> 24;
		(*blob)[(*blobfill) + 26] = self->files[counter].compressed_startdisk >> 16;
		(*blob)[(*blobfill) + 25] = self->files[counter].compressed_startdisk >> 8;
		(*blob)[(*blobfill) + 24] = self->files[counter].compressed_startdisk;

		(*blob)[(*blobfill) + 28] = self->files[counter].orig_FlaggedUTF8 ? 1 : 0;
		strcpy ((char *)(*blob) + 29 + (*blobfill), self->files[counter].orig_full_filepath);

		*blobfill += 29 + filenamesize + 1;
	}
}


static void zip_ensure_disk__callback_file (void *token, struct ocpfile_t *file)
{
	unsigned long long N;
	const char *name = 0;
	struct zip_instance_t *self = (struct zip_instance_t *)token;

	dirdbGetName_internalstr (file->dirdb_ref, &name);
	if (!name)
	{
		return;
	}
	if (strncmp (self->DiskBaseName, name, self->DiskBaseNameLen))
	{
		return;
	}
	if (!strcasecmp (name + self->DiskBaseNameLen, "IP"))
	{
		N = self->Total_number_of_disks - 1;
	} else {
		N = strtoull (name + self->DiskBaseNameLen, 0, 10) - 1;
	}
	if (N >= self->Total_number_of_disks)
	{
		fprintf (stderr, "%d > Total_number_of_disks (%d): %s\n", (int)N, (int)self->Total_number_of_disks, name);
	} else if ((N < (MAX_NUMBER_OF_DISKS - 1)) && (!self->DiskReference[N]))
	{
		file->ref (file);
		self->DiskReference[N] = file;
	}
}

static void zip_ensure_disk__callback_dir  (void *token, struct ocpdir_t *dir)
{
	return;
}

static int zip_ensure_disk (struct zip_instance_t *self, uint32_t Disk)
{
	if (!self->Disks_Ready)
	{
		int N;
		if (self->Total_number_of_disks <= 0)
		{
			return -1;
		}
		if (self->Total_number_of_disks > MAX_NUMBER_OF_DISKS)
		{
			return -1;
		}

		if (self->Total_number_of_disks != 1) /* we only need to scan if there are more than one file in the .ZIP archive */
		{
			ocpdirhandle_pt *handle;
			dirdbGetName_malloc (self->archive_file->dirdb_ref, &self->DiskBaseName);
			if (!self->DiskBaseName)
			{
				return -1;
			}
			self->DiskBaseNameLen = strlen (self->DiskBaseName) - 2;
			self->DiskBaseName[self->DiskBaseNameLen] = 0;
			handle = self->archive_file->parent->readdir_start (self->archive_file->parent, zip_ensure_disk__callback_file, zip_ensure_disk__callback_dir, self);
			if (handle)
			{
				while (self->archive_file->parent->readdir_iterate (handle))
				{

				}
				self->archive_file->parent->readdir_cancel (handle);
				handle = 0;
			}
			free (self->DiskBaseName);
			self->DiskBaseName = 0;
		}

		if (!self->DiskReference[self->Total_number_of_disks-1]) /* this should only be true if we didn't scan for multiple disks above */
		{
			self->archive_file->ref (self->archive_file);
			self->DiskReference[self->Total_number_of_disks-1] = self->archive_file;
		}

		for (N=0; N < self->Total_number_of_disks; N++)
		{
			if (!self->DiskReference[N])
			{
				return -1;
			}
		}
		self->Disks_Ready = 1;
	}

	if (Disk >= self->Total_number_of_disks)
	{
		return -1;
	}
	if (Disk != self->Number_of_this_disk)
	{
		DEBUG_PRINT ("[ZIP] Disk != self->Number_of_this_disk, clearing archive_filehandle\n");
		if (self->archive_filehandle)
		{
			self->archive_filehandle->unref (self->archive_filehandle);
			self->archive_filehandle = 0;
		}
		self->archive_filehandle = self->DiskReference[Disk]->open (self->DiskReference[Disk]);
		self->Number_of_this_disk = Disk;
	}

	return self->archive_filehandle ? 0 : -1;
}

static int zip_next_disk (struct zip_instance_t *self)
{
	return zip_ensure_disk (self, self->Number_of_this_disk + 1);
}

static void zip_io_ref (struct zip_instance_t *self)
{
	DEBUG_PRINT ( " zip_io_ref (old count = %d)\n", self->iorefcount);
	/* Calls to EnsureDisk will open file handler */
	self->iorefcount++;
}

static void zip_io_unref (struct zip_instance_t *self)
{
	DEBUG_PRINT (" zip_io_unref (old count = %d)\n", self->iorefcount);

	self->iorefcount--;
	if (self->iorefcount)
	{
		return;
	}

	if (self->archive_filehandle)
	{
		DEBUG_PRINT (" zip_io_unref => RELEASE archive_filehandle\n");
		self->archive_filehandle->unref (self->archive_filehandle);
		self->archive_filehandle = 0;
	}
	self->Number_of_this_disk = UINT32_MAX;
}

static uint32_t zip_instance_add_create_dir (struct zip_instance_t *self,
                                             const uint32_t         dir_parent,
                                             char                  *Dirpath,
                                             char                  *Dirname,
                                             int                    Filename_FlaggedUTF8)
{
	uint32_t *prev, iter;
	uint32_t dirdb_ref;
	DEBUG_PRINT ("[ZIP] create_dir: \"%s\" \"%s\" %d\n", Dirpath, Dirname, Filename_FlaggedUTF8);

	if (!Filename_FlaggedUTF8)
	{
		char *temp = 0;
		int templen = 0;

		zip_translate (self, Dirname, &temp, &templen);
		dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, temp ? temp : "???", dirdb_use_dir);
		free (temp); temp = 0;
	} else {
		dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, Dirname, dirdb_use_dir);
	}

	if (self->dir_fill == self->dir_size)
	{
		int size = self->dir_size + 16;
		struct zip_instance_dir_t **dirs = realloc (self->dirs, size * sizeof (self->dirs[0]));

		if (!dirs)
		{ /* out of memory */
			dirdbUnref (dirdb_ref, dirdb_use_dir);
			return 0;
		}

		self->dirs = dirs;
		self->dir_size = size;
	}

	self->dirs[self->dir_fill] = malloc (sizeof (*self->dirs[self->dir_fill]));
	if (!self->dirs[self->dir_fill])
	{ /* out of memory */
		dirdbUnref (dirdb_ref, dirdb_use_dir);
		return 0;
	}

	ocpdir_t_fill (&self->dirs[self->dir_fill]->head,
	                zip_dir_ref,
	                zip_dir_unref,
	               &self->dirs[dir_parent]->head,
	                zip_dir_readdir_start,
	                zip_dir_readflatdir_start,
	                zip_dir_readdir_cancel,
	                zip_dir_readdir_iterate,
	                zip_dir_readdir_dir,
	                zip_dir_readdir_file,
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
	self->dirs[self->dir_fill]->orig_FlaggedUTF8 = Filename_FlaggedUTF8;

	prev = &self->dirs[dir_parent]->dir_child;
	for (iter = *prev; iter != UINT32_MAX; iter = self->dirs[iter]->dir_next)
	{
		prev = &self->dirs[iter]->dir_next;
	};
	*prev = self->dir_fill;

	self->dir_fill++;

	return *prev;
}

static uint32_t zip_instance_add_file (struct zip_instance_t *self,
                                       const uint32_t         dir_parent,
                                       char                  *Filepath,
                                       char                  *Filename,
                                       int                    Filename_FlaggedUTF8,
                                       const uint64_t         CompressedSize,
                                       const uint64_t         UncompressedSize,
                                       const uint32_t         DiskNumber,
                                       const uint64_t         OffsetLocalHeader)
{
	uint32_t *prev, iter;
	uint32_t dirdb_ref;

	DEBUG_PRINT ("[ZIP] add_file: %s %s %d\n", Filepath, Filename, Filename_FlaggedUTF8);

	if (self->file_fill == self->file_size)
	{
		int size = self->file_size + 64;
		struct zip_instance_file_t *files = realloc (self->files, size * sizeof (self->files[0]));

		if (!files)
		{
			return UINT32_MAX;
		}

		self->files = files;
		self->file_size = size;
	}

	if (!Filename_FlaggedUTF8)
	{
		char *temp = 0;
		int templen = 0;

		zip_translate (self, Filename, &temp, &templen);
		dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, temp ? temp : "???", dirdb_use_file);
		free (temp); temp = 0;
	} else {
		dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent]->head.dirdb_ref, Filename, dirdb_use_file);
	}

	ocpfile_t_fill (&self->files[self->file_fill].head,
	                 zip_file_ref,
	                 zip_file_unref,
	                &self->dirs[dir_parent]->head,
	                 zip_file_open,
	                 zip_file_filesize,
	                 zip_file_filesize_ready,
	                 0, /* filename_override */
	                 dirdb_ref,
	                 0, /* refcount */
	                 0, /* is_nodetect */
	                 (CompressedSize == UncompressedSize) ?
	                     COMPRESSION_ADD_STORE  (self->archive_file->compression) :
	                     COMPRESSION_ADD_STREAM (self->archive_file->compression) );

	self->files[self->file_fill].owner      = self;
	self->files[self->file_fill].head.refcount   = 0;
	self->files[self->file_fill].dir_parent = dir_parent;
	self->files[self->file_fill].file_next  = UINT32_MAX;
	self->files[self->file_fill].orig_full_filepath = strdup (Filepath);
	self->files[self->file_fill].orig_FlaggedUTF8 = Filename_FlaggedUTF8;
	self->files[self->file_fill].uncompressed_filesize           = UncompressedSize;
	self->files[self->file_fill].compressed_filesize             = CompressedSize;
	self->files[self->file_fill].compressed_fileoffset_startdisk = OffsetLocalHeader;
	self->files[self->file_fill].compressed_startdisk            = DiskNumber;

	prev = &self->dirs[dir_parent]->file_child;
	for (iter = *prev; iter != UINT32_MAX; iter = self->files[iter].file_next)
	{
		prev = &self->files[iter].file_next;
	};
	*prev = self->file_fill;

	self->file_fill++;

	return *prev;
}

static uint32_t zip_instance_add (struct zip_instance_t *self,
                                  char                  *Filepath,
                                  const int              Filepath_FlaggedUTF8,
                                  const uint64_t         CompressedSize,
                                  const uint64_t         UncompressedSize,
                                  const uint32_t         DiskNumber,
                                  const uint64_t         OffsetLocalHeader)
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
			uint32_t dirdb_ref = UINT32_MAX;

			*slash = 0;
			if (strcmp (ptr, ".") && strcmp (ptr, "..") && strlen (ptr)) /* we ignore these entries */
			{
				/* check if we already have this node */
				for (search = 1; search < self->dir_fill; search++)
				{
					if (!strcmp (self->dirs[search]->orig_full_dirpath, Filepath))
					{
						iter = search;
						*slash = '/';
						ptr = slash + 1;
						iter = search;
						dirdbUnref (dirdb_ref, dirdb_use_dir);
						goto again; /* we need a break + continue; */
					}
				}

				/* no hit, create one */
				iter = zip_instance_add_create_dir (self, iter, Filepath, ptr, Filepath_FlaggedUTF8);
				*slash = '/';
				ptr = slash + 1;
				if (iter == 0)
				{
					return UINT32_MAX; /* out of memory.... */
				}
			}
			*slash = '/';
			ptr = slash + 1;
		} else {
			if (strcmp (ptr, ".") && strcmp (ptr, "..") && strlen (ptr)) /* we ignore entries that match these */
			{
				return zip_instance_add_file (self, iter, Filepath, ptr, Filepath_FlaggedUTF8, CompressedSize, UncompressedSize, DiskNumber, OffsetLocalHeader);
			}
			return UINT32_MAX; /* out of memory.... */
		}
	}
}

static int zip_parse_central_directory (struct zip_instance_t *self, uint8_t *searchptr, uint64_t searchlen, uint32_t Total_number_of_entries_in_the_central_directory)
{
	uint32_t filecounter;

        for (filecounter = 0; filecounter < Total_number_of_entries_in_the_central_directory; filecounter++)
        {
		uint64_t  CompressedSize;
		uint64_t  UncompressedSize;
		uint32_t  DiskNumber;
		uint64_t  OffsetLocalHeader;
		uint32_t  CRC;
		char     *Filename = 0;
		int       Filename_FlaggedUTF8;

                int r = central_directory_header (searchptr, searchlen, &CompressedSize, &UncompressedSize, &DiskNumber, &OffsetLocalHeader, &CRC, &Filename, &Filename_FlaggedUTF8);
                if (r != -1)
                {
			DEBUG_PRINT ("[ZIP] just got a central directory header %"PRId32" of %"PRId32" %s\n", filecounter + 1, Total_number_of_entries_in_the_central_directory, Filename);
			zip_instance_add (self, Filename, Filename_FlaggedUTF8, CompressedSize, UncompressedSize, DiskNumber, OffsetLocalHeader);
			free (Filename);
                        searchptr += r;
                        searchlen -= r;
                } else {
			DEBUG_PRINT ("[ZIP] just FAILED a central directory header %"PRId32" of %"PRId32"\n", filecounter + 1, Total_number_of_entries_in_the_central_directory);
			free (Filename);
                        return -1;
                }
        }
	return 0;
}

static int zip_scan (struct zip_instance_t *self)
{
	uint8_t *buffer;

	uint64_t filesize;
	int32_t testpos;
	uint64_t searchpos;
	uint64_t searchlen;

	int64_t r;

	int           found_end_of_central_directory_record = 0;
	int must_have_zip64_end_of_central_directory_record = 0;
	//int     found_zip64_end_of_central_directory_record = 0;
	int     found_zip64_end_of_central_directory_locator = 0;

	uint32_t Number_of_the_disk_with_the_start_of_the_end_of_central_directory;             /* [ZIP64 end of central directory record] 32bit    [End of central directory record] 16bit */
	uint64_t Total_number_of_entries_in_the_central_directory_on_this_disk;                 /* [ZIP64 end of central directory record] 64bit    [End of central directory record] 16bit */
	uint64_t Total_number_of_entries_in_the_central_directory;                              /* [ZIP64 end of central directory record] 64bit    [End of central directory record] 16bit */
	uint64_t Size_of_the_central_directory;                                                 /* [ZIP64 end of central directory record] 64bit    [End of central directory record] 32bit */
	uint64_t Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number; /* [ZIP64 end of central directory record] 64bit    [End of central directory record] 32bit */

	uint32_t Number_of_the_disk_with_the_start_of_the_zip64_end_of_central_directory;
	uint64_t Relative_offset_of_the_zip64_end_of_central_directory_record;

	buffer = malloc (22+65535);
	if (!buffer)
	{
		return -1;
	}

	/* we should not have done ANY I/O until this point */
	assert (!self->archive_filehandle);
	self->archive_filehandle = self->archive_file->open (self->archive_file);
	if (!self->archive_filehandle)
	{
		free (buffer);
		return -1;
	}

	/* we should not have done ANY I/O until this point */
	assert (self->iorefcount == 0);
	self->iorefcount = 1;

	filesize = self->archive_filehandle->filesize (self->archive_filehandle);
	if ((filesize == FILESIZE_STREAM) || (filesize == FILESIZE_ERROR))
	{
		goto abort;
	}

	if (filesize < (22+65535))
	{
		searchlen = filesize;
	} else {
		searchlen = 22+65535;
	}
	searchpos = filesize - searchlen;

	if (self->archive_filehandle->seek_set (self->archive_filehandle, searchpos) < 0)
	{
		goto abort;
	}
	if (self->archive_filehandle->read (self->archive_filehandle, buffer, searchlen) != searchlen)
	{
		goto abort;
	}

	for (testpos = searchlen - 22; testpos > 0; testpos--)
	{
		if (!memcmp (buffer + testpos, "PK\5\6", 4))
		{
			uint16_t t16;
			r = end_of_central_directory_record (buffer + testpos, searchlen - testpos,
			                                     &t16,
			                                     &Number_of_the_disk_with_the_start_of_the_end_of_central_directory,
			                                     &Total_number_of_entries_in_the_central_directory_on_this_disk,
			                                     &Total_number_of_entries_in_the_central_directory,
			                                     &Size_of_the_central_directory,
			                                     &Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number);
			if (r != -1)
			{
				must_have_zip64_end_of_central_directory_record = (r == -2);
				found_end_of_central_directory_record = 1;
				self->Number_of_this_disk = t16;
				self->Total_number_of_disks = t16 + 1; /* this record should be the last disk */
				break;
			}
		}
	}

	if (!found_end_of_central_directory_record)
	{
		VERBOSE_PRINT (" [ZIP] We can not find the \"End of central directory record\" in this file\n");
		goto abort;
	} else {
		DEBUG_PRINT (" [ZIP] end of central directory record located (testpos=%"PRId32" Size=%"PRIu64" Offset=0x%"PRIx64")\n", testpos, Size_of_the_central_directory, Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number);
	}

	if (testpos < 20)
	{
		if (testpos + searchpos < 20)
		{
			r = -1;
		} else {
			searchpos = searchpos + testpos - 20;
			searchlen = 20;
			if ((self->archive_filehandle->seek_set (self->archive_filehandle, searchlen) < 0) ||
			    (self->archive_filehandle->read (self->archive_filehandle, buffer, searchlen) != searchlen))
			{
				goto abort;
			}
			r = zip64_end_of_central_directory_locator (buffer, 20,
		                                                    &Number_of_the_disk_with_the_start_of_the_zip64_end_of_central_directory,
		                                                    &Relative_offset_of_the_zip64_end_of_central_directory_record,
		                                                    &self->Total_number_of_disks); /* we overwrite estimate from "End of central directory record" */
		}
	} else {
		r = zip64_end_of_central_directory_locator (buffer + testpos - 20, 20,
		                                            &Number_of_the_disk_with_the_start_of_the_zip64_end_of_central_directory,
		                                            &Relative_offset_of_the_zip64_end_of_central_directory_record,
		                                            &self->Total_number_of_disks); /* we overwrite estimate from "End of central directory record" */
	}

	if (r != -1)
	{
		found_zip64_end_of_central_directory_locator = 1;
		self->Number_of_this_disk = self->Total_number_of_disks - 1;
	}
	if (!found_zip64_end_of_central_directory_locator)
	{
		if (must_have_zip64_end_of_central_directory_record)
		{
			printf ("[ZIP] We can not find the \"Zip64 end of central directory locator\" in this file\n");
			goto abort;
		}
		goto start_central_directory_record;
	}

	/* Relative_offset_of_the_zip64_end_of_central_directory_record points bytes into the disk in question */
	{
		if (zip_ensure_disk (self, Number_of_the_disk_with_the_start_of_the_zip64_end_of_central_directory) ||
		    self->archive_filehandle->seek_set (self->archive_filehandle, Relative_offset_of_the_zip64_end_of_central_directory_record) ||
		   (self->archive_filehandle->read (self->archive_filehandle, buffer, 56) != 56) )
		{
			goto abort;
		}

		r = zip64_end_of_central_directory_record (buffer, 56,
		                                           &Number_of_the_disk_with_the_start_of_the_end_of_central_directory,
		                                           &Total_number_of_entries_in_the_central_directory_on_this_disk,
		                                           &Total_number_of_entries_in_the_central_directory,
		                                           &Size_of_the_central_directory,
		                                           &Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number);
		if (r != -1)
		{
			//found_zip64_end_of_central_directory_record = 1;
		} else {
			printf ("We can not find the \"Zip64 end of central directory record\" in this file\n");
			goto abort;
		}
	}

start_central_directory_record:

	free (buffer); buffer = 0;
	if (Size_of_the_central_directory > MAX_SIZE_OF_CENTRAL_DIRECTORY)
	{
		goto abort;
	}
	buffer = malloc (Size_of_the_central_directory);

	if (zip_ensure_disk (self, Number_of_the_disk_with_the_start_of_the_end_of_central_directory) ||
	    self->archive_filehandle->seek_set (self->archive_filehandle, Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number))
	{
		goto abort;
	}
	for (searchpos = 0; searchpos < Size_of_the_central_directory ;)
	{
		r = self->archive_filehandle->read (self->archive_filehandle, buffer + searchpos, Size_of_the_central_directory - searchpos);
		if (r < 0)
		{
			goto abort;
		}
		searchpos += r;
		if (searchpos != Size_of_the_central_directory)
		{
			if (zip_next_disk (self))
			{
				goto abort;
			}
		}
	}

	if (zip_parse_central_directory (self, buffer, Size_of_the_central_directory, Total_number_of_entries_in_the_central_directory))
	{
		goto abort;
	}

	free (buffer);
	zip_io_unref (self);

	/* disable charset API if all entries as UTF-8 */
	{
		int i;
		int nonUTF8 = 0;

		for (i=1; i < self->dir_fill; i++)
		{
			if (!self->dirs[i]->orig_FlaggedUTF8)
			{
				nonUTF8 = 1;
				break;
			}
		}
		if (!nonUTF8)
		{
			for (i=0; i < self->file_fill; i++)
			{
				if (!self->files[i].orig_FlaggedUTF8)
				{
					nonUTF8 = 1;
					break;
				}
			}
		}
		if (!nonUTF8)
		{ /* disable the charset API, since we have no non-UTF8 entries */
			self->dirs[0]->head.charset_override_API = 0;
		}
	}

	{
		uint8_t *metadata = 0;
		uint32_t metadatasize = 0;
		const char *filename = 0;

		zip_instance_encode_blob (self, &metadata, &metadatasize);
		dirdbGetName_internalstr (self->archive_file->dirdb_ref, &filename);
		adbMetaAdd (filename, self->archive_file->filesize (self->archive_file), "ZIP", metadata, metadatasize);
		free (metadata);
	}
	return 0;
abort:
	free (buffer);
	zip_io_unref (self);
	return 1;
}

static struct ocpdir_t *zip_check (const struct ocpdirdecompressor_t *self, struct ocpfile_t *file, const char *filetype)
{
	struct zip_instance_t *iter;
#ifdef ZIP_DEBUG
	char *fullpath;
#endif

	if (strcasecmp (filetype, ".zip"))
	{
		return 0;
	}

#ifdef ZIP_DEBUG
	dirdbGetFullname_malloc (file->dirdb_ref, &fullpath, 0);
	DEBUG_PRINT ("[ZIP %s] filetype (%s) matches .zip\n", fullpath, filetype);
	free (fullpath);
#endif

	/* Check the cache for an active instance */
	for (iter = zip_root; iter; iter = iter->next)
	{
		if (iter->dirs[0]->head.dirdb_ref == file->dirdb_ref)
		{
			DEBUG_PRINT ("[ZIP] found a cached entry for the given dirdb_ref => refcount the ROOT entry\n");
			iter->dirs[0]->head.ref (&iter->dirs[0]->head);
			return &iter->dirs[0]->head;
		}
	}

	iter = calloc (sizeof (*iter), 1);
	iter->dir_size = 16;
	iter->dirs = malloc (iter->dir_size * sizeof (iter->dirs[0]));
	iter->dirs[0] = &iter->dir0;

	DEBUG_PRINT( "[ZIP] creating a DIR using the same parent dirdb_ref\n");

	ocpdir_t_fill (&iter->dirs[0]->head,
	                zip_dir_ref,
	                zip_dir_unref,
	                file->parent,
	                zip_dir_readdir_start,
	                zip_dir_readflatdir_start,
	                zip_dir_readdir_cancel,
	                zip_dir_readdir_iterate,
	                zip_dir_readdir_dir,
	                zip_dir_readdir_file,
	                &zip_charset_API,
	                dirdbRef (file->dirdb_ref, dirdb_use_dir),
	                0, /* refcount */
	                1, /* is_archive */
	                0  /* is_playlist */,
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
	iter->Number_of_this_disk = UINT32_MAX;

	iter->next = zip_root;
	iter->iconv_handle = (iconv_t *)-1;
	zip_root = iter;

	/* filesize_ready() logic we ignore, since we must seek to the end of the file */
	{
		const char *filename = 0;
		uint8_t *metadata = 0;
		uint32_t metadatasize = 0;

		dirdbGetName_internalstr (iter->archive_file->dirdb_ref, &filename);
		if (!adbMetaGet (filename, iter->archive_file->filesize (iter->archive_file), "ZIP", &metadata, &metadatasize))
		{
			int i;
			int nonUTF8 = 0;

			DEBUG_PRINT ("[ZIP] We found adbmeta cache\n");
			zip_instance_decode_blob (iter, metadata, metadatasize);

			for (i=1; i < iter->dir_fill; i++)
			{
				if (!iter->dirs[i]->orig_FlaggedUTF8)
				{
					nonUTF8 = 1;
					break;
				}
			}
			if (!nonUTF8)
			{
				for (i=0; i < iter->file_fill; i++)
				{
					if (!iter->files[i].orig_FlaggedUTF8)
					{
						nonUTF8 = 1;
						break;
					}
				}
			}
			if (!nonUTF8)
			{ /* disable the charset API, since we have no non-UTF8 entries */
				iter->dirs[0]->head.charset_override_API = 0;
			}

			free (metadata);
			iter->ready = 1;
		}
	}

	if (!iter->ready)
	{
#ifdef ZIP_DEBUG
		dirdbGetFullname_malloc (file->dirdb_ref, &fullpath, 0);
		DEBUG_PRINT ("[ZIP %s] file is not ready, SCAN IT!\n", fullpath);
		free (fullpath);
#endif
		zip_translate_prepare (iter);
		zip_scan (iter);
		zip_translate_complete (iter);
	}

	DEBUG_PRINT ("[ZIP] finished creating the datastructures, refcount the ROOT entry\n");
	iter->dirs[0]->head.ref (&iter->dirs[0]->head);
	return &iter->dirs[0]->head;
}

static void zip_instance_ref (struct zip_instance_t *self)
{
	DEBUG_PRINT (" ZIP_INSTANCE_REF (old count = %d)\n", self->refcount);
	self->refcount++;
}

static void zip_instance_unref (struct zip_instance_t *self)
{
	uint32_t counter;
	struct zip_instance_t **prev, *iter;

	DEBUG_PRINT (" ZIP_INSTANCE_UNREF (old count = %d)\n", self->refcount);

	self->refcount--;
	if (self->refcount)
	{
		return;
	}

	DEBUG_PRINT (" KILL THE INSTANCE!!!\n");

	self->dirs[0]->head.parent->unref (self->dirs[0]->head.parent);
	self->dirs[0]->head.parent = 0;

	dirdbUnref (self->dirs[0]->head.dirdb_ref, dirdb_use_dir);
	free (self->dirs[0]->orig_full_dirpath);
	for (counter = 1; counter < self->dir_fill; counter++)
	{
		dirdbUnref (self->dirs[counter]->head.dirdb_ref, dirdb_use_dir);
		free (self->dirs[counter]->orig_full_dirpath);
		free (self->dirs[counter]);
	}

	for (counter = 0; counter < self->file_fill; counter++)
	{
		dirdbUnref (self->files[counter].head.dirdb_ref, dirdb_use_file);
		free (self->files[counter].orig_full_filepath);
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

	for (counter = 0; (counter < self->Total_number_of_disks) && (counter < MAX_NUMBER_OF_DISKS); counter++)
	{
		if (self->DiskReference[counter])
		{
			self->DiskReference[counter]->unref (self->DiskReference[counter]);
			self->DiskReference[counter] = 0;
		}
	}

	prev = &zip_root;
	for (iter = zip_root; iter; iter = iter->next)
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

static struct ocpdirdecompressor_t zipdecompressor =
{
	"zip",
	"ZIP archive fileformats",
	zip_check
};

void filesystem_zip_register (void)
{
	register_dirdecompressor (&zipdecompressor);
}

static void zip_dir_ref (struct ocpdir_t *_self)
{
	struct zip_instance_dir_t *self = (struct zip_instance_dir_t *)_self;
	DEBUG_PRINT ("ZIP_DIR_REF (old count = %d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		zip_instance_ref (self->owner);
	}
	self->head.refcount++;
}

static void zip_dir_unref (struct ocpdir_t *_self)
{
	struct zip_instance_dir_t *self = (struct zip_instance_dir_t *)_self;
	DEBUG_PRINT ("ZIP_DIR_UNREF (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		zip_instance_unref (self->owner);
	}
}

struct zip_instance_ocpdirhandle_t
{
	struct zip_instance_dir_t *dir;

	void(*callback_file)(void *token, struct ocpfile_t *);
        void(*callback_dir )(void *token, struct ocpdir_t *);
	void *token;

	/* fast-mode */
	uint32_t flatdir;

	uint32_t nextdir;
	uint32_t nextfile;
};

static ocpdirhandle_pt zip_dir_readdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                      void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct zip_instance_dir_t *self = (struct zip_instance_dir_t *)_self;
	struct zip_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	DEBUG_PRINT ("zip_dir_readdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->callback_dir = callback_dir;
	retval->token = token;

	retval->nextfile = self->file_child;
	retval->nextdir = self->dir_child;
	retval->flatdir = 0;
	DEBUG_PRINT ("\n");

	return retval;
}

static ocpdirhandle_pt zip_dir_readflatdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *), void *token)
{
	struct zip_instance_dir_t *self = (struct zip_instance_dir_t *)_self;
	struct zip_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	DEBUG_PRINT ("zip_dir_readflatdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->callback_dir = 0;
	retval->token = token;

	retval->nextfile = 0;
	retval->nextdir = UINT32_MAX;
	retval->flatdir = 1;
	DEBUG_PRINT ("\n");

	return retval;
}

static void zip_dir_readdir_cancel (ocpdirhandle_pt _self)
{
	struct zip_instance_ocpdirhandle_t *self = (struct zip_instance_ocpdirhandle_t *)_self;

	DEBUG_PRINT ("zip_dir_readdir_cancel, we need to UNREF\n");

	self->dir->head.unref (&self->dir->head);

	free (self);

	DEBUG_PRINT ("\n");
}

static int zip_dir_readdir_iterate (ocpdirhandle_pt _self)
{
	struct zip_instance_ocpdirhandle_t *self = (struct zip_instance_ocpdirhandle_t *)_self;

	if (self->flatdir)
	{
		if (self->nextfile >= self->dir->owner->file_fill)
		{
			return 0;
		}
		self->callback_file (self->token, &self->dir->owner->files[self->nextfile++].head);
		return 1;
	}

	if (self->nextdir != UINT32_MAX)
	{
		self->callback_dir (self->token, &self->dir->owner->dirs[self->nextdir]->head);
		self->nextdir = self->dir->owner->dirs[self->nextdir]->dir_next;
		return 1;
	}
	if (self->nextfile != UINT32_MAX)
	{
		self->callback_file (self->token, &self->dir->owner->files[self->nextfile].head);
		self->nextfile = self->dir->owner->files[self->nextfile].file_next;
		return 1;
	}
	return 0;
}

static struct ocpdir_t *zip_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct zip_instance_dir_t *dir = (struct zip_instance_dir_t *)_self;
	struct zip_instance_t *self = dir->owner;
	int i;
	for (i = 0; i < self->dir_fill; i++)
	{
		if (self->dirs[i]->head.dirdb_ref == dirdb_ref)
		{
			self->dirs[i]->head.ref (&self->dirs[i]->head);
			return &self->dirs[i]->head;
		}
	}
	return 0;
}

static struct ocpfile_t *zip_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct zip_instance_dir_t *dir = (struct zip_instance_dir_t *)_self;
	struct zip_instance_t *self = dir->owner;
	int i;
	for (i = 0; i < self->file_fill; i++)
	{
		if (self->files[i].head.dirdb_ref == dirdb_ref)
		{
			self->files[i].head.ref (&self->files[i].head);
			return &self->files[i].head;
		}
	}
	return 0;
}

static void zip_file_ref (struct ocpfile_t *_self)
{
	struct zip_instance_file_t *self = (struct zip_instance_file_t *)_self;
	DEBUG_PRINT ("zip_file_ref (old value=%d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		zip_instance_ref (self->owner);
	}
	self->head.refcount++;
	DEBUG_PRINT ("\n");
}

static void zip_file_unref (struct ocpfile_t *_self)
{
	struct zip_instance_file_t *self = (struct zip_instance_file_t *)_self;
	DEBUG_PRINT ("zip_file_unref (old value=%d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		zip_instance_unref (self->owner);
	}
	DEBUG_PRINT ("\n");
}

static int zip_filehandle_read_fill_inputbuffer (struct zip_instance_filehandle_t *self)
{
	uint64_t tofetch;
	uint64_t disksize;
	int res;

	DEBUG_PRINT ("[ZIP] zip_filehandle_read_fill_inputbuffer in_buffer empty, try to refill\n");

	if (self->in_buffer_diskpos >= self->file->compressed_filesize)
	{
		self->error = 1;
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_fill_inputbuffer in_buffer ran out of data on disk\n");
		return -1;
	}
next_disk:
	if (zip_ensure_disk (self->owner, self->CurrentDisk) < 0)
	{
		self->error = 1;
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_fill_inputbuffer zip_ensure_disk FAILED\n");
		return -1;
	}

	disksize = self->owner->archive_filehandle->filesize (self->owner->archive_filehandle);
	if ((disksize == FILESIZE_STREAM) || (disksize == FILESIZE_ERROR))
	{
		self->error = 1;
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_fill_inputbuffer filesize() FAILED\n");
		return -1;
	}

	if (self->CurrentDiskOffset >= disksize)
	{
		self->CurrentDisk++;
		self->CurrentDiskOffset = 0;
		goto next_disk;
	}
	if (self->owner->archive_filehandle->seek_set (self->owner->archive_filehandle, self->CurrentDiskOffset) < 0)
	{
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_fill_inputbuffer seek FAILED\n");
		self->error = 1;
		return -1;
	}

	tofetch = self->file->uncompressed_filesize - self->in_buffer_diskpos;
	if (tofetch > self->in_buffer_size)
	{
		tofetch = self->in_buffer_size;
	}
	DEBUG_PRINT ("[ZIP] zip_filehandle_read_fill_inputbuffer read fileoffset=%d fetchsize=%d\n", (int)self->owner->archive_filehandle->getpos (self->owner->archive_filehandle), (int)tofetch);
	res = self->owner->archive_filehandle->read (self->owner->archive_filehandle, self->in_buffer, tofetch);
	if (res < 0)
	{
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_fill_inputbuffer read FAILED\n");
		self->error = 1;
		return -1;
	}
	self->in_buffer_fill = res;
	self->in_buffer_readnext = self->in_buffer;
	self->CurrentDiskOffset += res;

	return 0;
}


static int zip_filehandle_read_stored (struct ocpfilehandle_t *_self, void *dst, int len)
{ /* we ignore in_buffer */
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	int retval = 0;

	if (self->error)
	{
		return -1;
	}

	/* ensure len is within range */
	if (len < 0)
	{
		return -1;
	}
	if ((self->filepos + len) >= self->file->uncompressed_filesize)
	{
		len = self->file->uncompressed_filesize - self->filepos;
	}
	if (len == 0)
	{
		return 0;
	}

	if (self->filepos < self->curpos)
	{ /* we need to reverse-seek */
		/* reset back to start */
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_stored reset, so we can reverse)\n");

		self->curpos = 0;
		self->CurrentDisk = self->file->compressed_startdisk;
		self->CurrentDiskOffset = self->file->compressed_fileoffset_startdisk + self->file->LocalHeaderSize;

		self->in_buffer_diskpos = 0;
		self->in_buffer_fill = 0;
		self->in_buffer_readnext = self->in_buffer;
	}

	while (len)
	{
		/* stored file uses inputbuffer as outputbuffer */
		if (self->in_buffer_fill)
		{
			if (self->curpos < self->filepos)
			{
				uint64_t skiplength = self->filepos - self->curpos;

				DEBUG_PRINT ("[ZIP] zip_filehandle_read_stored fastforward %"PRId64" (expectedpos=%"PRId64" vs actualpos=%"PRId64")\n", skiplength, self->filepos, self->curpos);

				if (skiplength > self->in_buffer_fill)
				{
					skiplength = self->in_buffer_fill;
				}
				self->in_buffer_fill     -= skiplength;
				self->in_buffer_readnext += skiplength;
				self->curpos += skiplength;
			} else {
				uint64_t copylen = len;

				if (copylen > self->in_buffer_fill)
				{
					copylen = self->in_buffer_fill;
				}
				DEBUG_PRINT ("[ZIP] zip_filehandle_read_stored eat %"PRId64"\n", copylen);
#ifdef ZIP_DEBUG
				{
					uint64_t i;
					DEBUG_PRINT ("[%08" PRIx64 "]", self->curpos);
					for (i=0; i < copylen; i++)
					{
						DEBUG_PRINT (" %02x", self->in_buffer_readnext[i]);
					}
					DEBUG_PRINT ("\n");
				}
#endif
				memcpy (dst, self->in_buffer_readnext, copylen);
				len -= copylen;
				dst = (uint8_t *)dst + copylen;
				self->in_buffer_fill      -= copylen;
				self->in_buffer_readnext  += copylen;
				self->curpos += copylen;
				self->filepos += copylen;
				retval += copylen;
			}
			continue;
		}

		if (!self->in_buffer_fill)
		{
			if (zip_filehandle_read_fill_inputbuffer (self))
			{
				self->error = 1;
				return -1;
			}
		}
	}

	return retval;
}

static int zip_filehandle_read_unshrink (struct ocpfilehandle_t *_self, void *dst, int len)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	int retval = 0;

	if (self->error)
	{
		return -1;
	}

	/* ensure len is within range */
	if (len < 0)
	{
		return -1;
	}
	if ((self->filepos + len) >= self->file->uncompressed_filesize)
	{
		len = self->file->uncompressed_filesize - self->filepos;
	}
	if (len == 0)
	{
		return 0;
	}

	if (self->filepos < self->curpos)
	{ /* we need to reverse-seek */
		/* reset back to start */
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_unshrink reset, so we can reverse)\n");

		self->curpos = 0;
		self->CurrentDisk = self->file->compressed_startdisk;
		self->CurrentDiskOffset = self->file->compressed_fileoffset_startdisk + self->file->LocalHeaderSize;
		zip_unshrink_init (self->unshrink_io);
		self->in_buffer_diskpos = 0;
		self->in_buffer_fill = 0;
		self->in_buffer_readnext = self->in_buffer;
	}

	while (len)
	{
		if (self->unshrink_io->out_buffer_fill)
		{
			if (self->curpos < self->filepos)
			{
				uint64_t skiplength = self->filepos - self->curpos;

				DEBUG_PRINT ("[ZIP] zip_filehandle_read_unshrink fastforward %"PRId64" (expectedpos=%"PRId64" vs actualpos=%"PRId64")\n", skiplength, self->filepos, self->curpos);

				if (skiplength > self->unshrink_io->out_buffer_fill)
				{
					skiplength = self->unshrink_io->out_buffer_fill;
				}
				self->unshrink_io->out_buffer_fill     -= skiplength;
				self->unshrink_io->out_buffer_readnext += skiplength;
				self->curpos += skiplength;
			} else {
				uint64_t copylen = len;

				DEBUG_PRINT ("[ZIP] zip_filehandle_read_unshrink try to eat %"PRId64"\n", copylen);

				if (copylen > self->unshrink_io->out_buffer_fill)
				{
					copylen = self->unshrink_io->out_buffer_fill;
				}
				memcpy (dst, self->unshrink_io->out_buffer_readnext, copylen);
				len -= copylen;
				dst = (uint8_t *)dst + copylen;
				self->unshrink_io->out_buffer_fill     -= copylen;
				self->unshrink_io->out_buffer_readnext += copylen;
				self->curpos += copylen;
				self->filepos += copylen;
				retval += copylen;
			}
			continue;
		}

		if (!self->in_buffer_fill)
		{
			if (zip_filehandle_read_fill_inputbuffer (self))
			{
				self->error = 1;
				return -1;
			}
		}

		if (zip_unshrink_feed (self->unshrink_io, *self->in_buffer_readnext) < 0)
		{
			DEBUG_PRINT ("[ZIP] zip_filehandle_read_unshrink feed FAILED\n");
			self->error = 1;
			return -1;
		}
		self->in_buffer_fill--;
		self->in_buffer_readnext++;
	}

	return retval;
}

static int zip_filehandle_read_explode (struct ocpfilehandle_t *_self, void *dst, int len)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	int retval = 0;

	if (self->error)
	{
		return -1;
	}

	/* ensure len is within range */
	if (len < 0)
	{
		return -1;
	}
	if ((self->filepos + len) >= self->file->uncompressed_filesize)
	{
		len = self->file->uncompressed_filesize - self->filepos;
	}
	if (len == 0)
	{
		return 0;
	}

	if (self->filepos < self->curpos)
	{ /* we need to reverse-seek */
		/* reset back to start */
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_explode reset, so we can reverse)\n");

		self->curpos = 0;
		self->CurrentDisk = self->file->compressed_startdisk;
		self->CurrentDiskOffset = self->file->compressed_fileoffset_startdisk + self->file->LocalHeaderSize;
		zip_explode_init (self->explode_io, self->explode_io->tree_literate ? 3 : 2, self->explode_io->K);
		self->in_buffer_diskpos = 0;
		self->in_buffer_fill = 0;
		self->in_buffer_readnext = self->in_buffer;
	}

	while (len)
	{
		if (self->explode_io->out_buffer_fill)
		{
			if (self->curpos < self->filepos)
			{
				uint64_t skiplength = self->filepos - self->curpos;

				DEBUG_PRINT ("[ZIP] zip_filehandle_read_explode fastforward %"PRId64" (expectedpos=%"PRId64" vs actualpos=%"PRId64")\n", skiplength, self->filepos, self->curpos);

				if (skiplength > self->explode_io->out_buffer_fill)
				{
					skiplength = self->explode_io->out_buffer_fill;
				}
				self->explode_io->out_buffer_fill     -= skiplength;
				self->explode_io->out_buffer_readnext += skiplength;
				self->curpos += skiplength;
			} else {
				uint64_t copylen = len;

				DEBUG_PRINT ("[ZIP] zip_filehandle_read_explode try to eat %"PRId64"\n", copylen);

				if (copylen > self->explode_io->out_buffer_fill)
				{
					copylen = self->explode_io->out_buffer_fill;
				}
				memcpy (dst, self->explode_io->out_buffer_readnext, copylen);
				len -= copylen;
				dst = (uint8_t *)dst + copylen;
				self->explode_io->out_buffer_fill -= copylen;
				self->explode_io->out_buffer_readnext += copylen;
				self->curpos += copylen;
				self->filepos += copylen;
				retval += copylen;
			}
			continue;
		}

		if (!self->in_buffer_fill)
		{
			if (zip_filehandle_read_fill_inputbuffer (self))
			{
				self->error = 1;
				return -1;
			}
		}

		if (zip_explode_feed (self->explode_io, *self->in_buffer_readnext) < 0)
		{
			DEBUG_PRINT ("[ZIP] zip_filehandle_read_explode feed FAILED\n");
			self->error = 1;
			return -1;
		}
		self->in_buffer_fill--;
		self->in_buffer_readnext++;
	}

	return retval;
}

static int zip_filehandle_read_inflate (struct ocpfilehandle_t *_self, void *dst, int len)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	int retval = 0;

	if (self->error)
	{
		return -1;
	}

	/* ensure len is within range */
	if (len < 0)
	{
		return -1;
	}
	if ((self->filepos + len) >= self->file->uncompressed_filesize)
	{
		len = self->file->uncompressed_filesize - self->filepos;
	}
	if (len == 0)
	{
		return 0;
	}

	if (self->filepos < self->curpos)
	{ /* we need to reverse-seek */
		/* reset back to start */
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_inflate reset, so we can reverse)\n");

		self->curpos = 0;
		self->CurrentDisk = self->file->compressed_startdisk;
		self->CurrentDiskOffset = self->file->compressed_fileoffset_startdisk + self->file->LocalHeaderSize;
		zip_inflate_done (self->inflate_io);
		if (zip_inflate_init (self->inflate_io))
		{
			self->error = 1;
			return -1;
		}
		self->in_buffer_diskpos = 0;
		self->in_buffer_fill = 0;
		self->in_buffer_readnext = self->in_buffer;
	}

	while (len)
	{
		int64_t res;

		if (self->inflate_io->out_buffer_fill)
		{
			if (self->curpos < self->filepos)
			{
				uint64_t skiplength = self->filepos - self->curpos;

				DEBUG_PRINT ("[ZIP] zip_filehandle_read_inflate fastforward %"PRId64" (expectedpos=%"PRId64" vs actualpos=%"PRId64")\n", skiplength, self->filepos, self->curpos);

				if (skiplength > self->inflate_io->out_buffer_fill)
				{
					skiplength = self->inflate_io->out_buffer_fill;
				}
				self->inflate_io->out_buffer_fill     -= skiplength;
				self->inflate_io->out_buffer_readnext += skiplength;
				self->curpos += skiplength;
			} else {
				uint64_t copylen = len;

				DEBUG_PRINT ("[ZIP] zip_filehandle_read_inflate try to eat %"PRId64"\n", copylen);

				if (copylen > self->inflate_io->out_buffer_fill)
				{
					copylen = self->inflate_io->out_buffer_fill;
				}
				memcpy (dst, self->inflate_io->out_buffer_readnext, copylen);
				len -= copylen;
				dst = (uint8_t *)dst + copylen;
				self->inflate_io->out_buffer_fill     -= copylen;
				self->inflate_io->out_buffer_readnext += copylen;
				self->curpos += copylen;
				self->filepos += copylen;
				retval += copylen;
			}
			continue;
		}
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_inflate digest\n");

		res = zip_inflate_digest (self->inflate_io);
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_inflate res=%d out_buffer_fill=%d\n", (int)res, (int)self->inflate_io->out_buffer_fill);
		if (res < 0)
		{
			self->error = 1;
			return -1;
		} else if (res > 0)
		{
			continue;
		}

		if (!self->in_buffer_fill)
		{
			if (zip_filehandle_read_fill_inputbuffer (self))
			{
				self->error = 1;
				return -1;
			}
		}

		DEBUG_PRINT ("[ZIP] zip_filehandle_read_inflate feed in_buffer_fill=%d\n", (int)self->in_buffer_fill);
		res = zip_inflate_feed (self->inflate_io, self->in_buffer_readnext, self->in_buffer_fill);
		self->in_buffer_fill = 0; /* all the buffers has been given to the beast */
		if (res < 0)
		{
			DEBUG_PRINT ("[ZIP] zip_filehandle_read_inflate feed FAILED\n");
			self->error = 1;
			return -1;
		}
	}

	return retval;
}

static int zip_filehandle_read_bzip2 (struct ocpfilehandle_t *_self, void *dst, int len)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	int retval = 0;

	if (self->error)
	{
		return -1;
	}

	/* ensure len is within range */
	if (len < 0)
	{
		return -1;
	}
	if ((self->filepos + len) >= self->file->uncompressed_filesize)
	{
		len = self->file->uncompressed_filesize - self->filepos;
	}
	if (len == 0)
	{
		return 0;
	}

	if (self->filepos < self->curpos)
	{ /* we need to reverse-seek */
		/* reset back to start */
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_bzip2 reset, so we can reverse)\n");

		self->curpos = 0;
		self->CurrentDisk = self->file->compressed_startdisk;
		self->CurrentDiskOffset = self->file->compressed_fileoffset_startdisk + self->file->LocalHeaderSize;
		zip_bzip2_done (self->bzip2_io);
		if (zip_bzip2_init (self->bzip2_io))
		{
			self->error = 1;
			return -1;
		}
		self->in_buffer_diskpos = 0;
		self->in_buffer_fill = 0;
		self->in_buffer_readnext = self->in_buffer;
	}

	while (len)
	{
		int64_t res;

		if (self->bzip2_io->out_buffer_fill)
		{
			if (self->curpos < self->filepos)
			{
				uint64_t skiplength = self->filepos - self->curpos;

				DEBUG_PRINT ("[ZIP] zip_filehandle_read_bzip2 fastforward %"PRId64" (expectedpos=%"PRId64" vs actualpos=%"PRId64")\n", skiplength, self->filepos, self->curpos);

				if (skiplength > self->bzip2_io->out_buffer_fill)
				{
					skiplength = self->bzip2_io->out_buffer_fill;
				}
				self->bzip2_io->out_buffer_fill     -= skiplength;
				self->bzip2_io->out_buffer_readnext += skiplength;
				self->curpos += skiplength;
			} else {
				uint64_t copylen = len;

				DEBUG_PRINT ("[ZIP] zip_filehandle_read_bzip2 try to eat %"PRId64"\n", copylen);

				if (copylen > self->bzip2_io->out_buffer_fill)
				{
					copylen = self->bzip2_io->out_buffer_fill;
				}
				memcpy (dst, self->bzip2_io->out_buffer_readnext, copylen);
				len -= copylen;
				dst = (uint8_t *)dst + copylen;
				self->bzip2_io->out_buffer_fill     -= copylen;
				self->bzip2_io->out_buffer_readnext += copylen;
				self->curpos += copylen;
				self->filepos += copylen;
				retval += copylen;
			}
			continue;
		}
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_bzip2 digest\n");

		res = zip_bzip2_digest (self->bzip2_io);
		DEBUG_PRINT ("[ZIP] zip_filehandle_read_bzip2 res=%d out_buffer_fill=%d\n", (int)res, (int)self->bzip2_io->out_buffer_fill);
		if (res < 0)
		{
			self->error = 1;
			return -1;
		} else if (res > 0)
		{
			continue;
		}

		if (!self->in_buffer_fill)
		{
			if (zip_filehandle_read_fill_inputbuffer (self))
			{
				self->error = 1;
				return -1;
			}
		}

		DEBUG_PRINT ("[ZIP] zip_filehandle_read_bzip2 feed in_buffer_fill=%d\n", (int)self->in_buffer_fill);
		res = zip_bzip2_feed (self->bzip2_io, self->in_buffer_readnext, self->in_buffer_fill);
		self->in_buffer_fill = 0; /* all the buffers has been given to the beast */
		if (res < 0)
		{
			DEBUG_PRINT ("[ZIP] zip_filehandle_read_bzip2 feed FAILED\n");
			self->error = 1;
			return -1;
		}
	}

	return retval;
}

static struct ocpfilehandle_t *zip_file_open (struct ocpfile_t *_self)
{
	struct zip_instance_file_t *self = (struct zip_instance_file_t *)_self;
	struct zip_instance_filehandle_t *retval;
	uint8_t *buffer;
	int bufferfill;
	int buffersize;
	int64_t r;
	uint16_t GeneralPurposeFlags;
	uint16_t CompressionMethod;
	int (*zip_filehandle_read) (struct ocpfilehandle_t *, void *dst, int len);


	if (zip_ensure_disk (self->owner, self->compressed_startdisk) < 0)
	{
		return 0;
	}

	if (self->owner->archive_filehandle->seek_set (self->owner->archive_filehandle, self->compressed_fileoffset_startdisk) < 0)
	{
		return 0;
	}

	buffersize = 65536;
	buffer = malloc (buffersize);
	bufferfill = self->owner->archive_filehandle->read (self->owner->archive_filehandle, buffer, buffersize);
	if (bufferfill <= 0)
	{
		free (buffer);
		return 0;
	}

	if ((r = local_file_header (buffer, bufferfill,
	                            &GeneralPurposeFlags,
	                            &CompressionMethod)) < 0)
	{
		free (buffer);
		return 0;
	}

	self->LocalHeaderSize = r;

	retval = calloc (sizeof (*retval), 1);

	switch (CompressionMethod)
	{
		case  0: /* stored */
			zip_filehandle_read = zip_filehandle_read_stored;
			break;
		case  1: /* shrunk */
			zip_filehandle_read = zip_filehandle_read_unshrink;
			retval->unshrink_io = calloc (sizeof (*retval->unshrink_io), 1);
			zip_unshrink_init (retval->unshrink_io);
			break;
#if 0
		case  2: /* reduced factor 1 */
		case  3: /* reduced factor 2 */
		case  4: /* reduced factor 3 */
		case  5: /* reduced factor 4 */
			zip_filehandle_read = zip_filehandle_read_expand;
			retval->expand_io = calloc (sizeof (*retval->expand_io), 1);
			zip_expand_init (retval->expand_io, CompressionMethod - 1);
			break;
#endif
		case 6: /* imploded */
			zip_filehandle_read = zip_filehandle_read_explode;
			retval->explode_io = calloc (sizeof (*retval->explode_io), 1);
			zip_explode_init (retval->explode_io, (GeneralPurposeFlags & 0x0004) ? 3 : 2, (GeneralPurposeFlags & 0x0002) ? 8 : 4);
			break;

		case 8: /* deflate */
		case 9:
			zip_filehandle_read = zip_filehandle_read_inflate;
			retval->inflate_io = calloc (sizeof (*retval->inflate_io), 1);
			if (zip_inflate_init (retval->inflate_io))
			{
				free (retval->inflate_io);
				retval->inflate_io = 0;

				free (buffer);
				free (retval);
				return 0;
			}
			break;

		case 12: /* bzip2 */
			zip_filehandle_read = zip_filehandle_read_bzip2;
			retval->bzip2_io = calloc (sizeof (*retval->bzip2_io), 1);

			if (zip_bzip2_init (retval->bzip2_io))
			{
				free (retval->bzip2_io);
				retval->bzip2_io = 0;

				free (buffer);
				free (retval);
				return 0;
			}
			break;

		default:
			free (buffer);
			free (retval);
			return 0;
	}

	if (bufferfill > (self->compressed_filesize + self->LocalHeaderSize))
	{
		bufferfill = self->compressed_filesize + self->LocalHeaderSize;
	}

	ocpfilehandle_t_fill (&retval->head,
	                       zip_filehandle_ref,
	                       zip_filehandle_unref,
	                       _self,
	                       zip_filehandle_seek_set,
	                       zip_filehandle_getpos,
	                       zip_filehandle_eof,
	                       zip_filehandle_error,
	                       zip_filehandle_read,
	                       0, /* ioctl */
	                       zip_filehandle_filesize,
	                       zip_filehandle_filesize_ready,
	                       0, /* filename_override */
	                       dirdbRef (self->head.dirdb_ref, dirdb_use_filehandle),
	                       1 /* refcount */
	);

	zip_io_ref (self->owner);
	zip_instance_ref (self->owner);

	retval->file = self;
	retval->owner = self->owner;
	retval->in_buffer = buffer;
	retval->in_buffer_diskpos = 0;
	retval->in_buffer_size = buffersize;
	retval->in_buffer_fill = bufferfill - self->LocalHeaderSize;
	retval->in_buffer_readnext = retval->in_buffer + self->LocalHeaderSize;
	retval->CurrentDisk       = self->compressed_startdisk;
	retval->CurrentDiskOffset = self->compressed_fileoffset_startdisk + bufferfill;

	DEBUG_PRINT ("We just created a ZIP handle\n");

	return &retval->head;
}

static uint64_t zip_file_filesize (struct ocpfile_t *_self)
{
	struct zip_instance_file_t *self = (struct zip_instance_file_t *)_self;
	return self->uncompressed_filesize;
}

static int zip_file_filesize_ready (struct ocpfile_t *_self)
{
	return 1;
}

static void zip_filehandle_ref (struct ocpfilehandle_t *_self)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	DEBUG_PRINT ("zip_filehandle_ref (old count = %d)\n", self->head.refcount);
	self->head.refcount++;
}

static void zip_filehandle_unref (struct ocpfilehandle_t *_self)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;

	DEBUG_PRINT ("zip_filehandle_unref (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		dirdbUnref (self->head.dirdb_ref, dirdb_use_filehandle);

		zip_io_unref (self->owner);
		zip_instance_unref (self->owner);

		free (self->unshrink_io);
		self->unshrink_io = 0;

		free (self->explode_io);
		self->explode_io = 0;

		if (self->inflate_io)
		{
			zip_inflate_done (self->inflate_io);

			free (self->inflate_io);
			self->inflate_io = 0;
		}

		if (self->bzip2_io)
		{
			zip_bzip2_done (self->bzip2_io);

			free (self->bzip2_io);
			self->bzip2_io = 0;
		}

		free (self->in_buffer);
		self->in_buffer = 0;

		free (self);
	}
	DEBUG_PRINT ("\n");
}

static int zip_filehandle_seek_set (struct ocpfilehandle_t *_self, int64_t pos)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;

	if (pos < 0) return -1;

	if (pos > self->file->uncompressed_filesize) return -1;

	self->filepos = pos;
	self->error = 0;

	return 0;
}

static uint64_t zip_filehandle_getpos (struct ocpfilehandle_t *_self)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	return self->filepos;
}

static int zip_filehandle_eof (struct ocpfilehandle_t *_self)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	return self->filepos >= self->file->uncompressed_filesize;
}

static int zip_filehandle_error (struct ocpfilehandle_t *_self)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	return self->error;
}

static uint64_t zip_filehandle_filesize (struct ocpfilehandle_t *_self)
{
	struct zip_instance_filehandle_t *self = (struct zip_instance_filehandle_t *)_self;
	return self->file->uncompressed_filesize;
}

static int zip_filehandle_filesize_ready (struct ocpfilehandle_t *_self)
{
	return 1;
}

static void zip_get_default_string (struct ocpdir_t *_self, const char **label, const char **key)
{
	//struct zip_instance_dir_t *self = (struct zip_instance_dir_t *)_self;
	*label = "ZIP standard (CP437)";
	*key = "CP437";
}

static const char *zip_get_byuser_string (struct ocpdir_t *_self)
{
	struct zip_instance_dir_t *self = (struct zip_instance_dir_t *)_self;
	return self->owner->charset_override;
}

static void zip_translate_prepare (struct zip_instance_t *self)
{
	const char *target;
	char *temp;
	target = self->charset_override ? self->charset_override : "CP437";

	DEBUG_PRINT ("zip_translate_prepare %s\n", self->charset_override ? self->charset_override : "(NULL) CP437");

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

static void zip_translate_complete (struct zip_instance_t *self)
{
	DEBUG_PRINT ("zip_translate_complete\n");

	if (self->iconv_handle != (iconv_t)-1)
	{
		iconv_close (self->iconv_handle);
		self->iconv_handle = (iconv_t)-1;
	}
}

static void zip_translate (struct zip_instance_t *self, char *src, char **buffer, int *buffersize)
{
	char *temp;
	size_t srclen;

	char *dst = *buffer;
	size_t dstlen = *buffersize;

	DEBUG_PRINT ("zip_translate %s =>", src);

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
				fprintf (stderr, "zip_translate: out of memory\n");
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
			fprintf (stderr, "zip_translate: out of memory\n");
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


static void zip_set_byuser_string (struct ocpdir_t *_self, const char *byuser)
{
	struct zip_instance_dir_t *self = (struct zip_instance_dir_t *)_self;
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
		uint32_t metadatasize = 0;
		const char *filename = 0;

		zip_instance_encode_blob (self->owner, &metadata, &metadatasize);
		dirdbGetName_internalstr (self->owner->archive_file->dirdb_ref, &filename);
		adbMetaAdd (filename, self->owner->archive_file->filesize (self->owner->archive_file), "ZIP", metadata, metadatasize);
		free (metadata);
	}

	zip_translate_prepare (self->owner);

	temp = 0;
	templen = 0;

	for (i=1; i < self->owner->dir_fill; i++)
	{
		zip_translate (self->owner, self->owner->dirs[i]->orig_full_dirpath, &temp, &templen);
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
		zip_translate (self->owner, self->owner->files[i].orig_full_filepath, &temp, &templen);
		if (temp)
		{
			dirdbUnref (self->owner->files[i].head.dirdb_ref, dirdb_use_file);
			self->owner->files[i].head.dirdb_ref = dirdbFindAndRef (
				self->owner->dirs
				[
					self->owner->files[i].dir_parent
				]->head.dirdb_ref, temp, dirdb_use_file);
		}
	}

	free (temp);

	zip_translate_complete (self->owner);
}

static char **zip_get_test_strings(struct ocpdir_t *__self)
{
	struct zip_instance_dir_t *_self = (struct zip_instance_dir_t *)__self;
	struct zip_instance_t *self = _self->owner;
	char **retval;

	int count = 0, i;
	for (i=1; i < self->dir_fill; i++)
	{
		count += !self->dirs[i]->orig_FlaggedUTF8;
	}
	for (i=0; i < self->file_fill; i++)
	{
		count += !self->files[i].orig_FlaggedUTF8;
	}
	retval = calloc (count + 1, sizeof (char *));
	if (!retval)
	{
		return 0;
	}
	count = 0;
	for (i=1; i < self->dir_fill; i++)
	{
		if (!self->dirs[i]->orig_FlaggedUTF8)
		{
			retval[count] = strdup (self->dirs[i]->orig_full_dirpath);
			if (!retval[count])
			{
				return retval;
			}
			count++;
		}
	}
	for (i=0; i < self->file_fill; i++)
	{
		if (!self->files[i].orig_FlaggedUTF8)
		{
			retval[count] = strdup (self->files[i].orig_full_filepath);
			if (!retval[count])
			{
				return retval;
			}
			count++;
		}
	}
	return retval;
}
