static const char *modland_com_official_mirror[] =
{
	/* "https://modland.com/", not the correct name */
	/*  "http://modland.com/", not the correct name */
	   "https://ftp.modland.com/", /* master */
	    "http://ftp.modland.com/",
	     "ftp://ftp.modland.com/",
	   "https://ftp.amigascne.org/mirrors/ftp.modland.com/", /* not announced            */
	    "http://ftp.amigascne.org/mirrors/ftp.modland.com/", /* not announced            */
	     "ftp://ftp.amigascne.org/mirrors/ftp.modland.com/", /*                very slow */
	/* "https://aero.exotica.org.uk/pub/mirrors/modland/",      not announced, certificate not valid, data not present */
	    "http://aero.exotica.org.uk/pub/mirrors/modland/",   /* not announced */
	/*    ftp://aero.exotica.org.uk/pub/mirrors/modland/",                     unable to connect */
	/* "https://modland.antarctica.no/",                        not announced, certificate not valid, data IS present */
	    "http://modland.antarctica.no/",
};

#define NUM_MIRRORS (sizeof(modland_com_official_mirror)/sizeof(modland_com_official_mirror[0]))

static void modland_com_mirror_Draw (
	struct console_t *console,
	const int origselected,
	const int selected,
	char **mirrorcustom,
	int *editmirrorquit
)
{
/*
********************** modland.com select mirror *************************
*                                                                        *
* Select a mirror with <UP>, <DOWN> and <SPACE>.                         *
* Edit custom with <ENTER>. Exit dialog with <ESC>.                      *
*                                                                        *
**************************************************************************
*                                                                        *
* (*) https://ftp.modland.com/                                           *
* ( )  http://ftp.modland.com/                                           *
* ( )   ftp://ftp.modland.com/                                           *
*                                                                        *
* ( ) custom:                                                            *
*     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX      *
*                                                                        *
**************************************************************************/
	const int mlHeight = 12 + NUM_MIRRORS;
	const int mlWidth = 74;

	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;
	int i;

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda" "%22C\xc4" " modland.com: select mirror " "%22C\xc4" "\xbf");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%0.7o" " Select a mirror with %.15o<UP>%.7o, %.15o<DOWN>%.7o and %.15o<SPACE>%.7o.%.9o" "%*C " "\xb3", mlWidth - 49);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%0.7o" " Edit custom with %.15o<ENTER>%.7o. Exit dialog with %.15o<ESC>%.7o.%.9o" "%*C " "\xb3", mlWidth - 52);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2);
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	for (i=0; i < NUM_MIRRORS; i++)
	{
		char mirror_padded[63];
		snprintf (mirror_padded, sizeof (mirror_padded), "%s%s",
			(!strncasecmp(modland_com_official_mirror[i], "ftp:", 4)) ? "  " : (!strncasecmp(modland_com_official_mirror[i], "http:", 5)) ? " " : "",
			modland_com_official_mirror[i]);
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "(%.2o%c%.9o) " "%*.*o" "%*s" "%0.9o     \xb3",
			(i==origselected) ? '*' : ' ',
			(i==selected) ? 7 : 0,
			(i==selected) ? 1 : 3,
			62, mirror_padded);
	}
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%.7o" "   custom: " "%*C " "%.9o" "\xb3",
			mlWidth - 13);

	if (editmirrorquit)
	{
		console->DisplayPrintf (mlTop, mlLeft, 0x09, 6, "\xb3 " "(%.2o%c%.9o) ",
			(origselected==NUM_MIRRORS) ? '*' : ' ');
		console->DisplayPrintf (mlTop, mlLeft+mlWidth-6, 0x09, 6, "     \xb3");
		switch (console->EditStringASCII(mlTop++, mlLeft + 6, mlWidth - 12, mirrorcustom))
		{
			case -1:
			case 0:
				*editmirrorquit = 1;
				break;
			default:
			case 1:
				break;
		}
	} else {
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3 " "(%.2o%c%.9o) " "%*.*o" "%*s" "%0.9o     " "\xb3",
			(origselected==NUM_MIRRORS) ? '*' : ' ',
			(selected == NUM_MIRRORS) ? 7 : 0,
			(selected == NUM_MIRRORS) ? 1 : 3,
			mlWidth - 12,
			*mirrorcustom);
	}

	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3" "%72C " "\xb3");
	console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

static void modland_com_mirror_Save (const struct DevInterfaceAPI_t *API, int selected)
{
	if (selected < NUM_MIRRORS)
	{
		free (modland_com.mirror);
		modland_com.mirror = modland_com_strdup_slash (modland_com_official_mirror[selected]);
	} else {
		char *t = modland_com.mirrorcustom;
		free (modland_com.mirror);
		modland_com.mirror = modland_com_strdup_slash (t);
		modland_com.mirrorcustom = modland_com_strdup_slash (t);
		free (t);
	}

	API->configAPI->SetProfileString ("modland.com", "mirror", modland_com.mirror);
	API->configAPI->SetProfileString ("modland.com", "mirrorcustom", modland_com.mirrorcustom);
	API->configAPI->SetProfileComment ("modland.com", "mirrorcustom", "; If a non-standard mirror has been used in the past, it is stored here");

	API->configAPI->StoreConfig();
}

static void modland_com_mirror_Run (const struct DevInterfaceAPI_t *API)
{
	int selected = 0;
	int origselected;
	int quit = 0;

	for (selected = 0; selected < NUM_MIRRORS; selected++)
	{
		if (!strcasecmp (modland_com.mirror, modland_com_official_mirror[selected]))
		{
			break;
		}
	}
	if (selected >= NUM_MIRRORS)
	{
		free (modland_com.mirrorcustom);
		modland_com.mirrorcustom = strdup (modland_com.mirror);
	}
	origselected = selected;

	while (!quit)
	{
		API->fsDraw();
		modland_com_mirror_Draw (API->console, origselected, selected, &modland_com.mirrorcustom, 0);
		while (API->console->KeyboardHit() && !quit)
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
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
					if (selected < NUM_MIRRORS)
					{
						selected++;
					}
					break;
				case ' ':
					origselected = selected;
					modland_com_mirror_Save (API, selected);
					break;
				case _KEY_ENTER:
					origselected = selected;
					if (selected == NUM_MIRRORS)
					{
						int innerquit = 0;
						while (!innerquit)
						{
							modland_com_mirror_Draw (API->console, origselected, selected, &modland_com.mirrorcustom, &innerquit);
							API->console->FrameLock();
						}
					}
					modland_com_mirror_Save (API, selected);
					break;
			}
		}
		API->console->FrameLock();
	}
}
