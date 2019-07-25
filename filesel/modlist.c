/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * the file-object code used by the File selector ][
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
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss040831   Stian Skjelstad <stian@nixia.no>
 *    -updated fs12name to not crash anymore
 *    -removed modlist->pathtothis
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "types.h"
#include "dirdb.h"
#include "modlist.h"
#include "stuff/compat.h"

void modlist_free(struct modlist *modlist)
{
	unsigned int i;
	for (i=0;i<modlist->num;i++)
	{
		dirdbUnref(modlist->files[i]->dirdbfullpath);
		free(modlist->files[i]);
	}
	if (modlist->max)
		free(modlist->files);
	free(modlist);
}

struct modlistentry *modlist_getcur(const struct modlist *modlist) /* does not Ref() */
{
	return modlist_get(modlist, modlist->pos);
}

struct modlistentry *modlist_get(const struct modlist *modlist, unsigned int index) /* does not Ref() */
{
	if (!(modlist->num))
		return NULL; /* should never happen */
	/*
	if (index<0)
		return modlist->files[0];
	*/
	if (index>=modlist->num)
		return modlist->files[modlist->num-1];
	return modlist->files[index];
}

void modlist_append(struct modlist *modlist, struct modlistentry *entry)
{
	if (!entry)
		return;
	if (!modlist->max) /* Keeps Electric Fence happy */
	{
		modlist->max=50;
		modlist->files=malloc(modlist->max*sizeof(modlist->files[0]));
	} else if (modlist->num==modlist->max)
	{
		modlist->max+=50;
		modlist->files=realloc(modlist->files, modlist->max*sizeof(modlist->files[0]));
	}
	dirdbRef(entry->dirdbfullpath);
	modlist->files[modlist->num]=malloc(sizeof(struct modlistentry)); /* TODO... make a cache pool of these */
	memcpy(modlist->files[modlist->num], entry, sizeof(struct modlistentry));
	modlist->num++;
}

void modlist_remove_all_by_path(struct modlist *modlist, uint32_t ref)
{
	unsigned int i;
	for (i=0;i<modlist->num;)
	{
		if (modlist->files[i]->dirdbfullpath == ref)
			modlist_remove(modlist, i, 1);
		else
			i++;
	}
}

void modlist_remove(struct modlist *modlist, unsigned int index, unsigned int count)
{
	unsigned int i;
	assert ((index+count) <= (modlist->num));
	if (index>=modlist->num)
		return;
	if (index+count>modlist->num)
		count=modlist->num-index;
	for (i=index;i<(index+count);i++)
	{
		dirdbUnref(modlist->files[i]->dirdbfullpath);
		free(modlist->files[i]);
	}
	memmove(&modlist->files[index], &modlist->files[index+count], (modlist->num-index-count)*sizeof(modlist->files[0]));
	modlist->num-=count;
	if ((modlist->max-modlist->num>75))
	{
		modlist->max-=50;
		modlist->files=realloc(modlist->files, modlist->max*sizeof(modlist->files[0]));
	}
	if (!modlist->num)
		modlist->pos = 0;
	else if (modlist->pos >= modlist->num)
		modlist->pos = modlist->num-1;
}

int modlist_find(struct modlist *modlist, const uint32_t path)
{
	unsigned int retval;
	for (retval=0;retval<modlist->num;retval++)
		if (path==modlist->files[retval]->dirdbfullpath)
			return retval;
	return -1;
}

void modlist_swap(struct modlist *modlist, unsigned int index1, unsigned int index2)
{
	struct modlistentry *entry;
	if (index1>=modlist->num)
		return;
	if (index2>=modlist->num)
		return;
	entry = modlist->files[index1];
	modlist->files[index1] = modlist->files[index2];
	modlist->files[index2] = entry;
}

static const char *fuzzycmp12(const char *dst, const char *src)
{
	char DST, SRC;
	int len=12;
	while ((*dst)&&len)
	{
		len--;
		DST=toupper(*dst);
		SRC=toupper(*src);
		if (DST==SRC)
		{
			dst++;
			src++;
		} else
			break;
	}
	return dst;
}

int modlist_fuzzyfind(struct modlist *modlist, const char *filename)
{
	unsigned int retval=0;
	int hitscore=0;
	unsigned int i;
	unsigned int len=strlen(filename);
	if (!len)
		return 0;
	for (i=0;i<modlist->num;i++)
	{
		const char *temp=modlist->files[i]->shortname;
		const char *diff=fuzzycmp12(temp, filename);
		int score=diff-temp;
		if ((unsigned)score==len)
			return i;
		else if (score>hitscore)
		{
			retval=i;
			hitscore=score;
		}
	}
	return retval;
}

static int mlecmp(const void *a, const void *b)
{
	const struct modlistentry *e1=*(struct modlistentry **)a, *e2=*(struct modlistentry **)b;
	if ((e1->flags&(MODLIST_FLAG_DIR|MODLIST_FLAG_ARC|MODLIST_FLAG_FILE|MODLIST_FLAG_DRV))!=(e2->flags&(MODLIST_FLAG_DIR|MODLIST_FLAG_ARC|MODLIST_FLAG_FILE|MODLIST_FLAG_DRV)))
	{
		if ((e1->flags&(MODLIST_FLAG_DIR|MODLIST_FLAG_ARC|MODLIST_FLAG_FILE|MODLIST_FLAG_DRV))>(e2->flags&(MODLIST_FLAG_DIR|MODLIST_FLAG_ARC|MODLIST_FLAG_FILE|MODLIST_FLAG_DRV)))
			return 1;
		else
			return -1;
	}
		return memicmp(e1->shortname, e2->shortname, 12);
	return 0;
}

void modlist_sort(struct modlist *modlist)
{
	qsort(modlist->files, modlist->num, sizeof(*modlist->files), mlecmp);
}

struct modlist *modlist_create(void)
{
	/* TODO ARCS */
	/*
	DIR *dir;
	*/
	struct modlist *retval=calloc(sizeof(struct modlist), 1);

	return retval;
}

void modlist_append_modlist(struct modlist *target, struct modlist *source)
{
	unsigned int i;
	for (i=0;i<source->num;i++)
		modlist_append(target, modlist_get(source, i));
}

void fs12name(char *shortname, const char *source)
{
	char temppath[NAME_MAX+1];
	char *lastdot;
	int length=strlen(source);
	strcpy(temppath, source);

/*
	if (length)
		if (temppath[length-1]=='/')
		{
			temppath[length-1]=0;
			length--;
		}*/

#if 0
	if (entry->flags&MODLIST_FLAG_FILE) /* this makes life more easy */
	{
#endif
		if (length>=8)
			if (!strcasecmp(temppath+length-8, ".tar.bz2"))
			{
				strcpy(temppath+length-8, ".tbz");
				length-=4;
/*
				entry->flags=MODLIST_FLAG_ARC;
*/
			}
		if (length>=7)
			if (!strcasecmp(temppath+length-7, ".tar.gz"))
			{
				strcpy(temppath+length-7, ".tgz");
				length-=3;
/*
				entry->flags=MODLIST_FLAG_ARC;
*/
			}
		if (length>=6)
			if (!strcasecmp(temppath+length-6, ".tar.Z"))
			{
				strcpy(temppath+length-6, ".tgz");
				length-=2;
/*
				entry->flags=MODLIST_FLAG_ARC;
*/
			}
#if 0
	}
#endif

	if ((lastdot=rindex(temppath+1, '.'))) /* we allow files to start with . */
	{
		int delta=lastdot-temppath;
		if (strlen(lastdot)>4)
			lastdot[4]=0;
		if ((delta)<=8)
		{
			strncpy(shortname, temppath, delta);
			strncpy(shortname+delta, "        ", 8-delta);
		} else
			strncpy(shortname, temppath, 8);
		strncpy(shortname+8, lastdot, 4);
		if ((length=strlen(lastdot))<4)
			strncpy(shortname+8+length, "    ", 4-length);
	} else {
		strncpy(shortname, temppath, 12);
		if ((length=strlen(temppath))<12)
			strncpy(shortname+length, "            ", 12-length);
	}
}
