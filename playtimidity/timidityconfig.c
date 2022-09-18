#include "config.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-mem.h"
#include "filesel/filesystem-setup.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/framelock.h"
#include "stuff/poutput.h"
#include "timidityconfig.h"
#include "timidity-git/timidity/sysdep.h"
#include "timidity-git/timidity/timidity.h"

#ifdef PLAYTIMIDITY_DEBUG
# define PRINT(fmt, args...) fprintf(stderr, "%s %s: " fmt, __FILE__, __func__, ##args)
#else
# define PRINT(a, ...) do {} while(0)
#endif

static int have_user_timidity = 0;
static char user_timidity_path[BUFSIZ];
static int  have_default_timidity = 0;
static char default_timidity_path[BUFSIZ];
static int    global_timidity_count = 0;
static char **global_timidity_path = 0;
static int    sf2_files_count = 0;
static char **sf2_files_path = 0;

static void append_global (const char *path)
{
	char **tmp;
	tmp = realloc (global_timidity_path, (global_timidity_count + 1) * sizeof (char *));
	if (!tmp) return;
	global_timidity_path = tmp;
	global_timidity_path[global_timidity_count] = strdup (path);
	if (global_timidity_path[sf2_files_count]) global_timidity_count++;
}

static void append_sf2 (const char *path)
{
	char **tmp = realloc (sf2_files_path, (sf2_files_count + 1) * sizeof (char *));
	if (!tmp) return;
	sf2_files_path = tmp;
	sf2_files_path[sf2_files_count] = strdup (path);
	if (sf2_files_path[sf2_files_count]) sf2_files_count++;
}

static void reset_configfiles (void)
{
	int i;
	for (i=0; i < global_timidity_count; i++)
	{
		free (global_timidity_path[i]);
	}
	for (i=0; i < sf2_files_count; i++)
	{
		free (sf2_files_path[i]);
	}
	free (global_timidity_path);
	free (sf2_files_path);
	global_timidity_count = 0;
	global_timidity_path = 0;
	sf2_files_count = 0;
	sf2_files_path = 0;
	have_user_timidity = 0;
	user_timidity_path[0] = 0;
	have_default_timidity = 0;
	default_timidity_path[0] = 0;
}

static void try_user (const char *path)
{
	struct stat st;
	if (lstat (path, &st))
	{
		PRINT ("Unable to lstat() user config via $HOME %s\n", path);
		return;
	}
	if ((st.st_mode & S_IFMT) == S_IFLNK)
	{
		if (stat (path, &st))
		{
			PRINT ("Unable to stat() user config via $HOME %s, broken symlink\n", path);
			return;
		}
	}
	if ((st.st_mode & S_IFMT) != S_IFREG)
	{
		PRINT ("Unable to use user config via $HOME %s, not a regular file\n", path);
		return;
	}
	if (have_user_timidity)
	{
		PRINT ("Unable to use user config via $HOME %s, already have one\n", path);
		return;
	}
	PRINT ("Found user config via $HOME %s\n", path);
	have_user_timidity = 1;
	snprintf (user_timidity_path, sizeof (user_timidity_path), "%s", path);
}

static void try_global (const char *path)
{
	struct stat st;
	if (lstat (path, &st))
	{
		PRINT ("Unable to lstat() global config %s\n", path);
		return;
	}
	if ((st.st_mode & S_IFMT) == S_IFLNK)
	{
		if (stat (path, &st))
		{
			PRINT ("Unable to stat() user global config %s, broken symlink\n", path);
			return;
		}
	}
	if ((st.st_mode & S_IFMT) != S_IFREG)
	{
		PRINT ("Unable to use global config %s, not a regular file\n", path);
		return;
	}
	PRINT ("Found global config %s%s", path, have_user_timidity?", with possible user overrides":" (no user overrides found earlier)\n");
	if (!have_default_timidity)
	{
		snprintf (default_timidity_path, sizeof (default_timidity_path), "%s", path);
		have_default_timidity = 1;
	}
}

static void scan_config_directory (const char *dpath)
{
	DIR *d = opendir (dpath);
	struct dirent *de;
	if (!d)
	{
		PRINT ("Unable to scan directory %s for global configuration files\n", dpath);
		return;
	}
	PRINT ("(directory scan of %s)\n", dpath);
	while ((de = readdir (d)))
	{
		char path[BUFSIZ];
		struct stat st;

		if (!strcmp (de->d_name, ".")) continue;
		if (!strcmp (de->d_name, "..")) continue;

		snprintf (path, sizeof (path), "%s%s%s", dpath, PATH_STRING, de->d_name);
		if ((strlen (de->d_name) < 5) || (strcasecmp (de->d_name + strlen(de->d_name) - 4, ".cfg")))
		{
			PRINT ("Ignoring %s, since it does not end with .cfg\n", de->d_name);
			continue;
		}

		if (lstat (path, &st))
		{
			PRINT ("Unable to lstat() global config %s\n", path);
			return;
		}
		if ((st.st_mode & S_IFMT) == S_IFLNK)
		{
			if (stat (path, &st))
			{
				PRINT ("Unable to stat() global config %s, broken symlink\n", path);
				return;
			}
		}
		if ((st.st_mode & S_IFMT) != S_IFREG)
		{
			PRINT ("Unable to use global config %s, not a regular file\n", path);
			return;
		}
		PRINT ("Found global config %s\n", path);
		append_global (path);
	}
	closedir (d);
}

static void scan_sf2_directory (const char *dpath)
{
	DIR *d = opendir (dpath);
	struct dirent *de;
	if (!d)
	{
		PRINT ("Unable to scan directory %s for global sf2 fonts\n", dpath);
		return;
	}
	PRINT ("(directory scan of %s sf2 files)\n", dpath);
	while ((de = readdir (d)))
	{
		char path[BUFSIZ];
		struct stat st;

		if (!strcmp (de->d_name, ".")) continue;
		if (!strcmp (de->d_name, "..")) continue;

		snprintf (path, sizeof (path), "%s%s%s", dpath, PATH_STRING, de->d_name);
		if ((strlen (de->d_name) < 5) || (strcasecmp (de->d_name + strlen(de->d_name) - 4, ".sf2")))
		{
			PRINT ("Ignoring %s, since it does not end with .sf2\n", de->d_name);
			continue;
		}

		if (lstat (path, &st))
		{
			PRINT ("Unable to lstat() sf2 file %s\n", path);
			return;
		}
		if ((st.st_mode & S_IFMT) == S_IFLNK)
		{
			if (stat (path, &st))
			{
				PRINT ("Unable to stat() sf2 file %s, broken symlink\n", path);
				return;
			}
		}
		if ((st.st_mode & S_IFMT) != S_IFREG)
		{
			PRINT ("Unable to use sf2 file %s, not a regular file\n", path);
			return;
		}
		PRINT ("Found sf2 %s\n", path);
		append_sf2 (path);
	}
	closedir (d);
}

static int mystrcmp(const void *a, const void *b)
{
	return strcmp (*(char **)a, *(char **)b);
}

static void refresh_configfiles (void)
{
	char *home;
	char path[BUFSIZ];

	reset_configfiles ();

	home = getenv("HOME");
#ifdef __W32__
/* HOME or home */
	if(home == NULL)
		home = getenv("HOMEPATH");
	if(home == NULL)
		home = getenv("home");
#endif
	if (home)
	{
#ifdef __W32__
		snprintf (path, sizeof (path), "%s" PATH_STRING "timidity.cfg", home); try_user (path);
		snprintf (path, sizeof (path), "%s" PATH_STRING "_timidity.cfg", home); try_user (path);
#endif
		snprintf (path, sizeof (path), "%s" PATH_STRING ".timidity.cfg", home); try_user (path);
	}

	if (strcmp(CONFIG_FILE, "/etc/timidity/timidity.cfg") &&
	    strcmp(CONFIG_FILE, "/etc/timidity.cfg") &&
	    strcmp(CONFIG_FILE, "/usr/local/share/timidity/timidity.cfg") &&
	    strcmp(CONFIG_FILE, "/usr/share/timidity/timidity.cfg"))
		try_global (CONFIG_FILE);
	try_global ("/etc/timidity/timidity.cfg");
	try_global ("/etc/timidity.cfg");
	try_global ("/usr/local/share/timidity/timidity.cfg");
	try_global ("/usr/share/timidity/timidity.cfg");

	scan_config_directory ("/etc/timidity");
	scan_config_directory ("/usr/local/share/timidity");
	scan_config_directory ("/usr/share/timidity");
	scan_sf2_directory ("/usr/local/share/sounds/sf2");
	scan_sf2_directory ("/usr/share/sounds/sf2");

	if (global_timidity_count >= 2)
	{
		qsort (global_timidity_path, global_timidity_count, sizeof (global_timidity_path[0]), mystrcmp);
	}
	if (sf2_files_count >= 2)
	{
		qsort (sf2_files_path, sf2_files_count, sizeof (sf2_files_path[0]), mystrcmp);
	}
}

static int timidityConfigInit (struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct cpifaceplayerstruct *)
{
	return 1;
}

static void timidityConfigFileSelectDraw (int dsel)
{
	int mlWidth = 75;
	int mlHeight = (plScrHeight >= 35) ? 33 : 23;
	int mlTop = (plScrHeight - mlHeight) / 2 ;
	int mlLeft = (plScrWidth - mlWidth) / 2;
	int mlLine;
	int half;
	int skip;
	int dot;
	int contentsel;
	int contentheight = 6 + global_timidity_count + sf2_files_count + (!global_timidity_count) + (!sf2_files_count);
#define LINES_NOT_AVAILABLE 5

	half = (mlHeight - LINES_NOT_AVAILABLE) / 2;

	if (dsel == 0)
	{
		contentsel = 1;
	} else if (dsel <= (global_timidity_count))
	{
		contentsel = dsel + 4;
	} else {
		contentsel = dsel + 6 + global_timidity_count + (!global_timidity_count);
	}

	if (contentheight <= (mlHeight - LINES_NOT_AVAILABLE))
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (contentsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (contentsel >= (contentheight - half))
	{ /* we are at the bottom part */
		skip = contentheight - (mlHeight - LINES_NOT_AVAILABLE);
		dot = mlHeight - LINES_NOT_AVAILABLE - 1;
	} else {
		skip = contentsel - half;
		dot = skip * (mlHeight - LINES_NOT_AVAILABLE) / (contentheight - (mlHeight - LINES_NOT_AVAILABLE));
	}

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda%*C\xc4\xbf", mlWidth - 2);
	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Please select a new configuration file using the arrow keys and press%*C %0.9o\xb3", mlWidth - 72);
	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%0.15o <ENTER>%0.7o when done, or %0.15o<ESC>%0.7o to cancel.%*C %.9o\xb3", mlWidth - 41);
	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2);

	for (mlLine = 4; (mlLine + 1) < mlHeight; mlLine++)
	{
		int masterindex = mlLine - 4 + skip;

		displaychr  (mlTop, mlLeft,               0x09, '\xb3', 1);
		displaychr  (mlTop, mlLeft + mlWidth - 1, 0x09, ((mlLine - 4) == dot) ? '\xdd' : '\xb3', 1);

		if (masterindex == 0)
		{
			displaystr  (mlTop++, mlLeft + 1, 0x03, "System default", mlWidth - 2);
			continue;
		}
		if (masterindex == 1)
		{
			displaychr (mlTop, mlLeft + 1, (dsel==0)?0x8a:0x0a, ' ', 2);
			if (have_default_timidity)
			{
				int pos = strlen (default_timidity_path) + 3;
				displaystr (mlTop, mlLeft + 3, (dsel==0)?0x8a:0x0a, default_timidity_path, strlen (default_timidity_path));
				if (have_user_timidity)
				{
					int len = (mlWidth - pos - 1) < 26 ? (mlWidth - pos - 1) : 26;
					displaystr (mlTop, mlLeft + pos, (dsel==0)?0x87:0x07, " with user overrides from ", len);
					pos += len;
					if (pos < (mlWidth - 2))
					{
						displaystr_utf8 (mlTop, mlLeft + pos, (dsel==0)?0x8f:0x0f, user_timidity_path, mlWidth - pos - 1);
					}
				} else {
					displaystr (mlTop, mlLeft + pos, (dsel==0)?0x87:0x07, " (with no user overrides)", mlWidth - 1 - pos);
				}
			} else {
				displaystr (mlTop, mlLeft + 1, (dsel==0)?0x8c:0x0c, " No global configuration file found", mlWidth - 2);
			}
			mlTop++;
			continue;
		}
		if (masterindex == 3)
		{
			displaystr  (mlTop++, mlLeft + 1, 0x03, "Global configuration files:", mlWidth - 2);
			continue;
		}
		if ((masterindex == 4) && (!global_timidity_count))
		{
			displaystr  (mlTop++, mlLeft + 1, 0x0c, " No configuration files found", mlWidth - 2);
			continue;
		}
		if ((masterindex >= 4) && (masterindex < (global_timidity_count + 4)))
		{
			display_nprintf (mlTop++, mlLeft + 1, (dsel==(masterindex - 4 + 1))?0x8f:0x0f, mlWidth - 2, " %.*S", mlWidth - 3, global_timidity_path[masterindex - 4]);
			continue;
		}
		if (masterindex == (global_timidity_count + 5))
		{
			displaystr  (mlTop++, mlLeft + 1,           0x03, "Global SF2 files:", mlWidth - 2);
			continue;
		}
		if ((masterindex == (global_timidity_count + 6)) && (!sf2_files_count))
		{
			displaystr  (mlTop++, mlLeft + 1,           0x0c, " No soundfonts found", mlWidth - 2);
			continue;
		}
		if ((masterindex >= (global_timidity_count + 6)) && (masterindex < (global_timidity_count + sf2_files_count + 6)))
		{
			display_nprintf (mlTop++, mlLeft + 1, (dsel==(masterindex - 6 + 1))?0x8f:0x0f, mlWidth - 2, " %.*S", mlWidth - 3, sf2_files_path[masterindex - global_timidity_count - 6]);
			continue;
		}
		displayvoid (mlTop++, mlLeft + 1, mlWidth - 2);
	}

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

static void ConfigDrawItems (const int lineno, int xpos, const int width, const char **list, const int listlength, const int selected, const int active)
{
	int i;
	int origxpos = xpos;
	for (i=0; i < listlength; i++)
	{
		int l = strlen (list[i]);
		if (selected == i)
		{
			display_nprintf (lineno, xpos, (active)?0x09:0x01, l + 2, "[%.*o%s%.*o]", (active)?0x0f:0x07, list[i], (active)?0x09:0x01);
		} else {
			display_nprintf (lineno, xpos, 0x00, l + 2, " %.*o%s%.0o ", (active)?0x07:0x08, list[i]);
		}
		xpos += l + 2;
	}
	displaychr (lineno, xpos, 0x07, ' ', width - xpos + origxpos);
}

static void ConfigDrawBar (const int lineno, int xpos, int width, int level, int maxlevel, const int active)
{
	char temp[7];
	int tw = width - 8;
	int pw = tw * level / maxlevel;
	int p1, p2, p3, p4;

	p1 = tw * 1 / 4;
	p2 = tw * 2 / 4;
	p3 = tw * 3 / 4;
	p4 = tw;
	if (p1 > pw)
	{
		p1 = pw;
		p2 = 0;
		p3 = 0;
		p4 = 0;
	} else if (p2 > pw)
	{
		p2 = pw - p1;
		p3 = 0;
		p4 = 0;
	} else if (p3 > pw)
	{
		p3 = pw - p2;
		p2 -= p1;
		p4 = 0;
	} else {
		p4 = pw - p3;
		p3 -= p2;
		p2 -= p1;
	}
	displaystr (lineno, xpos,                         (active)?0x07:0x08, "[", 1);
	displaychr (lineno, xpos + 1,                     (active)?0x01:0x08, '\xfe', p1);
	displaychr (lineno, xpos + 1 + p1,                (active)?0x09:0x08, '\xfe', p2);
	displaychr (lineno, xpos + 1 + p1 + p2,           (active)?0x0b:0x08, '\xfe', p3);
	displaychr (lineno, xpos + 1 + p1 + p2 + p3,      (active)?0x0f:0x08, '\xfe', p4);
	displaychr (lineno, xpos + 1 + p1 + p2 + p3 + p4, (active)?0x07:0x08, '\xfa', tw - p1 - p2 - p3 - p4);

	snprintf (temp, sizeof (temp), "]%5d", level);
	displaystr (lineno, xpos + width - 8, (active)?0x07:0x08, temp, 8);
}

static int DefaultReverbMode;
static int DefaultReverbLevel;
static int DefaultScaleRoom;
static int DefaultOffsetRoom;
static int DefaultPredelayFactor;
static int DefaultDelayMode;
static int DefaultDelay;
static int DefaultChorus;
static void timidityConfigDraw (int EditPos)
{
	int large;
	int mlWidth, mlHeight, mlTop, mlLeft;
	const char *configfile = cfGetProfileString ("timidity", "configfile", "");
	const char *reverbs[] = {"disable", "original", "global-original", "freeverb", "global-freeverb"};
	const char *effect_lr_modes[] = {"disable", "left", "right", "both"};
	const char *disable_enable[] = {"disable", "enable"};

	if (plScrHeight >= 43)
	{
		large = 2;
		mlHeight = 41;
	} else if (plScrHeight >= 35)
	{
		large = 1;
		mlHeight = 33;
	} else {
		large = 0;
		mlHeight = 23;
	}

	mlWidth = 70;
	mlTop = (plScrHeight - mlHeight) / 2;
	mlLeft = (plScrWidth - mlWidth) / 2;

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xda%*C\xc4\xbf", mlWidth - 2);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o Navigate with arrows and hit %.15o<ESC>%.7o to save and exit.%*C %.9o\xb3", mlWidth - 55);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o 1. TiMdity+ Configfile/Soundfont: %.15o<ENTER>%.7o to change%*C %.9o\xb3", mlWidth - 53 - 1);

	if (!configfile[0])
	{
		display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.3o    (Global default)%*C %.9o\xb3", mlWidth - 21 - 1);
		display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.3o    %*.7oSelect another file%0.7o%*C %.9o\xb3", (EditPos==0)?8:0, mlWidth - 23 - 2);
	} else {
		if ((strlen(configfile) > 4) && !strcmp (configfile + strlen (configfile) - 4, ".sf2"))
		{
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.3o    (SF2 sound font)%*C %.9o\xb3", mlWidth - 21 - 1);
		} else {
			display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.3o    (Specific config file)%*C %0.9o\xb3", mlWidth - 27 - 1);
		}
		display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o    %*o%*S%0.9o\xb3", (EditPos==0)?8:0, mlWidth - 6, configfile);
	}

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o 2. Default Reverb Mode:%*C %.9o\xb3", mlWidth - 25 - 1);

	if (large >= 2) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop, mlLeft, 0x09, mlWidth, "\xb3%.7o   ");
	ConfigDrawItems (mlTop, mlLeft + 4, mlWidth - 4 - 1, reverbs, 5, DefaultReverbMode, EditPos==1);
	displaychr (mlTop++, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o 3. Default Reverb Level:%*C %.9o\xb3", mlWidth - 26 - 1);

	if (large >= 2) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop, mlLeft, 0x09, 5, "\xb3%.7o    ");
	ConfigDrawBar (mlTop, mlLeft + 5, mlWidth - 5 - 1, DefaultReverbLevel, 127, EditPos==2);
	displaychr (mlTop++, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o 4. Default Scale Room:%*C %.9o\xb3", mlWidth - 24 - 1);

	if (large >= 2) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop, mlLeft, 0x09, 5, "\xb3%.7o    ");
	ConfigDrawBar (mlTop, mlLeft + 5, mlWidth - 5 - 1, DefaultScaleRoom, 1000, EditPos==3);
	displaychr (mlTop++, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o 5. Default Offset Room:%*C %.9o\xb3", mlWidth - 25 - 1);

	if (large >= 2) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop, mlLeft, 0x09, 5, "\xb3%.7o    ");
	ConfigDrawBar (mlTop, mlLeft + 5, mlWidth - 5 - 1, DefaultOffsetRoom, 1000, EditPos==4);
	displaychr (mlTop++, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o 6. Default Predelay Factor:%*C %.9o\xb3", mlWidth - 29 - 1);

	if (large >= 2) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop, mlLeft, 0x09, 5, "\xb3%.7o    ");
	ConfigDrawBar (mlTop, mlLeft + 5, mlWidth - 5 - 1, DefaultPredelayFactor, 1000, EditPos==5);
	displaychr (mlTop++, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o 7. Default Delay Mode:%*C %.9o\xb3", mlWidth - 24 - 1);

	if (large >= 2) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop, mlLeft, 0x09, mlWidth, "\xb3%.7o   ");
	ConfigDrawItems (mlTop, mlLeft + 4, mlWidth - 4 - 1, effect_lr_modes, 4, DefaultDelayMode, EditPos==6);
	displaychr (mlTop++, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o 8. Default Delay (ms):%*C %.9o\xb3", mlWidth - 24 - 1);

	if (large >= 2) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop, mlLeft, 0x09, 5, "\xb3%.7o    ");
	ConfigDrawBar (mlTop, mlLeft + 5, mlWidth - 5 - 1, DefaultDelay, 1000, EditPos==7);
	displaychr (mlTop++, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%.7o 9. Default Chorus:%*C %.9o\xb3", mlWidth - 20 - 1);

	if (large >= 2) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop, mlLeft, 0x09, mlWidth, "\xb3%.7o   ");
	ConfigDrawItems (mlTop, mlLeft + 4, mlWidth - 4 - 1, disable_enable, 2, DefaultChorus, EditPos==8);
	displaychr (mlTop++, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	if (large) display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xb3%*C \xb3", mlWidth - 2);

	display_nprintf (mlTop++, mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);
}

static interfaceReturnEnum timidityConfigRun (void)
{
	int esel = 0;

	refresh_configfiles ();

	DefaultReverbMode     = cfGetProfileInt ("timidity", "reverbmode",       2, 10);
	DefaultReverbLevel    = cfGetProfileInt ("timidity", "reverblevel",     40, 10);
	DefaultScaleRoom      = cfGetProfileInt ("timidity", "scaleroom",       28, 10);
	DefaultOffsetRoom     = cfGetProfileInt ("timidity", "offsetroom",      70, 10);
	DefaultPredelayFactor = cfGetProfileInt ("timidity", "predelayfactor", 100, 10);
	DefaultDelayMode      = cfGetProfileInt ("timidity", "delaymode",       -1, 10) + 1;
	DefaultDelay          = cfGetProfileInt ("timidity", "delay",           25, 10);
	DefaultChorus         = cfGetProfileInt ("timidity", "chorusenabled",    1, 10);
	if (DefaultReverbMode     <    0) DefaultReverbMode     =    0;
	if (DefaultReverbLevel    <    0) DefaultReverbLevel    =    0;
	if (DefaultScaleRoom      <    0) DefaultScaleRoom      =    0;
	if (DefaultOffsetRoom     <    0) DefaultOffsetRoom     =    0;
	if (DefaultPredelayFactor <    0) DefaultPredelayFactor =    0;
	if (DefaultDelayMode      <    0) DefaultDelayMode      =    0;
	if (DefaultDelay          <    0) DefaultDelay          =    0;
	if (DefaultChorus         <    0) DefaultChorus         =    0;
	if (DefaultReverbMode     >    4) DefaultReverbMode     =    2;
	if (DefaultReverbLevel    >  127) DefaultReverbLevel    =  127;
	if (DefaultScaleRoom      > 1000) DefaultScaleRoom      = 1000;
	if (DefaultOffsetRoom     > 1000) DefaultOffsetRoom     = 1000;
	if (DefaultPredelayFactor > 1000) DefaultPredelayFactor = 1000;
	if (DefaultDelayMode      >    3) DefaultDelayMode      =    3;
	if (DefaultDelay          > 1000) DefaultDelay          = 1000;
	if (DefaultChorus         >    1) DefaultChorus         =    1;

	while (1)
	{
		fsDraw();
		timidityConfigDraw (esel);
		while (ekbhit())
		{
			int key = egetch();
			static uint32_t lastpress = 0;
			static int repeat;
			if ((key != KEY_LEFT) && (key != KEY_RIGHT))
			{
				lastpress = 0;
				repeat = 1;
			} else {
				uint32_t newpress = clock_ms();
				if ((newpress-lastpress) > 250) /* 250 ms */
				{
					repeat = 1;
				} else {
					if (repeat < 20)
					{
						repeat += 1;
					}
				}
				lastpress = newpress;
			}

			switch (key)
			{
				case _KEY_ENTER:
					if (esel == 0)
					{
						int dsel = 0;
						int inner = 1;
						{ /* preselect dsel */
							const char *configfile = cfGetProfileString ("timidity", "configfile", "");
							int i;
							if (configfile[0])
							{
								for (i=0; i < global_timidity_count; i++)
								{
									if (!strcmp (configfile, global_timidity_path[i]))
									{
										dsel = i + 1;
										break;
									}
								}
								if (!dsel)
								{
									for (i=0; i < sf2_files_count; i++)
									{
										if (!strcmp (configfile, sf2_files_path[i]))
										{
											dsel = i + 1 + global_timidity_count;
										}
									}
								}
							}
						}

						while (inner)
						{
							fsDraw();
							timidityConfigFileSelectDraw (dsel);
							while (inner && ekbhit())
							{
								int key = egetch();
								switch (key)
								{
									case KEY_DOWN:
										if ((dsel + 1) < (1 + global_timidity_count + sf2_files_count))
										{
											dsel++;
										}
										break;
									case KEY_UP:
										if (dsel)
										{
											dsel--;
										}
										break;
									case KEY_EXIT:
									case KEY_ESC:
										inner = 0;
										break;
									case _KEY_ENTER:
										if (!dsel)
										{
											cfSetProfileString ("timidity", "configfile", "");
										} else if (dsel < (global_timidity_count + 1))
										{
											cfSetProfileString ("timidity", "configfile", global_timidity_path[dsel - 1]);
										} else {
											cfSetProfileString ("timidity", "configfile", sf2_files_path[dsel - 1 - global_timidity_count]);
										}
										inner = 0;
										break;
								}
							}
							framelock ();
						}
					}
					break;
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					esel = key - '1';
					break;
				case KEY_LEFT:
					switch (esel)
					{
						case 1:
							if (DefaultReverbMode)
							{
								DefaultReverbMode--;
							}
							break;
						case 2:
							if (repeat > DefaultReverbLevel)
							{
								DefaultReverbLevel = 0;
							} else {
								DefaultReverbLevel -= repeat;
							}
							break;
						case 3:
							if (repeat > DefaultScaleRoom)
							{
								DefaultScaleRoom = 0;
							} else {
								DefaultScaleRoom -= repeat;
							}
							break;
						case 4:
							if (repeat > DefaultOffsetRoom)
							{
								DefaultOffsetRoom = 0;
							} else {
								DefaultOffsetRoom -= repeat;
							}
							break;
						case 5:
							if (repeat > DefaultPredelayFactor)
							{
								DefaultPredelayFactor = 0;
							} else {
								DefaultPredelayFactor -= repeat;
							}
							break;
						case 6:
							if (DefaultDelayMode)
							{
								DefaultDelayMode -= 1;
							}
							break;
						case 7: if ((repeat + 1) > DefaultDelay)
							{
								DefaultDelay = 1;
							} else {
								DefaultDelay -= repeat;
							}
							break;
						case 8: if (DefaultChorus)
							{
								DefaultChorus -= 1;
							}
							break;
					}
					break;
				case KEY_RIGHT:
					switch (esel)
					{
						case 1:
							if (DefaultReverbMode < 4)
							{
								DefaultReverbMode ++;
							}
							break;
						case 2:
							if ((DefaultReverbLevel + repeat) > 127)
							{
								DefaultReverbLevel = 127;
							} else {
								DefaultReverbLevel += repeat;
							}
							break;
						case 3:
							if ((DefaultScaleRoom + repeat) > 1000)
							{
								DefaultScaleRoom = 1000;
							} else {
								DefaultScaleRoom += repeat;
							}
							break;
						case 4:
							if ((DefaultOffsetRoom + repeat) > 1000)
							{
								DefaultOffsetRoom = 1000;
							} else {
								DefaultOffsetRoom += repeat;
							}
							break;
						case 5:
							if ((DefaultPredelayFactor + repeat) > 1000)
							{
								DefaultPredelayFactor = 1000;
							} else {
								DefaultPredelayFactor += repeat;
							}
							break;
						case 6:
							if (DefaultDelayMode < 3)
							{
								DefaultDelayMode++;
							}
							break;
						case 7:
							if ((DefaultDelay + repeat) > 1000)
							{
								DefaultDelay = 1000;
							} else {
								DefaultDelay += repeat;
							}
							break;
						case 8:
							if (!DefaultChorus)
							{
								DefaultChorus = 1;
							}
							break;
					}
					break;
				case KEY_DOWN:
					if (esel < 8)
					{
						esel++;
					}
					break;
				case KEY_UP:
					if (esel)
					{
						esel--;
					}
					break;
				case KEY_EXIT:
				case KEY_ESC:
					cfStoreConfig();
					goto superexit;
					break;
			}
		}
		framelock ();
	}

superexit:
	reset_configfiles ();

	cfSetProfileInt ("timidity", "reverbmode",     DefaultReverbMode,     10);
	cfSetProfileInt ("timidity", "reverblevel",    DefaultReverbLevel,    10);
	cfSetProfileInt ("timidity", "scaleroom",      DefaultScaleRoom,      10);
	cfSetProfileInt ("timidity", "offsetroom",     DefaultOffsetRoom,     10);
	cfSetProfileInt ("timidity", "predelayfactor", DefaultPredelayFactor, 10);
	cfSetProfileInt ("timidity", "delaymode",      DefaultDelayMode - 1,  10);
	cfSetProfileInt ("timidity", "delay",          DefaultDelay,          10);
	cfSetProfileInt ("timidity", "chorusenabled",  DefaultChorus,         10);
	cfStoreConfig ();

	return interfaceReturnNextAuto;
}

static struct ocpfile_t      *timidityconfig; // needs to overlay an dialog above filebrowser, and after that the file is "finished"   Special case of DEVv

static int                    timidityConfigInit (struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct cpifaceplayerstruct *);
static interfaceReturnEnum    timidityConfigRun  (void);
static struct interfacestruct timidityConfigIntr = {timidityConfigInit, timidityConfigRun, 0, "TiMidity+ Config" INTERFACESTRUCT_TAIL};

int __attribute__ ((visibility ("internal"))) timidity_config_init (void)
{
	struct moduleinfostruct m;
	uint32_t mdbref;

	timidityconfig= mem_file_open (dmSetup->basedir, dirdbFindAndRef (dmSetup->basedir->dirdb_ref, "timidityconfig.dev", dirdb_use_file), strdup (timidityConfigIntr.name), strlen (timidityConfigIntr.name));
	dirdbUnref (timidityconfig->dirdb_ref, dirdb_use_file);
	mdbref = mdbGetModuleReference2 (timidityconfig->dirdb_ref, strlen (timidityConfigIntr.name));
	mdbGetModuleInfo (&m, mdbref);
	m.modtype.integer.i = MODULETYPE("DEVv");
	strcpy (m.title, "TiMidity+ Configuration");
	mdbWriteModuleInfo (mdbref, &m);
	filesystem_setup_register_file (timidityconfig);
	plRegisterInterface (&timidityConfigIntr);

	return errOk;
}

void __attribute__ ((visibility ("internal"))) timidity_config_done (void)
{
	plUnregisterInterface (&timidityConfigIntr);
	if (timidityconfig)
	{
		filesystem_setup_unregister_file (timidityconfig);
		timidityconfig = 0;
	}
}
