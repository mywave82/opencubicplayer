/* OpenCP Module Player
 * copyright (c) '04-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "types.h"
#include "dirdb.h"
#include "boot/psetting.h"
#include "stuff/compat.h"

struct dirdbEntry
{
	uint32_t parent;

	uint32_t next;
	uint32_t child;

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
const char dirdbsigv1[60] = "Cubic Player Directory Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
const char dirdbsigv2[60] = "Cubic Player Directory Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01";

static struct dirdbEntry *dirdbData=0;
static uint32_t dirdbNum=0;
static int dirdbDirty=0;

static uint32_t dirdbRootChild = DIRDB_NOPARENT;
static uint32_t dirdbFreeChild = DIRDB_NOPARENT;

#ifdef DIRDB_DEBUG
static void dumpdb_parent(uint32_t firstchild, int ident)
{
	uint32_t iter;

	for (iter = firstchild; iter != DIRDB_NOPARENT; iter = dirdbData[iter].next)
	{
		int j;
		assert (dirdbData[iter].name);

		fprintf(stderr, "0x%08x ", iter);
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

		if (dirdbData[iter].child != DIRDB_NOPARENT) /* nothing bad happens without this if, just  slows things down a bit */
		{
			dumpdb_parent(dirdbData[iter].child, ident+1);
		}
	}
}

static void dumpdirdb(void)
{
	dumpdb_parent(dirdbRootChild, 0);
}
#endif

int dirdbInit(void)
{
	char *path;
	struct dirdbheader header;
	int f;
	uint32_t i;
	int retval;
	int version;

	dirdbRootChild = DIRDB_NOPARENT;
	dirdbFreeChild = DIRDB_NOPARENT;

	path = malloc(strlen(cfConfigDir)+11+1);
	if (!path)
	{
		fprintf(stderr, "dirdbInit: malloc() failed\n");
		return 1;
	}
	strcpy(path, cfConfigDir);
	strcat(path, "CPDIRDB.DAT");

	if ((f=open(path, O_RDONLY))<0)
	{
		perror("open(cfConfigDir/CPDIRDB.DAT)");
		free (path);
		return 1;
	}

	fprintf(stderr, "Loading %s .. ", path);

	free (path);
	path = 0;

	if (read(f, &header, sizeof(header))!=sizeof(header))
	{
		fprintf(stderr, "No header\n");
		close(f);
		return 1;
	}
	if (memcmp(header.sig, dirdbsigv1, 60))
	{
		if (memcmp(header.sig, dirdbsigv2, 60))
		{
			fprintf(stderr, "Invalid header\n");
			close(f);
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
		if (read(f, &len, sizeof(uint16_t))!=sizeof(uint16_t))
		{
			goto endoffile;
		}
		if (len)
		{
			len = uint16_little(len);

			if (read(f, &dirdbData[i].parent, sizeof(uint32_t))!=sizeof(uint32_t))
				goto endoffile;
			dirdbData[i].parent = uint32_little(dirdbData[i].parent);

			if (read(f, &dirdbData[i].mdb_ref, sizeof(uint32_t))!=sizeof(uint32_t))
				goto endoffile;
			dirdbData[i].mdb_ref = uint32_little(dirdbData[i].mdb_ref);
			dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;

			if (version == 2)
			{
				uint32_t discard_adb_ref;
				if (read(f, &discard_adb_ref, sizeof(uint32_t))!=sizeof(uint32_t))
					goto endoffile;
			}

			dirdbData[i].name=malloc(len+1);
			if (!dirdbData[i].name)
				goto outofmemory;
			if (read(f, dirdbData[i].name, len)!=len)
			{
				free(dirdbData[i].name);
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
	close(f);
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
			} else if (!dirdbData[dirdbData[i].parent].name)
			{
				fprintf(stderr, "Invalid parent in a node .. (not in use)\n");
				dirdbData[i].parent = DIRDB_NOPARENT;
			}

			dirdbData[dirdbData[i].parent].refcount++;
#ifdef DIRDB_DEBUG
			dirdbData[dirdbData[i].parent].refcount_children++;
#endif
		}
		dirdbData[i].child = DIRDB_NOPARENT;
		dirdbData[i].next = DIRDB_NOPARENT;
	}

	for (i=0; i<dirdbNum; i++)
	{
		if (!dirdbData[i].name)
		{
			dirdbData[i].next = dirdbFreeChild;
			dirdbFreeChild = i;
		} else {
			uint32_t *parent;
			if (dirdbData[i].parent == DIRDB_NOPARENT)
			{
				parent = &dirdbRootChild;
			} else {
				parent = &dirdbData[dirdbData[i].parent].child;
			}
			dirdbData[i].next = *parent;
			*parent = i;
		}
	}

	fprintf(stderr, "Done\n");
	return 1;
endoffile:
	fprintf(stderr, "EOF\n");
	close(f);
	retval=1;
	goto unload;
outofmemory:
	fprintf(stderr, "out of memory\n");
	close(f);
	retval=0;
unload:
	for (i=0; i<dirdbNum; i++)
	{
		if (dirdbData[i].name)
		{
			free(dirdbData[i].name);
			dirdbData[i].name=0;
		}
		dirdbData[i].parent = DIRDB_NOPARENT;
		dirdbData[i].next = dirdbFreeChild;
		dirdbFreeChild = i;
	}
	return retval;
}

void dirdbClose(void)
{
	uint32_t i;
	if (!dirdbNum)
		return;
	for (i=0; i<dirdbNum; i++)
	{
		free(dirdbData[i].name);
	}
	free(dirdbData);
	dirdbData = 0;
	dirdbNum = 0;
	dirdbRootChild = DIRDB_NOPARENT;
	dirdbFreeChild = DIRDB_NOPARENT;
}

uint32_t dirdbFindAndRef(uint32_t parent, char const *name, enum dirdb_use use)
{
	uint32_t i, *prev;
	struct dirdbEntry *new;

#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbFindAndRef(0x%08x, %s%40s%s, use=%d)\n", parent, name?"\"":"", name?name:"NULL", name?"\"":"", use);
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
		fprintf (stderr, "dirdbFindAndRef: name containes /\n");
		return DIRDB_NOPARENT;
	}

	for (i = (parent != DIRDB_NOPARENT) ? dirdbData[parent].child : dirdbRootChild; i != DIRDB_NOPARENT; i = dirdbData[i].next)
	{
		assert (dirdbData[i].name);
		assert (dirdbData[i].parent == parent);
		if (!strcmp(name, dirdbData[i].name))
		{
			/*fprintf(stderr, " ++ %s (%d p=%d)\n", dirdbData[i].name, i, dirdbData[i].parent);*/
			dirdbData[i].refcount++;
#ifdef DIRDB_DEBUG
			switch (use)
			{
				case dirdb_use_children:      dirdbData[i].refcount_children++;      break;
				case dirdb_use_dir:           dirdbData[i].refcount_directories++;   break;
				case dirdb_use_file:          dirdbData[i].refcount_files++;         break;
				case dirdb_use_filehandle:    dirdbData[i].refcount_filehandles++;   break;
				case dirdb_use_drive_resolve: dirdbData[i].refcount_drive_resolve++; break;
				case dirdb_use_pfilesel:      dirdbData[i].refcount_pfilesel++;      break;
				case dirdb_use_medialib:      dirdbData[i].refcount_medialib++;      break;
				case dirdb_use_mdb_medialib:  dirdbData[i].refcount_mdb_medialib++;  break;
			}
#endif
			return i;
		}
	}

	if (dirdbFreeChild == DIRDB_NOPARENT)
	{
		uint32_t j;

		new=realloc(dirdbData, (dirdbNum+64)*sizeof(struct dirdbEntry));
		if (!new)
		{
			fprintf(stderr, "dirdbFindAndRef: realloc() failed, out of memory\n");
			return DIRDB_NOPARENT;
		}
		dirdbData=new;
		memset(dirdbData+dirdbNum, 0, 64*sizeof(struct dirdbEntry));
		i=dirdbNum;
		dirdbNum+=64;

		for (j=i;j<dirdbNum;j++)
		{
			dirdbData[j].mdb_ref = DIRDB_NO_MDBREF;
			dirdbData[j].newmdb_ref = DIRDB_NO_MDBREF;
			dirdbData[j].parent = DIRDB_NOPARENT;
			dirdbData[j].next = dirdbFreeChild;
			dirdbData[j].child = DIRDB_NOPARENT;
			dirdbFreeChild = j;
		}
	}

	if (parent == DIRDB_NOPARENT)
	{
		prev = &dirdbRootChild;
	} else {
		prev = &dirdbData[parent].child;
	}

	dirdbDirty=1;

	/* grab a free entry */
	i = dirdbFreeChild;
	dirdbData[i].name=strdup(name);
	if (!dirdbData[i].name)
	{
		fprintf (stderr, "dirdbFindAndRef: strdup() failed\n");
		return DIRDB_NOPARENT;
	}
	dirdbFreeChild = dirdbData[i].next;

	/* and insert it as the parent first child */
	dirdbData[i].next = *prev; /* take the previous value */
	*prev = i; /* before we replace it */
	dirdbData[i].parent=parent;
	dirdbData[i].refcount++;
#ifdef DIRDB_DEBUG
	switch (use)
	{
		case dirdb_use_children:      dirdbData[i].refcount_children++;      break;
		case dirdb_use_dir:           dirdbData[i].refcount_directories++;   break;
		case dirdb_use_file:          dirdbData[i].refcount_files++;         break;
		case dirdb_use_filehandle:    dirdbData[i].refcount_filehandles++;   break;
		case dirdb_use_drive_resolve: dirdbData[i].refcount_drive_resolve++; break;
		case dirdb_use_pfilesel:      dirdbData[i].refcount_pfilesel++;      break;
		case dirdb_use_medialib:      dirdbData[i].refcount_medialib++;      break;
		case dirdb_use_mdb_medialib:  dirdbData[i].refcount_mdb_medialib++;  break;
	}
#endif
	if (parent!=DIRDB_NOPARENT)
	{
		/*fprintf(stderr, "  + %s (%d p=%d)\n", dirdbData[i].name, i, dirdbData[i].parent);*/
		dirdbRef(parent, dirdb_use_children);
	}
#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif
	return i;
}

uint32_t dirdbRef(uint32_t node, enum dirdb_use use)
{
#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbRef(0x%08x)\n", node);
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
#ifndef __W32__
			char *home;

			newretval = dirdbFindAndRef (DIRDB_NOPARENT, "file:", TEMP_SPACE);
			dirdbUnref (retval, TEMP_SPACE);
			retval = newretval;

			home = getenv("HOME");
			if (!home)
			{
				dirdbUnref (retval, TEMP_SPACE);
				free (segment);
				return DIRDB_NOPARENT;
			}
			newretval = dirdbResolvePathWithBaseAndRef (retval, home, 0, TEMP_SPACE); /* we are already at the root */
			dirdbUnref (retval, TEMP_SPACE);
			retval = newretval;
#else
			#warning This is untested code
			char *home;

			home = getenv("HOMEPATH"); /* home is expected to contain DRIVE: */
			if (home = NULL)
			{
				dirdbUnref (retval, TEMP_SPACE);
				free (segment);
				return DIRDB_NOPARENT;
			}
			newretval = dirdbResolvePathWithBaseAndRef (DIRDB_NOPARENT, home, DIRDB_RESOLVE_DRIVE, TEMP_SPACE);
			dirdbUnref (retval, TEMP_SPACE);
			retval = newretval;
#endif
			next += 2;
			break;
		}

		/* test for ~username */
		if ((flags & DIRDB_RESOLVE_TILDE_USER) && (next[0] == '~'))
		{
#if defined(HAVE_GETPWNAM) && (!defined (__W32__))
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
		char *tmp = 0;
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
	uint32_t parent, *prev;
#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbUnref(0x%08x)\n", node);
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
	assert (dirdbData[node].child == DIRDB_NOPARENT);
	parent = dirdbData[node].parent;
	dirdbData[node].parent=DIRDB_NOPARENT;
	free(dirdbData[node].name);
	dirdbData[node].name=0;

	dirdbData[node].mdb_ref=DIRDB_NO_MDBREF; /* this should not be needed */
	dirdbData[node].newmdb_ref=DIRDB_NO_MDBREF; /* this should not be needed */

	if (parent == DIRDB_NOPARENT)
	{
		prev = &dirdbRootChild;
	} else {
		prev = &dirdbData[parent].child;
	}

	while (*prev != node)
	{
		assert ((*prev) != DIRDB_NOPARENT);
		prev = &dirdbData[*prev].next;
	}

	*prev = dirdbData[node].next;
	dirdbData[node].next = dirdbFreeChild;
	dirdbFreeChild = node;

#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif

	if (parent!=DIRDB_NOPARENT)
	{
		dirdbUnref (parent, dirdb_use_children);
	}
}

void dirdbGetName_internalstr(uint32_t node, char **name)
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

static void dirdbGetFullname_malloc_R(uint32_t node, char *name, int nobase)
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
		dirdbGetFullname_malloc_R(dirdbData[node].parent, name, nobase);
		strcat(name, "/");
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

	dirdbGetFullname_malloc_R (node, *name, flags&DIRDB_FULLNAME_NODRIVE);

	if (flags&DIRDB_FULLNAME_ENDSLASH)
	{
		strcat(*name, "/");
	}

	if (strlen(*name) != length)
	{
		fprintf (stderr, "dirdbGetFullname_malloc: WARNING, length calculation was off. Expected %d, but got %d\n", length, (int)strlen (*name));
	}
}


void dirdbFlush(void)
{
	char *path;
	int f;
	uint32_t i;
	uint32_t max;
	uint16_t buf16;
	uint32_t buf32;
	struct dirdbheader header;

	if (!dirdbDirty)
		return;

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

	path = malloc(strlen(cfConfigDir)+11+1);
	if (!path)
	{
		fprintf(stderr, "dirdbFlush: malloc() failed\n");
		return;
	}
	strcpy(path, cfConfigDir);
	strcat(path, "CPDIRDB.DAT");

	if ((f=open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE))<0)
	{
		perror("open(cfConfigDir/CPDIRDB.DAT)");
		free (path);
		return;
	}
	free (path);
	path=0;

	max=0;
	for (i=0;i<dirdbNum;i++)
		if (dirdbData[i].name)
			max=i+1;

	memcpy(header.sig, dirdbsigv2, sizeof(dirdbsigv2));
	header.entries=uint32_little(max);

	if (write(f, &header, sizeof(header))!=sizeof(header))
		goto writeerror;

	for (i=0;i<max;i++)
	{
		int len=(dirdbData[i].name?strlen(dirdbData[i].name):0);
		buf16=uint16_little(len);
		if (write(f, &buf16, sizeof(uint16_t))!=sizeof(uint16_t))
			goto writeerror;
		if (len)
		{
			buf32=uint32_little(dirdbData[i].parent);
			if (write(f, &buf32, sizeof(uint32_t))!=sizeof(uint32_t))
				goto writeerror;
			buf32=uint32_little(dirdbData[i].mdb_ref);
			if (write(f, &buf32, sizeof(uint32_t))!=sizeof(uint32_t))
				goto writeerror;
#warning remove-me this used to be ADB_REF
			buf32=0xffffffff; //ADB_REF
			if (write(f, &buf32, sizeof(uint32_t))!=sizeof(uint32_t))
				goto writeerror;
			if (write(f, dirdbData[i].name, len)!=len)
				goto writeerror;
		}
	}
	close(f);
	dirdbDirty=0;
	return;
writeerror:
	perror("dirdb write()");
	close(f);
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

static void _dirdbTagRemoveUntaggedAndSubmit(uint32_t node)
{
	uint32_t i, next, child;

#ifdef DIRDB_DEBUG
	fprintf (stderr, "_dirdbTagRemoveUntaggedAndSubmit(0x%"PRIx32")\n", node);
#endif

	for (i = node; i != DIRDB_NOPARENT; i = next)
	{
		next = dirdbData[i].next;
		child = dirdbData[i].child;
		if (dirdbData[i].newmdb_ref == dirdbData[i].mdb_ref)
		{
			if (dirdbData[i].mdb_ref == DIRDB_NO_MDBREF)
			{
				/* probably a dir */
			} else {
				dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;
				dirdbUnref(i, dirdb_use_mdb_medialib);
			}
		} else {
			if (dirdbData[i].mdb_ref == DIRDB_NO_MDBREF)
			{
				dirdbData[i].mdb_ref = dirdbData[i].newmdb_ref;
				dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;
				/* no need to unref/ref, since we are
				 * balanced. Since somebody can have
				 * named a file, the same name
				 * a directory used to have, we need
				 * to scan for siblings.
				 */
			} else if (dirdbData[i].newmdb_ref == DIRDB_NO_MDBREF)
			{
				dirdbData[i].mdb_ref = DIRDB_NO_MDBREF;
				dirdbUnref (i, dirdb_use_mdb_medialib);
				/* same as above regarding renaming */
			} else {
				dirdbData[i].mdb_ref = dirdbData[i].newmdb_ref;
				dirdbData[i].newmdb_ref = DIRDB_NO_MDBREF;
				dirdbUnref(i, dirdb_use_mdb_medialib);
			}
		}
		if (child != DIRDB_NOPARENT)
		{
			_dirdbTagRemoveUntaggedAndSubmit(dirdbData[i].child);
		}
	}
}

static void _dirdbTagPreserveTree(uint32_t node)
{
	uint32_t i;

	for (i = node; i != DIRDB_NOPARENT; i = dirdbData[i].next)
	{
		if ((dirdbData[i].newmdb_ref != dirdbData[i].mdb_ref) && (dirdbData[i].newmdb_ref == DIRDB_NOPARENT))
		{
			dirdbData[i].newmdb_ref = dirdbData[i].mdb_ref;
			dirdbRef(i, dirdb_use_mdb_medialib);
		}
		_dirdbTagPreserveTree (dirdbData[i].child);
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
			_dirdbTagPreserveTree (dirdbData[tagparentnode].child);
			return;
		}
	}

	/* if what we want to preserve is a subnode of the marked tree, preserve the subnode */
	for (iter = node; iter != DIRDB_NOPARENT; iter = dirdbData[iter].parent)
	{
		if (iter == tagparentnode)
		{
			_dirdbTagPreserveTree (dirdbData[node].child);
			return;
		}
	}
}

void dirdbTagRemoveUntaggedAndSubmit(void)
{
	if (tagparentnode != DIRDB_NOPARENT)
	{
		_dirdbTagRemoveUntaggedAndSubmit (dirdbData[tagparentnode].child);
	} else {
		_dirdbTagRemoveUntaggedAndSubmit (dirdbRootChild);
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
