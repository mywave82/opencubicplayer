/* OpenCP Module Player
 * copyright (c) 2005-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * MEDIALIBRARY search dialog
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

/* 0 = typing
 * 1 = query
 * 2 = ready
 */
static int mlSearchPerformed;
static char *mlSearchQuery = 0;
static struct ocpfile_t **mlSearchResult;
static int                mlSearchResultCount;
static int                mlSearchResultSize;
static int                mlSearchFirst = 1;
static uint32_t           mlSearchDirDbRef;

static void mlSearchClear (void)
{
	int i;
	mlSearchPerformed = 0;
	free (mlSearchQuery);
	      mlSearchQuery = 0;
	for (i=0; i < mlSearchResultCount; i++)
	{
		mlSearchResult[i]->unref (mlSearchResult[i]);
	}
	free (mlSearchResult);
	      mlSearchResult = 0;
	mlSearchResultCount = 0;
	mlSearchResultSize = 0;
	mlSearchFirst = 1;
}

static int mlSearchPerformQuery (void)
{
	struct dmDrive *drive = 0;
	struct ocpfile_t *file = 0;
	char *filename = 0;
	char *ptr, *ptr2;
	uint32_t mdb_ref;
	struct moduleinfostruct info;
	char buffer
	[
		MAX(sizeof(info.title),
		MAX(sizeof(info.composer),
		    sizeof(info.comment)))
	];

	if (!mlSearchQuery)
	{
		return 1;
	}

	while (1)
	{
		if (dirdbGetMdb(&mlSearchDirDbRef, &mdb_ref, &mlSearchFirst)) /* does not refcount.... */
		{
			return 1;
		}

		dirdbGetName_malloc (mlSearchDirDbRef, &filename);
		if (!filename)
		{ /* out of memory probably */
			return 1;
		}

		strupr(filename);

		if (strstr (filename, mlSearchQuery))
		{
			free (filename);
			      filename = 0;
			break; /* goto add; */
		}
		free (filename);
		      filename = 0;

		mdbGetModuleInfo(&info, mdb_ref);

		/* make each field upper case before trying to match */

		for (ptr=buffer, ptr2=info.title; *ptr2; ptr++, ptr2++)
		{
			*ptr = toupper (*ptr2);
		}
		if (strstr (buffer, mlSearchQuery))
		{
			break; /* goto add; */
		}

		for (ptr=buffer, ptr2=info.composer; *ptr2; ptr++, ptr2++)
		{
			*ptr = toupper (*ptr2);
		}
		if (strstr (buffer, mlSearchQuery))
		{
			break; /* goto add; */
		}

		for (ptr=buffer, ptr2=info.comment; *ptr2; ptr++, ptr2++)
		{
			*ptr = toupper (*ptr2);
		}
		if (strstr (buffer, mlSearchQuery))
		{
			break; /* goto add; */
		}

	}
//add:
	if (filesystem_resolve_dirdb_file (mlSearchDirDbRef, &drive, &file))
	{
		return 0;
	}

	if (mlSearchResultCount >= mlSearchResultSize)
	{
		struct ocpfile_t **temp = realloc (mlSearchResult, (mlSearchResultSize + 128) * sizeof (mlSearchResult[0]));
		if (!temp)
		{
			/* out of memory */
			file->unref (file);
			             file = 0;
			return 1;
		}
		mlSearchResult = temp;
		mlSearchResultSize += 128;
	}
	mlSearchResult[mlSearchResultCount] = file;
	mlSearchResultCount++;

	return 0;
}

static int mlSearchDraw(const char *title)
{
	unsigned int mlHeight;
	unsigned int mlTop;
	unsigned int mlLeft;
	unsigned int mlWidth;

	int i;

#if (CONSOLE_MIN_Y < 20)
# error mlSearchDraw() requires CONSOLE_MIN_Y >= 20
#endif

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
		displaystr (mlTop,     mlLeft + i, 0x04, "\xc4", 1);
		displaystr (mlTop + 2, mlLeft + i, 0x04, "\xc4", 1);
		displaystr (mlTop + 4, mlLeft + i, 0x04, "\xc4", 1);
	}

	displaystr (mlTop,     mlLeft,               0x04, "\xda", 1);
	displaystr (mlTop,     mlLeft + mlWidth - 1, 0x04, "\xbf", 1);
	displaystr (mlTop + 1, mlLeft,               0x04, "\xb3", 1);
	displaystr (mlTop + 1, mlLeft + mlWidth - 1, 0x04, "\xb3", 1);
	displaystr (mlTop + 2, mlLeft,               0x04, "\xc3", 1);
	displaystr (mlTop + 2, mlLeft + mlWidth - 1, 0x04, "\xb4", 1);
	displaystr (mlTop + 3, mlLeft,               0x04, "\xb3", 1);
	displaystr (mlTop + 3, mlLeft + mlWidth - 1, 0x04, "\xb3", 1);
	displaystr (mlTop + 4, mlLeft,               0x04, "\xc0", 1);
	displaystr (mlTop + 4, mlLeft + mlWidth - 1, 0x04, "\xd9", 1);

	do
	{
		int Left = 5 + (plScrWidth - strlen (title) - 2 - 10) / 2;
		displaystr (mlTop, Left,                      0x09, " ",   1);
		displaystr (mlTop, Left + 1,                  0x09, title, strlen (title));
		displaystr (mlTop, Left + 1 + strlen (title), 0x09, " ",   1);
	} while (0);

	displaystr (mlTop + 1, mlLeft + 1,  0x07, "Please type in something to search for, or press ", 49);
	displaystr (mlTop + 1, mlLeft + 50, 0x0f, "<esc>", 5);
	displaystr (mlTop + 1, mlLeft + 55, 0x07, " to abort", mlWidth - 56);

	/* Line 2: the current selected path */
	if (!mlSearchQuery)
	{
		mlSearchQuery = strdup ("");
	}
	return EditStringUTF8 (mlTop + 3, mlLeft + 1, mlWidth - 2, &mlSearchQuery);
}


static void ocpdir_search_ref (struct ocpdir_t *self)
{
	self->refcount++;
}

static void ocpdir_search_unref (struct ocpdir_t *self)
{
	self->refcount--;
	if (self->refcount <= 2)
	{
		mlSearchClear();
	}
}

struct search_readdir_handle_t
{
	struct ocpdir_t *self;
	void (*callback_file)(void *token, struct ocpfile_t *);
	void *token;
	int nextindex;
};

static ocpdirhandle_pt ocpdir_search_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *), void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct search_readdir_handle_t *retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		return 0;
	}
	retval->self = self;
	retval->callback_file = callback_file;
	retval->token = token;
	retval->nextindex = 0;
	self->ref (self);
	return retval;
}

static void ocpdir_search_readdir_cancel (ocpdirhandle_pt _handle)
{
	struct search_readdir_handle_t *handle = (struct search_readdir_handle_t *)_handle;
	handle->self->unref (handle->self);
	free (handle);
}

static int ocpdir_search_readdir_iterate (ocpdirhandle_pt _handle)
{
	struct search_readdir_handle_t *handle = (struct search_readdir_handle_t *)_handle;
	int res;

	switch (mlSearchPerformed)
	{
		case 0:
			res = mlSearchDraw("medialib search");
			if (res < 0)
			{
				mlSearchPerformed = 2;
				return 0;
			} else if (res == 0)
			{/* make query upper-case */
				strupr(mlSearchQuery);
				mlSearchPerformed = 1;
				return 1;
			}
			return 1;
		case 1:
			res = mlSearchPerformQuery();
			if (res < 0)
			{
				mlSearchPerformed = 2;
				return 0;
			} else if (res > 0)
			{
				mlSearchPerformed = 2;
				return 1;
			}
			return 1;
		default:
		case 2:
			while (handle->nextindex < mlSearchResultCount)
			{
				handle->callback_file (handle->token, mlSearchResult[handle->nextindex++]);
			}
			return 0;
	}
}

static struct ocpdir_t  *ocpdir_search_readdir_dir  (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	return 0;
}

static struct ocpfile_t *ocpdir_search_readdir_file (struct ocpdir_t *self, uint32_t dirdb_ref)
{
	return 0;
}
