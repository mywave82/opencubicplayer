struct medialibAddDirEntry
{
	const char *override_string; /* can only be ".." at the moment, in the future maybe a strdup of drive names for Windows */
	struct ocpdir_t *dir;
};
static struct ocpdir_t            *medialibAddCurDir;
static char                       *medialibAddPath;
static struct medialibAddDirEntry *medialibAddDirEntry;
static int                         medialibAddDirEntries;
static int                         medialibAddDirSize;

static int adecmp (const void *_a, const void *_b)
{
	const struct medialibAddDirEntry *a = _a;
	const struct medialibAddDirEntry *b = _b;

	char *s1;
	char *s2;

	dirdbGetName_internalstr (a->dir->dirdb_ref, &s1);
	dirdbGetName_internalstr (b->dir->dirdb_ref, &s2);

	return strcmp (s1, s2);
}

static void mlAddDraw(const char *title, const char *utf8_path, int dsel)
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

	half = (mlHeight - 5) / 2;
	if (medialibAddDirEntries <= mlHeight - 5)
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (dsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (dsel >= (medialibAddDirEntries - half))
	{ /* we are at the bottom part */
		skip = medialibAddDirEntries - (mlHeight - 5);
		dot = mlHeight - 5;
	} else {
		skip = dsel - half;
		dot = skip * (mlHeight - 5) / (medialibAddDirEntries - (mlHeight - 5));
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
		//displayvoid (mlTop + i, mlLeft + 1,                         mlWidth - 2);
		displaystr  (mlTop + i, mlLeft,               0x04,                         "\xb3", 1);
		displaystr  (mlTop + i, mlLeft + mlWidth - 1, 0x04, ((i-4) == dot) ? "\xdd":"\xb3", 1);
	}

	/* Line 1: "Browse the directory that you want to add. Press <insert> when located, or <esc> to abort"   of too long, chop off the last bit */
	displaystr (mlTop + 1, mlLeft + 1,  0x07, "Browse the directory that you want to add. Press ", 49);
	displaystr (mlTop + 1, mlLeft + 50, 0x0f, "<insert>", 8);
	displaystr (mlTop + 1, mlLeft + 58, 0x07, " when located", 13);
	if (mlWidth > 90)
	{
		displaystr (mlTop + 1, mlLeft + 71, 0x07, ", or ", 5);
		displaystr (mlTop + 1, mlLeft + 76, 0x0f, "<esc>", 5);
		displaystr (mlTop + 1, mlLeft + 81, 0x07, " to abort", mlWidth - 82);
	} else {
		displayvoid (mlTop + 1, mlLeft + 71, mlWidth - 72);
	}

	/* Line 2: the current selected path */
	displaystr_utf8_overflowleft (mlTop + 2, mlLeft + 1, 0x07, utf8_path, mlWidth - 2);

	for (i=0; i < (mlHeight - 5); i++)
	{
		if (i < medialibAddDirEntries)
		{
			char *path = (char *)medialibAddDirEntry[i + skip].override_string;
			assert ((i + skip) < medialibAddDirEntries);
			if (!path)
			{
				dirdbGetName_internalstr (medialibAddDirEntry[i + skip].dir->dirdb_ref, &path);
			}
			displaystr_utf8 (mlTop + 4 + i, mlLeft + 1, (dsel==(i + skip))?0x8f:0x0f, path, mlWidth - 2);
		} else {
			displayvoid (mlTop + 4 + i, mlLeft + 1, mlWidth - 2);
		}
	}
}

static void medialibAddRefresh_file (void *token, struct ocpfile_t *file)
{
	return;
}

static void medialibAddRefresh_dir (void *token, struct ocpdir_t *dir)
{
	if (medialibAddDirEntries >= medialibAddDirSize)
	{
		struct medialibAddDirEntry *temp = realloc (medialibAddDirEntry, (medialibAddDirSize + 32) * sizeof (medialibAddDirEntry[0]));
		if (!temp)
		{
			return; /* out of memory */
		}
		medialibAddDirEntry = temp;
		medialibAddDirSize += 32;
	}

	medialibAddDirEntry[medialibAddDirEntries].override_string = 0;

	dir->ref (dir);
	medialibAddDirEntry[medialibAddDirEntries].dir = dir;

	medialibAddDirEntries++;
}

static void medialibAddRefresh (void)
{
	int i;
	for (i=0; i < medialibAddDirEntries; i++)
	{
		medialibAddDirEntry[i].dir->unref (medialibAddDirEntry[i].dir);
	}
	medialibAddDirEntries = 0;

	if (medialibAddCurDir)
	{
		if (medialibAddCurDir->parent)
		{
			medialibAddRefresh_dir (0, medialibAddCurDir->parent);
			if (medialibAddDirEntry)
			{
				medialibAddDirEntry[medialibAddDirEntries-1].override_string = "..";
			}
		}
		ocpdirhandle_pt handle = medialibAddCurDir->readdir_start (medialibAddCurDir, medialibAddRefresh_file, medialibAddRefresh_dir, 0);
		if (handle)
		{
			while (medialibAddCurDir->readdir_iterate (handle))
			{
			}
			medialibAddCurDir->readdir_cancel (handle);
		}
	}

	if (medialibAddDirEntries > 1)
	{
		qsort (medialibAddDirEntry + 1, medialibAddDirEntries - 1, sizeof (medialibAddDirEntry[0]), adecmp);
	}
}

static void medialibAddClear (void)
{
	int i;

	for (i=0; i < medialibAddDirEntries; i++)
	{
		medialibAddDirEntry[i].dir->unref (medialibAddDirEntry[i].dir);
	}
	free (medialibAddDirEntry);
	      medialibAddDirEntry = 0;
	medialibAddDirEntries = 0;
	medialibAddDirSize = 0;

	medialibAddCurDir->unref (medialibAddCurDir);
	medialibAddCurDir = 0;
	free (medialibAddPath);
	      medialibAddPath = 0;
}

static int medialibAddInit (struct moduleinfostruct *info, struct ocpfilehandle_t *f)
{
#ifndef __W32__
	if (dmFILE->cwd)
	{
		medialibAddCurDir = dmFILE->cwd;
		medialibAddCurDir->ref (medialibAddCurDir);
		medialibAddPath = 0;
		dirdbGetFullname_malloc (medialibAddCurDir->dirdb_ref, &medialibAddPath, DIRDB_FULLNAME_ENDSLASH);
		medialibAddRefresh ();
		return 1;
	} else {
		return 0; /* should never happen */
	}
#else
	#error need API to detect last active drive
#endif
}

static int medialib_source_cmp(const void *p1, const void *p2)
{
	const struct medialib_source_t *q1 = p1;
	const struct medialib_source_t *q2 = p2;
	return strcmp (q1->path, q2->path);
}

static interfaceReturnEnum medialibAddRun (void)
{
	int dsel = 0;
	while (1)
	{
		fsDraw();
		mlAddDraw("Add files to medialib", medialibAddPath ? medialibAddPath : "out of memory", dsel);
		while (ekbhit())
		{
			int key = egetch();
			switch (key)
			{
				case KEY_HOME:
					dsel = 0;
					break;
				case KEY_END:
					if (medialibAddDirEntries)
					{
						dsel = medialibAddDirEntries - 1;
					}
					break;
				case KEY_UP:
					if (dsel)
					{
						dsel--;
					}
					break;
				case KEY_DOWN:
					if ((dsel + 1) < medialibAddDirEntries)
					{
						dsel++;
					}
					break;
				case _KEY_ENTER:
					if (dsel < medialibAddDirEntries)
					{
						int i;
						uint32_t old_dir = medialibAddCurDir->dirdb_ref;
						dirdbRef (old_dir, dirdb_use_medialib);

						medialibAddCurDir->unref (medialibAddCurDir);
						medialibAddCurDir = medialibAddDirEntry[dsel].dir;
						medialibAddCurDir->ref (medialibAddCurDir);
						free (medialibAddPath);
						      medialibAddPath = 0;
						dirdbGetFullname_malloc (medialibAddCurDir->dirdb_ref, &medialibAddPath, DIRDB_FULLNAME_ENDSLASH);
						medialibAddRefresh ();
						dsel = 0;
						for (i=0; i < medialibAddDirEntries; i++)
						{
							if (medialibAddDirEntry[i].dir->dirdb_ref == old_dir)
							{
								dsel = i;
								break;
							}
						}
						dirdbUnref (old_dir, dirdb_use_medialib);
					}
					break;
				case KEY_INSERT:
					dirdbTagSetParent (medialibAddCurDir->dirdb_ref);

					if (mlScan (medialibAddCurDir))
					{
						dirdbTagCancel ();
					} else {
						int i;
						for (i=0; i < medialib_sources_count; i++)
						{
							if (medialib_sources[i].dirdb_ref == medialibAddCurDir->dirdb_ref)
							{
								break;
							}
						}
						if (i == medialib_sources_count) /* if not already present, add it, even if a parent might already be on the list */
						{
							struct medialib_source_t *temp = realloc (medialib_sources, (medialib_sources_count + 1) * sizeof (medialib_sources[0]));
							if (temp)
							{
								medialib_sources = temp;
								dirdbRef (medialibAddCurDir->dirdb_ref, dirdb_use_medialib);
								medialib_sources[medialib_sources_count].dirdb_ref = medialibAddCurDir->dirdb_ref;
								medialib_sources[medialib_sources_count].path = medialibAddPath;
								medialibAddPath = 0; // we just stole it...
								medialib_sources_count++;
								qsort (medialib_sources, medialib_sources_count, sizeof (medialib_sources[0]), medialib_source_cmp);
							}
						}
						dirdbTagRemoveUntaggedAndSubmit ();
						dirdbFlush ();
						mdbUpdate ();
						mlFlushBlob ();
						adbMetaCommit ();
					}
					/* fall-trough */
				case KEY_ESC:
					medialibAddClear();
					return interfaceReturnNextAuto;
				default:
					break;
			}
		}
		framelock();
	}
}


