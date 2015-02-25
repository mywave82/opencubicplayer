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
	uint32_t mdbref;
	uint32_t adbref;
	char *name; /* we pollute malloc a lot with this */
	int refcount;

	uint32_t newadbref;
	uint32_t newmdbref; /* used during scan to find new nodes */
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
			fprintf(stderr, "%s (refcount=%d mdb 0x%08x adb 0x%08x)\n", dirdbData[i].name, dirdbData[i].refcount, dirdbData[i].mdbref, dirdbData[i].adbref);
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
	char path[PATH_MAX+1];
	struct dirdbheader header;
	int f;
	uint32_t i;
	int retval;
	int version;

	if ((strlen(cfConfigDir)+11)>PATH_MAX)
	{
		fprintf(stderr, "dirdb: CPDIRDB.DAT path is too long\n");
		return 1;
	}
	strcpy(path, cfConfigDir);
	strcat(path, "CPDIRDB.DAT");

	if ((f=open(path, O_RDONLY))<0)
	{
		perror("open(cfConfigDir/CPDIRDB.DAT)");
		return 1;
	}

	fprintf(stderr, "Loading %s .. ", path);

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
		} else
			version = 2;
	} else
		version = 1;
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
			fprintf(stderr, "EOF\n");
			close(f);
			return 1;
		}
		if (len)
		{
			len = uint16_little(len);

			if (read(f, &dirdbData[i].parent, sizeof(uint32_t))!=sizeof(uint32_t))
				goto endoffile;
			dirdbData[i].parent = uint32_little(dirdbData[i].parent);

			if (read(f, &dirdbData[i].mdbref, sizeof(uint32_t))!=sizeof(uint32_t))
				goto endoffile;
			dirdbData[i].mdbref = uint32_little(dirdbData[i].mdbref);

			if (version == 2)
			{
				if (read(f, &dirdbData[i].adbref, sizeof(uint32_t))!=sizeof(uint32_t))
					goto endoffile;
				dirdbData[i].adbref = uint32_little(dirdbData[i].adbref);
			} else
				dirdbData[i].adbref = DIRDB_NO_ADBREF;

			dirdbData[i].name=malloc(len+1);
			if (!dirdbData[i].name)
				goto outofmemory;
			if (read(f, dirdbData[i].name, len)!=len)
			{
				free(dirdbData[i].name);
				goto endoffile;
			}
			dirdbData[i].name[len]=0; /* terminate the string */
			if (dirdbData[i].mdbref!=DIRDB_NO_MDBREF)
				dirdbData[i].refcount++;
		}
	}
	close(f);
	for (i=0; i<dirdbNum; i++)
	{
		if (dirdbData[i].parent!=DIRDB_NOPARENT)
		{
			if (dirdbData[i].parent>=dirdbNum)
			{
				fprintf(stderr, "Invalid parent in a node ..");
				dirdbData[i].parent=0;
			} else
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


uint32_t dirdbFindAndRef(uint32_t parent, char const *name /* NAME_MAX + 1 */)
{
	uint32_t i;
	struct dirdbEntry *new;

#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbFindAndRef(0x%08x, \"%s\")\n", parent, name);
#endif

	if (strlen(name)>NAME_MAX)
	{
		fprintf(stderr, "dirdbFindAndRef: name too long\n");
		return DIRDB_NOPARENT;
	}

	if ((parent!=DIRDB_NOPARENT)&&(parent>=dirdbNum))
	{
		fprintf(stderr, "dirdbFindAndRef: invalid parent\n");
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
			dirdbData[i].parent=parent;
			dirdbData[i].refcount++;
			dirdbData[i].mdbref=DIRDB_NO_MDBREF;
			dirdbData[i].adbref=DIRDB_NO_ADBREF;
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
		fprintf(stderr, "dirdbFindAndRef: out of memory\n");
		_exit(1); /* we could exit nice here to the caller */
	}
	dirdbData=new;
	memset(dirdbData+dirdbNum, 0, 16*sizeof(struct dirdbEntry));
	i=dirdbNum;
	dirdbNum+=16;
	{
		uint32_t j;
		for (j=i;j<dirdbNum;j++)
		{
			dirdbData[j].adbref=DIRDB_NO_ADBREF;
			dirdbData[j].newadbref=DIRDB_NO_ADBREF;
			dirdbData[j].mdbref=DIRDB_NO_MDBREF;
			dirdbData[j].newmdbref=DIRDB_NO_MDBREF;
		}
	}
	goto reentry;
}

void dirdbRef(uint32_t node)
{
	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbFindAndRef: invalid node\n");
		return;
	}
	/*fprintf(stderr, "+++ %s (%d p=%d)\n", dirdbData[node].name, node, dirdbData[node].parent);*/
	dirdbData[node].refcount++;
}

uint32_t dirdbResolvePathWithBaseAndRef(uint32_t base, const char *name /* PATH_MAX + 1 */)
{
	char segment[PATH_MAX+1];
	const char *next;
	char *split;
	uint32_t retval=base, newretval;

#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbResolvePathWithBaseAndRef(0x%08x, \"%s\")\n", base, name);
#endif

	if (strlen(name)>PATH_MAX)
	{
		fprintf(stderr, "dirdbResolvPathWithBase: name too long\n");
		return DIRDB_NOPARENT;
	}
	next=name;
	if (retval!=DIRDB_NOPARENT)
		dirdbRef(retval);
	while (next)
	{
		if (*next=='/')
			next++;
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
		dirdbUnref(retval);
		retval=newretval;
	}
#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif
	return retval;
}

extern uint32_t dirdbResolvePathAndRef(const char *name /*PATH_MAX + 1 */)
{
	char segment[PATH_MAX+1];
	const char *next;
	char *split;
	uint32_t retval=DIRDB_NOPARENT, newretval;

#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbResolvePathAndRef(\"%s\")\n", name);
#endif
	if (strlen(name)>PATH_MAX)
	{
		fprintf(stderr, "dirdbResolvPathWithBase: name too long\n");
		return DIRDB_NOPARENT;
	}
	next=name;
	while (next)
	{
		if (*next=='/')
			next++;
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
	dirdbData[node].parent=0;
	free(dirdbData[node].name);
	dirdbData[node].name=0;

	dirdbData[node].mdbref=DIRDB_NO_MDBREF; /* this should not be needed */
	dirdbData[node].newmdbref=DIRDB_NO_MDBREF; /* this should not be needed */
	dirdbData[node].adbref=DIRDB_NO_ADBREF; /* this should not be needed */
	dirdbData[node].newadbref=DIRDB_NO_ADBREF; /* this should not be needed */

#ifdef DIRDB_DEBUG
	dumpdirdb();
#endif
	if (parent!=DIRDB_NOPARENT)
		dirdbUnref(parent);
}

void dirdbGetname(uint32_t node, char *name /*NAME_MAX+1*/)
{
	name[0]=0;
	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbGetname: invalid node #1\n");
		return;
	}
	if (!dirdbData[node].name)
	{
		fprintf(stderr, "dirdbGetname: invalid node #2\n");
		return;
	}
	strcpy(name, dirdbData[node].name);
}

static void dirdbGetFullnameR(uint32_t node, char *name, unsigned int *left, int nobase)
{
	if (dirdbData[node].parent!=DIRDB_NOPARENT)
	{
		dirdbGetFullnameR(dirdbData[node].parent, name, left, nobase);
		if (!*left)
			goto errorout;
		strcat(name, "/");
		(*left)--;
	} else
		if (nobase)
			return;

	if ((*left)<=strlen(dirdbData[node].name))
		goto errorout;
	strcat(name, dirdbData[node].name);
	(*left)-=strlen(dirdbData[node].name);
	return;
errorout:
	fprintf(stderr, "dirdbGetFullname: string grows to long\n");
	return;
}

void dirdbGetFullName(uint32_t node, char *name /* PATH_MAX+1, ends not with a / */, int flags)
{
	unsigned int i = PATH_MAX;
	*name=0;
	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbGetFullname: invalid node\n");
		return;
	}
	dirdbGetFullnameR(node, name, &i, flags&DIRDB_FULLNAME_NOBASE);
	if (flags&DIRDB_FULLNAME_ENDSLASH)
		if (strlen(name)+1<PATH_MAX)
			strcat(name, "/");
}

void dirdbFlush(void)
{
	char path[PATH_MAX+1];
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
			if (!dirdbData[i].refcount)
			{
				dirdbData[i].refcount++;
				dirdbUnref(i);
			}
	}

	if ((strlen(cfConfigDir)+11)>PATH_MAX)
	{
		fprintf(stderr, "dirdb: CPDIRDB.DAT path is too long\n");
		return;
	}
	strcpy(path, cfConfigDir);
	strcat(path, "CPDIRDB.DAT");

	if ((f=open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE))<0)
	{
		perror("open(cfConfigDir/CPDIRDB.DAT)");
		return;
	}

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
			buf32=uint32_little(dirdbData[i].mdbref);
			if (write(f, &buf32, sizeof(uint32_t))!=sizeof(uint32_t))
				goto writeerror;
			buf32=uint32_little(dirdbData[i].adbref);
			if (write(f, &buf32, sizeof(uint32_t))!=sizeof(uint32_t))
				goto writeerror;
			if (dirdbData[i].name)
			{
				if (write(f, dirdbData[i].name, len)!=len)
					goto writeerror;
			}
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
	if (node>=dirdbNum)
		return DIRDB_NOPARENT;
	if ((retval=dirdbData[node].parent)!=DIRDB_NOPARENT)
		dirdbData[dirdbData[node].parent].refcount++;
	return retval;
}

#if 0
void dirdbMakeMdbRef(uint32_t node, uint32_t mdbref)
{
#ifdef DIRDB_DEBUG
	fprintf(stderr, "dirdbMakeMdbRef(node 0x%08x, mdbref 0x%08x)\n", node, mdbref);
#endif
	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbMakeMdbRef: invalid node\n");
		return;
	}
	if (mdbref==DIRDB_NO_MDBREF)
	{
		if (dirdbData[node].mdbref!=DIRDB_NO_MDBREF)
		{
			dirdbData[node].mdbref=DIRDB_NO_MDBREF;
			dirdbDirty=1;
			dirdbUnref(node);
		}
	} else {
		int doref = (dirdbData[node].mdbref==DIRDB_NO_MDBREF);
		dirdbData[node].mdbref=mdbref;
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
		dirdbData[i].newmdbref=DIRDB_NO_MDBREF;
		dirdbData[i].newadbref=DIRDB_NO_ADBREF; /* is this actually needed? */
	}

	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbTagSetParent: invalid node\n");
		return;
	}
	tagparentnode = node;
	dirdbRef(node);
}

void dirdbMakeMdbAdbRef(uint32_t node, uint32_t mdbref, uint32_t adbref)
{
	if (node>=dirdbNum)
	{
		fprintf(stderr, "dirdbMakeMdbRef: invalid node\n");
		return;
	}
	/* the madness below is in order to keep track of references the correct way */
	if (mdbref==DIRDB_NO_MDBREF)
	{
		if (dirdbData[node].newmdbref!=DIRDB_NO_MDBREF)
		{
			dirdbData[node].newmdbref=DIRDB_NO_MDBREF;
			dirdbUnref(node);
		} /* else, no change */
	} else {
		if (dirdbData[node].mdbref==DIRDB_NO_MDBREF)
		{
			dirdbData[node].newmdbref=mdbref;
			dirdbRef(node);
		} else {
			/*dirdbUnref(node);*/
			dirdbData[node].newmdbref=mdbref;
			/*dirdbRef(node);  overkill to unref and re-ref just for the name's sake*/
		}
	}

	dirdbData[node].newadbref = adbref;
}

void dirdbTagCancel(void)
{
	uint32_t i;
	for (i=0;i<dirdbNum;i++)
	{
		if (dirdbData[i].newmdbref!=DIRDB_NO_MDBREF)
		{
			dirdbData[i].newmdbref=DIRDB_NO_MDBREF;
			dirdbUnref(i);
		}
		dirdbData[i].newadbref = DIRDB_NO_ADBREF;
	}
	if (tagparentnode==DIRDB_NOPARENT)
	{
		fprintf(stderr, "dirdbTagCancel: parent is not set\n");
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
		if (dirdbData[i].parent==node)
		{
			dirdbData[i].adbref=dirdbData[i].newadbref;
			if (dirdbData[i].newmdbref==dirdbData[i].mdbref)
			{
				if (dirdbData[i].mdbref==DIRDB_NO_MDBREF)
				{
					/* probably a dir */
					_dirdbTagRemoveUntaggedAndSubmit(i);
				} else {
					/* mdbref changed */
					dirdbData[i].mdbref=dirdbData[i].newmdbref;
					dirdbData[i].newmdbref=DIRDB_NO_MDBREF;
					dirdbUnref(i);
				}
			} else {
				if (dirdbData[i].mdbref==DIRDB_NO_MDBREF)
				{
					dirdbData[i].mdbref=dirdbData[i].newmdbref;
					dirdbData[i].newmdbref=DIRDB_NO_MDBREF;
					/* no need to unref/ref, since we are
					 * balanced. Since somebody can have
					 * named a file, the same name
					 * a directory used to have, we need
					 * to scan for siblings.
					 */
					_dirdbTagRemoveUntaggedAndSubmit(i);
					/* this can probably be done more elegant later */
				} else if (dirdbData[i].newmdbref==DIRDB_NO_MDBREF)
				{
					dirdbData[i].mdbref=DIRDB_NO_MDBREF;
					dirdbUnref(i);
					/* same as above regarding renaming */
					_dirdbTagRemoveUntaggedAndSubmit(i);
					/* this can probably be done more elegant later */
				} else {
					dirdbData[i].mdbref=dirdbData[i].newmdbref;
					dirdbData[i].newmdbref=DIRDB_NO_MDBREF;
					dirdbUnref(i);
				}
			}
		}
	}
}

void dirdbTagRemoveUntaggedAndSubmit(void)
{
	if (tagparentnode==DIRDB_NOPARENT)
	{
		fprintf(stderr, "dirdbTagRemoveUntaggedAndSubmit: parent is not set\n");
		return;
	}
	/* if parent has changed mdbref, we can't detect this.. NB NB NB */
	_dirdbTagRemoveUntaggedAndSubmit(tagparentnode);
	dirdbUnref(tagparentnode);
	tagparentnode=DIRDB_NOPARENT;
	dirdbDirty=1;
}

int dirdbGetMdbAdb(uint32_t *dirdbnode, uint32_t *mdbnode, uint32_t *adbref, int *first)
{
	if (*first)
	{
		*dirdbnode=0;
		*adbref=DIRDB_NO_ADBREF;
		*first=0;
	} else
		(*dirdbnode)++;
	for (;*dirdbnode<dirdbNum;(*dirdbnode)++)
		if ((dirdbData[*dirdbnode].name)&&(dirdbData[*dirdbnode].mdbref!=DIRDB_NO_MDBREF))
		{
			*mdbnode=dirdbData[*dirdbnode].mdbref;
			*adbref=dirdbData[*dirdbnode].adbref;
			return 0;
		}
	return -1;
}
