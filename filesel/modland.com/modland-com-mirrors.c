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
	int mlHeight = 12 + NUM_MIRRORS;
	int mlWidth = 74;

	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;
	int i;

	console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "modland.com: select mirror", 0, 5, 0);
	mlWidth  -= 2;
	mlHeight -= 2;
	mlTop++;
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "Select a mirror with %.15o<UP>%.7o, %.15o<DOWN>%.7o and %.15o<SPACE>%.7o.");
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Edit custom with %.15o<ENTER>%.7o. Exit dialog with %.15o<ESC>%.7o.");
	mlTop++;
	mlTop++; // 5: horizontal line
	mlTop++;
	for (i=0; i < NUM_MIRRORS; i++)
	{
		char mirror_padded[63];
		snprintf (mirror_padded, sizeof (mirror_padded), "%s%s",
			(!strncasecmp(modland_com_official_mirror[i], "ftp:", 4)) ? "  " : (!strncasecmp(modland_com_official_mirror[i], "http:", 5)) ? " " : "",
			modland_com_official_mirror[i]);
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, " (%.2o%c%.9o) " "%*.*o" "%*s" "%0.7o ",
			(i==origselected) ? '*' : ' ',
			(i==selected) ? 7 : 0,
			(i==selected) ? 1 : 3,
			62, mirror_padded);
	}
	console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "   custom: ");

	if (editmirrorquit)
	{
		console->DisplayPrintf (mlTop, mlLeft, 0x09, 6, " (%.2o%c%.9o) ",
			(origselected==NUM_MIRRORS) ? '*' : ' ');
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
		console->DisplayPrintf (mlTop++, mlLeft, 0x09, mlWidth, " " "(%.2o%c%.9o) " "%*.*o" "%*s" "%0.7o ",
			(origselected==NUM_MIRRORS) ? '*' : ' ',
			(selected == NUM_MIRRORS) ? 7 : 0,
			(selected == NUM_MIRRORS) ? 1 : 3,
			mlWidth - 10,
			*mirrorcustom);
	}

	mlTop++;
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
