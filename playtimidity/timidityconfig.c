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
#include "stuff/err.h"
#include "stuff/framelock.h"
#include "stuff/poutput.h"
#include "timidity-git/timidity/sysdep.h"
#include "timidity-git/timidity/timidity.h"

#ifdef TIMIDITY_DEBUG
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

static int timidityConfigInit (struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct interfaceparameters *ip)
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

	displaychr  (mlTop + 0, mlLeft,               0x09, '\xda', 1);
	displaychr  (mlTop + 0, mlLeft + 1,           0x09, '\xc4', mlWidth - 2);
	displaychr  (mlTop + 0, mlLeft + mlWidth - 1, 0x09, '\xbf', 1);

	displaychr  (mlTop + 1, mlLeft,               0x09, '\xb3', 1);
	displaystr  (mlTop + 1, mlLeft + 1,           0x07, " Please select a new configuration file using the arrow keys and press", mlWidth - 2);
	displaychr  (mlTop + 1, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	displaychr  (mlTop + 2, mlLeft,               0x09, '\xb3', 1);
	displaystr  (mlTop + 2, mlLeft + 1,           0x0f, " <ENTER>", 8);
	displaystr  (mlTop + 2, mlLeft + 9,           0x07, "  when done, or ", 16);
	displaystr  (mlTop + 2, mlLeft + 25,          0x0f, "<ESC>", 5);
	displaystr  (mlTop + 2, mlLeft + 30,          0x07, " to cancel.", mlWidth - 30 - 1);
	displaychr  (mlTop + 2, mlLeft + mlWidth - 1, 0x09, '\xb3', 1);

	displaychr  (mlTop + 3, mlLeft,               0x09, '\xc3', 1);
	displaychr  (mlTop + 3, mlLeft + 1,           0x09, '\xc4', mlWidth - 2);
	displaychr  (mlTop + 3, mlLeft + mlWidth - 1, 0x09, '\xb4', 1);

	for (mlLine = 4; (mlLine + 1) < mlHeight; mlLine++)
	{
		int masterindex = mlLine - 4 + skip;

		displaychr  (mlTop + mlLine, mlLeft,               0x09, '\xb3', 1);
		displaychr  (mlTop + mlLine, mlLeft + mlWidth - 1, 0x09, ((mlLine - 4) == dot) ? '\xdd' : '\xb3', 1);

		if (masterindex == 0)
		{
			displaystr  (mlTop + mlLine, mlLeft + 1,           0x03, "System default", mlWidth - 2);
			continue;
		}
		if (masterindex == 1)
		{
			displaychr (mlTop + mlLine, mlLeft + 1,            (dsel==0)?0x8a:0x0a, ' ', 2);
			if (have_default_timidity)
			{
				int pos = strlen (default_timidity_path) + 3;
				displaystr (mlTop + mlLine, mlLeft + 3,    (dsel==0)?0x8a:0x0a, default_timidity_path, strlen (default_timidity_path));
				if (have_user_timidity)
				{
					int len = (mlWidth - pos - 1) < 26 ? (mlWidth - pos - 1) : 26;
					displaystr (mlTop + mlLine, mlLeft + pos, (dsel==0)?0x87:0x07, " with user overrides from ", len);
					pos += len;
					if (pos < (mlWidth - 2))
					{
						displaystr_utf8 (mlTop + mlLine, mlLeft + pos, (dsel==0)?0x8f:0x0f, user_timidity_path, mlWidth - pos - 1);
					}
				} else {
					displaystr (mlTop + mlLine, mlLeft + pos, (dsel==0)?0x87:0x07, " (with no user overrides)", mlWidth - 1 - pos);
				}
			} else {
				displaystr (mlTop + mlLine, mlLeft + 1, (dsel==0)?0x8c:0x0c, " No global configuration file found", mlWidth - 2);
			}
			continue;
		}
		if (masterindex == 3)
		{
			displaystr  (mlTop + mlLine, mlLeft + 1,           0x03, "Global configuration files:", mlWidth - 2);
			continue;
		}
		if ((masterindex == 4) && (!global_timidity_count))
		{
			displaystr  (mlTop + mlLine, mlLeft + 1,           0x0c, " No configuration files found", mlWidth - 2);
			continue;
		}
		if ((masterindex >= 4) && (masterindex < (global_timidity_count + 4)))
		{
			displaychr      (mlTop + mlLine, mlLeft + 1,       (dsel==(masterindex - 4 + 1))?0x8f:0x0f, ' ', 1);
			displaystr_utf8 (mlTop + mlLine, mlLeft + 2,       (dsel==(masterindex - 4 + 1))?0x8f:0x0f, global_timidity_path[masterindex - 4], mlWidth - 3);
			continue;
		}
		if (masterindex == (global_timidity_count + 5))
		{
			displaystr  (mlTop + mlLine, mlLeft + 1,           0x03, "Global SF2 files:", mlWidth - 2);
			continue;
		}
		if ((masterindex == (global_timidity_count + 6)) && (!sf2_files_count))
		{
			displaystr  (mlTop + mlLine, mlLeft + 1,           0x0c, " No soundfonts found", mlWidth - 2);
			continue;
		}
		if ((masterindex >= (global_timidity_count + 6)) && (masterindex < (global_timidity_count + sf2_files_count + 6)))
		{
			displaychr      (mlTop + mlLine, mlLeft + 1,       (dsel==(masterindex - 6 + 1))?0x8f:0x0f, ' ', 1);
			displaystr_utf8 (mlTop + mlLine, mlLeft + 2,       (dsel==(masterindex - 6 + 1))?0x8f:0x0f, sf2_files_path[masterindex - global_timidity_count - 6], mlWidth - 3);
			continue;
		}
		displayvoid (mlTop + mlLine, mlLeft + 1,                         mlWidth - 2);
	}

	displaychr (mlTop + mlHeight - 1, mlLeft,               0x09, '\xc0', 1);
	displaychr (mlTop + mlHeight - 1, mlLeft + 1,           0x09, '\xc4', mlWidth - 2);
	displaychr (mlTop + mlHeight - 1, mlLeft + mlWidth - 1, 0x09, '\xd9', 1);
}

static interfaceReturnEnum timidityConfigRun (void)
{
	int dsel = 0;

	refresh_configfiles ();

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

	while (1)
	{
		fsDraw();
		timidityConfigFileSelectDraw (dsel);
		while (ekbhit())
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
				case KEY_ESC:
					goto superexit;
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
					cfStoreConfig();
					goto superexit;
					break;
			}
		}
		framelock ();
	}
superexit:
	reset_configfiles ();

	return interfaceReturnNextAuto;
}

static struct ocpfile_t      *timidityconfig; // needs to overlay an dialog above filebrowser, and after that the file is "finished"   Special case of DEVv

static int                    timidityConfigInit (struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct interfaceparameters *ip);
static interfaceReturnEnum    timidityConfigRun  (void);
static struct interfacestruct timidityConfigIntr = {timidityConfigInit, timidityConfigRun, 0, "TiMidity+ Config" INTERFACESTRUCT_TAIL};

static int timidity_config_init (void)
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

static void timidity_config_done (void)
{
	plUnregisterInterface (&timidityConfigIntr);
	if (timidityconfig)
	{
		filesystem_setup_unregister_file (timidityconfig);
		timidityconfig = 0;
	}
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "timidityconfig", .desc = "OpenCP UNIX TiMidity+ configuration (c) 2022 Stian Skjelstad", .ver = DLLVERSION, .size = 0, .Init = timidity_config_init, .Close = timidity_config_done};
