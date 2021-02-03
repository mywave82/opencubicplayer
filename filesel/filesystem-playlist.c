/* OpenCP Module Player
 * copyright (c) 2020-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Common code for M3U and PLS parser
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-drive.h"
#include "filesystem-playlist.h"
#include "modlist.h"
#include "pfilesel.h"
#include "stuff/compat.h"

struct playlist_instance_t *playlist_root;

struct playlist_instance_dir_search_t
{
	struct playlist_instance_t *owner;
	int base, count;
	int skiplen;
};

void playlist_dir_resolve_strings (struct playlist_instance_t *self)
{
	uint32_t dirdb_ref;

	/* are we done, if so clear the string TODO list */
	if (self->string_pos >= self->string_count)
	{
		int i;
		for (i=0; i < self->string_count; i++)
		{
			free (self->string_data[i].string);
		}
		self->string_count = 0;
		self->string_pos = 0;
		return;
	}

	dirdb_ref = dirdbResolvePathWithBaseAndRef (self->head.parent->dirdb_ref, self->string_data[self->string_pos].string, self->string_data[self->string_pos].flags, dirdb_use_dir);
	if (dirdb_ref != DIRDB_NOPARENT)
	{
		struct ocpfile_t *file = 0;
		filesystem_resolve_dirdb_file (dirdb_ref, 0, &file);
		dirdbUnref (dirdb_ref, dirdb_use_dir);

		if (file)
		{
			/* can we fit more files? */
			if (self->ocpfile_count >= self->ocpfile_size)
			{
				struct ocpfile_t **re;
				self->ocpfile_size += 64;
				re = realloc (self->ocpfile_data, sizeof (struct ocpfile_t *) * self->ocpfile_size);
				if (!re)
				{
					fprintf (stderr, "playlist_dir_resolve_strings: out of memory!\n");
					self->ocpfile_size -= 64;
					return;
				}
				self->ocpfile_data = re;
			}
			/* add file reference to our list */
			self->ocpfile_data[self->ocpfile_count++] = file;
		}
	}
	self->string_pos++;
}

static void playlist_dir_ref (struct ocpdir_t *_self)
{
	struct playlist_instance_t *self = (struct playlist_instance_t *)_self;
	self->head.refcount++;
}

static void playlist_dir_unref (struct ocpdir_t *_self)
{
	struct playlist_instance_t **prev, *iter;
	int i;
	struct playlist_instance_t *self = (struct playlist_instance_t *)_self;

	self->head.refcount--;
	if (self->head.refcount)
	{
		return;
	}
	if (self->head.parent)
	{
		self->head.parent->unref (self->head.parent);
		self->head.parent = 0;
	}
	for (i=0; i < self->string_count; i++)
	{
		free (self->string_data[i].string);
	}
	free (self->string_data);
	for (i=0; i < self->ocpfile_count; i++)
	{
		self->ocpfile_data[i]->unref (self->ocpfile_data[i]);
	}
	free (self->ocpfile_data);

	dirdbUnref (self->head.dirdb_ref, dirdb_use_dir);

	prev = &playlist_root;
	for (iter = *prev; iter; iter = iter->next)
	{
		if (iter == self)
		{
			*prev = iter->next;
			break;
		}
		prev = &iter->next;
	}
	free (self);
}

struct playlist_dir_readdir_handle_t
{
	struct playlist_instance_t *owner;
	void (*callback_file)(void *token, struct ocpfile_t *);
	void *token;
	int nextfile;
};

static ocpdirhandle_pt playlist_dir_readflatdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *), void *token)
{
	struct playlist_instance_t *self = (struct playlist_instance_t *)_self;
	struct playlist_dir_readdir_handle_t *handle;
	handle = calloc (1, sizeof (*handle));
	if (!handle)
	{
		return 0;
	}

#if 0
	if (self->string_count)
	{
		qsort (self->string_data, self->string_count, sizeof (char *), cmpstringp);
	}
#endif

	self->head.ref (&self->head);
	handle->owner = self;
	handle->callback_file = callback_file;
	handle->token = token;

	return handle;
}

static ocpdirhandle_pt playlist_dir_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                     void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	return playlist_dir_readflatdir_start (self, callback_file, token);
}

static void playlist_dir_readdir_cancel (ocpdirhandle_pt p)
{
	struct playlist_dir_readdir_handle_t *handle = p;
	handle->owner->head.unref (&handle->owner->head);
	free (handle);
}

static int playlist_dir_readdir_iterate (ocpdirhandle_pt p)
{
	struct playlist_dir_readdir_handle_t *handle = p;

	if (handle->owner->string_count)
	{
		playlist_dir_resolve_strings (handle->owner);
		return 1;
	}
	if (handle->nextfile >= handle->owner->ocpfile_count)
	{
		return 0;
	}
	handle->callback_file (handle->token, handle->owner->ocpfile_data[handle->nextfile]);
	handle->nextfile++;
	return 1;
}

static struct ocpdir_t *playlist_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	return 0; /* this always fails */
}

static struct ocpfile_t *playlist_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct playlist_instance_t *self = (struct playlist_instance_t *)_self;
	int i;

	if (self->string_count)
	{
		playlist_dir_resolve_strings (self);
		return 0;
	}

	for (i = 0; i < self->ocpfile_count; i++)
	{
		if (self->ocpfile_data[i]->dirdb_ref == dirdb_ref)
		{
			self->ocpfile_data[i]->ref (self->ocpfile_data[i]);
			return self->ocpfile_data[i];
		}
	}
	return 0;
}

void playlist_add_string (struct playlist_instance_t *self, char *string, int flags)
{
	if (self->string_count >= self->string_size)
	{
		struct playlist_string_entry_t *temp;
		self->string_size += 64;
		temp = realloc (self->string_data, self->string_size * sizeof (self->string_data[0]));
		if (!temp)
		{
			fprintf (stderr, "playlist_add_string: out of memory!\n");
			self->string_size -= 64;
			free (string);
			return;
		}
		self->string_data = temp;
	}
	self->string_data[self->string_count].string = string;
	self->string_data[self->string_count].flags = flags;
	self->string_count++;
}

/* steals dirdb_ref */
struct playlist_instance_t *playlist_instance_allocate (struct ocpdir_t *parent, uint32_t dirdb_ref)
{
	struct playlist_instance_t *retval;

	retval = calloc (sizeof (*retval), 1);

	if (!retval)
	{
		fprintf (stderr, "playlist_instance_allocate: out of memory\n");
		return 0;
	}

	ocpdir_t_fill (&retval->head,
	                playlist_dir_ref,
	                playlist_dir_unref,
			parent,
	                playlist_dir_readdir_start,
	                playlist_dir_readflatdir_start,
	                playlist_dir_readdir_cancel,
	                playlist_dir_readdir_iterate,
	                playlist_dir_readdir_dir,
	                playlist_dir_readdir_file,
	                0,
			dirdbRef (dirdb_ref, dirdb_use_dir),
	                1, /* refcount */
	                0, /* is_arhive */
	                1 /* playlist */);

	if (parent)
	{
		parent->ref (parent);
	}

	retval->next = playlist_root;
	playlist_root = retval;

	return retval;
}
