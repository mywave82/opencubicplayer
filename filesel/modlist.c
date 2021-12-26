/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
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
 *    -updated fs_8dot3_name to not crash anymore
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
#include "filesystem.h"
#include "filesystem-drive.h"
#include "mdb.h"
#include "modlist.h"
#include "stuff/compat.h"
#include "stuff/poutput.h"
#include "stuff/utf-8.h"

void modlist_free (struct modlist *modlist)
{
	unsigned int i;
	for (i=0;i<modlist->num;i++)
	{
		if (modlist->files[i].dir)
		{
			modlist->files[i].dir->unref (modlist->files[i].dir);
			modlist->files[i].dir = 0;
		}
		if (modlist->files[i].file)
		{
			modlist->files[i].file->unref (modlist->files[i].file);
			modlist->files[i].file = 0;
		}
	}
	free (modlist->files);
	free (modlist->sortindex);
	free(modlist);
}

struct modlistentry *modlist_getcur (const struct modlist *modlist) /* does not Ref() */
{
	return modlist_get(modlist, modlist->pos);
}

struct modlistentry *modlist_get (const struct modlist *modlist, unsigned int index) /* does not Ref() */
{
	if (! modlist->num)
	{
		return NULL; /* should never happen */
	}
	if (index >= modlist->num)
	{
		index = modlist->num - 1;
	}
	return &modlist->files[modlist->sortindex[index]];
}

void modlist_append (struct modlist *modlist, struct modlistentry *entry)
{
	if (!entry)
		return;
	if (modlist->num==modlist->max)
	{
		int *newindex;
		struct modlistentry *newfiles;

		newfiles = realloc(modlist->files, (modlist->max + 50) *sizeof(modlist->files[0]));
		if (!newfiles)
		{ /* out of memory */
			fprintf (stderr, "modlist_append: out of memory\n");
			return;
		}
		modlist->files=newfiles;

		newindex = realloc (modlist->sortindex, (modlist->max + 50) *sizeof(modlist->sortindex[0]));
		if (!newindex)
		{ /* out of memory */
			fprintf (stderr, "modlist_append: out of memory\n");
			return;
		}
		modlist->sortindex = newindex;

		modlist->max += 50;
	}
	modlist->files[modlist->num] = *entry;
	modlist->sortindex[modlist->num] = modlist->num;

	if (entry->file)
	{
		entry->file->ref (entry->file);
	}
	if (entry->dir)
	{
		entry->dir->ref (entry->dir);
	}
	modlist->num++;
}

void modlist_append_dir (struct modlist *modlist, struct ocpdir_t *dir)
{
	struct modlistentry entry = {0};
	const char *childpath = 0;

	if (!dir)
	{
		return;
	}

	entry.dir = dir; /* modlist_append will do a ref */
	dirdbGetName_internalstr (dir->dirdb_ref, &childpath);
	utf8_XdotY_name (8, 3, entry.utf8_8_dot_3, childpath);
	utf8_XdotY_name (16, 3, entry.utf8_16_dot_3, childpath);

	entry.mdb_ref = UINT32_MAX;

	modlist_append (modlist, &entry);
}

void modlist_append_dotdot (struct modlist *modlist, struct ocpdir_t *dir)
{
	struct modlistentry entry = {0};

	if (!dir)
	{
		return;
	}

	entry.dir = dir; /* modlist_append will do a ref */
	entry.flags = MODLIST_FLAG_DOTDOT;
	strcpy (entry.utf8_8_dot_3,  "..");
	strcpy (entry.utf8_16_dot_3, "..");

	entry.mdb_ref = UINT32_MAX;

	modlist_append (modlist, &entry);
}

void modlist_append_drive (struct modlist *modlist, struct dmDrive *drive)
{
	struct modlistentry entry = {0};
	const char *childpath = 0;

	if (!drive)
	{
		return;
	}

	entry.dir = drive->cwd; /* modlist_append will do a ref */
	entry.flags |= MODLIST_FLAG_DRV;
	dirdbGetName_internalstr (drive->basedir->dirdb_ref, &childpath);
	utf8_XdotY_name (8, 3, entry.utf8_8_dot_3, childpath);
	utf8_XdotY_name (16, 3, entry.utf8_16_dot_3, childpath);

	entry.mdb_ref = UINT32_MAX;

	modlist_append (modlist, &entry);
}

void modlist_append_file (struct modlist *modlist, struct ocpfile_t *file)
{
	struct modlistentry entry = {0};
	const char *childpath = 0;

	if (!file)
	{
		return;
	}

	entry.file = file; /* modlist_append will do a ref */
	childpath = file->filename_override (file);
	if (!childpath)
	{
		dirdbGetName_internalstr (file->dirdb_ref, &childpath);
	}
	utf8_XdotY_name (8, 3, entry.utf8_8_dot_3, childpath);
	utf8_XdotY_name (16, 3, entry.utf8_16_dot_3, childpath);

	entry.mdb_ref = mdbGetModuleReference2 (file->dirdb_ref, file->filesize (file));

	modlist_append (modlist, &entry);
}


void modlist_remove_all_by_path(struct modlist *modlist, uint32_t ref)
{
	unsigned int i;
	for (i=0;i<modlist->num;)
	{
		if ( modlist->files[modlist->sortindex[i]].file &&
		    (modlist->files[modlist->sortindex[i]].file->dirdb_ref == ref) )
		{
			modlist_remove(modlist, i);
		} else if ( modlist->files[modlist->sortindex[i]].dir &&
		           (modlist->files[modlist->sortindex[i]].dir->dirdb_ref == ref) )
		{
			modlist_remove(modlist, i);
		} else {
			i++;
		}
	}
}

void modlist_clear(struct modlist *modlist)
{
	int i;
	for (i=0;i<modlist->num;i++)
	{
		if (modlist->files[i].dir)
		{
			modlist->files[i].dir->unref (modlist->files[i].dir);
			modlist->files[i].dir = 0;
		}
		if (modlist->files[i].file)
		{
			modlist->files[i].file->unref (modlist->files[i].file);
			modlist->files[i].file = 0;
		}
	}
	modlist->num = 0;
}

void modlist_remove(struct modlist *modlist, unsigned int index) /* by sortindex */
{
	unsigned int i;
	assert (index < modlist->num);
	unsigned int realindex = modlist->sortindex[index];

	if (modlist->files[realindex].file)
	{
		modlist->files[realindex].file->unref (modlist->files[realindex].file);
	}
	if (modlist->files[realindex].dir)
	{
		modlist->files[realindex].dir->unref (modlist->files[realindex].dir);
	}
	memmove(&modlist->files[realindex], &modlist->files[realindex+1], (modlist->num - realindex - 1) * sizeof(modlist->files[0]));
	memmove(&modlist->sortindex[index], &modlist->sortindex[index+1], (modlist->num - index - 1) * sizeof (modlist->sortindex[0]));
	modlist->num -= 1;

	/* repair the sort-index */
	for (i = 0; i < modlist->num; i++)
	{
		if (modlist->sortindex[i] >= realindex)
		{
			modlist->sortindex[i]--;
		}
	}

#if 0
	if ((modlist->max - modlist->num>75))
	{
		modlist->max- = 50;
		modlist->files = realloc (modlist->files, modlist->max * sizeof(modlist->files[0]));
		modlist->sortindex = realloc (modlist->sortindex, modlist->max * sizeof (modlist->sortindex[0]));
	}
#endif
	if (!modlist->num)
		modlist->pos = 0;
	else if (modlist->pos >= modlist->num)
		modlist->pos = modlist->num - 1;
}

int modlist_find(struct modlist *modlist, const uint32_t path)
{
	unsigned int retval;
	for (retval=0; retval < modlist->num; retval++)
	{
		int realindex = modlist->sortindex[retval];
		if ( modlist->files[realindex].file &&
		    (modlist->files[realindex].file->dirdb_ref == path) )
		{
			return retval;
		}
		if ( modlist->files[realindex].dir &&
		    (modlist->files[realindex].dir->dirdb_ref == path) )
		{
			return retval;
		}
	}
	return -1;
}

void modlist_swap(struct modlist *modlist, unsigned int index1, unsigned int index2)
{
	int entry;
	entry = modlist->sortindex[index1];
	modlist->sortindex[index1] = modlist->sortindex[index2];
	modlist->sortindex[index2] = entry;
}

static const char *fuzzycmp(const char *dst, const char *src)
{
	char DST, SRC;
	while ((*dst)&&(*src))
	{
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

#warning input is CP437, search is done on UTF-8
int modlist_fuzzyfind(struct modlist *modlist, const char *filename)
{
	unsigned int retval=0;
	int hitscore=0;
	unsigned int i;
	unsigned int len = strlen(filename);
	if (!len)
		return 0;
	for (i=0;i<modlist->num;i++)
	{
		const char *temp = 0;
		const char *diff;
		int score;
		int index = modlist->sortindex[i];
		struct modlistentry *m = &modlist->files[index];

		dirdbGetName_internalstr (m->file ? m->file->dirdb_ref : m->dir->dirdb_ref, &temp);
		diff = fuzzycmp(temp, filename);
		score = diff - temp;

		if ((unsigned)score==len)
		{
			return i;
		} else if (score>hitscore)
		{
			retval=i;
			hitscore=score;
		}

		diff = fuzzycmp(m->utf8_16_dot_3, filename);
		score = diff - m->utf8_16_dot_3;
		if ((unsigned)score==len)
		{
			return i;
		} else if (score>hitscore)
		{
			retval=i;
			hitscore=score;
		}
	}
	return retval;
}

static struct modlist *sorting;
static int mlecmp_score (const struct modlistentry *e1)
{
	int i1;

	if (e1->dir)
	{
		if (e1->flags & MODLIST_FLAG_DOTDOT)
		{
			i1 = 16;
		} else if (e1->flags & MODLIST_FLAG_DRV)
		{
			i1 = 0;
		} else {
			i1 = 8;

			if (e1->dir->is_archive)
			{
				i1 = 4;
			}
			if (e1->dir->is_playlist)
			{
				i1 = 2;
			}
		}
	} else {
		i1 = 1;
	}

	return i1;
}
static int mlecmp (const void *a, const void *b)
{
	int _1 = *(int *)a;
	int _2 = *(int *)b;
	const struct modlistentry *e1 = &sorting->files[_1];
	const struct modlistentry *e2 = &sorting->files[_2];

	int i1 = mlecmp_score (e1);
	int i2 = mlecmp_score (e2);

	const char *n1, *n2;

	if (i1 != i2)
	{
		return i2 - i1;
	}

	dirdbGetName_internalstr (e1->file ? e1->file->dirdb_ref : e1->dir->dirdb_ref, &n1);
	dirdbGetName_internalstr (e2->file ? e2->file->dirdb_ref : e2->dir->dirdb_ref, &n2);

	return strcasecmp(n1, n2);
}

void modlist_sort (struct modlist *modlist)
{
	sorting = modlist; /* dirty HACK that is not thread-safe / reentrant what so ever */
	qsort(modlist->sortindex, modlist->num, sizeof(modlist->sortindex[0]), mlecmp);
	sorting = 0;
}

struct modlist *modlist_create (void)
{
	/* TODO ARCS */
	/*
	DIR *dir;
	*/
	struct modlist *retval=calloc(sizeof(struct modlist), 1);

	return retval;
}

void modlist_append_modlist (struct modlist *target, struct modlist *source)
{
	unsigned int i;
	for (i=0;i<source->num;i++)
		modlist_append(target, modlist_get(source, i));
}
