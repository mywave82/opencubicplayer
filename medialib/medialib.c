/* OpenCP Module Player
 * copyright (c) 2005-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * MEDIALIBRARY filebrowser
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
 *
 * revision history: (please note changes here)
 *  -ss050430   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "filesel/adbmeta.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-dir-mem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/modlist.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/framelock.h"
#include "stuff/poutput.h"
#include "stuff/utf-8.h"

#define MAX(a,b) ((a)>(b)?(a):(b))

static struct dmDrive *dmMEDIALIB;
static struct ocpdir_mem_t *medialib_root;

struct medialib_source_t
{
	char *path;
	uint32_t dirdb_ref;
};
static struct medialib_source_t *medialib_sources;
static int                       medialib_sources_count;

static struct ocpfile_t    *addfiles; // needs to overlay an dialog above filebrowser, and after that the file is "finished"   Special case of DEVv ?
static struct ocpfile_t    *refreshfiles; // needs to overlay an dialog above filebrowser, and after that the file is "finished"   Special case of DEVv ?
static struct ocpfile_t    *removefiles;  // needs to overlay an dialog above filebrowser, and after that the file is "finished"   Special case of DEVv ?
static struct ocpdir_t      listall;  // complete query
static struct ocpdir_t      search;   // needs to throttle a dialog, before it can complete!! upon listing

static int  medialibAddInit (void **token, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API);
static void medialibAddRun  (void **token, const struct DevInterfaceAPI_t *API);

static int  medialibRefreshInit (void **token, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API);
static void medialibRefreshRun  (void **token, const struct DevInterfaceAPI_t *API);

static int  medialibRemoveInit (void **token, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API);
static void medialibRemoveRun  (void **token, const struct DevInterfaceAPI_t *API);

static void              ocpdir_listall_ref (struct ocpdir_t *self);
static void              ocpdir_listall_unref (struct ocpdir_t *self);
static ocpdirhandle_pt   ocpdir_listall_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
static void              ocpdir_listall_readdir_cancel (ocpdirhandle_pt);
static int               ocpdir_listall_readdir_iterate (ocpdirhandle_pt);
static struct ocpdir_t  *ocpdir_listall_readdir_dir  (struct ocpdir_t *self, uint32_t dirdb_ref);
static struct ocpfile_t *ocpdir_listall_readdir_file (struct ocpdir_t *self, uint32_t dirdb_ref);

static void              ocpdir_search_ref (struct ocpdir_t *self);
static void              ocpdir_search_unref (struct ocpdir_t *self);
static ocpdirhandle_pt   ocpdir_search_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
static void              ocpdir_search_readdir_cancel (ocpdirhandle_pt);
static int               ocpdir_search_readdir_iterate (ocpdirhandle_pt);
static struct ocpdir_t  *ocpdir_search_readdir_dir  (struct ocpdir_t *self, uint32_t dirdb_ref);
static struct ocpfile_t *ocpdir_search_readdir_file (struct ocpdir_t *self, uint32_t dirdb_ref);

static void medialib_decode_blob (uint8_t *blob, uint32_t blobsize)
{
	uint8_t *eos;

	while (blobsize && (eos = memchr (blob, 0, blobsize)))
	{
		struct medialib_source_t *newlist = realloc (medialib_sources, (medialib_sources_count + 1) * sizeof (medialib_sources[0]));
		if (!newlist)
		{ /* out of memory */
			return;
		}
		medialib_sources = newlist;
		medialib_sources[medialib_sources_count].path = strdup ((char *)blob);
		if (!medialib_sources[medialib_sources_count].path)
		{ /* out of memory */
			return;
		}

		medialib_sources[medialib_sources_count].dirdb_ref = dirdbResolvePathWithBaseAndRef(DIRDB_NOPARENT, medialib_sources[medialib_sources_count].path, DIRDB_RESOLVE_DRIVE, dirdb_use_medialib);
		if (medialib_sources[medialib_sources_count].dirdb_ref == DIRDB_NOPARENT)
		{ /* resolve failed */
			free (medialib_sources[medialib_sources_count].path);
			medialib_sources[medialib_sources_count].path = 0;
			continue;
		}
		medialib_sources_count++;
		eos++;
		blobsize -= (eos - blob);
		blob = eos;
	}
}

static void medialib_encode_blob (uint8_t **blob, uint32_t *blobsize)
{
	int i;
	char *ptr;

	*blob = 0;
	*blobsize = 0;

	for (i=0; i < medialib_sources_count; i++)
	{
		*blobsize += strlen (medialib_sources[i].path) + 1;
	}

	if (*blobsize)
	{
		*blob = malloc (*blobsize);
	}
	if (!*blob)
	{ /* catches both empty data, and out-of-memory */
		*blobsize = 0;
		return;
	}

	for (ptr = (char *)*blob, i=0; i < medialib_sources_count; i++)
	{
		strcpy (ptr, medialib_sources[i].path);
		ptr += strlen (medialib_sources[i].path) + 1;
	}
}

static void mlFlushBlob (void)
{
	uint8_t *data = 0;
	uint32_t datasize = 0;
	medialib_encode_blob (&data, &datasize);
	if (datasize)
	{
		adbMetaAdd ("medialib", 1, "ML", data, datasize);
	} else {
		adbMetaRemove ("medialib", 1, "ML");
	}
	free (data);
}

#include "medialib-scan.c"

#include "medialib-add.c"

#include "medialib-refresh.c"

#include "medialib-remove.c"

#include "medialib-listall.c"

#include "medialib-search.c"

static int mlint (const struct configAPI_t *configAPI)
{
	struct ocpdir_t *r;
	unsigned char *data = 0;
	uint32_t datasize = 0;

	medialib_root = ocpdir_mem_alloc (0, "medialib:");
	if (!medialib_root)
	{
		return errAllocMem;
	}
	r = ocpdir_mem_getdir_t (medialib_root);

	dmMEDIALIB=RegisterDrive("medialib:", r, r);

	if (!adbMetaGet ("medialib", 1, "ML", &data, &datasize))
	{
		medialib_decode_blob (data, datasize);
		free (data);
	}

	addfiles = dev_file_create (
		r, /* parent-dir */
		"add.dev",
		"medialib add source",
		"",
		0, /* token */
		medialibAddInit,
		medialibAddRun,
		0, /* Close */
		0  /* Destructor */
	);
	ocpdir_mem_add_file (medialib_root, addfiles);

	refreshfiles = dev_file_create (
		r, /* parent-dir */
		"refresh.dev",
		"medialib refresh source",
		"",
		0, /* token */
		medialibRefreshInit,
		medialibRefreshRun,
		0, /* Close */
		0  /* Destructor */
	);
	ocpdir_mem_add_file (medialib_root, refreshfiles);

	removefiles = dev_file_create (
		r, /* parent-dir */
		"remove.dev",
		"medialib remove source",
		"",
		0, /* token */
		medialibRemoveInit,
		medialibRemoveRun,
		0, /* Close */
		0  /* Destructor */
	);
	ocpdir_mem_add_file (medialib_root, removefiles);

	ocpdir_t_fill (&listall,
	                ocpdir_listall_ref,
	                ocpdir_listall_unref,
	                r,
	                ocpdir_listall_readdir_start,
	                0,
	                ocpdir_listall_readdir_cancel,
	                ocpdir_listall_readdir_iterate,
	                ocpdir_listall_readdir_dir,
	                ocpdir_listall_readdir_file,
	                0,
	                dirdbFindAndRef (r->dirdb_ref, "listall", dirdb_use_dir),
	                0,
	                0,
	                0,
	                COMPRESSION_NONE);
	ocpdir_mem_add_dir (medialib_root, &listall);

	ocpdir_t_fill (&search,
	                ocpdir_search_ref,
	                ocpdir_search_unref,
	                r,
	                ocpdir_search_readdir_start,
	                0,
	                ocpdir_search_readdir_cancel,
	                ocpdir_search_readdir_iterate,
	                ocpdir_search_readdir_dir,
	                ocpdir_search_readdir_file,
	                0,
	                dirdbFindAndRef (r->dirdb_ref, "search", dirdb_use_dir),
	                0,
	                0,
	                0,
	                COMPRESSION_NONE);
	ocpdir_mem_add_dir (medialib_root, &search);

	return errOk;
}

static void mlclose(void)
{
	int i;

	if (!medialib_root)
	{
		return;
	}

	mlSearchClear();

	if (removefiles)
	{
		ocpdir_mem_remove_file (medialib_root, removefiles);
		removefiles->unref (removefiles);
		removefiles = 0;
	}

	if (refreshfiles)
	{
		ocpdir_mem_remove_file (medialib_root, refreshfiles);
		refreshfiles->unref (refreshfiles);
		refreshfiles = 0;
	}

	if (addfiles)
	{
		ocpdir_mem_remove_file (medialib_root, addfiles);
		addfiles->unref (addfiles);
		addfiles = 0;
	}

	ocpdir_mem_remove_dir (medialib_root, &listall);
	dirdbUnref (listall.dirdb_ref, dirdb_use_dir);
	listall.dirdb_ref = DIRDB_NOPARENT;

	ocpdir_mem_remove_dir (medialib_root, &search);
	dirdbUnref (search.dirdb_ref, dirdb_use_dir);
	search.dirdb_ref = DIRDB_NOPARENT;

	for (i=0; i < medialib_sources_count; i++)
	{
		free (medialib_sources[i].path);
		dirdbUnref (medialib_sources[i].dirdb_ref, dirdb_use_medialib);
	}
	free (medialib_sources); medialib_sources = 0;
	medialib_sources_count = 0;

	if (medialib_root)
	{
		struct ocpdir_t *r = ocpdir_mem_getdir_t (medialib_root);
		r->unref (r);
		medialib_root = 0;
	}
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "medialib", .desc = "OpenCP medialib (c) 2005-'25 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 90, .Init = mlint, .Close = mlclose};
