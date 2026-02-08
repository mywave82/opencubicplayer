/* OpenCP Module Player
 * copyright (c) 2024-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Support for accessing https://modland.com from the filebrowser
 *  - Setup dialog, cachedir
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

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#undef DIRSEPARATOR
#ifdef _WIN32
# define DIRSEPARATOR "\\"
#else
# define DIRSEPARATOR "/"
#endif


static void modland_com_cachedir_Draw (
	struct console_t *console,
	const int origselected,
	const int selected,
	const char *ocpdatahome_modland_com,
	const char *home_modland_com,
	const char *ocpdata_modland_com,
	const char *temp_modland_com,
	const char *custom_modland_com,
	char **cacheconfigcustom,
	int *editcacheconfigquit
)
{
/*
********************* modland.com: select cachedir ***********************
*                                                                        *
* Select a cachedir with <UP>, <DOWN> and <SPACE>.                       *
* Edit custom with <ENTER>. Exit dialog with <ESC>.                      *
*                                                                        *
**************************************************************************
*                                                                        *
* ( ) $OCPDATAHOME/modland.com (default)                                 *
*   => /home/stian/.local/share/ocp/modland.com                          *
*                                                                        *
* ( ) $HOME/modland.com                                                  *
*   => /home/stian/tmp/modland.com                                       *
*                                                                        *
* ( ) $OCPDATA/modland.com (might not be writable)                       *
*   => /usr/local/share/ocp/modland.com                                  *
*                                                                        *
* ( ) $TEMP/modland.com (might not be system uniqe and writable)         *
*   => /tmp/modland.com                                                  *
*                                                                        *
* ( ) custom                                                             *
*   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX   *
*                                                                        *
**************************************************************************/
	int compact = plScrHeight < 23;
	int mlHeight = compact ? 19 : 23;
	int mlWidth = MAX(74, plScrWidth - 30);
	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;

#if (CONSOLE_MIN_Y < 19)
# error alsaSetupRun() requires CONSOLE_MIN_Y >= 19
#endif

	console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "modland.com: select cachedir ", 0, 5, 0);
	mlHeight -= 2;
	mlWidth -= 2;

	mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Select a cachedir with %.15o<UP>%.7o, %.15o<DOWN>%.7o and %.15o<SPACE>%.7o.");
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Edit custom with %.15o<ENTER>%.7o. Exit dialog with %.15o<ESC>%.7o.");

	mlTop++;

	mlTop++; // 5: horizontal line

	mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, " (%.2o%c%.9o) " "%*.*o" "$OCPDATAHOME" DIRSEPARATOR "modland.com" "%0.7o (default)",
		(0==origselected) ? '*' : ' ',
		(0==selected) ? 7 : 0,
		(0==selected) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "     => %*S", mlWidth - 8, ocpdatahome_modland_com);

	if (!compact) mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, " (%.2o%c%.9o) " "%*.*o" "$HOME" DIRSEPARATOR "modland.com%0.7o",
		(1==origselected) ? '*' : ' ',
		(1==selected) ? 7 : 0,
		(1==selected) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "     => %*S", mlWidth - 8, home_modland_com);

	if (!compact) mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, " (%.2o%c%.9o) " "%*.*o" "$OCPDATA" DIRSEPARATOR "modland.com" "%0.7o (might not be writable)",
		(2==origselected) ? '*' : ' ',
		(2==selected) ? 7 : 0,
		(2==selected) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "     => %*S", mlWidth - 8, ocpdata_modland_com);

	if (!compact) mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, " (%.2o%c%.9o) " "%*.*o" "$TEMP" DIRSEPARATOR "modland.com" "%0.7o (might not be system uniqe and writable)",
		(3==origselected) ? '*' : ' ',
		(3==selected) ? 7 : 0,
		(3==selected) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "     => %*S", mlWidth - 8, temp_modland_com);

	if (!compact) mlTop++;

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " custom:");

	if (editcacheconfigquit)
	{
		console->DisplayPrintf (mlTop, mlLeft, 0x09, 4, " (%.2o%c%.9o)",
			(4==origselected) ? '*' : ' ');
		switch (console->EditStringUTF8(mlTop++, mlLeft + 5, mlWidth - 10, cacheconfigcustom))
		{
			case -1:
			case 0:
				*editcacheconfigquit = 1;
				break;
			default:
			case 1:
				break;
		}
	} else {
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, " (%.2o%c%.9o) " "%*.*o" "%*S" "%0.9o ",
			(4==origselected) ? '*' : ' ',
			(4==selected) ? 7 : 0,
			(4==selected) ? 1 : 3,
			mlWidth - 10,
			*cacheconfigcustom);
	}

	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "     => %*s", mlWidth - 8, custom_modland_com);
}

static char *modland_com_resolve_cachedir3 (const char *src)
{
	char *retval = malloc (strlen (src) + 2);
	char *iter;
	if (!retval)
	{
		return 0;
	}
	sprintf (retval, "%s/", src); /* ensure that it ends with a slash */

	for (iter = retval; *iter;)
	{ /* check for double slash */
		if ((!strncmp (iter, "//", 2)) ||
		    (!strncmp (iter, "\\\\", 2)) ||
		    (!strncmp (iter, "/\\", 2)) ||
		    (!strncmp (iter, "\\/", 2)))
		{
			memmove (iter, iter+1, strlen (iter + 1) + 1);
		} else { /* flip slashes if they are the wrong direction */
#ifdef _WIN32
			if (*iter == '/')
			{
				*iter = '\\';
			}
#else
			if (*iter == '\\')
			{
				*iter = '/';
			}
#endif
			iter++;
		}
	}
	return retval;
}

static char *modland_com_resolve_cachedir2 (const char *src1, const char *src2)
{
	int len = strlen (src1) + strlen (src2) + 1;
	char *temp = malloc (len);
	char *retval;
	if (!temp)
	{
		return 0;
	}

	snprintf (temp, len, "%s%s", src1, src2);
	retval = modland_com_resolve_cachedir3 (temp);
	free (temp);
	return retval;
}

static char *modland_com_resolve_cachedir (const struct configAPI_t *configAPI, const char *src)
{
	if ((!strncmp (src, "~\\", 2)) ||
	    (!strncmp (src, "~/", 2)))
	{
		return modland_com_resolve_cachedir2 (configAPI->HomePath, src+2);
	} else if ((!strncmp (src, "$HOME\\", 6)) ||
	           (!strncmp (src, "$HOME/", 6)))
	{
		return modland_com_resolve_cachedir2 (configAPI->HomePath, src+6);

	} else if ((!strncmp (src, "$OCPDATAHOME\\", 13)) ||
	           (!strncmp (src, "$OCPDATAHOME/", 13)))
	{
		return modland_com_resolve_cachedir2 (configAPI->DataHomePath, src+13);
	} else if ((!strncmp (src, "$OCPDATA\\", 9)) ||
	           (!strncmp (src, "$OCPDATA/", 9)))
	{
		return modland_com_resolve_cachedir2 (configAPI->DataPath, src+9);
	} else if ((!strncmp (src, "$TEMP\\", 6)) ||
	           (!strncmp (src, "$TEMP/", 6)))
	{
		return modland_com_resolve_cachedir2 (configAPI->TempPath, src+6);
	} else {
		return modland_com_resolve_cachedir3 (src);
	}
}

static void modland_com_cachedir_Save (const struct DevInterfaceAPI_t *API, int selected, char **custom_modland_com)
{
	free (modland_com.cacheconfig);
	switch (selected)
	{
		case 0: modland_com.cacheconfig = modland_com_strdup_slash_filesystem ("$OCPDATAHOME" DIRSEPARATOR "modland.com"); break;
		case 1: modland_com.cacheconfig = modland_com_strdup_slash_filesystem ("$HOME" DIRSEPARATOR "modland.com"); break;
		case 2: modland_com.cacheconfig = modland_com_strdup_slash_filesystem ("$OCPDATA" DIRSEPARATOR "modland.com/"); break;
		case 3: modland_com.cacheconfig = modland_com_strdup_slash_filesystem ("$TEMP" DIRSEPARATOR "modland.com/"); break;

		default:
		case 4:
		{
			char *t = modland_com.cacheconfigcustom;
			modland_com.cacheconfig = modland_com_strdup_slash_filesystem (t);
			modland_com.cacheconfigcustom = modland_com_strdup_slash_filesystem (t);
			free (t);

			free (*custom_modland_com);
			*custom_modland_com = modland_com_resolve_cachedir (API->configAPI, modland_com.cacheconfigcustom);
		}
	}

	API->configAPI->SetProfileString ("modland.com", "cachedir", modland_com.cacheconfig);
	API->configAPI->SetProfileString ("modland.com", "cachedircustom", modland_com.cacheconfigcustom);
	API->configAPI->SetProfileComment ("modland.com", "cachedircustom", "; If a non-standard cachedir has been used in the past, it is stored here");

	API->configAPI->StoreConfig();

	free (modland_com.cachepath);
	modland_com.cachepath = 0;
	modland_com.cachepath = modland_com_resolve_cachedir (API->configAPI, modland_com.cacheconfig);

	free (modland_com.cachepathcustom);
	modland_com.cachepathcustom = 0;
	modland_com.cachepathcustom = modland_com_resolve_cachedir (API->configAPI, modland_com.cacheconfigcustom);
}

static void modland_com_cachedir_Run (const struct DevInterfaceAPI_t *API)
{
	char *home_modland_com        = modland_com_resolve_cachedir2 (API->configAPI->HomePath,     "modland.com");
	char *ocpdatahome_modland_com = modland_com_resolve_cachedir2 (API->configAPI->DataHomePath, "modland.com");
	char *ocpdata_modland_com     = modland_com_resolve_cachedir2 (API->configAPI->DataPath,     "modland.com");
	char *temp_modland_com        = modland_com_resolve_cachedir2 (API->configAPI->TempPath,     "modland.com");
	char *custom_modland_com      = modland_com_resolve_cachedir  (API->configAPI, modland_com.cacheconfigcustom);
	int selected;
	int origselected;
	int quit = 0;

	if (((!strncmp (modland_com.cacheconfig, "~\\", 2)) ||
	     (!strncmp (modland_com.cacheconfig, "~/" , 2))) &&
            (!strcmp (modland_com.cacheconfig + 2, "modland.com" DIRSEPARATOR)))
	{
		selected = 1;
	} else if (((!strncmp (modland_com.cacheconfig, "$HOME\\", 6)) ||
	            (!strncmp (modland_com.cacheconfig, "$HOME/" , 6))) &&
                   (!strcmp (modland_com.cacheconfig + 6, "modland.com" DIRSEPARATOR)))
	{
		selected = 1;

	} else if (((!strncmp (modland_com.cacheconfig, "$OCPDATAHOME\\", 13)) ||
	            (!strncmp (modland_com.cacheconfig, "$OCPDATAHOME/", 13))) &&
                   (!strcmp (modland_com.cacheconfig + 13, "modland.com" DIRSEPARATOR)))
	{
		selected = 0;
	} else if (((!strncmp (modland_com.cacheconfig, "$OCPDATA\\", 9)) ||
	            (!strncmp (modland_com.cacheconfig, "$OCPDATA/", 9))) &&
                   (!strcmp (modland_com.cacheconfig + 9, "modland.com" DIRSEPARATOR)))
	{
		selected = 2;
	} else if (((!strncmp (modland_com.cacheconfig, "$TEMP\\", 6)) ||
	            (!strncmp (modland_com.cacheconfig, "$TEMP/", 6))) &&
                   (!strcmp (modland_com.cacheconfig + 6, "modland.com" DIRSEPARATOR)))
	{
		selected = 3;
	} else {
		selected = 4;
		free (modland_com.cacheconfigcustom);
		modland_com.cacheconfigcustom = strdup (modland_com.cacheconfig);
	}

	origselected = selected;

	while (!quit)
	{
		API->fsDraw();
		modland_com_cachedir_Draw (
			API->console,
			origselected,
			selected,
			ocpdatahome_modland_com,
			home_modland_com,
			ocpdata_modland_com,
			temp_modland_com,
			custom_modland_com,
			&modland_com.cacheconfigcustom,
			0
		);

		while (API->console->KeyboardHit() && !quit)
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
					goto free_return;
					return;
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
					if (selected < 4)
					{
						selected++;
					}
					break;
				case ' ':
					origselected = selected;
					modland_com_cachedir_Save (API, selected, &custom_modland_com);
					break;
				case _KEY_ENTER:
					origselected = selected;
					if (selected == 4)
					{
						int innerquit = 0;
						while (!innerquit)
						{
							modland_com_cachedir_Draw (
								API->console,
								origselected,
								selected,
								ocpdatahome_modland_com,
								home_modland_com,
								ocpdata_modland_com,
								temp_modland_com,
								custom_modland_com,
								&modland_com.cacheconfigcustom,
								&innerquit
							);

							API->console->FrameLock();
						}
					}
					modland_com_cachedir_Save (API, selected, &custom_modland_com);
					break;
			}
		}
		API->console->FrameLock();
	}


free_return:
	free (home_modland_com);
	free (ocpdatahome_modland_com);
	free (ocpdata_modland_com);
	free (temp_modland_com);
	free (custom_modland_com);
}
