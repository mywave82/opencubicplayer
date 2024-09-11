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
	const int mlHeight = 20;
	const int mlWidth = 74;

	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;

	console->DisplayPrintf (mlTop + 0, mlLeft, 0x09, mlWidth, "\xda" "%26C\xc4" " modland.com: setup " "%26C\xc4" "\xbf");

	console->DisplayPrintf (mlTop + 1, mlLeft, 0x09, mlWidth, "\xb3%.7o Use arrow keys and %.15o<ENTER>%.7o to navigate. %.15o<ESC>%.7o to close.%*C %.9o\xb3", mlWidth - 58);

	console->DisplayPrintf (mlTop + 2, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop + 3, mlLeft, 0x09, mlWidth, "\xb3" " %.7oCurrent mirror: %.2o%55S" "%.9o\xb3", currentmirror);

	console->DisplayPrintf (mlTop + 4, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	if (numfileentries)
	{
		console->DisplayPrintf (mlTop + 5, mlLeft, 0x09, mlWidth - 1, "\xb3 " "%.7o" "Database datestamp is %0.2o%04d-%02d-%02d.", year, month, day);
		console->DisplayPrintf (mlTop + 5, mlLeft + mlWidth - 1, 0x09, 1, "\xb3");

		console->DisplayPrintf (mlTop + 6, mlLeft, 0x09, mlWidth - 1, "\xb3 " "%.2o" "%d" "%.7o" " file-entries stored in the database.", numfileentries);
		console->DisplayPrintf (mlTop + 6, mlLeft + mlWidth - 1, 0x09, 1, "\xb3");
	} else {
		console->DisplayPrintf (mlTop + 5, mlLeft, 0x09, mlWidth, "\xb3 " "%.7o" "No database loaded" "%53C " "\xb3");
		console->DisplayPrintf (mlTop + 6, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	}

	console->DisplayPrintf (mlTop + 7, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop + 8, mlLeft, 0x09, mlWidth - 1, "\xb3" " %.7oFile cache is stored in %.2o%S%.7o =>", symbolicstore);
	console->DisplayPrintf (mlTop + 8, mlLeft + mlWidth - 1, 0x09, 1, "\xb3");
	console->DisplayPrintf (mlTop + 9, mlLeft, 0x09, mlWidth, "\xb3" " %.2o%71S%.9o\xb3", resolvedstore);

	console->DisplayPrintf (mlTop + 10, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	if (showrelevantdirectoriesonly)
	{
		console->DisplayPrintf (mlTop + 11, mlLeft, 0x09, mlWidth, "\xb3" " %.7oOCP currently %.2oonly shows relevant%.7o directories%26C %.9o\xb3");
	} else {
		console->DisplayPrintf (mlTop + 11, mlLeft, 0x09, mlWidth, "\xb3" " %.7oOCP currently %.2oshows all%.7o directories%36C %.9o\xb3");
	}

	console->DisplayPrintf (mlTop + 12, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop + 13, mlLeft, 0x09, mlWidth, "\xb3" " %*.*o1.%.*o Select mirror                        %30C %0.9o \xb3", (selected == 0) ? 7 : 0, (selected == 0) ? 1 : 7, (selected == 0) ? 1 : 3);
	if (!numfileentries)
	{
		console->DisplayPrintf (mlTop + 14, mlLeft, 0x09, mlWidth, "\xb3" " %*.*o2.%.*o Fetch database                       %30C %0.9o \xb3", (selected == 1) ? 7 : 0, (selected == 1) ? 1 : 7, (selected == 1) ? 1 : 3);
	} else {
		console->DisplayPrintf (mlTop + 14, mlLeft, 0x09, mlWidth, "\xb3" " %*.*o2.%.*o Refresh database                     %30C %0.9o \xb3", (selected == 1) ? 7 : 0, (selected == 1) ? 1 : 7, (selected == 1) ? 1 : 3);
	}
	console->DisplayPrintf (mlTop + 15, mlLeft, 0x09, mlWidth, "\xb3" " %*.*o3.%.*o Remove database                      %30C %0.9o \xb3", (selected == 2) ? 7 : 0, (selected == 2) ? 1 : 7, (selected == 2) ? 1 : 3);
	console->DisplayPrintf (mlTop + 16, mlLeft, 0x09, mlWidth, "\xb3" " %*.*o4.%.*o Select cache directory               %30C %0.9o \xb3", (selected == 3) ? 7 : 0, (selected == 3) ? 1 : 7, (selected == 3) ? 1 : 3);
	console->DisplayPrintf (mlTop + 17, mlLeft, 0x09, mlWidth, "\xb3" " %*.*o5.%.*o Wipe cache directory                 %30C %0.9o \xb3", (selected == 4) ? 7 : 0, (selected == 4) ? 1 : 7, (selected == 4) ? 1 : 3);
	console->DisplayPrintf (mlTop + 18, mlLeft, 0x09, mlWidth, "\xb3" " %*.*o6.%.*o Toggle only show relevant directories%30C %0.9o \xb3", (selected == 5) ? 7 : 0, (selected == 5) ? 1 : 7, (selected == 5) ? 1 : 3);
	console->DisplayPrintf (mlTop + 19, mlLeft, 0x09, mlWidth, "\xc0" "%72C\xc4" "\xd9");
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
