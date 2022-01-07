/* OpenCP Module Player
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * MEDIALIBRARY listall dialog
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

static void ocpdir_listall_ref (struct ocpdir_t *self)
{
	self->refcount++;
}

static void ocpdir_listall_unref (struct ocpdir_t *self)
{
	self->refcount--;
}

struct ocpdir_listall_handle_t
{
	void (*callback_file)(void *token, struct ocpfile_t *);
	//void (*callback_dir)(void *token, struct ocpdir_t *);
	void *token;
	int first;
	uint32_t dirdbnode;
};

static ocpdirhandle_pt ocpdir_listall_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct ocpdir_listall_handle_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "ocpdir_listall_readdir_start(): out of memory\n");
		return 0;
	}
	retval->callback_file = callback_file;
	retval->token = token;
	retval->first = 1;
	retval->dirdbnode = DIRDB_NOPARENT;

	return retval;
}

static void ocpdir_listall_readdir_cancel (ocpdirhandle_pt handle)
{
	free (handle);
}

static int ocpdir_listall_readdir_iterate (ocpdirhandle_pt _handle)
{
	uint32_t mdb_ref = UINT32_MAX;
	struct ocpdir_listall_handle_t *handle = (struct ocpdir_listall_handle_t *)_handle;
	if (!dirdbGetMdb(&handle->dirdbnode, &mdb_ref, &handle->first)) /* does not refcount.... */
	{
		struct dmDrive *drive = 0;
		struct ocpfile_t *file = 0;
		if (!filesystem_resolve_dirdb_file (handle->dirdbnode, &drive, &file))
		{
			handle->callback_file (handle->token, file);
			file->unref (file);
		}
		return 1;
	}
	return 0;
}

static struct ocpdir_t  *ocpdir_listall_readdir_dir  (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	return 0;
}

static struct ocpfile_t *ocpdir_listall_readdir_file (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	int first = 0;
	uint32_t dirdbnode = DIRDB_NOPARENT;
	uint32_t mdb_ref = UINT32_MAX;
	const char *temp1 = 0;
	dirdbGetName_internalstr (dirdb_ref, &temp1);
	while (!dirdbGetMdb(&dirdbnode, &mdb_ref, &first))
	{
		const char *temp2 = 0;
		dirdbGetName_internalstr (dirdbnode, &temp2);
		if (!strcmp (temp1, temp2))
		{
			struct dmDrive *drive = 0;
			struct ocpfile_t *file = 0;
			if (!filesystem_resolve_dirdb_file (dirdbnode, &drive, &file))
			{
				return file;
			}
			return 0;
		}
	}
	return 0;
}
