#if 0

****************************************************************************  1
* Use arrow keys and <ENTER> to navigate. <ESC> to close.                  *  2
*                                                                          *  3
* Current mirror: https://modland.com/                                     *  4
*                                                                          *  5
* 123456 File-entries stored in database                                   *  6
*                                                                          *  7
* File cache is stored in $OCPDATA/modland.com =>                          *  8
*  /home/stian/.local/share/ocp/modland.com                                *  9
*                                                                          * 10
* OCP currently only shows relevant directories                            * 11
*                                                                          * 12
* 1. Select mirror                                                         * 13
* 2. Fetch database                                                        * 14
* 2. Refresh database                                                      * 14
* 3. Remove database                                                       * 15
* 4. Select cache directory                                                * 16
* 5. Wipe cache directory                                                  * 17
* 6. Show all directories                                                  * 18
* 6. Show only relevant directories                                        * 18
**************************************************************************** 19

#endif

static void modland_com_setup_Draw
(
	struct console_t *console,
	const int selected,

	const char *currentmirror,
	const int numfileentries,
	const int year, const int month, const int day,
	const char *symbolicstore,
	const char *resolvedstore,
	const int showrelevantdirectoriesonly
)
{
	int mlHeight = 20;
	int mlWidth = 74;

	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;

	console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "modland.com: setup", 0, 0, 0);
	mlWidth -= 2;
	mlHeight -= 2;

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Use arrow keys and %.15o<ENTER>%.7o to navigate. %.15o<ESC>%.7o to close.");

	mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Current mirror: %.2o%55S", currentmirror);

	mlTop++;

	if (numfileentries)
	{
		console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Database datestamp is %0.2o%04d-%02d-%02d.", year, month, day);
		console->DisplayPrintf (mlTop++, mlLeft, 0x02, mlWidth, " %d" "%.7o" " file-entries stored in the database.", numfileentries);
	} else {
		console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " No database loaded");
		mlTop++;
	}

	mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " File cache is stored in %.2o%S%.7o =>", symbolicstore);
	console->DisplayPrintf (mlTop++, mlLeft, 0x02, mlWidth, " %71S", resolvedstore);

	mlTop++;

	if (showrelevantdirectoriesonly)
	{
		console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " OCP currently %.2oonly shows relevant%.7o directories");
	} else {
		console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " OCP currently %.2oshows all%.7o directories");
	}

	mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " %*.*o1.%.*o Select mirror                        %30C %0.7o ", (selected == 0) ? 7 : 0, (selected == 0) ? 1 : 7, (selected == 0) ? 1 : 3);
	if (!numfileentries)
	{
		console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " %*.*o2.%.*o Fetch database                       %30C %0.7o ", (selected == 1) ? 7 : 0, (selected == 1) ? 1 : 7, (selected == 1) ? 1 : 3);
	} else {
		console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " %*.*o2.%.*o Refresh database                     %30C %0.7o ", (selected == 1) ? 7 : 0, (selected == 1) ? 1 : 7, (selected == 1) ? 1 : 3);
	}
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " %*.*o3.%.*o Remove database                      %30C %0.7o ", (selected == 2) ? 7 : 0, (selected == 2) ? 1 : 7, (selected == 2) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " %*.*o4.%.*o Select cache directory               %30C %0.7o ", (selected == 3) ? 7 : 0, (selected == 3) ? 1 : 7, (selected == 3) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " %*.*o5.%.*o Wipe cache directory                 %30C %0.7o ", (selected == 4) ? 7 : 0, (selected == 4) ? 1 : 7, (selected == 4) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " %*.*o6.%.*o Toggle only show relevant directories%30C %0.7o ", (selected == 5) ? 7 : 0, (selected == 5) ? 1 : 7, (selected == 5) ? 1 : 3);
}

static void modland_com_setup_Run (void **token, const struct DevInterfaceAPI_t *API)
{
	int selected = 0;
	int quit = 0;

	while (!quit)
	{
		API->fsDraw();
		modland_com_setup_Draw (API->console, selected, modland_com.mirror, modland_com.database.fileentries_n,
		                        modland_com.database.year, modland_com.database.month, modland_com.database.day,
		                        modland_com.cacheconfig, modland_com.cachepath, modland_com.showrelevantdirectoriesonly);
		while (API->console->KeyboardHit() && !quit)
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
				case KEY_ESC:
					quit = 1;
					break;
				case KEY_UP:
					if (selected)
					{
						selected--;
					}
					break;
				case KEY_DOWN:
					if (selected < 5)
					{
						selected++;
					}
					break;
				case '1': selected = 0; break;
				case '2': selected = 1; break;
				case '3': selected = 2; break;
				case '4': selected = 3; break;
				case '5': selected = 4; break;
				case '6': selected = 5; break;
				case _KEY_ENTER:
					switch (selected)
					{
						case 0:
							modland_com_mirror_Run (API);
							break;
						case 1:
						{
							void **innertoken = 0;
							modland_com_initialize_Run (innertoken, API);
							break;
						}

						case 2:
						{
							API->fsForceNextRescan();
							modland_com_database_clear ();
							modland_com_filedb_save ();
							break;
						}

						case 3:
							modland_com_cachedir_Run (API);
							break;

						case 4:
							modland_com_wipecache_Run (API);
							break;

						case 5:
						{
							modland_com.showrelevantdirectoriesonly = !modland_com.showrelevantdirectoriesonly;
							API->configAPI->SetProfileBool ("modland.com", "showrelevantdirectoriesonly", modland_com.showrelevantdirectoriesonly);
							API->configAPI->StoreConfig();
							break;
						}
					}
					break;
			}
		}
	}
}
