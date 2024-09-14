/*
*************** modland.com: wipe/remove cachedir ************************
*                                                                        *
* $OCPDATAHOME/modland.com (default)                                     *
*  => /home/stian/.local/share/ocp/modland.com                           *
*                                                                        *
* 123 directories                                                        *
* 12345 files                                                            *
* 12345 KBytes                                                           *
* (and still counting)                                                   *
*                                                                        *
*       [remove directory]      [move to recycle bin]      [abort]       *
*                                                                        *
**************************************************************************

*************** modland.com: wiping/removing cachedir ********************
*                                                                        *
* $OCPDATAHOME/modland.com (default)                                     *
*  => /home/stian/.local/share/ocp/modland.com                           *
*                                                                        *
* 123 directories (and 123 failed)                                       *
* 12345 files (and 123 failed)                                           *
*                                                                        *
* Finished                                                               *
*                                                                        *
*       [abort]                                               [ok]       *
*                                                                        *
**************************************************************************/

static void modland_com_wipecache_Draw (
	struct console_t *console,
	const int selected,
	const char *configured_path,
	const char *resolved_path,
	const uint_fast32_t directories_n,
	const uint_fast32_t files_n,
	const uint64_t datasize,
	const int stillcounting,
	const int display_recycle
)
{
	const int mlHeight = 13;
	const int mlWidth = 74;

	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda" "%18C\xc4" " modland.com: wipe/remove cachedir " "%19C\xc4" "\xbf");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "%.3o" "%71S" "%.9o\xb3", configured_path);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " " %.7o=> %67S" "%.9o\xb3", resolved_path);

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop  , mlLeft, 0x09, mlWidth-1, "\xb3 " "%.3o" "%"PRIuFAST32 "%.7o" " directories", directories_n);
	console->DisplayPrintf (mlTop++, mlLeft+73, 0x09, 1, "\xb3");

	console->DisplayPrintf (mlTop  , mlLeft, 0x09, mlWidth, "\xb3 " "%.3o" "%"PRIuFAST32 "%.7o" " files", files_n);
	console->DisplayPrintf (mlTop++, mlLeft+73, 0x09, 1, "\xb3");

	if (datasize >= 4194304)
	{
		console->DisplayPrintf (mlTop  , mlLeft, 0x09, mlWidth, "\xb3 " "%.3o" "%"PRIu64 "%.7o" " MBytes", datasize >> 20);
	} else if (datasize >= 65536)
	{
		console->DisplayPrintf (mlTop  , mlLeft, 0x09, mlWidth, "\xb3 " "%.3o" "%"PRIu64 "%.7o" " KBytes", datasize >> 10);
	} else {
		console->DisplayPrintf (mlTop  , mlLeft, 0x09, mlWidth, "\xb3 " "%.3o" "%"PRIu64 "%.7o" " Bytes", datasize);
	}

	console->DisplayPrintf (mlTop++, mlLeft+73, 0x09, 1, "\xb3");

	if (stillcounting)
	{
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%.7o" " (and still counting)" "%.9o" "%51C " "\xb3");
	} else {
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	}

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	if (stillcounting)
	{
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "      " "%.8o"  "< REMOVE DIRECTORY >"         "    "         "< MOVE TO RECYCLE BIN >"         "    "         "< ABORT >" "%.9o"  "      " "\xb3");
	} else {
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "      " "%*.*o" "< REMOVE DIRECTORY >" "%0.7o" "    " "%*.*o" "< MOVE TO RECYCLE BIN >" "%0.7o" "    " "%*.*o" "< ABORT >" "%0.9o" "      " "\xb3",
			(selected == 0) ? 7 : 0,
			(selected == 0) ? 1 : 3,
			(selected == 1) ? 7 : (display_recycle ? 0 : 0),
			(selected == 1) ? 1 : (display_recycle ? 3 : 8),
			(selected == 2) ? 7 : 0,
			(selected == 2) ? 1 : 3
		);
	}

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

static void modland_com_dowipecache_Draw (
	struct console_t *console,
	const char *configured_path,
	const char *resolved_path,
	const uint_fast32_t directories_n,
	const uint_fast32_t directories_target_n,
	const uint_fast32_t directories_failed_n,
	const uint_fast32_t files_n,
	const uint_fast32_t files_target_n,
	const uint_fast32_t files_failed_n,
	const int stillremoving
)
{
	const int mlHeight = 13;
	const int mlWidth = 74;

	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda" "%16C\xc4" " modland.com: wiping/removing cachedir " "%17C\xc4" "\xbf");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "%.3o" "%71S" "%.9o\xb3", configured_path);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " " %.7o=> %67S" "%.9o\xb3", resolved_path);

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	if (directories_failed_n)
	{
		console->DisplayPrintf (mlTop  , mlLeft, 0x09, mlWidth-1, "\xb3 " "%.3o" "%"PRIuFAST32 "%.7o" " of " "%.3o" "%"PRIuFAST32 "%.7o" " directories (%"PRIuFAST32" failed)", directories_n, directories_target_n, directories_failed_n);
	} else {
		console->DisplayPrintf (mlTop  , mlLeft, 0x09, mlWidth-1, "\xb3 " "%.3o" "%"PRIuFAST32 "%.7o" " of " "%.3o" "%"PRIuFAST32 "%.7o" " directories", directories_n, directories_target_n);
	}
	console->DisplayPrintf (mlTop++, mlLeft+73, 0x09, 1, "\xb3");

	if (files_failed_n)
	{
		console->DisplayPrintf (mlTop  , mlLeft, 0x09, mlWidth, "\xb3 " "%.3o" "%"PRIuFAST32 "%.7o" " of ""%.3o" "%"PRIuFAST32 "%.7o" " files (%"PRIuFAST32" failed)", files_n, files_target_n, files_failed_n);
	} else {
		console->DisplayPrintf (mlTop  , mlLeft, 0x09, mlWidth, "\xb3 " "%.3o" "%"PRIuFAST32 "%.7o" " of ""%.3o" "%"PRIuFAST32 "%.7o" " files", files_n, files_target_n);
	}
	console->DisplayPrintf (mlTop++, mlLeft+73, 0x09, 1, "\xb3");

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	if (stillremoving)
	{
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	} else {
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o" " Finished%0.9o%63C " "\xb3");
	}

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%10C " "%*.*o" "[ ABORT ]" "%0.9o" "%37C " "%*.*o" "[ OK ]" "%0.9o" "%10C " "\xb3",
		stillremoving ? 7 : 0,
		stillremoving ? 1 : 8,
		stillremoving ? 0 : 7,
		stillremoving ? 8 : 1);

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

/* actual delete the data */
static void modland_com_dowipecache_Run (const struct DevInterfaceAPI_t *API, uint_fast32_t directories_target_n, uint_fast32_t files_target_n)
{
	int quit = 0;
	struct osdir_delete_t d = {0};

	if (osdir_delete_start (&d, modland_com.cachepath))
	{
		goto removefilescomplete;
	}

	while (!quit)
	{
		API->fsDraw();
		modland_com_dowipecache_Draw (
			API->console,
			modland_com.cacheconfig,
			modland_com.cachepath,
			d.removed_directories_n + d.failed_directories_n,
			directories_target_n,
			d.failed_directories_n,
			d.removed_files_n + d.failed_files_n,
			files_target_n,
			d.failed_files_n,
			1
		);

		while (API->console->KeyboardHit() && !quit)
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
				case KEY_ESC:
					osdir_delete_cancel (&d);
					return;
			}
		}

		if (!quit)
		{
			do
			{
				if (!osdir_delete_iterate (&d))
				{
					quit = 1;
					break;
				}
			} while (!API->console->PollFrameLock());
		}
	}

removefilescomplete:
	quit = 0;
	while (!quit)
	{
		API->fsDraw();
		modland_com_dowipecache_Draw (
			API->console,
			modland_com.cacheconfig,
			modland_com.cachepath,
			d.removed_directories_n + d.failed_directories_n,
			directories_target_n,
			d.failed_directories_n,
			d.removed_files_n + d.failed_files_n,
			files_target_n,
			d.failed_files_n,
			0
		);

		while (API->console->KeyboardHit() && !quit)
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case _KEY_ENTER:
				case KEY_EXIT:
				case KEY_ESC:
					quit = 1;
					break;
			}
		}
		API->console->FrameLock();
	}
}

/* calculate */
static void modland_com_wipecache_Run (const struct DevInterfaceAPI_t *API)
{
	int quit = 0;
	int selected = 2;
	int can_recycle = osdir_trash_available (modland_com.cachepath);
	struct osdir_size_t s = {0};

	if (osdir_size_start (&s, modland_com.cachepath))
	{
		goto displaymenu;
	}

	while (!quit)
	{
		API->fsDraw();
		modland_com_wipecache_Draw (
			API->console,
			selected,
			modland_com.cacheconfig,
			modland_com.cachepath,
			s.directories_n,
			s.files_n,
			s.files_size,
			1,
			can_recycle
		);

		while (API->console->KeyboardHit() && !quit)
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
				case KEY_ESC:
					osdir_size_cancel (&s);
					return;
				case KEY_LEFT:
					if (selected)
					{
						selected--;
						if ((!can_recycle) && (selected == 1))
						{
							selected--;
						}
					}
					break;
				case KEY_RIGHT:
					if (selected < 2)
					{
						selected++;
						if ((!can_recycle) && (selected == 1))
						{
							selected++;
						}
					}
					break;
			}
		}

		if (!quit)
		{
			do
			{
				if (!osdir_size_iterate (&s))
				{
					quit = 1;
					break;
				}
			} while (!API->console->PollFrameLock());
		}
	}

displaymenu:

	quit = 0;
	while (!quit)
	{
		API->fsDraw();
		modland_com_wipecache_Draw (
			API->console,
			selected,
			modland_com.cacheconfig,
			modland_com.cachepath,
			s.directories_n,
			s.files_n,
			s.files_size,
			0,
			can_recycle
		);

		while (API->console->KeyboardHit() && !quit)
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
				case KEY_ESC:
					quit = 1;
					break;
				case KEY_LEFT:
					if (selected)
					{
						selected--;
						if ((!can_recycle) && (selected == 1))
						{
							selected--;
						}
					}
					break;
				case KEY_RIGHT:
					if (selected < 2)
					{
						selected++;
						if ((!can_recycle) && (selected == 1))
						{
							selected++;
						}
					}
					break;
				case _KEY_ENTER:
					if (selected == 0)
					{
						modland_com_dowipecache_Run (API, s.directories_n, s.files_n);
						quit = 1;
					} else if (selected == 1)
					{
						osdir_trash_perform (modland_com.cachepath);
						quit = 1;
#warning detect error......
					} else if (selected == 2)
					{
						quit = 1;
					}
					break;
			}
		}
		API->console->FrameLock();
	}
}
