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
*   => /tmp/modland.com                                                  *
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
	const int mlHeight = 23;
	const int mlWidth = 74;

	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda" "%21C\xc4" " modland.com: select cachedir " "%21C\xc4" "\xbf");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%0.7o" " Select a cachedir with %.15o<UP>%.7o, %.15o<DOWN>%.7o and %.15o<SPACE>%.7o.%.9o" "%*C " "\xb3", mlWidth - 51);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%0.7o" " Edit custom with %.15o<ENTER>%.7o. Exit dialog with %.15o<ESC>%.7o.%.9o" "%*C " "\xb3", mlWidth - 52);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "(%.2o%c%.9o) " "%*.*o" "$OCPDATAHOME/modland.com" " %0.7o(default)%.9o%33C \xb3",
		(0==origselected) ? '*' : ' ',
		(0==selected) ? 7 : 0,
		(0==selected) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "    %.7o=> %64s" "%.9o\xb3", ocpdatahome_modland_com);

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "(%.2o%c%.9o) " "%*.*o" "$HOME/modland.com" "%.9o%50C \xb3",
		(1==origselected) ? '*' : ' ',
		(1==selected) ? 7 : 0,
		(1==selected) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "    %.7o=> %64s" "%.9o\xb3", home_modland_com);

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "(%.2o%c%.9o) " "%*.*o" "$OCPDATA/modland.com" " %0.7o(might not be writable)%.9o%23C \xb3",
		(2==origselected) ? '*' : ' ',
		(2==selected) ? 7 : 0,
		(2==selected) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "    %.7o=> %64s" "%.9o\xb3", home_modland_com);

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "(%.2o%c%.9o) " "%*.*o" "$TEMP/modland.com" " %0.7o(might not be system uniqe and writable)%.9o%9C \xb3",
		(3==origselected) ? '*' : ' ',
		(3==selected) ? 7 : 0,
		(3==selected) ? 1 : 3);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "    %.7o=> %64s" "%.9o\xb3", temp_modland_com);

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "%.7o" "custom: " "%*C " "%.9o" "\xb3",
		mlWidth - 11);

	if (editcacheconfigquit)
	{
		console->DisplayPrintf (mlTop, mlLeft, 0x09, 6, "\xb3 " "(%.2o%c%.9o) ",
			(4==origselected) ? '*' : ' ');
		console->DisplayPrintf (mlTop, mlLeft+mlWidth-6, 0x09, 6, "     \xb3");
		switch (console->EditStringASCII(mlTop++, mlLeft + 6, mlWidth - 12, cacheconfigcustom))
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
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "(%.2o%c%.9o) " "%*.*o" "%*s" "%0.9o     " "\xb3",
			(4==origselected) ? '*' : ' ',
			(4==selected) ? 7 : 0,
			(4==selected) ? 1 : 3,
			mlWidth - 12,
			*cacheconfigcustom);
	}

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "    %.7o=> %64s" "%.9o\xb3", custom_modland_com);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
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
		case 0: modland_com.cacheconfig = modland_com_strdup_slash ("$OCPDATAHOME/modland.com/"); break;
		case 1: modland_com.cacheconfig = modland_com_strdup_slash ("$HOME/modland.com/"); break;
		case 2: modland_com.cacheconfig = modland_com_strdup_slash ("$OCPDATA/modland.com/"); break;
		case 3: modland_com.cacheconfig = modland_com_strdup_slash ("$TEMP/modland.com/"); break;

		default:
		case 4:
		{
			char *t = modland_com.cacheconfigcustom;
			modland_com.cacheconfig = modland_com_strdup_slash (t);
			modland_com.cacheconfigcustom = modland_com_strdup_slash (t);
			free (t);

			free (*custom_modland_com);
			*custom_modland_com = modland_com_resolve_cachedir (API->configAPI, modland_com.cacheconfigcustom);
		}
	}

	API->configAPI->SetProfileString ("modland.com", "cachedir", modland_com.cacheconfig);
	API->configAPI->SetProfileString ("modland.com", "cachedircustom", modland_com.cacheconfigcustom);
	API->configAPI->SetProfileComment ("modland.com", "cachedircustom", "; If a non-standard cachedir has been used in the past, it is stored here");

	API->configAPI->StoreConfig();
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
            (!strcmp (modland_com.cacheconfig + 2, "modland.com")))
	{
		selected = 1;
	} else if (((!strncmp (modland_com.cacheconfig, "$HOME\\", 6)) ||
	            (!strncmp (modland_com.cacheconfig, "$HOME/" , 6))) &&
                   (!strcmp (modland_com.cacheconfig + 6, "modland.com/")))
	{
		selected = 1;

	} else if (((!strncmp (modland_com.cacheconfig, "$OCPDATAHOME\\", 13)) ||
	            (!strncmp (modland_com.cacheconfig, "$OCPDATAHOME/", 13))) &&
                   (!strcmp (modland_com.cacheconfig + 13, "modland.com/")))
	{
		selected = 0;
	} else if (((!strncmp (modland_com.cacheconfig, "$OCPDATA\\", 9)) ||
	            (!strncmp (modland_com.cacheconfig, "$OCPDATA/", 9))) &&
                   (!strcmp (modland_com.cacheconfig + 9, "modland.com/")))
	{
		selected = 2;
	} else if (((!strncmp (modland_com.cacheconfig, "$TEMP\\", 6)) ||
	            (!strncmp (modland_com.cacheconfig, "$TEMP/", 6))) &&
                   (!strcmp (modland_com.cacheconfig + 9, "modland.com/")))
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
