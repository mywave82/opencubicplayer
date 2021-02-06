static int medialibRemoveSelected;

static void mlRemoveDraw(const char *title)
{
	unsigned int mlHeight;
	unsigned int mlTop;
	unsigned int mlLeft;
	unsigned int mlWidth;

	unsigned int i, skip, half, dot;

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
	displaystr (mlTop + 1, mlLeft + 26, 0x0f, "<delete>", 8);
	displaystr (mlTop + 1, mlLeft + 34, 0x07, ", or ", 5);
	displaystr (mlTop + 1, mlLeft + 39, 0x0f, "<esc>", 5);
	displaystr (mlTop + 1, mlLeft + 44, 0x07, " to abort", mlWidth - 45);

	for (i=0; i < (mlHeight - 4); i++)
	{
		if (i < medialib_sources_count)
		{
			displaystr_utf8 (mlTop + 3 + i, mlLeft + 1, (medialibRemoveSelected==(i + skip))?0x8f:0x0f, medialib_sources[i].path, mlWidth - 2);
		} else {
			displayvoid (mlTop + 3 + i, mlLeft + 1, mlWidth - 2);
		}
	}
}

static int medialibRemoveInit (struct moduleinfostruct *info, struct ocpfilehandle_t *f)
{
	if (medialib_sources_count)
	{
		medialibRemoveSelected = 0;
		return 1;
	} else {
		return 0; /* no items in the list to refresh */
	}
}

static interfaceReturnEnum medialibRemoveRun (void)
{
	while (1)
	{
		fsDraw();
		mlRemoveDraw("Remove files from medialib");
		while (ekbhit())
		{
			int key = egetch();
			switch (key)
			{
				case KEY_HOME:
					medialibRemoveSelected = 0;
					break;
				case KEY_END:
					medialibRemoveSelected = medialib_sources_count - 1;
					break;
				case KEY_UP:
					if (medialibRemoveSelected)
					{
						medialibRemoveSelected--;
					}
					break;
				case KEY_DOWN:
					if ((medialibRemoveSelected + 1) < medialib_sources_count)
					{
						medialibRemoveSelected++;
					}
					break;
				case KEY_DELETE:
					{
						int i;

						dirdbTagSetParent (medialib_sources[medialibRemoveSelected].dirdb_ref);

						for (i=0; i < medialib_sources_count; i++)
						{
							if (i != medialibRemoveSelected)
							{
								dirdbTagPreserveTree (medialib_sources[i].dirdb_ref);
							}
						}
						dirdbTagRemoveUntaggedAndSubmit ();
						dirdbFlush ();
						mdbUpdate ();
						adbMetaCommit ();
					}

					/* remove the entry from the list */
					dirdbUnref (medialib_sources[medialibRemoveSelected].dirdb_ref, dirdb_use_medialib);
					free (medialib_sources[medialibRemoveSelected].path);
					memmove (medialib_sources + medialibRemoveSelected, medialib_sources + medialibRemoveSelected + 1, sizeof (medialib_sources[0]) * (medialib_sources_count - medialibRemoveSelected - 1));
					medialib_sources = realloc (medialib_sources, (medialib_sources_count - 1) * sizeof (medialib_sources [0]));
					medialib_sources_count--;

					mlFlushBlob ();
					/* fall-trough */
				case KEY_ESC:
					return interfaceReturnNextAuto;
				default:
					break;
			}
		}
		framelock();
	}
}
