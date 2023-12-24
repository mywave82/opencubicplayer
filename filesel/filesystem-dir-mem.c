/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to hold a virtual static directory
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
#include "types.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-dir-mem.h"

struct ocpdir_mem_t
{
	struct ocpdir_t head;
	struct ocpdir_t  **dirs;
	struct ocpfile_t **files;
	int                dirs_count;
	int                files_count;
	int                dirs_size;
	int                files_size;
};

static void ocpdir_mem_ref (struct ocpdir_t *_self)
{
	struct ocpdir_mem_t *self = (struct ocpdir_mem_t *)_self;
	self->head.refcount++;
}

static void ocpdir_mem_unref (struct ocpdir_t *_self)
{
	struct ocpdir_mem_t *self = (struct ocpdir_mem_t *)_self;

	self->head.refcount--;

	if (!self->head.refcount)
	{
		assert (!self->dirs_count); /* children ref-count me, so I can not be killed if I have children */
		assert (!self->files_count);

		dirdbUnref (self->head.dirdb_ref, dirdb_use_dir);

		if (self->head.parent)
		{
			self->head.parent->unref (self->head.parent);
			self->head.parent = 0;
		}

		free (self->dirs);
		free (self->files);
		free (self);
	}
}

struct ocpdir_mem_readdir_handle_t
{
	struct ocpdir_mem_t *self;
	void *token;
	void (*callback_file)(void *token, struct ocpfile_t *);
	void (*callback_dir )(void *token, struct ocpdir_t *);
	int nextdir;
	int nextfile;
};

static ocpdirhandle_pt ocpdir_mem_readdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                         void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct ocpdir_mem_t *self = (struct ocpdir_mem_t *)_self;
	struct ocpdir_mem_readdir_handle_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "ocpdir_mem_readdir_start(): out of memory\n!");
		return 0;
	}
	self->head.ref (&self->head);
	retval->self = self;
	retval->token = token;
	retval->callback_file = callback_file;
	retval->callback_dir = callback_dir;

	return retval;
}

static void ocpdir_mem_readdir_cancel (ocpdirhandle_pt _handle)
{
	struct ocpdir_mem_readdir_handle_t *handle = (struct ocpdir_mem_readdir_handle_t *)_handle;
	handle->self->head.unref (&handle->self->head);
	free (handle);
}

static int ocpdir_mem_readdir_iterate (ocpdirhandle_pt _handle)
{
	struct ocpdir_mem_readdir_handle_t *handle = (struct ocpdir_mem_readdir_handle_t *)_handle;
	if (handle->nextdir < handle->self->dirs_count)
	{
		handle->callback_dir (handle->token, handle->self->dirs[handle->nextdir]);
		handle->nextdir++;
		return 1;
	}
	if (handle->nextfile < handle->self->files_count)
	{
		handle->callback_file (handle->token, handle->self->files[handle->nextfile]);
		handle->nextfile++;
		return 1;
	}
	return 0;
}

static struct ocpdir_t *ocpdir_mem_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct ocpdir_mem_t *self = (struct ocpdir_mem_t *)_self;
	int i;
	for (i = 0; i < self->dirs_count; i++)
	{
		if (self->dirs[i]->dirdb_ref == dirdb_ref)
		{
			self->dirs[i]->ref (self->dirs[i]);
			return self->dirs[i];
		}
	}
	return 0;
}

static struct ocpfile_t *ocpdir_mem_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct ocpdir_mem_t *self = (struct ocpdir_mem_t *)_self;
	int i;
	for (i = 0; i < self->files_count; i++)
	{
		if (self->files[i]->dirdb_ref == dirdb_ref)
		{
			self->files[i]->ref (self->files[i]);
			return self->files[i];
		}
	}
	return 0;
}

struct ocpdir_mem_t *ocpdir_mem_alloc (struct ocpdir_t *parent, const char *name)
{
	struct ocpdir_mem_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "ocpdir_mem_alloc(): out of memory!\n");
		return 0;
	}
	if (parent)
	{
		parent->ref (parent);
	}
	ocpdir_t_fill (
		&retval->head,
		ocpdir_mem_ref,
		ocpdir_mem_unref,
		parent,
		ocpdir_mem_readdir_start,
		0,
		ocpdir_mem_readdir_cancel,
		ocpdir_mem_readdir_iterate,
		ocpdir_mem_readdir_dir,
		ocpdir_mem_readdir_file,
		0,
		dirdbFindAndRef (parent?parent->dirdb_ref:DIRDB_NOPARENT, name, dirdb_use_dir),
		1, /* refcount */
		0, /* is_archive */
		0, /* is_playlist */
	        COMPRESSION_NONE);

	if (parent)
	{
		parent->ref (parent);
	}
	return retval;
}

void ocpdir_mem_add_dir (struct ocpdir_mem_t *self, struct ocpdir_t *child)
{
	int i;
	struct ocpdir_t **temp;

	for (i=0; i < self->dirs_count; i++)
	{
		if (self->dirs[i] == child)
		{
			//child->ref (child);
			return;
		}
	}

	if (self->dirs_count >= self->dirs_size)
	{
		self->dirs_size += 64;
		temp = realloc (self->dirs, (self->dirs_size) * sizeof (self->dirs[0]));
		if (!temp)
		{
			self->dirs_size -= 64;
			fprintf (stderr, "ocpdir_mem_add_dir(): out of memory!\n");
			return;
		}
		self->dirs = temp;
	}

	self->dirs[self->dirs_count] = child;
	child->ref (child);
	self->dirs_count++;
}

void ocpdir_mem_remove_dir (struct ocpdir_mem_t *self, struct ocpdir_t *child)
{
	int i;
	for (i=0; i < self->dirs_count; i++)
	{
		if (self->dirs[i] == child)
		{
			child->unref (child);
			memmove (self->dirs + i, self->dirs + i + 1, sizeof (self->dirs[0]) * self->dirs_count - i - 1);
			self->dirs_count--;
			return;
		}
	}

	fprintf (stderr, "ocpdir_mem_remove_dir(): dir not found\n");
	return;
}

void ocpdir_mem_add_file (struct ocpdir_mem_t *self, struct ocpfile_t *child)
{
	int i;
	struct ocpfile_t **temp;

	for (i=0; i < self->files_count; i++)
	{
		if (self->files[i] == child)
		{
			//child->ref (child);
			return;
		}
	}

	if (self->files_count >= self->files_size)
	{
		self->files_size += 64;
		temp = realloc (self->files, (self->files_size) * sizeof (self->files[0]));
		if (!temp)
		{
			self->files_size -= 64;
			fprintf (stderr, "ocpdir_mem_add_file(): out of memory!\n");
			return;
		}
		self->files = temp;
	}

	self->files[self->files_count] = child;
	child->ref (child);
	self->files_count++;
}

void ocpdir_mem_remove_file (struct ocpdir_mem_t *self, struct ocpfile_t *child)
{
	int i;
	for (i=0; i < self->files_count; i++)
	{
		if (self->files[i] == child)
		{
			child->unref (child);
			memmove (self->files + i, self->files + i + 1, sizeof (self->files[0]) * self->files_count - i - 1);
			self->files_count--;
			return;
		}
	}

	fprintf (stderr, "ocpdir_mem_remove_file(): file not found\n");
	return;
}

struct ocpdir_t *ocpdir_mem_getdir_t (struct ocpdir_mem_t *self) /* typecast */
{
	return &self->head;
}
