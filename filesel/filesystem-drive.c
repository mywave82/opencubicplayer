/* OpenCP Module Player
 * copyright (c) 2020-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to hold a the glue logic for drives ( file: setup: )
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
#include "filesystem-drive.h"
#include "../stuff/compat.h"

struct dmDrive *dmDrives=0;

struct dmDrive *RegisterDrive(const char *dmDrive, struct ocpdir_t *basedir, struct ocpdir_t *cwd)
{
	struct dmDrive *ref = dmDrives;

	while (ref)
	{
		if (!strcasecmp(ref->drivename, dmDrive))
			return ref;
		ref = ref->next;
	}

	ref=calloc(1, sizeof(struct dmDrive));
	strcpy(ref->drivename, dmDrive);

	basedir->ref (basedir);
	ref->basedir = basedir;

	cwd->ref (cwd);
	ref->cwd = cwd;

	ref->next=dmDrives;
	dmDrives=ref;

	return ref;
}

struct dmDrive *dmFindDrive(const char *drivename) /* to get the correct drive from a given string */
{
	struct dmDrive *cur=dmDrives;
	while (cur)
	{
		if (!strncasecmp(cur->drivename, drivename, strlen(cur->drivename)))
			return cur;
		cur=cur->next;
	}
	return NULL;
}

void filesystem_drive_init (void)
{

}

void filesystem_drive_done (void)
{
	while (dmDrives)
	{
		struct dmDrive *next = dmDrives->next;

		dmDrives->basedir->unref (dmDrives->basedir);
		dmDrives->cwd->unref (dmDrives->cwd);

		free (dmDrives);

		dmDrives = next;
	}
}

static int _filesystem_resolve_dirdb_dir (uint32_t ref, struct dmDrive **drive, struct ocpdir_t **dir)
{
	uint32_t parent;
	assert (drive);
	assert (dir);

	parent = dirdbGetParentAndRef (ref, dirdb_use_drive_resolve);
	if (parent == DIRDB_CLEAR)
	{
		struct dmDrive *iter;
		const char *str1 = 0;
		dirdbGetName_internalstr (ref, &str1);
		for (iter = dmDrives; iter; iter = iter->next)
		{
			const char *str2 = 0;
			dirdbGetName_internalstr (iter->basedir->dirdb_ref, &str2);
			if (!strcasecmp (str1, str2))
			{
				*drive = iter;
				*dir = iter->basedir;
				iter->basedir->ref (iter->basedir);
				return 0;
			}
		}
		dirdbUnref (parent, dirdb_use_drive_resolve);
		return -1;
	} else {
		struct ocpdir_t *parentdir = 0;

		if (_filesystem_resolve_dirdb_dir (parent, drive, &parentdir))
		{
			return -1;
		}

		*dir = parentdir->readdir_dir (parentdir, ref);

		if (*dir)
		{
			parentdir->unref (parentdir);
			dirdbUnref (parent, dirdb_use_drive_resolve);

			return 0;
		} else {
			struct ocpfile_t *file = parentdir->readdir_file (parentdir, ref);

			parentdir->unref (parentdir);
			dirdbUnref (parent, dirdb_use_drive_resolve);

			if (file)
			{
				char *filetype = 0;
				const char *orig = 0;

				dirdbGetName_internalstr (ref, &orig);
				getext_malloc (orig, &filetype);
				if (filetype)
				{
					*dir = ocpdirdecompressor_check (file, filetype);
					free (filetype);
					file->unref (file);
					file = 0;
					if (*dir)
					{
						return 0;
					}
				}
			}
			return -1;
		}
	}
}

int filesystem_resolve_dirdb_dir (uint32_t ref, struct dmDrive **drive, struct ocpdir_t **dir)
{
	struct dmDrive  *retval_drive = 0;
	struct ocpdir_t *retval_dir = 0;
	if (_filesystem_resolve_dirdb_dir (ref, &retval_drive, &retval_dir))
	{
		if (drive)
		{
			*drive = 0;
		}
		if (dir)
		{
			*dir = 0;
		}
		return -1;
	} else {
		if (drive)
		{
			*drive = retval_drive;
		}
		if (dir)
		{
			*dir = retval_dir;
		} else {
			retval_dir->unref (retval_dir);
		}
		return 0;
	}
}

int filesystem_resolve_dirdb_file (uint32_t ref, struct dmDrive **drive, struct ocpfile_t **file)
{
	uint32_t parentdir;
	struct dmDrive  *retval_drive = 0;
	struct ocpdir_t *retval_dir = 0;
	struct ocpfile_t *retval_file = 0;
	if (drive)
	{
		*drive = 0;
	}
	if (file)
	{
		*file = 0;
	}


	parentdir = dirdbGetParentAndRef (ref, dirdb_use_drive_resolve);
	if (parentdir == DIRDB_CLEAR)
	{
		return -1;
	}
	if (_filesystem_resolve_dirdb_dir (parentdir, &retval_drive, &retval_dir))
	{
		dirdbUnref (parentdir, dirdb_use_drive_resolve);
		return -1;
	}
	dirdbUnref (parentdir, dirdb_use_drive_resolve);

	retval_file = retval_dir->readdir_file (retval_dir, ref);

	retval_dir->unref (retval_dir);
	retval_dir = 0;

	if (!retval_file)
	{
		return -1;
	}  else {
		if (drive)
		{
			*drive = retval_drive;
		}
		if (file)
		{
			*file = retval_file;
		} else {
			retval_file->unref (retval_file);
		}
		return 0;
	}
}

struct dmDrive *ocpdir_get_drive (struct ocpdir_t *dir)
{
	struct dmDrive *iter;
	if (!dir)
	{
		return 0;
	}
	while (dir->parent)
	{
		dir = dir->parent;
	}
	for (iter = dmDrives; iter; iter = iter->next)
	{
		if (iter->basedir->dirdb_ref == dir->dirdb_ref)
		{
			return iter;
		}
	}
	return 0;
}
