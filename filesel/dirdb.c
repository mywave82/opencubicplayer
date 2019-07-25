/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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

#include "config.h"
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

struct dirdbEntry
{
	uint32_t parent;
	uint32_t mdb_ref;
	uint32_t adb_ref;
	char *name; /* we pollute malloc a lot with this */
	int refcount;

	uint32_t newadb_ref;
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

#ifdef DIRDB_DEBUG
static void dumpdb_parent(uint32_t parent, int ident)
{
	uint32_t i;
	for (i=0;i<dirdbNum;i++)
	{
		if ((dirdbData[i].parent==parent)&&(dirdbData[i].name))
		{
			int j;
			fprintf(stderr, "0x%08x ", i);
			for (j=0;j<ident;j++)
				fprintf(stderr, " ");
			fprintf(stderr, "%s (refcount=%d mdb 0x%08x adb 0x%08x)\n", dirdbData[i].name, dirdbData[i].refcount, dirdbData[i].mdb_ref, dirdbData[i].adb_ref);
			dumpdb_parent(i, ident+1);
		}
	}
}

static void dumpdirdb(void)
{
	dumpdb_parent(DIRDB_NOPARENT, 0);
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

			if (version == 2)
			{
				if (read(f, &dirdbData[i].adb_ref, sizeof(uint32_t))!=sizeof(uint32_t))
					goto endoffile;
				dirdbData[i].adb_ref = uint32_little(dirdbData[i].adb_ref);
			} else
				dirdbData[i].adb_ref = DIRDB_NO_ADBREF;

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
				dirdbData[i].refcount++;
		} else {
			dirdbData[i].parent = DIRDB_NOPARENT;
			dirdbData[i].adb_ref = DIRDB_NO_ADBREF;
			dirdbData[i].mdb_ref = DIRDB_NO_MDBREF;
			dirdbData[i].newadb_ref=DIRDB_NO_ADBREF;
			dirdbData[i].newmdb_ref=DIRDB_NO_MDBREF;
			/* name is already NULL due to calloc() */
		}
	}
	close(f);
	for (i=0; i<dirdbNum; i++)
	{
		if (dirdbData[i].parent!=DIRDB_NOPARENT)
		{
			if (dirdbData[i].parent>=dirdbNum)
			{
				fprintf(stderr, "Invalid parent in a node .. (out of range)\n");
				dirdbData[i].parent=0;
			} else if (!dirdbData[dirdbData[i].parent].name)
			{
				fprintf(stderr, "Invalid parent in a node .. (not in use)\n");
				dirdbData[i].parent=0;
			}

			dirdbData[dirdbData[i].parent].refcount++;
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
		dirdbData[i].parent=0;
	}
	return retval;
}

void dirdbClose(void)
{
	uint32_t i;
	if (!dirdbNum)
		return;
	for (i=0; i<dirdbNum; i++)
		if (dirdbData[i].name)
			free(dirdbData[i].name);
	free(dirdbData);
	dirdbData = 0;
	dirdbNum = 0;
}


uint32_t dirdbFindAndRef(uint32_t parent, char const *name)
{
	uint32_t i;
	struct dirdbEntry *new;

#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbFindAndRef(0x%08x, %s%40s%s)\n", parent, name?"\"":"", name?name:"NULL", name?"\"":"");
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

	for (i=0;i<dirdbNum;i++)
		if (dirdbData[i].name)
			if ((dirdbData[i].parent==parent)&&(!strcmp(name, dirdbData[i].name)))
			{
				/*fprintf(stderr, " ++ %s (%d p=%d)\n", dirdbData[i].name, i, dirdbData[i].parent);*/
				dirdbData[i].refcount++;
				return i;
			}
	dirdbDirty=1;
	for (i=0;i<dirdbNum;i++)
		if (!dirdbData[i].name)
		{
reentry:
			dirdbData[i].name=strdup(name);
			if (!dirdbData[i].name)
			{
				fprintf (stderr, "dirdbFindAndRef: strdup() failed\n");
				return DIRDB_NOPARENT;
			}
			dirdbData[i].parent=parent;
			dirdbData[i].refcount++;
			dirdbData[i].mdb_ref=DIRDB_NO_MDBREF;
			dirdbData[i].adb_ref=DIRDB_NO_ADBREF;
			if (parent!=DIRDB_NOPARENT)
			{
				/*fprintf(stderr, "  + %s (%d p=%d)\n", dirdbData[i].name, i, dirdbData[i].parent);*/
				dirdbData[parent].refcount++;
			}
#ifdef DIRDB_DEBUG
			dumpdirdb();
#endif
			return i;
		}

	new=realloc(dirdbData, (dirdbNum+16)*sizeof(struct dirdbEntry));
	if (!new)
	{
		fprintf(stderr, "dirdbFindAndRef: realloc() failed, out of memory\n");
		return DIRDB_NOPARENT;
	}
	dirdbData=new;
	memset(dirdbData+dirdbNum, 0, 16*sizeof(struct dirdbEntry));
	i=dirdbNum;
	dirdbNum+=16;
	{
		uint32_t j;
		for (j=i;j<dirdbNum;j++)
		{
			dirdbData[j].adb_ref=DIRDB_NO_ADBREF;
			dirdbData[j].newadb_ref=DIRDB_NO_ADBREF;
			dirdbData[j].mdb_ref=DIRDB_NO_MDBREF;
			dirdbData[j].newmdb_ref=DIRDB_NO_MDBREF;
			dirdbData[j].parent=DIRDB_NOPARENT;
			dirdbData[j].name=0;
			dirdbData[j].refcount=0;
		}
	}
	goto reentry;
}

void dirdbRef(uint32_t node)
{
	if (node == DIRDB_NOPARENT)
	{
		return;
	}
	if ((node>=dirdbNum) || (!dirdbData[node].name))
	{
		fprintf(stderr, "dirdbFindAndRef: invalid node\n");
		return;
	}
	/*fprintf(stderr, "+++ %s (%d p=%d)\n", dirdbData[node].name, node, dirdbData[node].parent);*/
	dirdbData[node].refcount++;
}

uint32_t dirdbResolvePathWithBaseAndRef(uint32_t base, const char *name)
{
	char *segment;
	const char *next;
	char *split;
	uint32_t retval=base, newretval;

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
		dirdbRef(retval);
	}
	while (next)
	{
		if ((split=strchr(next, '/')))
		{
			strncpy(segment, next, split-next);
			segment[split-next]=0;
			next=split+1;
			if (!next)
				next=0;
		} else {
			strcpy(segment, next);
			next=0;
		}

		if (!strlen(segment))
		{ /* empty segment, happens if you have a // in the path */
			continue;
		}

		newretval=dirdbFindAndRef(retval, segment);

		if (retval!=DIRDB_NOPARENT)
		{
			dirdbUnref(retval);
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
#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif
	return retval;
}

extern uint32_t dirdbResolvePathAndRef(const char *name)
{
	char *segment;
	const char *next;
	char *split;
	uint32_t retval=DIRDB_NOPARENT, newretval;

#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbResolvePathAndRef(%s%40s%s)\n", name?"\"":"", name?name:"NULL", name?"\"":"");
#endif
	if (!name)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(): name is NULL\n");
		return DIRDB_NOPARENT;
	}

	segment = malloc (strlen(name)+1); /* We never will need more than this */
	if (!segment)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(): malloc() failed\n");
		return DIRDB_NOPARENT;
	}

	next=name;
	while (next)
	{
#if 0 /* caught by the common if(!strlen(segment)) test further down */
		if (*next=='/')
		{
			next++;
			continue;
		}
#endif
		if ((split=strchr(next, '/')))
		{
			strncpy(segment, next, split-next);
			segment[split-next]=0;
			next=split+1;
			if (!next)
				next=0;
		} else {
			strcpy(segment, next);
			next=0;
		}
		if (!strlen(segment))
			continue;
		newretval=dirdbFindAndRef(retval, segment);
		if (retval!=DIRDB_NOPARENT)
			dirdbUnref(retval);
		retval=newretval;
	}
	free (segment);
#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif
	return retval;
}

void dirdbUnref(uint32_t node)
{
	uint32_t parent;
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
err:
		abort();
		return;
	}
	if (!dirdbData[node].refcount)
	{
		fprintf (stderr, "dirdbUnref: refcount == 0\n");
		goto err;
	}
	/* fprintf(stderr, "--- %s (%d p=%d)\n", dirdbData[node].name, node, dirdbData[node].parent);*/
	dirdbData[node].refcount--;
	if (dirdbData[node].refcount)
		return;
	/* fprintf(stderr, "DELETE\n");*/
	dirdbDirty=1;
	parent = dirdbData[node].parent;
	dirdbData[node].parent=DIRDB_NOPARENT;
	free(dirdbData[node].name);
	dirdbData[node].name=0;

	dirdbData[node].mdb_ref=DIRDB_NO_MDBREF; /* this should not be needed */
	dirdbData[node].newmdb_ref=DIRDB_NO_MDBREF; /* this should not be needed */
	dirdbData[node].adb_ref=DIRDB_NO_ADBREF; /* this should not be needed */
	dirdbData[node].newadb_ref=DIRDB_NO_ADBREF; /* this should not be needed */

#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif
	if (parent!=DIRDB_NOPARENT)
		dirdbUnref(parent);
}

void dirdbGetName_internalstr(uint32_t node, char **name)
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
			if (flags & DIRDB_FULLNAME_NOBASE)
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

	dirdbGetFullname_malloc_R (node, *name, flags&DIRDB_FULLNAME_NOBASE);

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
				dirdbUnref(i);
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
			buf32=uint32_little(dirdbData[i].adb_ref);
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

uint32_t dirdbGetParentAndRef (uint32_t node)
{
	uint32_t retval;
	if ((node>=dirdbNum) || (!dirdbData[node].name))
	{
		fprintf(stderr, "dirdbGetParentAndRef: invalid node\n");
		return DIRDB_NOPARENT;
	}
	retval=dirdbData[node].parent;
	if (retval!=DIRDB_NOPARENT)
		dirdbData[dirdbData[node].parent].refcount++;
	return retval;
}

#if 0
void dirdbMakeMdbRef(uint32_t node, uint32_t mdb_ref)
{
#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbMakeMdbRef(node 0x%08x, mdb_ref 0x%08x)\n", node, mdb_ref);
#endif
	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbMakeMdbRef: invalid node\n");
		return;
	}
	if (mdb_ref==DIRDB_NO_MDBREF)
	{
		if (dirdbData[node].mdb_ref!=DIRDB_NO_MDBREF)
		{
			dirdbData[node].mdb_ref=DIRDB_NO_MDBREF;
			dirdbDirty=1;
			dirdbUnref(node);
		}
	} else {
		int doref = (dirdbData[node].mdb_ref==DIRDB_NO_MDBREF);
		dirdbData[node].mdb_ref=mdb_ref;
		dirdbDirty=1;
		if (doref)
			dirdbRef(node);
	}
#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif
}
#endif

void dirdbTagSetParent(uint32_t node)
{
	uint32_t i;
	if (tagparentnode!=DIRDB_NOPARENT)
	{
		fprintf(stderr, "dirdbTagSetParent: warning, a node was already set as parent\n");
		dirdbUnref(tagparentnode);
		tagparentnode=DIRDB_NOPARENT;
	}

	for (i=0;i<dirdbNum;i++)
	{
		dirdbData[i].newmdb_ref=DIRDB_NO_MDBREF;
		dirdbData[i].newadb_ref=DIRDB_NO_ADBREF; /* is this actually needed? */
	}

	if ((node != DIRDB_NOPARENT) && ((node>=dirdbNum) || (!dirdbData[node].name)))
	{	/* DIRDB_NOPARENT is legal! */
		fprintf(stderr, "dirdbTagSetParent: invalid node\n");
		return;
	}
	tagparentnode = node;
	if (node != DIRDB_NOPARENT)
	{
		dirdbRef(node);
	}
}

void dirdbMakeMdbAdbRef(uint32_t node, uint32_t mdb_ref, uint32_t adb_ref)
{
	if ((node>=dirdbNum) || (!dirdbData[node].name))
	{
		fprintf(stderr, "dirdbMakeMdbRef: invalid node\n");
		return;
	}
	/* the madness below is in order to keep track of references the correct way */
	if (mdb_ref==DIRDB_NO_MDBREF)
	{
		if (dirdbData[node].newmdb_ref!=DIRDB_NO_MDBREF)
		{
			dirdbData[node].newmdb_ref=DIRDB_NO_MDBREF;
			dirdbUnref(node);
		} /* else, no change */
	} else {
		if (dirdbData[node].newmdb_ref==DIRDB_NO_MDBREF)
		{
			dirdbData[node].newmdb_ref=mdb_ref;
			dirdbRef(node);
		} else {
			/*dirdbUnref(node);*/
			dirdbData[node].newmdb_ref=mdb_ref;
			/*dirdbRef(node);  overkill to unref and re-ref just for the name's sake*/
		}
	}

	dirdbData[node].newadb_ref = adb_ref;
}

void dirdbTagCancel(void)
{
	uint32_t i;
	for (i=0;i<dirdbNum;i++)
	{
		if (dirdbData[i].newmdb_ref!=DIRDB_NO_MDBREF)
		{
			dirdbData[i].newmdb_ref=DIRDB_NO_MDBREF;
			dirdbUnref(i);
		}
		dirdbData[i].newadb_ref = DIRDB_NO_ADBREF;
	}
	if (tagparentnode==DIRDB_NOPARENT)
	{
		//fprintf(stderr, "dirdbTagCancel: parent is not set\n");
		return;
	}
	dirdbUnref(tagparentnode);
	tagparentnode=DIRDB_NOPARENT;
}

static void _dirdbTagRemoveUntaggedAndSubmit(uint32_t node)
{
	uint32_t i;

	for (i=0;i<dirdbNum;i++)
	{
		if ((dirdbData[i].parent==node) && dirdbData[i].name)
		{
			dirdbData[i].adb_ref=dirdbData[i].newadb_ref;
			if (dirdbData[i].newmdb_ref==dirdbData[i].mdb_ref)
			{
				if (dirdbData[i].mdb_ref==DIRDB_NO_MDBREF)
				{
					/* probably a dir */
					_dirdbTagRemoveUntaggedAndSubmit(i);
				} else {
					/* mdb_ref is the same */
					dirdbData[i].mdb_ref=dirdbData[i].newmdb_ref;
					dirdbData[i].newmdb_ref=DIRDB_NO_MDBREF;
					dirdbUnref(i);
				}
			} else {
				if (dirdbData[i].mdb_ref==DIRDB_NO_MDBREF)
				{
					dirdbData[i].mdb_ref=dirdbData[i].newmdb_ref;
					dirdbData[i].newmdb_ref=DIRDB_NO_MDBREF;
					/* no need to unref/ref, since we are
					 * balanced. Since somebody can have
					 * named a file, the same name
					 * a directory used to have, we need
					 * to scan for siblings.
					 */
					_dirdbTagRemoveUntaggedAndSubmit(i);
					/* this can probably be done more elegant later */
				} else if (dirdbData[i].newmdb_ref==DIRDB_NO_MDBREF)
				{
					dirdbData[i].mdb_ref=DIRDB_NO_MDBREF;
					dirdbUnref(i);
					/* same as above regarding renaming */
					_dirdbTagRemoveUntaggedAndSubmit(i);
					/* this can probably be done more elegant later */
				} else {
					dirdbData[i].mdb_ref=dirdbData[i].newmdb_ref;
					dirdbData[i].newmdb_ref=DIRDB_NO_MDBREF;
					dirdbUnref(i);
				}
			}
		}
	}
}

void dirdbTagRemoveUntaggedAndSubmit(void)
{
#if 0
	/* removing from node / */
	if (tagparentnode==DIRDB_NOPARENT)
	{
		fprintf(stderr, "dirdbTagRemoveUntaggedAndSubmit: parent is not set\n");
		return;
	}
#endif
	/* if parent has changed mdb_ref, we can't detect this.. NB NB NB */
	_dirdbTagRemoveUntaggedAndSubmit(tagparentnode);
	if (tagparentnode!=DIRDB_NOPARENT)
	{
		dirdbUnref(tagparentnode);
	}
	tagparentnode=DIRDB_NOPARENT;
	dirdbDirty=1;
}

int dirdbGetMdbAdb(uint32_t *dirdbnode, uint32_t *mdb_ref, uint32_t *adb_ref, int *first)
{
	if (*first)
	{
		*dirdbnode=0;
		*adb_ref=DIRDB_NO_ADBREF;
		*first=0;
	} else
		(*dirdbnode)++;
	for (;*dirdbnode<dirdbNum;(*dirdbnode)++)
		if ((dirdbData[*dirdbnode].name)&&(dirdbData[*dirdbnode].mdb_ref!=DIRDB_NO_MDBREF))
		{
			*mdb_ref=dirdbData[*dirdbnode].mdb_ref;
			*adb_ref=dirdbData[*dirdbnode].adb_ref;
			return 0;
		}
	return -1;
}
