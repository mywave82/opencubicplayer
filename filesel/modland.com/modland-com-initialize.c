/* OpenCP Module Player
 * copyright (c) 2024-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Support for accessing https://modland.com from the filebrowser
 *  - Setup dialog, initialize filename cache database
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

/******************************************************************************** 1
                                                                                * 2
                                                                                * 3
                                                                                * 4
                                                                                * 5
   ######################### modland.com: intialize #########################   * 6
   #                                                                        #   * 7
   # [ ] Download allmods.zip metafile.                                     #   * 8
   #     HTTP/2 error. A problem was detected in the HTTP2 framing layer.   #   * 9
   #     This is somewhat generic and can be one out of several problems,   #   * 10
   #     see the error message for details.                                 #   * 11

   #     Successfully downloaded 10000KB of data, datestamped 2024-03-04    #   * 9
   #                                                                        #   * 10
   #                                                                        #   * 11
   # [ ] Parsing allmods.txt inside allmods.zip.                            #   * 12
   #     Failed to locate allmods.txt                                       #   * 13
   #                                                                        #   * 14

   #     Located 123456 files-entries in 12345 directories.                 #   * 13
   #     0 invalid entries                                                  #   * 14

   # [ ] Save cache to disk                                                 #   * 15
   #                                                                        #   * 16
   #                                                                        #   * 17
   #                    < CANCEL >                < OK >                    #   * 18
   #                                                                        #   * 19
   ##########################################################################   * 20
                                                                                * 21
                                                                                * 22
                                                                                * 23
                                                                                * 24
*********************************************************************************/



static void modland_com_initialize_Draw (
	struct console_t *console,
	int download, /* 1 = in process, 2 = OK, 3 = Failed, see message */
	const char *download_message,
	int download_size,
	int year, int month, int day,
	int parsing, /* 1 = in process, 2 = OK, 3 = Failed, see message */
	const char *parsing_message,
	int parsing_files,
	int parsing_directories,
	int parsing_invalid,
	int save,
	const char *save_message,
	int cancel, int ok
)
{
	int mlHeight = 15;
	int mlWidth  = 74;

	const char *download_string[3];
	int         download_length[3];

	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;

	console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "modland.com: initialize", 0, 0, 0);
	mlHeight -= 2;
	mlWidth -= 2;

	mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " [" "%.*o" "%c" "%.7o" "] Download allmods.zip metafile.",
		download==1 ? /* WHITE */ 15 : download==2 ? /* GREEN */ 10 : /* RED */ 12,
		download==1 ? '*'            : download==2 ? 'v'            : download==3 ? 'x' : ' ');

	if ((download == 1) && (download_size))
	{
		char temp[70];
		snprintf (temp, sizeof (temp), "Downloaded %dKB", (download_size + 512 )/ 1024);
		console->DisplayPrintf (mlTop++, mlLeft, 0x02, mlWidth, "     %67s", temp);
		mlTop++;
		mlTop++;
	} else if (download == 2)
	{
		char temp[70];
		snprintf (temp, sizeof (temp), "Successfully downloaded %dKB of data, datestamped %04d-%02d-%02d", (download_size + 512 )/ 1024, year, month, day);
		console->DisplayPrintf (mlTop++, mlLeft, 0x02, mlWidth, "     %67s" "%.9o", temp);
		mlTop++;
		mlTop++;
	} else if (download == 3)
	{
		const char *temp = download_message ? download_message : "";
		int i;

		/* Split the download message up into 3 lines (or less) */

		for (i = 0; i < 3; i++)
		{
			if (strlen (temp) <= 66)
			{
				download_string[i] = temp;
				download_length[i] = strlen (temp);
				temp += download_length[i];
			} else {
				const char *iter;
				for (iter = temp + 66; iter >= temp; iter--)
				{
					if (*iter == ' ')
					{
						iter++;
						download_string[i] = temp;
						download_length[i] = iter - temp - 1;
						temp += iter - temp;
						break;
					}
				}
			}
		}

		console->DisplayPrintf (mlTop++, mlLeft, 0x04, mlWidth, "     %67.*s", download_length[0], download_string[0]);
		console->DisplayPrintf (mlTop++, mlLeft, 0x04, mlWidth, "     %67.*s", download_length[1], download_string[1]);
		console->DisplayPrintf (mlTop++, mlLeft, 0x04, mlWidth, "     %67.*s", download_length[2], download_string[2]);
	} else {
		mlTop++;
		mlTop++;
		mlTop++;
	}

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " [" "%.*o" "%c" "%.7o" "] Parsing allmods.txt inside allmods.zip.",
		parsing==1 ? /* WHITE */ 15 : parsing==2 ? /* GREEN */ 10 : /* RED */ 12,
		parsing==1 ? '*'            : parsing==2 ? 'v'            : parsing==3 ? 'x' : ' ');

	if ((parsing == 2) || (parsing == 1))
	{
		console->DisplayPrintf (mlTop++, mlLeft, 0x02, mlWidth, "     Located %d file-entries in %d directories.", parsing_files, parsing_directories);
		console->DisplayPrintf (mlTop++, mlLeft, parsing_invalid ? 0x04:0x02, mlWidth, "     %d invalid entries.", parsing_invalid);
	} else if (parsing == 3)
	{
		console->DisplayPrintf (mlTop++, mlLeft, 0x02, mlWidth, "     %67s", parsing_message);
		mlTop++;
	} else {
		mlTop++;
		mlTop++;
	}

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " [" "%.*o" "%c" "%.7o" "] Save cache to disk.",
		save==1 ? /* WHITE */ 15 : save==2 ? /* GREEN */ 10 : /* RED */ 12,
		save==1 ? '*'            : save==2 ? 'v'            : save==3 ? 'x' : ' ');

	if (save == 3)
	{
		console->DisplayPrintf (mlTop++, mlLeft, 0x02, mlWidth, "     %67s", save_message);
	} else {
		mlTop++;
	}

	mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft+20, 0x07, mlWidth - 20, "%*.*o" "%s" "%0.7o" "%16C ""%*.*o" "%s" "%0.7o ",
		(cancel == 2) ? 7 : 0,
		(cancel == 2) ? 0 : 1,
		cancel ? "< CANCEL >" : "          ",
		(ok == 2) ? 7 : 0,
		(ok == 2) ? 0 : 1,
		ok ? "< OK >" : "      ");

	mlTop++;
}

static void modland_com_initialize_Draw_Until_Enter_Or_Exit (
	const struct DevInterfaceAPI_t *API,
	int download, /* 1 = in process, 2 = OK, 3 = Failed, see message */
	const char *download_message,
	int download_size,
	int year, int month, int day,
	int parsing, /* 1 = in process, 2 = OK, 3 = Failed, see message */
	const char *parsing_message,
	int parsing_files,
	int parsing_directories,
	int parsing_invalid,
	int save,
	const char *save_message
)
{
	while (1)
	{
		API->console->FrameLock();
		API->fsDraw();
		modland_com_initialize_Draw (API->console, download, download_message, download_size, year, month, day,
		                                           parsing, parsing_message, parsing_files, parsing_directories, parsing_invalid,
		                                           save, save_message,
		                                           0, 2);
		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
				case KEY_ESC:
				case _KEY_ENTER:
					return;
			}
		}
	}
}

static void modland_com_initialize_Run (void **token, const struct DevInterfaceAPI_t *API)
{
	struct download_request_t *download_allmods_zip; /* do not free until done parsing, due to file being open locks the file in Windows */
	struct ocpfilehandle_t *allmods_zip_filehandle;
	struct ocpdir_t *allmods_zip_dir;
	uint32_t allmods_txt_dirdb_ref;
	struct ocpfile_t *allmods_txt_file;
	struct ocpfilehandle_t *allmods_txt;
	struct textfile_t *allmods_txt_textfile;
	int save_complete = 0;
	const char *save_message = 0;
	char save_message_buffer[50];

	struct modland_com_initialize_t s;

	API->fsForceNextRescan();

	memset (&s, 0, sizeof (s));

	/* create request */

	{
		int len = strlen (modland_com.mirror ? modland_com.mirror : "") + 11 + 1;
		char *url = malloc (len);
		if (!url)
		{
			modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 3, "malloc() URL failed", 0, 0, 0, 0,
			                                                 0, 0, 0, 0, 0,
			                                                 0, 0);
			return;
		}
		snprintf (url, len, "%sallmods.zip", modland_com.mirror ? modland_com.mirror : "");
		download_allmods_zip = download_request_spawn (API->configAPI, 0, url);
		free (url);
	}
	if (!download_allmods_zip)
	{
		modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 3, "Failed to create process", 0, 0, 0, 0,
		                                                 0, 0, 0, 0, 0,
		                                                 0, 0);
		return;
	}

	/* wait for request to finish */

	while (1)
	{
		API->console->FrameLock();
		if (!download_request_iterate (download_allmods_zip))
		{
			break;
		}
		API->fsDraw();
		modland_com_initialize_Draw (API->console, 1, 0, download_allmods_zip->ContentLength, 0, 0, 0, /* pre-liminary size is available */
		                                           0, 0, 0, 0, 0,
		                                           0, 0,
		                                           2, 0);
		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();

			switch (key)
			{
				case KEY_EXIT:
				case KEY_ESC:
				case _KEY_ENTER:
					download_request_cancel (download_allmods_zip);
					download_request_free (download_allmods_zip);
					download_allmods_zip = 0;
					return;
			}
		}
	}

	/* did the request fail? */
	if (download_allmods_zip->errmsg)
	{
		modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 3, download_allmods_zip->errmsg, 0, 0, 0, 0,
		                                                 0, 0, 0, 0, 0,
		                                                 0, 0);
		download_request_free (download_allmods_zip);
		download_allmods_zip = 0;
		return;
	}

	/* get the file */
	allmods_zip_filehandle = download_request_getfilehandle (download_allmods_zip);
	if (!allmods_zip_filehandle)
	{
		modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 2, 0, download_allmods_zip->ContentLength, download_allmods_zip->Year, download_allmods_zip->Month, download_allmods_zip->Day,
		                                                 3, "Unable to open the .ZIP file", 0, 0, 0,
		                                                 0, 0);
		download_request_free (download_allmods_zip);
		download_allmods_zip = 0;
		return;
	}

	/* open the ZIP file */

	allmods_zip_dir = ocpdirdecompressor_check (allmods_zip_filehandle->origin, ".zip");
	allmods_zip_filehandle->unref (allmods_zip_filehandle);
	allmods_zip_filehandle = 0;

	if (!allmods_zip_dir)
	{
		modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 2, 0, download_allmods_zip->ContentLength, download_allmods_zip->Year, download_allmods_zip->Month, download_allmods_zip->Day,
		                                                 3, "File is not a valid .ZIP file", 0, 0, 0,
		                                                 0, 0);
		download_request_free (download_allmods_zip);
		download_allmods_zip = 0;
		return;
	}

	/* locate allmods.txt inside ZIP file */

	allmods_txt_dirdb_ref = dirdbFindAndRef (allmods_zip_dir->dirdb_ref, "allmods.txt", dirdb_use_file);
	allmods_txt_file = allmods_zip_dir->readdir_file (allmods_zip_dir, allmods_txt_dirdb_ref);
	allmods_zip_dir->unref (allmods_zip_dir);
	allmods_zip_dir = 0;
	dirdbUnref (allmods_txt_dirdb_ref, dirdb_use_file);
	if (!allmods_txt_file)
	{
		modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 2, 0, download_allmods_zip->ContentLength, download_allmods_zip->Year, download_allmods_zip->Month, download_allmods_zip->Day,
		                                                 3, "Failed to locate allmods.txt inside allmods.zip", 0, 0, 0,
		                                                 0, 0);
		download_request_free (download_allmods_zip);
		download_allmods_zip = 0;
		return;
	}

	/* open allmods.txt */

	allmods_txt = allmods_txt_file->open (allmods_txt_file);
	allmods_txt_file->unref (allmods_txt_file);
	allmods_txt_file = 0;
	if (!allmods_txt)
	{
		modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 2, 0, download_allmods_zip->ContentLength, download_allmods_zip->Year, download_allmods_zip->Month, download_allmods_zip->Day,
		                                                 3, "Failed to open allmods.txt inside allmods.zip", 0, 0, 0,
		                                                 0, 0);
		download_request_free (download_allmods_zip);
		download_allmods_zip = 0;
		return;
	}

	modland_com_database_clear();
	modland_com.database.year  = download_allmods_zip->Year;
	modland_com.database.month = download_allmods_zip->Month;
	modland_com.database.day   = download_allmods_zip->Day;

	/* and start to parse it as lines of text */

	allmods_txt_textfile = textfile_start (allmods_txt);
	allmods_txt->unref (allmods_txt);
	allmods_txt = 0;
	if (!allmods_txt_textfile)
	{
		modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 2, 0, download_allmods_zip->ContentLength, download_allmods_zip->Year, download_allmods_zip->Month, download_allmods_zip->Day,
		                                                 3, "Failed to open allmods.txt inside allmods.zip as textfile", 0, 0, 0,
		                                                 0, 0);
		download_request_free (download_allmods_zip);
		download_allmods_zip = 0;
		return;
	}

	/* parse the text */

	{
		int n = 0;
		const char *line;
		int maxcount = 1000;
		int nodec = 0;

		while ((line = textfile_fgets (allmods_txt_textfile)))
		{
			char *end;
			long filesize;

			if (!strlen (line))
			{
				continue;
			}

			/* before ending with number */
			filesize = strtol (line, &end, 10);
			if (end == line)
			{
				continue;
			}
			if (filesize <= 0)
			{
				continue;
			}
			line = end;
			while ((*line == '\t') || (*line == ' ')) line++;

			modland_com_add_data_line (&s, line, filesize);

			n++;
			if (n >= maxcount)
			{
				n = 0;
				if (API->console->PollFrameLock())
				{
					if ((maxcount >= 200) && (!nodec))
					{
						maxcount -= 100;
					}
					nodec = 0;
					API->fsDraw();

					modland_com_initialize_Draw (API->console, 2, 0, download_allmods_zip->ContentLength, download_allmods_zip->Year, download_allmods_zip->Month, download_allmods_zip->Day,
					                                           1, 0, modland_com.database.fileentries_n, modland_com.database.direntries_n, s.invalid_entries,
					                                           0, 0,
					                                           2, 0);
					while (API->console->KeyboardHit())
					{
						int key = API->console->KeyboardGetChar();

						switch (key)
						{
							case KEY_EXIT:
							case KEY_ESC:
							case _KEY_ENTER:
								textfile_stop (allmods_txt_textfile);
								allmods_txt_textfile = 0;

								download_request_free (download_allmods_zip);
								download_allmods_zip = 0;

								modland_com_database_clear ();

								return;
						}
					}
				} else {
					maxcount += 100;
					nodec = 1;
				}
			}
		}
	}
	textfile_stop (allmods_txt_textfile);
	allmods_txt_textfile = 0;

	/* sort the database and finalize the database */

	if (modland_com_sort ())
	{
		modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 2, 0, download_allmods_zip->ContentLength, download_allmods_zip->Year, download_allmods_zip->Month, download_allmods_zip->Day,
		                                                 2, 0, 0, 0, 0,
		                                                 3, "Out of memory");
		modland_com_database_clear ();

		download_request_free (download_allmods_zip);
		download_allmods_zip = 0;
		return;
	}

	if (modland_com_filedb_save_start())
	{
		save_complete = 2;
		save_message = "Failed to initialize saving";
	}

	while (!save_complete)
	{
		if (API->console->PollFrameLock())
		{
			API->fsDraw();

			save_message = save_message_buffer;
			snprintf (save_message_buffer, sizeof (save_message_buffer), "Written %lu of %lu file names", (unsigned long)(modland_com_filedb_save_f + 1), (unsigned long)(modland_com.database.fileentries_n));

			modland_com_initialize_Draw (API->console, 2, 0, download_allmods_zip->ContentLength, download_allmods_zip->Year, download_allmods_zip->Month, download_allmods_zip->Day,
			                                           1, 0, modland_com.database.fileentries_n, modland_com.database.direntries_n, s.invalid_entries,
			                                           save_complete + 1, save_message,
			                                           2, 0);
			while (API->console->KeyboardHit())
			{
				int key = API->console->KeyboardGetChar();

				switch (key)
				{
					case KEY_EXIT:
					case KEY_ESC:
					case _KEY_ENTER:
						modland_com_filedb_save_abort();
						save_complete = 2;
						save_message = "Save aborted";
						return;
				}
			}
		}
		switch (modland_com_filedb_save_iterate())
		{
			default:
			case -1:
				save_message = "Writing data failed";
				save_complete = 2;
				break;
			case 0:
				save_message = "Completed successfully";
				save_complete = 1;
				break;
			case 1:
				break;
		}
	}

	/* we are finished */

	modland_com_initialize_Draw_Until_Enter_Or_Exit (API, 2, 0, download_allmods_zip->ContentLength, download_allmods_zip->Year, download_allmods_zip->Month, download_allmods_zip->Day,
	                                                 2, 0, modland_com.database.fileentries_n, modland_com.database.direntries_n, s.invalid_entries,
	                                                 save_complete + 1, save_message);
	download_request_free (download_allmods_zip);
	download_allmods_zip = 0;
}
