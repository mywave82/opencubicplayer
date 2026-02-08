/* OpenCP Module Player
 * copyright (c) 2005-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * MEDIALIBRARY refresh dialog
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

static int medialibRefreshSelected;

static void mlRefreshDraw(const char *title)
{
	unsigned int mlHeight;
	unsigned int mlTop;
	unsigned int mlLeft;
	unsigned int mlWidth;

	unsigned int i, skip, half, dot;

#if (CONSOLE_MIN_Y < 20)
# error mlRefreshDraw() requires CONSOLE_MIN_Y >= 20
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

	half = (mlHeight - 4) / 2;
	if (medialib_sources_count <= mlHeight - 4)
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (medialibRefreshSelected < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (medialibRefreshSelected >= (medialib_sources_count - half))
	{ /* we are at the bottom part */
		skip = medialib_sources_count - (mlHeight - 4);
		dot = mlHeight - 4;
	} else {
		skip = medialibRefreshSelected - half;
		dot = skip * (mlHeight - 4) / (medialib_sources_count - (mlHeight - 4));
	}

	/* Draw the main frame */
	for (i=1; i < (mlWidth - 1); i++)
	{
		displaystr (mlTop,                mlLeft + i, 0x04, "\xc4", 1);
		displaystr (mlTop + 2,            mlLeft + i, 0x04, "\xc4", 1);
		displaystr (mlTop + mlHeight - 1, mlLeft + i, 0x04, "\xc4", 1);
	}

	displaystr (mlTop,                mlLeft,               0x04, "\xda", 1);
	displaystr (mlTop,                mlLeft + mlWidth - 1, 0x04, "\xbf", 1);
	displaystr (mlTop + 1,            mlLeft,               0x04, "\xb3", 1);
	displaystr (mlTop + 1,            mlLeft + mlWidth - 1, 0x04, "\xb3", 1);
	displaystr (mlTop + 2,            mlLeft,               0x04, "\xc3", 1);
	displaystr (mlTop + 2,            mlLeft + mlWidth - 1, 0x04, "\xb4", 1);
	displaystr (mlTop + mlHeight - 1, mlLeft,               0x04, "\xc0", 1);
	displaystr (mlTop + mlHeight - 1, mlLeft + mlWidth - 1, 0x04, "\xd9", 1);

	do
	{
		int Left = 5 + (plScrWidth - strlen (title) - 2 - 10) / 2;
		displaystr (mlTop, Left,                      0x09, " ",   1);
		displaystr (mlTop, Left + 1,                  0x09, title, strlen (title));
		displaystr (mlTop, Left + 1 + strlen (title), 0x09, " ",   1);
	} while (0);

	for (i = 3; i < (mlHeight-1); i++)
	{
		displaystr  (mlTop + i, mlLeft,               0x04,                         "\xb3", 1);
		displaystr  (mlTop + i, mlLeft + mlWidth - 1, 0x04, ((i-3) == dot) ? "\xdd":"\xb3", 1);
	}

	/* Line 1: "Select an item and press <enter>, or <esc> to abort" */
	displaystr (mlTop + 1, mlLeft + 1,  0x07, "Select an item and press ", 25);
	displaystr (mlTop + 1, mlLeft + 26, 0x0f, "<enter>", 7);
	displaystr (mlTop + 1, mlLeft + 33, 0x07, ", or ", 5);
	displaystr (mlTop + 1, mlLeft + 38, 0x0f, "<esc>", 5);
	displaystr (mlTop + 1, mlLeft + 43, 0x07, " to abort", mlWidth - 44);

	for (i=0; i < (mlHeight - 4); i++)
	{
		if (i < medialib_sources_count)
		{
			displaystr_utf8 (mlTop + 3 + i, mlLeft + 1, (medialibRefreshSelected==(i + skip))?0x8f:0x0f, medialib_sources[i].path, mlWidth - 2);
		} else {
			displayvoid (mlTop + 3 + i, mlLeft + 1, mlWidth - 2);
		}
	}
}

static int medialibRefreshInit (void **token, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API)
{
	if (medialib_sources_count)
	{
		medialibRefreshSelected = 0;
		return 1;
	} else {
		return 0; /* no items in the list to refresh */
	}
}

static void medialibRefreshRun (void **token, const struct DevInterfaceAPI_t *API)
{
	while (1)
	{
		API->fsDraw();
		mlRefreshDraw("Refresh files in medialib");
		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_HOME:
					medialibRefreshSelected = 0;
					break;
				case KEY_END:
					medialibRefreshSelected = medialib_sources_count - 1;
					break;
				case KEY_UP:
					if (medialibRefreshSelected)
					{
						medialibRefreshSelected--;
					}
					break;
				case KEY_DOWN:
					if ((medialibRefreshSelected + 1) < medialib_sources_count)
					{
						medialibRefreshSelected++;
					}
					break;
				case _KEY_ENTER:
				//case KEY_INSERT:
					{
						struct dmDrive *drive = 0;
						struct ocpdir_t *dir = 0;

						filesystem_resolve_dirdb_dir (medialib_sources[medialibRefreshSelected].dirdb_ref, &drive, &dir);
						if (!dir)
						{
							return;
						}

						dirdbTagSetParent (medialib_sources[medialibRefreshSelected].dirdb_ref);

						if (mlScan (dir))
						{
							dirdbTagCancel ();
						} else {
							dirdbTagRemoveUntaggedAndSubmit ();
							dirdbFlush ();
							mdbUpdate ();
							adbMetaCommit ();
						}
						dir->unref (dir);
					}
					/* fall-trough */
				case KEY_EXIT:
				case KEY_ESC:
					return;
				default:
					break;
			}
		}
		API->console->FrameLock();
	}
}
