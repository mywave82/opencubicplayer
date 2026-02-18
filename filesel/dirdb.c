/* OpenCP Module Player
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * database for storing a tree of filenames
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


#warning ADB_REF is outdated, should no longer be included

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#ifdef HAVE_PWD_H
# include <pwd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/console.h"
#include "dirdb.h"
#include "mdb.h"
#include "boot/psetting.h"
#include "stuff/compat.h"
#include "stuff/file.h"
#include "stuff/poutput.h"
#include "stuff/utf-16.h"
#include "stuff/utf-8.h"

#ifdef CFHOMEDIR_OVERRIDE
# define CFHOMEDIR CFHOMEDIR_OVERRIDE
#else
# define CFHOMEDIR configAPI.HomePath
#endif

#ifdef CFDATAHOMEDIR_OVERRIDE
# define CFDATAHOMEDIR CFDATAHOMEDIR_OVERRIDE
#else
# define CFDATAHOMEDIR configAPI->DataHomePath
#endif

#ifdef MEASURESTR_UTF8_OVERRIDE
# undef measurestr_utf8
# define measurestr_utf8(x,y) (y)
#endif

struct dirdbEntry
{
	uint32_t parent;

	uint32_t *children;
	uint32_t  children_fill;
	uint32_t  children_size;

	uint32_t mdb_ref;
	char *name; /* we pollute malloc a lot with this */
	int refcount;
#ifdef DIRDB_DEBUG
	int refcount_children;
	int refcount_directories;
	int refcount_files;
	int refcount_filehandles;
	int refcount_drive_resolve;
	int refcount_pfilesel;
	int refcount_medialib;
	int refcount_mdb_medialib;
#endif

	uint32_t newmdb_ref; /* used during scan to find new nodes */
};
static uint32_t tagparentnode = DIRDB_NOPARENT;

struct __attribute__((packed)) dirdbheader
{
	char sig[60];
	uint32_t entries;
};
const char dirdbsigv1[60] = "Cubic Player Directory Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
const char dirdbsigv2[60] = "Cubic Player Directory Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00";

static osfile            *dirdbFile;
static struct dirdbEntry *dirdbData = 0;
static uint32_t dirdbNum = 0;
static int dirdbDirty = 0;

static uint32_t *dirdbRootChildren = 0;
static uint32_t  dirdbRootChildren_fill = 0;
static uint32_t  dirdbRootChildren_size = 0;
static uint32_t *dirdbFreeChildren = 0;
static uint32_t  dirdbFreeChildren_fill = 0;
static uint32_t  dirdbFreeChildren_size = 0;

#define FREE_MINSIZE 128
#define GROW_CHILDREN_0 64
#define GROW_CHILDREN_N 256

#ifdef DIRDB_DEBUG
static void dumpdb_parent(const uint32_t * const children, const uint32_t children_fill, int ident)
{
	uint32_t i;

	for (i = 0; i < children_fill; i++)
	{
		uint32_t iter = children[i];
		int j;

		fprintf(stderr, "0x%08x ", iter);
		assert (dirdbData[iter].name);
		for (j=0;j<ident;j++)
		{
			fprintf(stderr, " ");
		}
		fprintf(stderr, "%s (refcount=%d", dirdbData[iter].name, dirdbData[iter].refcount);
		if (dirdbData[iter].refcount_children)
		{
			fprintf(stderr, "  children=%d", dirdbData[iter].refcount_children);
		}
		if (dirdbData[iter].refcount_directories)
		{
			fprintf(stderr, "  directories=%d", dirdbData[iter].refcount_directories);
		}
		if (dirdbData[iter].refcount_files)
		{
			fprintf(stderr, "  files=%d", dirdbData[iter].refcount_files);
		}
		if (dirdbData[iter].refcount_filehandles)
		{
			fprintf(stderr, "  filehandles=%d", dirdbData[iter].refcount_filehandles);
		}
		if (dirdbData[iter].refcount_drive_resolve)
		{
			fprintf(stderr, "  drive_resolve=%d", dirdbData[iter].refcount_drive_resolve);
		}
		if (dirdbData[iter].refcount_pfilesel)
		{
			fprintf(stderr, "  pfilesel=%d", dirdbData[iter].refcount_pfilesel);
		}
		if (dirdbData[iter].refcount_medialib)
		{
			fprintf(stderr, "  medialib=%d", dirdbData[iter].refcount_medialib);
		}
		if (dirdbData[iter].refcount_mdb_medialib)
		{
			fprintf(stderr, "  mdbs=%d", dirdbData[iter].refcount_mdb_medialib);
		}
		if (dirdbData[iter].mdb_ref != DIRDB_NOPARENT)
		{
			fprintf (stderr, "  mdb=0x%08x", dirdbData[iter].mdb_ref);
		}
		if (dirdbData[iter].newmdb_ref != DIRDB_NOPARENT)
		{
			fprintf (stderr, "  newmdb=0x%08x", dirdbData[iter].newmdb_ref);
		}
		fprintf (stderr, ")\n");

		if (dirdbData[iter].children_fill) /* nothing bad happens without this if, just  slows things down a bit */
		{
			dumpdb_parent(dirdbData[iter].children, dirdbData[iter].children_fill, ident+1);
		}
	}
}

static void dumpdirdb(void)
{
	dumpdb_parent(dirdbRootChildren, dirdbRootChildren_fill, 0);
}
#endif

static int dirdbChildren_cmp (const void *a, const void *b)
{
	const uint32_t *index1 = (const uint32_t *)a;
	const uint32_t *index2 = (const uint32_t *)b;

	return strcmp (dirdbData[*index1].name, dirdbData[*index2].name);
}

int dirdbInit (const struct configAPI_t *configAPI)
{
	struct dirdbheader header;
	uint32_t i;
	int version;
	char *dirdbPath;

	dirdbFreeChildren_size = FREE_MINSIZE;
	dirdbFreeChildren_fill = 0;
	dirdbFreeChildren = malloc (sizeof (dirdbFreeChildren[0]) * dirdbFreeChildren_size);
	if (!dirdbFreeChildren)
	{
		fprintf(stderr, "dirdbInit: malloc() failed\n");
		return 0;
	}

	dirdbPath = malloc ( strlen(CFDATAHOMEDIR) + 11 + 1);
	if (!dirdbPath)
	{
		fprintf(stderr, "dirdbInit: malloc() failed\n");
		return 0;
	}
	sprintf (dirdbPath, "%sCPDIRDB.DAT", CFDATAHOMEDIR);
#ifdef _WIN32
	uint16_t *wdirdbPath = utf8_to_utf16 (dirdbPath);
	fwprintf(stderr, L"Loading %ls .. ", wdirdbPath);
	free (wdirdbPath);
#else
	fprintf(stderr, "Loading %s .. ", dirdbPath);
#endif

	dirdbFile = osfile_open_readwrite (dirdbPath, 1, 0);
	free (dirdbPath);
	dirdbPath = 0;
	if (!dirdbFile)
	{
		return 1;
	}

	if ( osfile_read (dirdbFile, &header, sizeof(header)) != sizeof(header) )
	{
		fprintf(stderr, "Empty\n");
		return 1;
	}
	if (memcmp(header.sig, dirdbsigv1, 60))
	{
		if (memcmp(header.sig, dirdbsigv2, 60))
		{
			fprintf(stderr, "Invalid header\n");
			return 1;
		} else {
			version = 2;
		}
	} else {
		version = 1;
	}
	dirdbNum=uint32_little(header.entries);
	if (!dirdbNum)
		goto endoffile;
	dirdbData = calloc (dirdbNum, sizeof(struct dirdbEntry));
	if (!dirdbData)
	{
		dirdbNum=0;
		goto outofmemory;
	}

	for (i=0; i<dirdbNum; i++)
	{
		uint16_t len;

		if ( osfile_read(dirdbFile, &len, sizeof(uint16_t)) != sizeof(uint16_t) )
		{
			goto endoffile;
		}
		if (len)
		{
			len = uint16_little(len);

			if ( osfile_read (dirdbFile, &dirdbData[i].parent, sizeof(uint32_t)) != sizeof(uint32_t) )
				goto endoffile;
			dirdbData[i].parent = uint32_little(dirdbData[i].parent);

			if ( osfile_read (dirdbFile, &dirdbData[i].mdb_ref, sizeof(uint32_t)) != sizeof(uint32_t) )
				goto endoffile;
			/* If mdb has been reset, we need to clear all references */
			dirdbData[i].mdb_ref = mdbCleanSlate ? DIRDB_NO_MDBREF : uint32_little(dirdbData[i].mdb_ref);
			dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;

			if (version == 2)
			{
				uint32_t discard_adb_ref;
				if ( osfile_read (dirdbFile, &discard_adb_ref, sizeof(uint32_t)) != sizeof(uint32_t) )
					goto endoffile;
			}

			dirdbData[i].name=malloc(len+1);
			if (!dirdbData[i].name)
				goto outofmemory;
			if ( osfile_read (dirdbFile, dirdbData[i].name, len) != len)
			{
				free(dirdbData[i].name);
				dirdbData[i].name = 0;
				goto endoffile;
			}
			dirdbData[i].name[len]=0; /* terminate the string */
			if (dirdbData[i].mdb_ref!=DIRDB_NO_MDBREF)
			{
				dirdbData[i].refcount++;
#ifdef DIRDB_DEBUG
				dirdbData[i].refcount_mdb_medialib++;
#endif
			}
		} else {
			dirdbData[i].parent = DIRDB_NOPARENT;
			dirdbData[i].mdb_ref = DIRDB_NO_MDBREF;
			dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;
			/* name is already NULL due to calloc() */
		}
	}

	if (0)
	{
endoffile:
		fprintf(stderr, "premature EOF\n");
		for (; i<dirdbNum; i++)
		{
			dirdbData[i].parent = DIRDB_NOPARENT;
			dirdbData[i].mdb_ref = DIRDB_NO_MDBREF;
			dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;
		}
	}

	/* Search for orphaned entries (invalid parent), recursive until database appears healthy */
	while (1)
	{
		int changed = 0;
		for (i=0; i<dirdbNum; i++)
		{
			if (dirdbData[i].parent != DIRDB_NOPARENT)
			{
				if (dirdbData[i].parent >= dirdbNum)
				{
					fprintf(stderr, "Invalid parent in a node .. (out of range)\n");
					dirdbData[i].parent = DIRDB_NOPARENT;
					free (dirdbData[i].name);
					dirdbData[i].name = 0;
					changed = 1;
				} else if (!dirdbData[dirdbData[i].parent].name)
				{
					fprintf(stderr, "Invalid parent in a node .. (not in use)\n");
					dirdbData[i].parent = DIRDB_NOPARENT;
					dirdbData[i].mdb_ref = DIRDB_NO_MDBREF;
					dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;
					free (dirdbData[i].name);
					dirdbData[i].name = 0;
					changed = 1;
				}
			}
		}
		if (!changed)
		{
			break;
		}
	}

	/* Reference the parents */
	for (i=0; i<dirdbNum; i++)
	{
		if (dirdbData[i].parent != DIRDB_NOPARENT)
		{
			dirdbData[dirdbData[i].parent].refcount++;
#ifdef DIRDB_DEBUG
			dirdbData[dirdbData[i].parent].refcount_children++;
#endif
		}
	}


	/* collect all the holes into the free area */
	for (i=0; i<dirdbNum; i++)
	{
		if (!dirdbData[i].name)
		{
			dirdbFreeChildren_fill++;
		}
	}
	if (dirdbFreeChildren_fill)
	{
		dirdbFreeChildren_size = (dirdbFreeChildren_fill + (FREE_MINSIZE - 1)) & ~(FREE_MINSIZE - 1);
		dirdbFreeChildren_fill = 0;
		dirdbFreeChildren = malloc (sizeof (dirdbFreeChildren[0]) * dirdbFreeChildren_size);
		if (!dirdbFreeChildren)
		{
			dirdbFreeChildren_size = 0;
			goto outofmemory;
		}
		for (i=0; i<dirdbNum; i++)
		{
			if (!dirdbData[i].name)
			{
				dirdbFreeChildren[dirdbFreeChildren_fill++] = i;
			}
		}
	}

	/* build the children lists
	    step one: count children */
	for (i=0; i<dirdbNum; i++)
	{
		if (dirdbData[i].name)
		{
			if (dirdbData[i].parent == DIRDB_NOPARENT)
			{
				dirdbRootChildren_fill++;
			} else {
				dirdbData[dirdbData[i].parent].children_fill++;
			}
		}
	}
	/* step two: allocate */
	if (dirdbRootChildren_fill)
	{
		dirdbRootChildren_size = (dirdbRootChildren_fill + 127) & ~127;
		dirdbRootChildren_fill = 0;
		dirdbRootChildren = malloc (sizeof (dirdbRootChildren[0]) * dirdbRootChildren_size);
		if (!dirdbRootChildren)
		{
			dirdbRootChildren_size = 0;
			goto outofmemory;
		}
	}
	for (i=0; i<dirdbNum; i++)
	{
		if (dirdbData[i].children_fill)
		{
			dirdbData[i].children_size = (dirdbData[i].children_fill + 63) & ~63;
			dirdbData[i].children_fill = 0;
			dirdbData[i].children = malloc (sizeof (dirdbData[0]) * dirdbData[i].children_size);
			if (!dirdbData[i].children)
			{
				dirdbData[i].children_size = 0;
				goto outofmemory;
			}
		}
	}
	/* step three: add children */
	for (i=0; i<dirdbNum; i++)
	{
		if (dirdbData[i].name)
		{
			if (dirdbData[i].parent == DIRDB_NOPARENT)
			{
				dirdbRootChildren[dirdbRootChildren_fill++] = i;
			} else {
				uint32_t parent = dirdbData[i].parent;
				dirdbData[parent].children[dirdbData[parent].children_fill++] = i;
			}
		}
	}
	/* step four: sort */
	if (dirdbRootChildren_fill > 1)
	{
		qsort (dirdbRootChildren, dirdbRootChildren_fill, sizeof (dirdbRootChildren[0]), dirdbChildren_cmp);
	}
	for (i=0; i<dirdbNum; i++)
	{
		if (dirdbData[i].children_fill > 1)
		{
			qsort (dirdbData[i].children, dirdbData[i].children_fill, sizeof (dirdbData[i].children[0]), dirdbChildren_cmp);
		}
	}

#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif

	osfile_purge_readahead_cache (dirdbFile);

	fprintf(stderr, "Done\n");
	return 1;
outofmemory:
	fprintf(stderr, "out of memory\n");
/*unload:*/
	for (i=0; i<dirdbNum; i++)
	{
		free (dirdbData[i].name);
		free (dirdbData[i].children);
	}
	free (dirdbData);
	dirdbData = 0;
	dirdbNum = 0;

	free (dirdbRootChildren);
	dirdbRootChildren = 0;
	dirdbRootChildren_fill = 0;
	dirdbRootChildren_size = 0;

	free (dirdbFreeChildren);
	dirdbFreeChildren = 0;
	dirdbFreeChildren_fill = 0;
	dirdbFreeChildren_size = 0;

	osfile_purge_readahead_cache (dirdbFile);
	return 0;
}

void dirdbClose(void)
{
	uint32_t i;
	if (dirdbFile)
	{
		osfile_close (dirdbFile);
		dirdbFile = 0;
	}
	if (!dirdbNum)
	{
		return;
	}
	for (i=0; i<dirdbNum; i++)
	{
		free (dirdbData[i].name);
		free (dirdbData[i].children);
	}
	free (dirdbData);
	dirdbData = 0;
	dirdbNum = 0;
	free (dirdbRootChildren);
	free (dirdbFreeChildren);
	dirdbRootChildren = 0;
	dirdbRootChildren_fill = 0;
	dirdbRootChildren_size = 0;
	dirdbFreeChildren = 0;
	dirdbFreeChildren_fill = 0;
	dirdbFreeChildren_size = 0;
}

static uint32_t dirdbBinarySearchName (const uint32_t *children, const uint32_t children_fill, const char *name, int *hit)
{
	uint_fast32_t searchbase = 0, searchlen = children_fill;

	*hit = 0;

	if (!children_fill)
	{
		return 0;
	}

	while (searchlen > 1)
	{
		uint_fast32_t halfmark;
		int result;

		halfmark = searchlen >> 1;

		result = strcmp (name, dirdbData[children[searchbase + halfmark]].name);
		if (!result)
		{
			*hit = 1;
			return searchbase + halfmark;
		} else if (result > 0)
		{
			searchbase += halfmark;
			searchlen -= halfmark;
		} else {
			searchlen = halfmark;
		}
	}

	/* fine-tune the position */
	if (searchbase < children_fill)
	{
		int result = strcmp (name, dirdbData[children[searchbase]].name);
		if (!result)
		{
			*hit = 1;
		} else {
			if (result > 0)
			{
				searchbase++;
			}
		}
	}

	return searchbase;
}

uint32_t dirdbFindAndRef(uint32_t parent, char const *name, enum dirdb_use use)
{
	int duplicate;
	uint32_t i, node;
	uint32_t **children;
	uint32_t *children_fill;
	uint32_t *children_size;

#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbFindAndRef(0x%08x, %s%40s%s, use=%d) ", parent, name?"\"":"", name?name:"NULL", name?"\"":"", use);
#endif

	if (!name)
	{
		fprintf (stderr, "dirdbFindAndRef: name is NULL\n");
		return DIRDB_NOPARENT;
	}
	if (strlen(name) > UINT16_MAX)
	{
		fprintf (stderr, "dirdbFindAndRef: strlen(name) > UINT16_MAX, can not store this in DB\n");
		return DIRDB_NOPARENT;
	}
	if (!name[0])
	{
		fprintf (stderr, "dirdbFindAndRef: zero-length name\n");
		return DIRDB_NOPARENT;
	}

	if ((parent!=DIRDB_NOPARENT)&&((parent>=dirdbNum)||(!dirdbData[parent].name)))
	{
		fprintf (stderr, "dirdbFindAndRef: invalid parent\n");
		return DIRDB_NOPARENT;
	}

	if (!strcmp(name, "."))
	{
		fprintf (stderr, "dirdbFindAndRef: . is not a valid name\n");
		return DIRDB_NOPARENT;
	}

	if (!strcmp(name, ".."))
	{
		fprintf (stderr, "dirdbFindAndRef: .. is not a valid name\n");
		return DIRDB_NOPARENT;
	}
	if (strchr(name, '/'))
	{
		fprintf (stderr, "dirdbFindAndRef: name contains /\n");
		return DIRDB_NOPARENT;
	}

	if (parent == DIRDB_NOPARENT)
	{
		children      = &dirdbRootChildren;
		children_fill = &dirdbRootChildren_fill;
		children_size = &dirdbRootChildren_size;
	} else {
		children = &dirdbData[parent].children;
		children_fill = &dirdbData[parent].children_fill;
		children_size = &dirdbData[parent].children_size;
	}
	i = dirdbBinarySearchName (*children, *children_fill, name, &duplicate);
#ifdef DIRDB_DEBUG
	fprintf (stderr, " dirdbBinarySearchName => i=%u, duplicate=%d\n", (unsigned)i, duplicate);
#endif

	if (duplicate)
	{
		node = (*children)[i];
	} else {
		/* ensure we can fit a child into the parent */
		if (*children_fill >= *children_size)
		{
#ifdef DIRDB_DEBUG
			fprintf (stderr, " Grow parent children by 64 entries\n");
#endif
			uint32_t grow = (*children_size) ? GROW_CHILDREN_N : GROW_CHILDREN_0;
			uint32_t *newchildren = realloc (*children, sizeof (**children) * ((*children_size) + grow));
			if (!newchildren)
			{
				fprintf(stderr, "dirdbFindAndRef: realloc() failed, out of memory\n");
				return DIRDB_NOPARENT;
			}
			(*children) = newchildren;
			(*children_size) += grow;
		}

		/* ensure we have a free node available */
		if (!dirdbFreeChildren_fill)
		{
			struct dirdbEntry *new2;
			int j;
#ifdef DIRDB_DEBUG
			fprintf (stderr, " no dirdbFreeChildren, add some\n");
#endif
			new2=realloc(dirdbData, (dirdbNum + FREE_MINSIZE) * sizeof(struct dirdbEntry));
			if (!new2)
			{
				fprintf(stderr, "dirdbFindAndRef: realloc() failed, out of memory\n");
				return DIRDB_NOPARENT;
			}
			dirdbData = new2;
			memset (dirdbData + dirdbNum, 0, FREE_MINSIZE * sizeof (dirdbData[0]));
			for (j=0;j < FREE_MINSIZE;j++)
			{
				dirdbFreeChildren[dirdbFreeChildren_fill++] = dirdbNum + j;

				dirdbData[dirdbNum + j].mdb_ref = DIRDB_NO_MDBREF;
				dirdbData[dirdbNum + j].newmdb_ref = DIRDB_NO_MDBREF;
				dirdbData[dirdbNum + j].parent = DIRDB_NOPARENT;
			}
			dirdbNum += FREE_MINSIZE;

			/* refresh pointers into dirdbData area */
			if (parent == DIRDB_NOPARENT)
			{
#if 0
				children      = &dirdbRootChildren;
				children_fill = &dirdbRootChildren_fill;
				children_size = &dirdbRootChildren_size;
#endif
			} else {
				children = &dirdbData[parent].children;
				children_fill = &dirdbData[parent].children_fill;
				children_size = &dirdbData[parent].children_size;
			}
		}

		{
			char *namedup = strdup (name);
			if (!namedup)
			{
				fprintf(stderr, "dirdbFindAndRef: strdup() failed, out of memory\n");
				return DIRDB_NOPARENT;
			}

#ifdef DIRDB_DEBUG
			assert (i < (*children_size));
			assert (i <= (*children_fill));
			fprintf (stderr, " Add space in parent children list, (fill=%u, bytes to move=%u)\n", (unsigned)(*children_fill), (unsigned)(sizeof(uint32_t) * ((*children_fill) - i)));
#endif
			memmove ((*children) + i + 1, (*children) + i, sizeof(uint32_t) * ((*children_fill) - i));
			/* grab a node from the free list, move it to the parent children list and populate it */
			node = (*children)[i] = dirdbFreeChildren[--dirdbFreeChildren_fill];
			(*children_fill)++;
			dirdbData[node].name = namedup;
			dirdbData[node].parent = parent;
			dirdbData[node].mdb_ref = DIRDB_NO_MDBREF;
			dirdbData[node].newmdb_ref = DIRDB_NO_MDBREF;
			if (parent != DIRDB_NOPARENT)
			{
				dirdbRef(parent, dirdb_use_children);
			}
			dirdbDirty=1;
		}
	}
	dirdbData[node].refcount++;
#ifdef DIRDB_DEBUG
	switch (use)
	{
		case dirdb_use_children:      dirdbData[node].refcount_children++;      break;
		case dirdb_use_dir:           dirdbData[node].refcount_directories++;   break;
		case dirdb_use_file:          dirdbData[node].refcount_files++;         break;
		case dirdb_use_filehandle:    dirdbData[node].refcount_filehandles++;   break;
		case dirdb_use_drive_resolve: dirdbData[node].refcount_drive_resolve++; break;
		case dirdb_use_pfilesel:      dirdbData[node].refcount_pfilesel++;      break;
		case dirdb_use_medialib:      dirdbData[node].refcount_medialib++;      break;
		case dirdb_use_mdb_medialib:  dirdbData[node].refcount_mdb_medialib++;  break;
	}

	if (!duplicate)
	{
		dumpdirdb();
	}
#endif
	return node;
}

uint32_t dirdbRef(uint32_t node, enum dirdb_use use)
{
#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbRef(0x%08x) ", node);
	switch (use)
	{
		case dirdb_use_children:      fprintf (stderr, "children");      break;
		case dirdb_use_dir:           fprintf (stderr, "directories");   break;
		case dirdb_use_file:          fprintf (stderr, "files");         break;
		case dirdb_use_filehandle:    fprintf (stderr, "filehandles");   break;
		case dirdb_use_drive_resolve: fprintf (stderr, "drive_resolve"); break;
		case dirdb_use_pfilesel:      fprintf (stderr, "pfilesel");      break;
		case dirdb_use_medialib:      fprintf (stderr, "medialib");      break;
		case dirdb_use_mdb_medialib:  fprintf (stderr, "mdb_medialib");  break;
	}
	fprintf (stderr, "\n");
#endif
	if (node == DIRDB_NOPARENT)
	{
		return DIRDB_NOPARENT;
	}
	if ((node>=dirdbNum) || (!dirdbData[node].name))
	{
		fprintf(stderr, "dirdbRef: invalid node\n");
		return DIRDB_NOPARENT;
	}
	/*fprintf(stderr, "+++ %s (%d p=%d)\n", dirdbData[node].name, node, dirdbData[node].parent);*/
	dirdbData[node].refcount++;
#ifdef DIRDB_DEBUG
	switch (use)
	{
		case dirdb_use_children:      dirdbData[node].refcount_children++;      break;
		case dirdb_use_dir:           dirdbData[node].refcount_directories++;   break;
		case dirdb_use_file:          dirdbData[node].refcount_files++;         break;
		case dirdb_use_filehandle:    dirdbData[node].refcount_filehandles++;   break;
		case dirdb_use_drive_resolve: dirdbData[node].refcount_drive_resolve++; break;
		case dirdb_use_pfilesel:      dirdbData[node].refcount_pfilesel++;      break;
		case dirdb_use_medialib:      dirdbData[node].refcount_medialib++;      break;
		case dirdb_use_mdb_medialib:  dirdbData[node].refcount_mdb_medialib++;  break;
	}
#endif
	return node;
}

#define TEMP_SPACE dirdb_use_children
uint32_t dirdbResolvePathWithBaseAndRef(uint32_t base, const char *name, int flags, enum dirdb_use use)
{
	char *segment;
	const char *next;
	const char *split;
	uint32_t retval=base, newretval;
	const char dirsplit = (flags & DIRDB_RESOLVE_WINDOWS_SLASH) ? '\\': '/';

#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbResolvePathWithBaseAndRef(0x%08x, %s%40s%s)\n", base, name?"\"":"", name?name:"NULL", name?"\"":"");
#endif
	if (!name)
	{
		fprintf (stderr, "dirdbResolvePathWithBaseAndRef(): name is NULL\n");
		return DIRDB_NOPARENT;
	}

	segment = malloc (strlen(name)+1); /* We never will need more than this */
	if (!segment)
	{
		fprintf (stderr, "dirdbResolvePathWithBaseAndRef(): malloc() failed\n");
		return DIRDB_NOPARENT;
	}

	next=name;
	if (retval!=DIRDB_NOPARENT)
	{
		/* we unref each-time we change retval, so we need the initial reference */
		dirdbRef(retval, TEMP_SPACE);
	}

	/* test for all special start of the target string */
	do /* so we can use break instead of nasty goto */
	{ /* Test for all special start of the target string */

		/* test for drive */
		if ((flags & DIRDB_RESOLVE_DRIVE) && (*next != dirsplit))
		{
			const char *ptr;
			split = strchr (next, dirsplit);
			if (!split) /* emulate strchrnul */
			{
				split = next + strlen (next);
			}
			if (split == next)
			{
				goto nodrive;
			}
			if (split[-1] != ':') /* last character must be : */
			{
				goto nodrive;
			}
			for (ptr = next; ptr < split - 1;ptr++) /* check that there is only one : in the drive name */
			{
				if (*ptr == ':')
				{
					goto nodrive;
				}
			}
			strncpy (segment, next, split - next);
			segment[split - next] = 0;
#ifdef _WIN32
			/* Force drive letters to upper-case */
			if (segment[1] == ':')
			{
				segment[0] = toupper(segment[0]);
			} else
#endif
			{	/* force lower-case for all protocols */
				char *iter;
				for (iter = segment; *iter; iter++)
				{
					*iter = tolower (*iter);
				}
			}

			if (flags & DIRDB_RESOLVE_WINDOWS_SLASH)
			{
				strreplace (segment, '/', '\\');
			}

			if (*split)
			{
				next = split + 1;
			} else {
				next = split;
			}

			newretval = dirdbFindAndRef (DIRDB_NOPARENT, segment, TEMP_SPACE);
			dirdbUnref (retval, TEMP_SPACE);
			retval = newretval;
			break;
		}
nodrive:

		/* test for ~/ */
		if ((flags & DIRDB_RESOLVE_TILDE_HOME) && (next[0] == '~') && (next[1] == dirsplit))
		{
#ifndef _WIN32
			newretval = dirdbFindAndRef (DIRDB_NOPARENT, "file:", TEMP_SPACE);
			dirdbUnref (retval, TEMP_SPACE);
			retval = newretval;

			newretval = dirdbResolvePathWithBaseAndRef (retval, CFHOMEDIR, 0, TEMP_SPACE); /* we are already at the root */
			dirdbUnref (retval, TEMP_SPACE);
			retval = newretval;
#else
			newretval = dirdbResolvePathWithBaseAndRef (DIRDB_NOPARENT, CFHOMEDIR, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH, TEMP_SPACE);

			dirdbUnref (retval, TEMP_SPACE);
			retval = newretval;
#endif
			next += 2;
			break;
		}

		/* test for ~username */
		if ((flags & DIRDB_RESOLVE_TILDE_USER) && (next[0] == '~'))
		{
#if defined(HAVE_GETPWNAM) && (!defined (_WIN32))
			struct passwd *e;

			split = strchr (next, dirsplit);
			if (!split) /* emulate strchrnul */
			{
				split = next + strlen (next);
			}

			strncpy (segment, next, split - next);
			segment[split - next] = 0;

			if (*split)
			{
				next = split + 1;
			} else {
				next = split;
			}

			newretval = dirdbFindAndRef (DIRDB_NOPARENT, "file:", TEMP_SPACE);
			dirdbUnref (retval, TEMP_SPACE);
			retval = newretval;

			e = getpwnam (segment+1);
			if (!e)
			{
				dirdbUnref (retval, TEMP_SPACE);
				free (segment);
				return DIRDB_NOPARENT;
			}
			newretval = dirdbResolvePathWithBaseAndRef (retval, e->pw_dir, 0, TEMP_SPACE);
			dirdbUnref (retval, TEMP_SPACE);
			retval = newretval;

			break;
#else /* not support on Windows */
			dirdbUnref (retval, TEMP_SPACE);
			free (segment);
			return DIRDB_NOPARENT;
#endif
		}

		/* If the very first character is /, we go to root on the current drive*/
		if (*next == dirsplit)
		{
			while ((retval != DIRDB_NOPARENT) && (dirdbData[retval].parent != DIRDB_NOPARENT))
			{
				newretval = dirdbData[retval].parent;
				dirdbRef (newretval, TEMP_SPACE);
				dirdbUnref (retval, TEMP_SPACE);
				retval = newretval;
			}
			next++;
			break;
		}
	} while (0);

	while (*next)
	{
		if ((split=strchr(next, dirsplit)))
		{
			strncpy(segment, next, split-next);
			segment[split-next]=0;
			next = split + 1;
		} else {
			strcpy(segment, next);
			next = next + strlen (next);
		}

		if (!strlen(segment))
		{ /* empty segment, happens if you have a // in the path */
			continue;
		}

		if (!strcmp (segment, "."))
		{
			continue;
		}

		if (!strcmp (segment, ".."))
		{ /* .. works as long as you are not a drive */
			if ((retval != DIRDB_NOPARENT) && (dirdbData[retval].parent != DIRDB_NOPARENT))
			{
				newretval = dirdbData[retval].parent;
				dirdbRef (newretval, TEMP_SPACE);
				dirdbUnref (retval, TEMP_SPACE);
				retval = newretval;
			}
			continue;
		}

		if (flags & DIRDB_RESOLVE_WINDOWS_SLASH)
		{
			strreplace (segment, '/', '\\');
		}
		newretval=dirdbFindAndRef(retval, segment, TEMP_SPACE);

		if (retval!=DIRDB_NOPARENT)
		{
			dirdbUnref(retval, TEMP_SPACE);
		}

		if (newretval == DIRDB_NOPARENT)
		{
			fprintf (stderr, "dirdbResolvePathWithBaseAndRef: a part of the path failed\n");
			free (segment);
			return DIRDB_NOPARENT;
		}

		retval=newretval;
	}
	free (segment);

/* transfer the refcount to the correct one */
	if (retval != DIRDB_NOPARENT)
	{
		dirdbRef (retval, use);
		dirdbUnref (retval, TEMP_SPACE);
	}

#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif
	return retval;
}
#undef TEMP_SPACE

static int dirdbGetStack (uint32_t node, uint32_t **stack, int *stacklen)
{
	uint32_t iter;
	int size = 0;

	for (iter = node; iter != DIRDB_NOPARENT; iter = dirdbData[iter].parent)
	{
		size++;
	}

	*stack = malloc ((size + 1) * sizeof (uint32_t));
	if (!*stack)
	{
		return -1;
	}
	*stacklen = size;

	(*stack)[size] = DIRDB_NOPARENT;

	for (iter = node; iter != DIRDB_NOPARENT; iter = dirdbData[iter].parent)
	{
		size--;
		(*stack)[size] = iter;
	}

	return 0;
}

char *dirdbDiffPath(uint32_t base, uint32_t node, const int flags)
{
	int retval_size;
	int retval_fill;
	char *retval;
	const char dirsplit = (flags & DIRDB_RESOLVE_WINDOWS_SLASH) ? '\\': '/';

	int       stack_base_fill = 0;
	uint32_t *stack_base_data = 0;

	int       stack_node_fill = 0;
	uint32_t *stack_node_data = 0;

	int i, j;

	if (node == DIRDB_NOPARENT)
	{
		return 0;
	}

	if (base == node)
	{
		return strdup ("./");
	}

	retval_size = 1024; /* keep in mind the \0 terminator */
	retval_fill = 0;
	retval = calloc (1024, 1);

	if (!retval)
	{
		fprintf (stderr, "dirdbDiffPath: out of memory!\n");
		return 0;
	}

	if (dirdbGetStack (base, &stack_base_data, &stack_base_fill))
	{
		free (retval);
		return 0;
	}

	if (dirdbGetStack (node, &stack_node_data, &stack_node_fill))
	{
		free (stack_base_data);
		free (retval);
		return 0;
	}

	/* fast forward ALL common nodes */
	for (i=0; (i < stack_base_fill) && (i < stack_node_fill) && (stack_base_data[i] == stack_node_data[i]); i++)
	{
	};

	if ((i == 1) && (stack_base_fill != 1)) /* same drive, but mode it back all the way to root */
	{
		if ((retval_fill + 2) >= retval_size)
		{
			char *temp;
			retval_size += 1024;
			temp = realloc (retval, retval_size);
			if (!temp)
			{
				fprintf (stderr, "dirdbDiffPath: out of memory!\n");
				free (stack_base_data);
				free (stack_node_data);
				free (retval);
				return 0;
			}
			retval = temp;
		}
		retval[retval_fill++] = dirsplit;
		retval[retval_fill] = 0;
	} else if (i != 0) /* same drive.... i==0 will cause string to contain drive */
	{
		for (j=i; j < stack_base_fill; j++)
		{
			if ((retval_fill + 4) >= retval_size)
			{
				char *temp;
				retval_size += 1024;
				temp = realloc (retval, retval_size);
				if (!temp)
				{
					fprintf (stderr, "dirdbDiffPath: out of memory!\n");
					free (stack_base_data);
					free (stack_node_data);
					free (retval);
					return 0;
				}
				retval = temp;
			}
			retval[retval_fill++] = '.';
			retval[retval_fill++] = '.';
			retval[retval_fill++] = dirsplit;
			retval[retval_fill] = 0;
		}
	}
	for (j=i; j < stack_node_fill; j++)
	{
		const char *tmp = 0;
		int len;

		dirdbGetName_internalstr (stack_node_data[j], &tmp);
		len = strlen (tmp);

		if ((retval_fill + len + 2) >= retval_size)
		{
			char *temp;
			retval_size += len + 2 + 1024;
			temp = realloc (retval, retval_size);
			if (!temp)
			{
				fprintf (stderr, "dirdbDiffPath: out of memory!\n");
				free (stack_base_data);
				free (stack_node_data);
				free (retval);
				return 0;
			}
			retval = temp;
		}
		strcpy (retval + retval_fill, tmp);
		if (flags & DIRDB_DIFF_WINDOWS_SLASH)
		{
			strreplace (retval + retval_fill, '\\', '/');
		}
		retval_fill += len;
		if ( ((j + 1) != stack_node_fill) ||                    /* add / after each node except the last one, */
		     ((i == 0) && (j == 0) && (stack_node_fill == 1)) ) /* unless it is only a drive path.            */

		{
			retval[retval_fill++] = dirsplit;
		}
		retval[retval_fill] = 0;
	}

	free (stack_base_data);
	free (stack_node_data);

	return retval;
}

void dirdbUnref(uint32_t node, enum dirdb_use use)
{
	uint32_t parent, i;
	int hit;

	uint32_t **children;
	uint32_t *children_fill;

#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbUnref(0x%08x) ", node);
	switch (use)
	{
		case dirdb_use_children:      fprintf (stderr, "children");      break;
		case dirdb_use_dir:           fprintf (stderr, "directories");   break;
		case dirdb_use_file:          fprintf (stderr, "files");         break;
		case dirdb_use_filehandle:    fprintf (stderr, "filehandles");   break;
		case dirdb_use_drive_resolve: fprintf (stderr, "drive_resolve"); break;
		case dirdb_use_pfilesel:      fprintf (stderr, "pfilesel");      break;
		case dirdb_use_medialib:      fprintf (stderr, "medialib");      break;
		case dirdb_use_mdb_medialib:  fprintf (stderr, "mdb_medialib");  break;
	}
	fprintf (stderr, "\n");
#endif

	if (node == DIRDB_NOPARENT)
	{
		return;
	}
	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbUnref: invalid node (node %d >= dirdbNum %d)\n", node, dirdbNum);
		abort();
		return;
	}
	if (!dirdbData[node].refcount)
	{
		fprintf (stderr, "dirdbUnref: refcount == 0\n");
		abort();
		return;
	}
	/* fprintf(stderr, "--- %s (%d p=%d)\n", dirdbData[node].name, node, dirdbData[node].parent);*/

	dirdbData[node].refcount--;
#ifdef DIRDB_DEBUG
	switch (use)
	{
		case dirdb_use_children:
			assert (dirdbData[node].refcount_children);
			dirdbData[node].refcount_children--;
			break;
		case dirdb_use_dir:
			assert (dirdbData[node].refcount_directories);
			dirdbData[node].refcount_directories--;
			break;
		case dirdb_use_file:
			assert (dirdbData[node].refcount_files);
			dirdbData[node].refcount_files--;
			break;
		case dirdb_use_filehandle:
			assert (dirdbData[node].refcount_filehandles);
			dirdbData[node].refcount_filehandles--;
			break;
		case dirdb_use_drive_resolve:
			assert (dirdbData[node].refcount_drive_resolve);
			dirdbData[node].refcount_drive_resolve--;
			break;
		case dirdb_use_pfilesel:
			assert (dirdbData[node].refcount_pfilesel);
			dirdbData[node].refcount_pfilesel--;
			break;
		case dirdb_use_medialib:
			assert (dirdbData[node].refcount_medialib);
			dirdbData[node].refcount_medialib--;
			break;
		case dirdb_use_mdb_medialib:
			assert (dirdbData[node].refcount_mdb_medialib);
			dirdbData[node].refcount_mdb_medialib--;
			break;
	}
#endif

	if (dirdbData[node].refcount)
	{
		return;
	}
	/* fprintf(stderr, "DELETE\n");*/
	dirdbDirty=1;
	parent = dirdbData[node].parent;

	if (parent == DIRDB_NOPARENT)
	{
		children      = &dirdbRootChildren;
		children_fill = &dirdbRootChildren_fill;
	} else {
		children = &dirdbData[parent].children;
		children_fill = &dirdbData[parent].children_fill;
	}

	i = dirdbBinarySearchName (*children, *children_fill, dirdbData[node].name, &hit);
	assert (hit);

	memmove ((*children) + i, (*children) + i + 1, sizeof(uint32_t) * ((*children_fill) - i - 1));
	(*children_fill)--;

	if (dirdbFreeChildren_fill >= dirdbFreeChildren_size)
	{
		dirdbFreeChildren_size += 64;
		dirdbFreeChildren = realloc (dirdbFreeChildren, sizeof (uint32_t) * dirdbFreeChildren_size);
		if (!dirdbFreeChildren)
		{
			fprintf (stderr, "dirdbUnref: realloc(dirdbFreeChildren) failed\n");
			abort();
			return;
		}
	}
	dirdbFreeChildren[dirdbFreeChildren_fill++] = node;

	dirdbData[node].parent=DIRDB_NOPARENT;
	free(dirdbData[node].name);
	dirdbData[node].name=0;
	dirdbData[node].mdb_ref=DIRDB_NO_MDBREF; /* this should not be needed */
	dirdbData[node].newmdb_ref=DIRDB_NO_MDBREF; /* this should not be needed */

	assert (dirdbData[node].children_fill == 0);
	free (dirdbData[node].children);
	dirdbData[node].children = 0;
	dirdbData[node].children_size = 0;

#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif

	if (parent!=DIRDB_NOPARENT)
	{
#ifdef DIRDB_DEBUG
		fprintf (stderr, " unref => remove from parent\n");
#endif
		dirdbUnref (parent, dirdb_use_children);
	}
}

void dirdbGetName_internalstr(uint32_t node, const char **name)
{
	*name = 0;
	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbGetName_internalstr: invalid node #1\n");
		return;
	}
	if (!dirdbData[node].name)
	{
		fprintf(stderr, "dirdbGetName_internalstr: invalid node #2\n");
		return;
	}
	*name = dirdbData[node].name;
}

extern void dirdbGetName_malloc(uint32_t node, char **name)
{
	*name = 0;
	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbGetName_malloc: invalid node #1\n");
		return;
	}
	if (!dirdbData[node].name)
	{
		fprintf(stderr, "dirdbGetName_malloc: invalid node #2\n");
		return;
	}
	*name = strdup (dirdbData[node].name);
	if (!*name)
	{
		fprintf (stderr, "dirdbGetName_malloc: strdup() failed\n");
		return;
	}
}

static void dirdbGetFullname_malloc_R(uint32_t node, char *name, int nobase, int backslash)
{
	if (node == DIRDB_NOPARENT)
	{
		return;
	}
	if (dirdbData[node].parent == DIRDB_NOPARENT)
	{
		if (nobase)
		{
			return;
		}
	} else {
		dirdbGetFullname_malloc_R(dirdbData[node].parent, name, nobase, backslash);
		strcat(name, backslash ? "\\" : "/");
	}
	strcat(name, dirdbData[node].name);
}

void dirdbGetFullname_malloc(uint32_t node, char **name, int flags)
{
	int length = 0;
	int iter;

	*name=0;
	if ((node == DIRDB_NOPARENT) || (node >= dirdbNum) || (!dirdbData[node].name))
	{
		fprintf(stderr, "dirdbGetFullname_malloc: invalid node\n");
		return;
	}

	for (iter = node; iter != DIRDB_NOPARENT; iter = dirdbData[iter].parent)
	{
		if (dirdbData[iter].parent == DIRDB_NOPARENT)
		{
			if (flags & DIRDB_FULLNAME_NODRIVE)
			{
				continue;
			}
		} else {
			length++;
		}
		length += strlen (dirdbData[iter].name);
	}
	if (flags&DIRDB_FULLNAME_ENDSLASH)
	{
		length++;
	}

	*name = malloc(length+1);
	if (!*name)
	{
		fprintf (stderr, "dirdbGetFullname_malloc(): malloc() failed\n");
		return;
	}
	(*name)[0] = 0;

	dirdbGetFullname_malloc_R (node, *name, flags&DIRDB_FULLNAME_NODRIVE, flags&DIRDB_FULLNAME_BACKSLASH);

	if (flags&DIRDB_FULLNAME_ENDSLASH)
	{
		strcat(*name, (flags&DIRDB_FULLNAME_BACKSLASH) ? "\\" : "/");
	}

	if (strlen(*name) != length)
	{
		fprintf (stderr, "dirdbGetFullname_malloc: WARNING, length calculation was off. Expected %d, but got %d\n", length, (int)strlen (*name));
	}
}

void dirdbFlush(void)
{
	uint32_t i;
	uint32_t max;
	uint16_t buf16;
	uint32_t buf32;
	struct dirdbheader header;

	if ((!dirdbDirty) || (!dirdbFile))
		return;

	osfile_setpos (dirdbFile, 0);

	for (i=0;i<dirdbNum;i++)
	{
		if (dirdbData[i].name)
		{
			if (!dirdbData[i].refcount)
			{
				fprintf (stderr, "dirdbFlush: node had name, but no refcount...\n");
				dirdbData[i].refcount++;
#ifdef DIRDB_DEBUG
				dirdbData[i].refcount_children++;
#endif
				dirdbUnref(i, dirdb_use_children);
			}
		}
	}

	max=0;
	for (i=0;i<dirdbNum;i++)
		if (dirdbData[i].name)
			max=i+1;

	memcpy(header.sig, dirdbsigv2, sizeof(dirdbsigv2));
	header.entries=uint32_little(max);

	if (osfile_write (dirdbFile, &header, sizeof(header)) != sizeof(header) )
		goto writeerror;

	for (i=0;i<max;i++)
	{
		int len=(dirdbData[i].name?strlen(dirdbData[i].name):0);
		buf16=uint16_little(len);
		if ( osfile_write (dirdbFile, &buf16, sizeof(uint16_t)) != sizeof(uint16_t) )
			goto writeerror;
		if (len)
		{
			buf32=uint32_little(dirdbData[i].parent);
			if ( osfile_write (dirdbFile, &buf32, sizeof(uint32_t)) != sizeof(uint32_t) )
				goto writeerror;
			buf32=uint32_little(dirdbData[i].mdb_ref);
			if ( osfile_write (dirdbFile, &buf32, sizeof(uint32_t)) != sizeof(uint32_t) )
				goto writeerror;
#warning remove-me this used to be ADB_REF
			buf32=0xffffffff; //ADB_REF
			if ( osfile_write (dirdbFile, &buf32, sizeof(uint32_t)) != sizeof(uint32_t) )
				goto writeerror;
			if ( osfile_write (dirdbFile, dirdbData[i].name, len) != len )
				goto writeerror;
		}
	}
	dirdbDirty=0;
	return;
writeerror:
	{}
}

uint32_t dirdbGetParentAndRef (uint32_t node, enum dirdb_use use)
{
	uint32_t retval;
	if ((node>=dirdbNum) || (!dirdbData[node].name))
	{
		fprintf(stderr, "dirdbGetParentAndRef: invalid node\n");
		return DIRDB_NOPARENT;
	}
	retval=dirdbData[node].parent;
	if (retval!=DIRDB_NOPARENT)
	{
		dirdbRef (retval, use);
	}
	return retval;
}

void dirdbTagSetParent(uint32_t node)
{
	uint32_t i;
	if (tagparentnode!=DIRDB_NOPARENT)
	{
		fprintf(stderr, "dirdbTagSetParent: warning, a node was already set as parent\n");
		dirdbUnref(tagparentnode, dirdb_use_mdb_medialib);
		tagparentnode=DIRDB_NOPARENT;
	}

	for (i=0;i<dirdbNum;i++)
	{
		if (dirdbData[i].newmdb_ref != DIRDB_NO_MDBREF)
		{
			dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;
			dirdbUnref (i, dirdb_use_mdb_medialib);
		}
	}

	if ((node != DIRDB_NOPARENT) && ((node>=dirdbNum) || (!dirdbData[node].name)))
	{	/* DIRDB_NOPARENT is legal! */
		fprintf(stderr, "dirdbTagSetParent: invalid node\n");
		return;
	}
	tagparentnode = node;
	if (node != DIRDB_NOPARENT)
	{
		dirdbRef (node, dirdb_use_mdb_medialib);
	}
}

void dirdbMakeMdbRef(uint32_t node, uint32_t mdb_ref)
{
#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbMakeMdbRef(node 0x%08x, mdb_ref 0x%08x)\n", node, mdb_ref);
#endif

	if ((node>=dirdbNum) || (!dirdbData[node].name))
	{
		fprintf(stderr, "dirdbMakeMdbRef: invalid node\n");
		return;
	}
	if (mdb_ref == DIRDB_NO_MDBREF)
	{
		if (dirdbData[node].newmdb_ref != DIRDB_NO_MDBREF)
		{
			dirdbData[node].newmdb_ref = DIRDB_NO_MDBREF;
			dirdbUnref (node, dirdb_use_mdb_medialib);
		}
	} else {
		if (dirdbData[node].newmdb_ref == DIRDB_NO_MDBREF)
		{
			dirdbData[node].newmdb_ref = mdb_ref;
			dirdbRef (node, dirdb_use_mdb_medialib);
		} else {
			/*dirdbUnref(node, dirdb_use_mdb_medialib);*/
			dirdbData[node].newmdb_ref = mdb_ref;
			/*dirdbRef(node, dirdb_use_mdb_medialib);  overkill to unref and re-ref just for the name's sake*/
		}
	}

#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif
}

void dirdbTagCancel(void)
{
	uint32_t i;
	for (i=0;i<dirdbNum;i++)
	{
		if (dirdbData[i].newmdb_ref != DIRDB_NO_MDBREF)
		{
			dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;
			dirdbUnref (i, dirdb_use_mdb_medialib);
		}
	}
	if (tagparentnode == DIRDB_NOPARENT)
	{
		return;
	}
	dirdbUnref (tagparentnode, dirdb_use_mdb_medialib);
	tagparentnode=DIRDB_NOPARENT;
}

static void _dirdbTagRemoveUntaggedAndSubmit(const uint32_t *nodes, const uint32_t nodecount)
{
	uint32_t i;

#ifdef DIRDB_DEBUG
	fprintf (stderr, "_dirdbTagRemoveUntaggedAndSubmit(%p, nodecount=%"PRIu32")\n", nodes, nodecount);
#endif

	for (i = 0; i < nodecount; i++)
	{
		uint32_t node = nodes[i];
		if (dirdbData[node].newmdb_ref == dirdbData[node].mdb_ref)
		{
			if (dirdbData[node].mdb_ref == DIRDB_NO_MDBREF)
			{
				/* probably a dir */
			} else {
				dirdbData[node].newmdb_ref = DIRDB_NO_MDBREF;
				dirdbUnref(node, dirdb_use_mdb_medialib);
			}
		} else {
			if (dirdbData[node].mdb_ref == DIRDB_NO_MDBREF)
			{
				dirdbData[node].mdb_ref = dirdbData[node].newmdb_ref;
				dirdbData[node].newmdb_ref = DIRDB_NO_MDBREF;
				/* no need to unref/ref, since we are
				 * balanced. Since somebody can have
				 * named a file, the same name
				 * a directory used to have, we need
				 * to scan for siblings.
				 */
			} else if (dirdbData[node].newmdb_ref == DIRDB_NO_MDBREF)
			{
				dirdbData[node].mdb_ref = DIRDB_NO_MDBREF;
				dirdbUnref (node, dirdb_use_mdb_medialib);
				/* same as above regarding renaming */
			} else {
				dirdbData[node].mdb_ref = dirdbData[node].newmdb_ref;
				dirdbData[node].newmdb_ref = DIRDB_NO_MDBREF;
				dirdbUnref(node, dirdb_use_mdb_medialib);
			}
		}
		if (dirdbData[node].children_fill)
		{
			_dirdbTagRemoveUntaggedAndSubmit(dirdbData[node].children, dirdbData[node].children_fill);
		}
	}
}

static void _dirdbTagPreserveTree(const uint32_t * const nodes, const uint32_t nodecount)
{
	uint32_t i;

	for (i = 0; i < nodecount; i++)
	{
		uint32_t node = nodes[i];
		if ((dirdbData[node].newmdb_ref != dirdbData[node].mdb_ref) && (dirdbData[node].newmdb_ref == DIRDB_NOPARENT))
		{
			dirdbData[node].newmdb_ref = dirdbData[node].mdb_ref;
			dirdbRef(node, dirdb_use_mdb_medialib);
		}
		_dirdbTagPreserveTree (dirdbData[node].children, dirdbData[node].children_fill);
	}
}

void dirdbTagPreserveTree(uint32_t node)
{
	uint32_t iter;

	/* if what we want to preserve is the parent of the marked tree, preserve the entire marked tree */
	for (iter = tagparentnode; iter != DIRDB_NOPARENT; iter = dirdbData[iter].parent)
	{
		if (iter == node)
		{
			_dirdbTagPreserveTree (dirdbData[tagparentnode].children, dirdbData[tagparentnode].children_fill);
			return;
		}
	}

	/* if what we want to preserve is a subnode of the marked tree, preserve the subnode */
	for (iter = node; iter != DIRDB_NOPARENT; iter = dirdbData[iter].parent)
	{
		if (iter == tagparentnode)
		{
			_dirdbTagPreserveTree (dirdbData[node].children, dirdbData[node].children_fill);
			return;
		}
	}
}

void dirdbTagRemoveUntaggedAndSubmit(void)
{
	if (tagparentnode != DIRDB_NOPARENT)
	{
		_dirdbTagRemoveUntaggedAndSubmit (dirdbData[tagparentnode].children, dirdbData[tagparentnode].children_fill);
	} else {
		_dirdbTagRemoveUntaggedAndSubmit (dirdbRootChildren, dirdbRootChildren_fill);
	}
	if (tagparentnode != DIRDB_NOPARENT)
	{
		dirdbUnref (tagparentnode, dirdb_use_mdb_medialib);
	}
	tagparentnode=DIRDB_NOPARENT;
	dirdbDirty=1;
}

int dirdbGetMdb(uint32_t *dirdbnode, uint32_t *mdb_ref, int *first)
{
	if (*first)
	{
		*dirdbnode=0;
		*first=0;
	} else {
		(*dirdbnode)++;
	}
	for (;*dirdbnode<dirdbNum;(*dirdbnode)++)
	{
		if ((dirdbData[*dirdbnode].name)&&(dirdbData[*dirdbnode].mdb_ref!=DIRDB_NO_MDBREF))
		{
			*mdb_ref=dirdbData[*dirdbnode].mdb_ref;
			return 0;
		}
	}
	return -1;
}

static size_t strlen_width (const char *source)
{
	return measurestr_utf8 (source, strlen (source));
}

static void strlcat_width (char *dst, char *src, int length)
{
	while (*dst)
	{
		dst++;
	}
	while (length && *src)
	{
		int inc = 0;
		int visuallen;
		utf8_decode (src, strlen (src), &inc);
		visuallen = measurestr_utf8 (src, inc);
		if (visuallen > length)
		{
			break;
		}
		length -= visuallen;
		memcpy (dst, src, inc);
		dst += inc;
		src += inc;
	}
	*dst = 0;
}

static void strlcpy_width (char *dst, char *src, int length)
{
	while (length && *src)
	{
		int inc = 0;
		int visuallen;
		utf8_decode (src, strlen (src), &inc);
		visuallen = measurestr_utf8 (src, inc);
		if (visuallen > length)
		{
			break;
		}
		length -= visuallen;
		memcpy (dst, src, inc);
		dst += inc;
		src += inc;
	}
	*dst = 0;
}
void utf8_XdotY_name (const int X, const int Y, char *shortname, const char *source)
{
	char *temppath;
	char *lastdot;
	int length=strlen(source);

	temppath = strdup (source);

	if ((lastdot = strrchr(temppath + 1, '.'))) /* we allow files to start with . */
	{
		*lastdot = 0; /* modify the source - most easy way around the problem */

		strlcpy_width (shortname, temppath, X);
		length = strlen_width (shortname);
		if (length < X)
		{
			char *target = shortname + strlen (shortname);
			memset (target, ' ', X - length);
			target [X - length] = 0;
		}

		strcat (shortname, ".");

		strlcat_width (shortname, lastdot + 1, Y);
		length = strlen_width (lastdot + 1);
		if (length < Y)
		{
			char *target = shortname + strlen (shortname);
			memset (target, ' ', Y - length);
			target [Y - length] = 0;
		}
	} else {
		strlcpy_width(shortname, temppath, X + Y + 1);
		length = strlen_width (temppath);
		if (length < (X + Y + 1))
		{
			char *target = shortname + strlen (shortname);
			memset (target, ' ', (X + Y + 1) - length);
			target [(X + Y + 1) - length] = 0;
		}
	}
	free (temppath);
}

const struct dirdbAPI_t dirdbAPI =
{
	dirdbGetFullname_malloc,
	dirdbGetName_internalstr,
	dirdbGetName_malloc,
	dirdbRef,
	dirdbUnref,
	dirdbGetParentAndRef,
	dirdbResolvePathWithBaseAndRef,
	dirdbFindAndRef,
	dirdbDiffPath
};
