/* OpenCP Module Player
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * MEDIALIBRARY scan dialog
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

struct scanlist_t
{
	char *path;
	struct ocpfile_t **files;
	int entries;
	int size;
	int abort;
};

static void mlScanDraw(const char *title, struct scanlist_t *token)
{
	unsigned int mlHeight;
	unsigned int mlTop;
	unsigned int mlLeft;
	unsigned int mlWidth;

	int i, lineno = 0;

	/* SETUP the framesize */
	mlHeight = plScrHeight - 20;
	if (mlHeight < 20)
	{
		mlHeight = 20;
	}
	mlTop = (plScrHeight - mlHeight) / 2;

	mlLeft = 5;
	mlWidth = plScrWidth - 10;
	while (mlWidth < 72)
	{
		mlLeft--;
		mlWidth += 2;
	}

	/* Draw the main frame */
	for (i=1; i < (mlWidth - 1); i++)
	{
		displaystr (mlTop,                mlLeft + i, 0x04, "\xc4", 1);
		displaystr (mlTop + 3,            mlLeft + i, 0x04, "\xc4", 1);
		displaystr (mlTop + mlHeight - 1, mlLeft + i, 0x04, "\xc4", 1);
	}

	displaystr (mlTop,                mlLeft,               0x04, "\xda", 1);
	displaystr (mlTop,                mlLeft + mlWidth - 1, 0x04, "\xbf", 1);
	displaystr (mlTop + 1,            mlLeft,               0x04, "\xb3", 1);
	displaystr (mlTop + 1,            mlLeft + mlWidth - 1, 0x04, "\xb3", 1);
	displaystr (mlTop + 2,            mlLeft,               0x04, "\xb3", 1);
	displaystr (mlTop + 2,            mlLeft + mlWidth - 1, 0x04, "\xb3", 1);
	displaystr (mlTop + 3,            mlLeft,               0x04, "\xc3", 1);
	displaystr (mlTop + 3,            mlLeft + mlWidth - 1, 0x04, "\xb4", 1);
	displaystr (mlTop + mlHeight - 1, mlLeft,               0x04, "\xc0", 1);
	displaystr (mlTop + mlHeight - 1, mlLeft + mlWidth - 1, 0x04, "\xd9", 1);

	do
	{
		int Left = 5 + (plScrWidth - strlen (title) - 2 - 10) / 2;
		displaystr (mlTop, Left,                      0x09, " ",   1);
		displaystr (mlTop, Left + 1,                  0x09, title, strlen (title));
		displaystr (mlTop, Left + 1 + strlen (title), 0x09, " ",   1);
	} while (0);

	for (i = 4; i < (mlHeight-1); i++)
	{
		displaystr  (mlTop + i, mlLeft,               0x04, "\xb3", 1);
		displaystr  (mlTop + i, mlLeft + mlWidth - 1, 0x04, "\xb3", 1);
	}

	/* Line 1: "Currently scanning filesystem, press <esc> to abort" */
	displaystr (mlTop + 1, mlLeft + 1,  0x07, "Currently scanning filesystem, press ", 37);
	displaystr (mlTop + 1, mlLeft + 38, 0x0f, "<esc>", 5);
	displaystr (mlTop + 1, mlLeft + 43, 0x07, " to abort", mlWidth - 44);

	/* Line 2: the current selected path */
	displaystr_utf8_overflowleft (mlTop + 2, mlLeft + 1, 0x07, token->path, mlWidth - 2);

	for (i=0; i < token->entries; i++)
	{
		const char *filename = 0;
		dirdbGetName_internalstr (token->files[i]->dirdb_ref, &filename);
		displaystr_utf8 (mlTop + 4 + (lineno % (mlHeight - 5)), mlLeft + 1, 0x07, filename, mlWidth - 2);
		lineno++;
	}

	/* clear the bottom if needed */
	for (; lineno < mlHeight - 5; lineno++)
	{
		displayvoid (mlTop + 4 + lineno, mlLeft + 1, mlWidth - 2);
	}

	while (ekbhit())
	{
		int key = egetch();
		switch (key)
		{
			case KEY_ESC:
				token->abort = 1;
				break;
			default:
				break;
		}
	}
}

static int mlScan(struct ocpdir_t *dir);

static void mlScan_dir (void *_token, struct ocpdir_t *dir)
{
	struct scanlist_t *token = _token;
	if (mlScan (dir))
	{
		token->abort = 1;
	}
}

static void mlScan_file (void *_token, struct ocpfile_t *file)
{
	struct scanlist_t *token = _token;
	char *curext = 0;
	const char *filename = 0;
	uint32_t mdbref = UINT32_MAX;

	if (poll_framelock())
	{
		mlScanDraw ("Scanning", token);
	}

	if (token->abort)
	{
		return;
	}

	dirdbGetName_internalstr (file->dirdb_ref, &filename);

	getext_malloc (filename, &curext);
	if (!curext)
	{
		return;
	}

	if (fsScanArcs)
	{
		struct ocpdir_t *dir;

		dir = ocpdirdecompressor_check (file, curext);
		if (dir)
		{
			if (!dir->is_playlist)
			{
				if (mlScan (dir))
				{
					token->abort = 1;
				}
			}
			dir->unref (dir);
			free (curext);
			return;
		}
	}

	if (!fsIsModule (curext))
	{
		free (curext);
		return;
	}
	free (curext);
	curext = 0;

	mdbref = mdbGetModuleReference2 (file->dirdb_ref, file->filesize(file));
	if (!mdbInfoIsAvailable (mdbref))
	{
		mdbScan(file, mdbref);
	}
	dirdbMakeMdbRef(file->dirdb_ref, mdbref);

	if (token->entries >= token->size)
	{
		struct ocpfile_t **temp = realloc (token->files, (token->size + 64) * sizeof (token->files[0]));
		if (!temp)
		{
			return;
		}
		token->files = temp;
		token->size += 64;
	}
	file->ref(file);
	token->files[token->entries] = file;
	token->entries++;
}

/* returns non-zero on KEY_ESC */
static int mlScan(struct ocpdir_t *dir)
{
	struct scanlist_t token;
	int i;
	ocpdirhandle_pt *handle;

	bzero (&token, sizeof (token));

	dirdbGetFullname_malloc (dir->dirdb_ref, &token.path, DIRDB_FULLNAME_ENDSLASH);
	if (!token.path)
	{
		return 0;
	}

	handle = dir->readdir_start (dir, mlScan_file, mlScan_dir, &token);
	if (!handle)
	{
		free (token.path);
		return 0;
	}
	while (dir->readdir_iterate (handle) && (!token.abort))
	{
		if (poll_framelock())
		{
			mlScanDraw ("Scanning", &token);
		}
	}
	dir->readdir_cancel (handle);

	for (i=0; i < token.entries; i++)
	{
		token.files[i]->unref (token.files[i]);
	}
	free (token.files);

	free (token.path);
	return token.abort;
}
