/* OpenCP Module Player
 * copyright (c) 2022-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Timidity config (setup:) editor
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

#include "config.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef _WIN32
# include <handleapi.h>
# include <fileapi.h>
# include <sysinfoapi.h>
#else
# include <sys/stat.h>
#endif
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/filesystem-setup.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "timidityconfig.h"
#include "timidity-git/timidity/timidity.h"
#include "timidity-git/timidity/sysdep.h"

#ifdef PLAYTIMIDITY_DEBUG
# define PRINT(fmt, args...) fprintf(stderr, "%s %s: " fmt, __FILE__, __func__, ##args)
#else
# define PRINT(a, ...) do {} while(0)
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

static char *user_timidity_path;
static int   have_user_timidity;
static int   have_default_timidity;
static char default_timidity_path[BUFSIZ];
static int    global_timidity_count;
static int    global_timidity_count_or_none;
static char **global_timidity_path;
static int    sf2_files_count;
static int    sf2_files_count_or_none;
static char **sf2_files_path;

static char            *sf2_manual;
static uint32_t         sf2_manual_dirdb_ref = DIRDB_CLEAR;
static struct ocpdir_t *sf2_manual_dir;

struct path_attempt
{
        char *path;
        int displaywidth;
        int found;
};
static struct path_attempt *config_attempt = 0;
static int                  config_attempt_n = 0;
static struct path_attempt *global_configs_attempt = 0;
static int                  global_configs_attempt_n = 0;
static struct path_attempt *global_sf2_attempt = 0;
static int                  global_sf2_attempt_n = 0;

struct BrowseSF2_t
{
	int isparent;
#ifdef _WIN32
	char drive;
#endif
	struct ocpfile_t *file;
	struct ocpdir_t *dir;
};

static struct BrowseSF2_t *BrowseSF2_entries_data;
static int                 BrowseSF2_entries_count;
static int                 BrowseSF2_entries_size;

static void path_attempt_append (const struct DevInterfaceAPI_t *API, const char *path, int found, struct path_attempt **e, int *n)
{
	struct path_attempt *e2 = realloc (*e, sizeof (struct path_attempt) * ((*n) + 1));
	if (!e2)
	{
		fprintf (stderr, "path_attempt_append: realloc failed\n");
		return;
	}
	(*e) = e2;
	(*e)[*n].path = strdup (path);
	if (!(*e)[*n].path)
	{
		fprintf (stderr, "path_attempt_append: strdup() failed\n");
		return;
	}
	(*e)[*n].displaywidth = API->console->Driver->MeasureStr_utf8 (path, strlen (path));
	(*e)[*n].found = found;
	(*n) = (*n) + 1;
}

static void path_attempt_clear (struct path_attempt **e, int *n)
{
	int i;
	for (i = 0; i < *n; i++)
	{
		free ( (*e)[i].path );
	}
	free (*e);
	*e = 0;
	*n = 0;
}

static int path_attempt_get_height (const char *string_prefix, const struct path_attempt *s, int n, const int w)
{
	int RetVal = 1;
	int CurrentOffset = strlen (string_prefix) + 1;
	int CurrentWidth = w - CurrentOffset;

	while (n)
	{
		/* If next item can not fit, and we already have some text, add a new line.
		 * Also include space for a comma if we alreadu have some text.
		 */
		if ((CurrentWidth <= 0) || (((s->displaywidth + ((CurrentOffset && (n>2))?1:0)) >= CurrentWidth) && (CurrentOffset != 0)))
		{
			CurrentOffset = 0;
			CurrentWidth = w;
			RetVal++;
		}

		CurrentOffset += s->displaywidth;
		CurrentWidth -= s->displaywidth;
		n--;s++;

		if (n > 1) // add comma and a space
		{
			if (CurrentOffset)
			{
				CurrentWidth-=2;
				CurrentOffset+=2;
				/* new-line will be handled by start of the while loop */
			}
		} else if (n == 1) // add and, with possible spaces
		{
			if (CurrentWidth < 4) /* no room, add a new-line already now */
			{
				CurrentOffset = 0;
				CurrentWidth = w;
				RetVal++;

				CurrentOffset += 4; // "and "
				CurrentWidth -= 4;
			} else {
				CurrentOffset += 5; //" and "
				CurrentWidth -= 5;
			}
		}
	}
	return RetVal;
}

static void path_attempt_print (const struct DevInterfaceAPI_t *API, const char *string_prefix, const struct path_attempt *s, int n, const int Left, const int Width, int Top, int Height)
{
	int CurrentOffset = strlen (string_prefix) + 1;
	int CurrentWidth = Width - CurrentOffset;

	API->console->DisplayPrintf (Top, Left, 0x07, Width, "%s ", string_prefix);

	while (n && Height)
	{
		/* If next item can not fit, and we already have some text, add a new line.
		 * Also include space for a comma if we alreadu have some text.
		 */
		if ((CurrentWidth <= 0) || (((s->displaywidth + ((CurrentOffset && (n>2))?1:0)) >= CurrentWidth) && (CurrentOffset != 0)))
		{
			CurrentOffset = 0;
			CurrentWidth = Width;
			Top++;
			if (!(Height--))
			{
				return; // should not be reachable
			}
		}

		API->console->DisplayPrintf (Top, Left + CurrentOffset, s->found ? 0x0a : 0x0c, CurrentWidth, "%S", s->path);

		CurrentOffset += s->displaywidth;
		CurrentWidth -= s->displaywidth;
		n--;s++;

		if (n > 1) // add comma and a space
		{
			if (CurrentOffset)
			{
				API->console->DisplayPrintf (Top, Left + CurrentOffset, 0x07, CurrentWidth, ", ");

				CurrentWidth-=2;
				CurrentOffset+=2;
				/* new-line will be handled by start of the while loop */
			}
		} else if (n == 1) // add and, with possible spaces
		{
			if (CurrentWidth < 4) /* no room, add a new-line already now */
			{
				CurrentOffset = 0;
				CurrentWidth = Width;
				Top++;
				if (!(Height--))
				{
					return; // should not be reachable
				}

				API->console->DisplayPrintf (Top, Left + CurrentOffset, 0x07, CurrentWidth, "and ");
				CurrentOffset += 4; // "and "
				CurrentWidth -= 4;
			} else {
				API->console->DisplayPrintf (Top, Left + CurrentOffset, 0x07, CurrentWidth, " and ");
				CurrentOffset += 5; //" and "
				CurrentWidth -= 5;
			}
		}
	}

}

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

static void reset_configfiles (const struct DevInterfaceAPI_t *API)
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
	free (user_timidity_path);
	global_timidity_count = 0;
	global_timidity_path = 0;
	sf2_files_count = 0;
	sf2_files_path = 0;
	have_user_timidity = 0;
	user_timidity_path = 0;
	have_default_timidity = 0;
	default_timidity_path[0] = 0;

	global_timidity_count_or_none = 1;
	sf2_files_count_or_none = 1;

	path_attempt_clear (&config_attempt,         &config_attempt_n);
	path_attempt_clear (&global_configs_attempt, &global_configs_attempt_n);
	path_attempt_clear (&global_sf2_attempt,     &global_sf2_attempt_n);

	free (sf2_manual);
	sf2_manual = 0;
	if (sf2_manual_dir)
	{
		sf2_manual_dir->unref (sf2_manual_dir);
		sf2_manual_dir = 0;
	}
	if (sf2_manual_dirdb_ref != DIRDB_CLEAR)
	{
		API->dirdb->Unref (sf2_manual_dirdb_ref, dirdb_use_file);
		sf2_manual_dirdb_ref = DIRDB_CLEAR;
	}
}

static void try_user (const struct DevInterfaceAPI_t *API, const char *filename)
{
	struct ocpfile_t *f;

	f = ocpdir_readdir_file (API->configAPI->HomeDir, filename, API->dirdb);
	{
		int len = strlen (API->configAPI->HomePath) + strlen (filename) + 1;
		char *temp = malloc (len);
		if (temp)
		{
			snprintf (temp, len, "%s%s", API->configAPI->HomePath, filename);
			path_attempt_append (API, temp, !!f, &config_attempt, &config_attempt_n);
			free (temp);
		}
	}
	if (!f)
	{
		PRINT ("Unable to use user config via cfHOME %s\n", filename);
		return;
	}
	if (have_user_timidity)
	{
		PRINT ("Already have a user config\n");
	} else {
		PRINT ("Found user config via $HOME %s\n", filename);
		API->dirdb->GetFullname_malloc (f->dirdb_ref, &user_timidity_path, 0);
		have_user_timidity = 1;
	}
	f->unref (f);
}

static void try_global (const struct DevInterfaceAPI_t *API, const char *path)
{
#ifdef _WIN32
	DWORD st;

	st = GetFileAttributes (path);

	if (st == INVALID_FILE_ATTRIBUTES)
	{
		path_attempt_append (API, path, 0, &config_attempt, &config_attempt_n);
		return;
	}
	if (st & FILE_ATTRIBUTE_DIRECTORY)
	{
		path_attempt_append (API, path, 0, &config_attempt, &config_attempt_n);
		return;
	}
#else
	struct stat st;
	if (lstat (path, &st))
	{
		PRINT ("Unable to lstat() global config %s\n", path);
		path_attempt_append (API, path, 0, &config_attempt, &config_attempt_n);
		return;
	}
	if ((st.st_mode & S_IFMT) == S_IFLNK)
	{
		if (stat (path, &st))
		{
			PRINT ("Unable to stat() user global config %s, broken symlink\n", path);
			path_attempt_append (API, path, 0, &config_attempt, &config_attempt_n);
			return;
		}
	}
	if ((st.st_mode & S_IFMT) != S_IFREG)
	{
		PRINT ("Unable to use global config %s, not a regular file\n", path);
		path_attempt_append (API, path, 0, &config_attempt, &config_attempt_n);
		return;
	}
#endif
	PRINT ("Found global config %s%s", path, have_user_timidity?", with possible user overrides":" (no user overrides found earlier)\n");
	path_attempt_append (API, path, 1, &config_attempt, &config_attempt_n);
	if (!have_default_timidity)
	{
		snprintf (default_timidity_path, sizeof (default_timidity_path), "%s", path);
		have_default_timidity = 1;
	}
}

static void scan_config_directory (const struct DevInterfaceAPI_t *API, const char *dpath)
{
#ifdef _WIN32
	HANDLE FindHandle;
	WIN32_FIND_DATAA FindData;
	char path[BUFSIZ];

	char *dpath_star = malloc (strlen (dpath) + 2);

	if (!dpath_star)
	{
		return;
	}
	sprintf (dpath_star, "%s*", dpath);
	FindHandle = FindFirstFile (dpath_star, &FindData);
	free (dpath_star);
	dpath_star = 0;
	path_attempt_append (API, dpath, FindHandle != INVALID_HANDLE_VALUE, &global_configs_attempt, &global_configs_attempt_n);
	if (FindHandle == INVALID_HANDLE_VALUE)
	{
		PRINT ("Unable to scan directory %s for global configuration files\n", dpath);
		return;
	}

	do
	{
		snprintf (path, sizeof (path), "%s%s", dpath, FindData.cFileName);

		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			PRINT ("Ignoring %s, since it is a directory\n", path);
			continue;
		}
		if (strlen (FindData.cFileName) < 4)
		{
			PRINT ("Ignoring %s, since it does not end with .cfg (too short)\n", path);
			continue;
		}
		if (strcasecmp (FindData.cFileName + strlen (FindData.cFileName) - 4, ".cfg"))
		{
			PRINT ("Ignoring %s, since it does not end with .cfg\n", path);
			continue;
		}
		PRINT ("Found global config %s\n", path);
		append_global (path);
	} while (FindNextFile (FindHandle, &FindData));
	FindClose (FindHandle);
#else
	DIR *d = opendir (dpath);
	struct dirent *de;

	char *dpath_slash = malloc (strlen (dpath) + 2);

	if (dpath_slash)
	{
		sprintf (dpath_slash, "%s/", dpath);
		path_attempt_append (API, dpath_slash, !!d, &global_configs_attempt, &global_configs_attempt_n);
		free (dpath_slash);
	}

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
#endif
}

static void scan_sf2_directory (const struct DevInterfaceAPI_t *API, const char *dpath)
{
#ifdef _WIN32
	HANDLE FindHandle;
	WIN32_FIND_DATAA FindData;
	char path[BUFSIZ];

	char *dpath_star = malloc (strlen (dpath) + 2);

	if (!dpath_star)
	{
		return;
	}
	sprintf (dpath_star, "%s*", dpath);
	FindHandle = FindFirstFile (dpath_star, &FindData);
	free (dpath_star);
	dpath_star = 0;
	path_attempt_append (API, dpath, FindHandle != INVALID_HANDLE_VALUE, &global_sf2_attempt, &global_sf2_attempt_n);

	if (FindHandle == INVALID_HANDLE_VALUE)
	{
		PRINT ("Unable to scan directory %s for global sf2 fonts\n", dpath);
		return;
	}

	do
	{
		snprintf (path, sizeof (path), "%s%s", dpath, FindData.cFileName);

		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			PRINT ("Ignoring %s, since it is a directory\n", path);
			continue;
		}
		if (strlen (FindData.cFileName) < 4)
		{
			PRINT ("Ignoring %s, since it does not end with .sf2 (too short)\n", path);
			continue;
		}
		if (strcasecmp (FindData.cFileName + strlen (FindData.cFileName) - 4, ".sf2"))
		{
			PRINT ("Ignoring %s, since it does not end with .sf2\n", path);
			continue;
		}
		PRINT ("Found sf2 %s\n", path);
		append_sf2 (path);
	} while (FindNextFile (FindHandle, &FindData));
	FindClose (FindHandle);
#else
	DIR *d = opendir (dpath);
	struct dirent *de;

	char *dpath_slash = malloc (strlen (dpath) + 2);

	if (dpath_slash)
	{
		sprintf (dpath_slash, "%s/", dpath);
		path_attempt_append (API, dpath_slash, !!d, &global_sf2_attempt, &global_sf2_attempt_n);
		free (dpath_slash);
	}

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

		snprintf (path, sizeof (path), "%s/%s", dpath, de->d_name);
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
#endif
}

static int mystrcmp(const void *a, const void *b)
{
	return strcmp (*(char **)a, *(char **)b);
}

static void refresh_sf2manual (const struct DevInterfaceAPI_t *API)
{
	const char *temp = API->configAPI->GetProfileString ("timidity", "sf2manual", 0); // We will strdup, if this returns a value
	if (temp)
	{
		sf2_manual = strdup (temp);
	}
	if (sf2_manual)
	{

#ifdef _WIN32
		sf2_manual_dirdb_ref = API->dirdb->ResolvePathWithBaseAndRef (DIRDB_NOPARENT, sf2_manual, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_WINDOWS_SLASH, dirdb_use_file);
#else
		sf2_manual_dirdb_ref = API->dirdb->ResolvePathWithBaseAndRef (DIRDB_NOPARENT, sf2_manual, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
#endif

		if (sf2_manual_dirdb_ref != DIRDB_CLEAR)
		{
			struct ocpfile_t *sf2_manual_file = 0;

			API->filesystem_resolve_dirdb_file (sf2_manual_dirdb_ref, 0, &sf2_manual_file);

			if (sf2_manual_file)
			{
				sf2_manual_dir = sf2_manual_file->parent;
				sf2_manual_dir->ref (sf2_manual_dir);
				sf2_manual_file->unref (sf2_manual_file);
			}
		}
	}
}

static void refresh_configfiles (const struct DevInterfaceAPI_t *API)
{
	reset_configfiles (API);

	try_user (API, "timidity.cfg"); /* Vanilla TiMidity only scans this on WIN32 */
	try_user (API, "_timidity.cfg"); /* Vanilla TiMidity only scan this on WIN32 */
	try_user (API, ".timidity.cfg");

#ifdef _WIN32
	{
		DWORD length;
		uint16_t *wpath;
		char *path, *filepath;
		if (!(length = GetWindowsDirectoryW (NULL, 0)))
		{
			goto other_global;
		}
		if (!(wpath = calloc (length, sizeof (uint16_t))))
		{
			goto other_global;
		}
		if (GetWindowsDirectoryW (wpath, length) != (length-1))
		{
			free (wpath);
			goto other_global;
		}
		path = API->utf16_to_utf8 (wpath);
		free (wpath); wpath = 0;
		if (!path)
		{
			goto other_global;
		}
		length = strlen (path) + 13 + 1;
		filepath = malloc (length);
		if (!filepath)
		{
			free (path);
			goto other_global;
		}
		snprintf (filepath, length, "%s\\TIMIDITY.CFG", path);
		free (path);
		try_global (API, filepath); // "C:\\WINDOWS\\timidity.cfg"
		free (filepath);
	}
other_global:
	try_global (API, "C:\\timidity\\timidity.cfg");
	try_global (API, "C:\\TiMidity++\\timidity.cfg");

	scan_config_directory (API, "C:\\timidity\\");
	scan_config_directory (API, "C:\\TiMidity++\\");
	scan_config_directory (API, "C:\\timidity\\soundfonts\\");
	scan_config_directory (API, "C:\\TiMidity++\\soundfonts\\");
	scan_config_directory (API, "C:\\timidity\\musix\\");
	scan_config_directory (API, "C:\\TiMidity++\\musix\\");

	scan_sf2_directory (API, "C:\\timidity\\soundfonts\\");
	scan_sf2_directory (API, "C:\\TiMidity++\\soundfonts\\");
	scan_sf2_directory (API, "C:\\timidity\\musix\\");
	scan_sf2_directory (API, "C:\\TiMidity++\\musix\\");
	scan_sf2_directory (API, API->configAPI->DataPath);
#else
	if (strcmp(CONFIG_FILE, "/etc/timidity/timidity.cfg") &&
	    strcmp(CONFIG_FILE, "/etc/timidity.cfg") &&
	    strcmp(CONFIG_FILE, "/usr/local/share/timidity/timidity.cfg") &&
	    strcmp(CONFIG_FILE, "/usr/share/timidity/timidity.cfg"))
	{
		try_global (API, CONFIG_FILE);
	}
	try_global (API, "/etc/timidity/timidity.cfg");
	try_global (API, "/etc/timidity.cfg");
	try_global (API, "/usr/local/share/timidity/timidity.cfg");
	try_global (API, "/usr/share/timidity/timidity.cfg");

	scan_config_directory (API, "/etc/timidity");
	scan_config_directory (API, "/usr/local/share/timidity");
	scan_config_directory (API, "/usr/share/timidity");

	scan_sf2_directory (API, "/usr/local/share/sounds/sf2");
	scan_sf2_directory (API, "/usr/share/sounds/sf2");
#endif

	if (global_timidity_count >= 2)
	{
		qsort (global_timidity_path, global_timidity_count, sizeof (global_timidity_path[0]), mystrcmp);
	}
	if (sf2_files_count >= 2)
	{
		qsort (sf2_files_path, sf2_files_count, sizeof (sf2_files_path[0]), mystrcmp);
	}

	global_timidity_count_or_none = global_timidity_count ?  global_timidity_count : 1;
	sf2_files_count_or_none = sf2_files_count ? sf2_files_count : 1;

	refresh_sf2manual (API);
}

static void timidityBrowseSF2Draw (int contentsel, const char *path, const struct DevInterfaceAPI_t *API)
{
	int mlWidth = MAX(75, API->console->TextWidth * 3 / 4);
	int mlHeight = (API->console->TextHeight >= 35) ? 33 : 23;
	int mlTop = (API->console->TextHeight - mlHeight) / 2 ;
	int mlLeft = (API->console->TextWidth - mlWidth) / 2;
	int skip;
	int dot;
	int maxcontentheight = BrowseSF2_entries_count;
	const int LINES_NOT_AVAILABLE = 4;
	int contentheight = mlHeight - LINES_NOT_AVAILABLE;
	int half = (contentheight) / 2;
	int masterindex;

	if (maxcontentheight <= contentheight)
	{ /* all entries can fit */
		skip = 0;
		dot = 3;
	} else if (contentsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 4;
	} else if (contentsel >= (maxcontentheight - half))
	{ /* we are at the bottom part */
		skip = maxcontentheight - contentheight;
		dot = mlHeight - 2;
	} else {
		skip = contentsel - half;
		dot = skip * contentheight / (maxcontentheight - contentheight) + 4;
	}

	API->console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "Browse SF2 file for TiMidity++", dot, 2, 0);
	mlWidth -= 2;
	mlHeight -= 2;
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "%S", path);
	//API->console->DisplayPrintf (mlTop++, mlLeft, 0x0f, mlWidth, " <ENTER>%0.7o when done, or %0.15o<ESC>%0.7o to cancel.");

	mlTop++; // 2: horizontal bar
	mlHeight-=2;

	for (masterindex = skip; mlHeight; mlTop++, masterindex++, mlHeight--)
	{
		if (masterindex < BrowseSF2_entries_count)
		{
			if (BrowseSF2_entries_data[masterindex].isparent)
			{
				API->console->DisplayPrintf (mlTop, mlLeft, (contentsel==masterindex)?0x61:0x01, mlWidth, "..");
#ifdef _WIN32
			} else if (BrowseSF2_entries_data[masterindex].drive)
			{
				API->console->DisplayPrintf (mlTop, mlLeft, (contentsel==masterindex)?0x61:0x01, mlWidth, "%c:", BrowseSF2_entries_data[masterindex].drive);
#endif
			} else if (BrowseSF2_entries_data[masterindex].dir)
			{
				const char *p = 0;
				API->dirdb->GetName_internalstr (BrowseSF2_entries_data[masterindex].dir->dirdb_ref, &p);
				API->console->DisplayPrintf (mlTop, mlLeft, (contentsel==masterindex)?0x61:0x01, mlWidth, "%S", p);
			} else {
				const char *p = 0;
				API->dirdb->GetName_internalstr (BrowseSF2_entries_data[masterindex].file->dirdb_ref, &p);
				API->console->DisplayPrintf (mlTop, mlLeft, (contentsel==masterindex)?0x60:0x07, mlWidth, "%S", p);
			}
		}
	}
}


static void timidityConfigFileSelectDraw (int dsel, const struct DevInterfaceAPI_t *API)
{
	int mlWidth = MAX(75, API->console->TextWidth * 3 / 4);
	int mlHeight = (API->console->TextHeight >= 35) ? 33 : 23;
	int mlTop = (API->console->TextHeight - mlHeight) / 2 ;
	int mlLeft = (API->console->TextWidth - mlWidth) / 2;
	int mlLine;
	int half;
	int skip;
	int dot;
	int contentsel;
	int maxcontentheight = 6 + global_timidity_count_or_none + sf2_files_count_or_none + 3;
	const int LINES_NOT_AVAILABLE = 6;
	int config_attempt_height         = path_attempt_get_height ("Checked for", config_attempt,         config_attempt_n,         mlWidth - 2);
	int global_configs_attempt_height = path_attempt_get_height ("Scanned",     global_configs_attempt, global_configs_attempt_n, mlWidth - 2);
	int global_sf2_attempt_height     = path_attempt_get_height ("Scanned",     global_sf2_attempt,     global_sf2_attempt_n,     mlWidth - 2);

	int attempts_height_max = MAX (config_attempt_height, MAX (global_configs_attempt_height, global_sf2_attempt_height ) );

	int contentheight = mlHeight - LINES_NOT_AVAILABLE - attempts_height_max;

	half = (mlHeight - LINES_NOT_AVAILABLE - attempts_height_max) / 2;

	if (dsel == 0)
	{
		contentsel = 1;
	} else if (dsel < (1 + global_timidity_count_or_none))
	{
		contentsel = dsel + 3;
	} else if (dsel <= (1 + global_timidity_count_or_none + sf2_files_count_or_none))
	{
		contentsel = dsel + 5;
  } else {
		contentsel = dsel + 7;
	}

	if (maxcontentheight <= contentheight)
	{ /* all entries can fit */
		skip = 0;
		dot = 0;
	} else if (contentsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 4;
	} else if (contentsel >= (maxcontentheight - half))
	{ /* we are at the bottom part */
		skip = maxcontentheight - contentheight;
		dot = mlHeight - attempts_height_max - 3;
	} else {
		skip = contentsel - half;
		dot = skip * contentheight / (maxcontentheight - contentheight) + 4;
	}

	API->console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "Select TiMidity++ configuration file", dot, 3, mlHeight - attempts_height_max - 2);
	mlWidth -= 2;
	mlHeight -= 2;
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Please select a new configuration file using the arrow keys and press");
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x0f, mlWidth, " <ENTER>%0.7o when done, or %0.15o<ESC>%0.7o to cancel.");

	mlTop++; // 2: horizontal bar

	for (mlLine = 4; (mlLine - 1) < (mlHeight - attempts_height_max - 1); mlLine++)
	{
		int masterindex = mlLine - 4 + skip;

		if (masterindex == 0)
		{
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, 0x03, "System default", mlWidth);
			continue;
		}
		if (masterindex == 1)
		{
			if (have_default_timidity)
			{
				if (have_user_timidity)
				{
					API->console->DisplayPrintf (mlTop++, mlLeft, (dsel==0)?0x8a:0x0a, mlWidth, " %S %.7owith user overrides from %.15o%S", default_timidity_path, user_timidity_path);
				} else {
					API->console->DisplayPrintf (mlTop++, mlLeft, (dsel==0)?0x8a:0x0a, mlWidth, " %S %.7o(with no user overrides)", default_timidity_path);
				}
			} else {
				if (have_user_timidity)
				{
					API->console->DisplayPrintf (mlTop++, mlLeft, (dsel==0)?0x8c:0x0c, mlWidth, " No global configuration file found, but user override present");
				} else {
					API->console->DisplayPrintf (mlTop++, mlLeft, (dsel==0)?0x8c:0x0c, mlWidth, " No global configuration file found");
				}
			}
			continue;
		}
		if (masterindex == 3)
		{
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, 0x03, "Global configuration files:", mlWidth);
			continue;
		}
		if ((masterindex == 4) && (!global_timidity_count))
		{
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, (dsel == 1) ? 0x8c : 0x0c, " No configuration files found", mlWidth);
			continue;
		}
		if ((masterindex >= 4) && (masterindex < (global_timidity_count + 4)))
		{
			API->console->DisplayPrintf (mlTop++, mlLeft, (dsel==(masterindex - 4 + 1))?0x8f:0x0f, mlWidth, " %.*S", mlWidth - 1, global_timidity_path[masterindex - 4]);
			continue;
		}
		if (masterindex == (global_timidity_count_or_none + 5))
		{
#ifdef _WIN32
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, 0x03, "SF2 files:", mlWidth);
#else
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, 0x03, "Global SF2 files:", mlWidth);
#endif
			continue;
		}
		if ((masterindex == (global_timidity_count_or_none + 6)) && (!sf2_files_count))
		{
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, (dsel == (masterindex - 6 + 1)) ? 0x8c : 0x0c, " No soundfonts found", mlWidth);
			continue;
		}
		if ((masterindex >= (global_timidity_count_or_none + 6)) && (masterindex < (global_timidity_count_or_none + sf2_files_count + 6)))
		{
			API->console->DisplayPrintf (mlTop++, mlLeft, (dsel == (masterindex - 6 + 1) )?0x8f:0x0f, mlWidth, " %.*S", mlWidth - 1, sf2_files_path[masterindex - global_timidity_count_or_none - 6]);
			continue;
		}

		if (masterindex == (6 + global_timidity_count_or_none + sf2_files_count_or_none + 1))
		{
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, 0x03, "Browse SF2 file:", mlWidth);
			continue;
		}

		if (masterindex == (6 + global_timidity_count_or_none + sf2_files_count_or_none + 2))
		{
			API->console->DisplayPrintf  (mlTop++, mlLeft, (dsel==(masterindex - 8 + 1)) ? 0x8f : 0x0f, mlWidth, " %.*S", mlWidth - 1, sf2_manual ? sf2_manual : " - ");
			continue;
		}

		mlTop++;
	}

	mlTop++; // horizontal bar

	if (dsel == 0)
	{
		path_attempt_print (API, "Checked for", config_attempt, config_attempt_n, mlLeft, mlWidth, mlTop, config_attempt_height);
	} else if (dsel < (global_timidity_count_or_none + 1))
	{
		path_attempt_print (API, "Scanned", global_configs_attempt, global_configs_attempt_n, mlLeft, mlWidth, mlTop, global_configs_attempt_height);
	} else if (dsel < (global_timidity_count_or_none + sf2_files_count_or_none + 1))
	{
		path_attempt_print (API, "Scanned", global_sf2_attempt, global_sf2_attempt_n, mlLeft, mlWidth, mlTop, global_sf2_attempt_height);
	}
}

static void ConfigDrawItems (const int lineno, int xpos, const int width, const char **list, const int listlength, const int selected, const int active, const struct DevInterfaceAPI_t *API)
{
	int i;
	int origxpos = xpos;
	for (i=0; i < listlength; i++)
	{
		int l = strlen (list[i]);
		if (selected == i)
		{
			API->console->DisplayPrintf (lineno, xpos, (active)?0x09:0x01, l + 2, "[%.*o%s%.*o]", (active)?0x0f:0x07, list[i], (active)?0x09:0x01);
		} else {
			API->console->DisplayPrintf (lineno, xpos, 0x00, l + 2, " %.*o%s%.0o ", (active)?0x07:0x08, list[i]);
		}
		xpos += l + 2;
	}
	API->console->Driver->DisplayChr (lineno, xpos, 0x07, ' ', width - xpos + origxpos);
}

static void ConfigDrawBar (const int lineno, int xpos, int width, int level, int maxlevel, const int active, const struct DevInterfaceAPI_t *API)
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
	API->console->Driver->DisplayStr (lineno, xpos,                         (active)?0x07:0x08, "[", 1);
	API->console->Driver->DisplayChr (lineno, xpos + 1,                     (active)?0x01:0x08, '\xfe', p1);
	API->console->Driver->DisplayChr (lineno, xpos + 1 + p1,                (active)?0x09:0x08, '\xfe', p2);
	API->console->Driver->DisplayChr (lineno, xpos + 1 + p1 + p2,           (active)?0x0b:0x08, '\xfe', p3);
	API->console->Driver->DisplayChr (lineno, xpos + 1 + p1 + p2 + p3,      (active)?0x0f:0x08, '\xfe', p4);
	API->console->Driver->DisplayChr (lineno, xpos + 1 + p1 + p2 + p3 + p4, (active)?0x07:0x08, '\xfa', tw - p1 - p2 - p3 - p4);

	snprintf (temp, sizeof (temp), "]%5d", level);
	API->console->Driver->DisplayStr (lineno, xpos + width - 8, (active)?0x07:0x08, temp, 8);
}

static int DefaultReverbMode;
static int DefaultReverbLevel;
static int DefaultScaleRoom;
static int DefaultOffsetRoom;
static int DefaultPredelayFactor;
static int DefaultDelayMode;
static int DefaultDelay;
static int DefaultChorus;
static void timidityConfigDraw (int EditPos, const struct DevInterfaceAPI_t *API)
{
	int large;
	int mlWidth, mlHeight, mlTop, mlLeft;
	const char *configfile = API->configAPI->GetProfileString ("timidity", "configfile", "");
	const char *reverbs[] = {"disable", "original", "global-original", "freeverb", "global-freeverb"};
	const char *effect_lr_modes[] = {"disable", "left", "right", "both"};
	const char *disable_enable[] = {"disable", "enable"};

	if (API->console->TextHeight >= 45)
	{
		large = 2;
		mlHeight = 43;
	} else if (API->console->TextHeight >= 37)
	{
		large = 1;
		mlHeight = 35;
	} else {
		large = 0;
		mlHeight = 23;
	}

	mlWidth = 70;
	mlTop = (API->console->TextHeight - mlHeight) / 2;
	mlLeft = (API->console->TextWidth - mlWidth) / 2;

	API->console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "TiMidity++ configuration", 0, (!!large) + 2 + (!!large), 0);
	mlWidth -= 2;
	mlHeight -= 2;

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Navigate with arrows and hit %.15o<ESC>%.7o to save and exit.");

	if (large) mlTop++;

	mlTop++; // 1 or 3: horizontal bar

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " 1. TiMdity+ Configfile/Soundfont: %.15o<ENTER>%.7o to change");

	if (!configfile[0])
	{
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x03, mlWidth, "    (Global default)");
		API->console->DisplayPrintf (mlTop++, mlLeft, (EditPos==0)?0x87:0x07, mlWidth, "    Select another file");
	} else {
		if ((strlen(configfile) > 4) && !strcasecmp (configfile + strlen (configfile) - 4, ".sf2"))
		{
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x03, mlWidth, "    (SF2 sound font)");
		} else {
			API->console->DisplayPrintf (mlTop++, mlLeft, 0x03, mlWidth, "    (Specific config file)");
		}
		API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, "    %*o%*S%0.9o ",(EditPos==0)?8:0, mlWidth - 5, configfile);
	}

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " 2. Default Reverb Mode:");

	if (large >= 2) mlTop++;

	ConfigDrawItems (mlTop++, mlLeft + 3, mlWidth - 3, reverbs, 5, DefaultReverbMode, EditPos==1, API);

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " 3. Default Reverb Level:");

	if (large >= 2) mlTop++;

	ConfigDrawBar (mlTop++, mlLeft + 4, mlWidth - 4, DefaultReverbLevel, 127, EditPos==2, API);

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " 4. Default Scale Room:");

	if (large >= 2) mlTop++;

	ConfigDrawBar (mlTop++, mlLeft + 4, mlWidth - 4, DefaultScaleRoom, 1000, EditPos==3, API);

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " 5. Default Offset Room:");

	if (large >= 2) mlTop++;

	ConfigDrawBar (mlTop++, mlLeft + 4, mlWidth - 4, DefaultOffsetRoom, 1000, EditPos==4, API);

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " 6. Default Predelay Factor:");

	if (large >= 2) mlTop++;

	ConfigDrawBar (mlTop++, mlLeft + 4, mlWidth - 4, DefaultPredelayFactor, 1000, EditPos==5, API);

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " 7. Default Delay Mode:");

	if (large >= 2) mlTop++;

	ConfigDrawItems (mlTop++, mlLeft + 3, mlWidth - 3, effect_lr_modes, 4, DefaultDelayMode, EditPos==6, API);

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " 8. Default Delay (ms):");

	if (large >= 2) mlTop++;

	ConfigDrawBar (mlTop++, mlLeft + 4, mlWidth - 4, DefaultDelay, 1000, EditPos==7, API);

	if (large) mlTop++;

	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " 9. Default Chorus:");

	if (large >= 2) mlTop++;

	ConfigDrawItems (mlTop++, mlLeft + 3, mlWidth - 3, disable_enable, 2, DefaultChorus, EditPos==8, API);

	if (large) mlTop++;
}

static int BrowseSF2_entries_append (void)
{
	void *temp;
	if (BrowseSF2_entries_count < BrowseSF2_entries_size)
	{
		return 0;
	}
	temp = realloc (BrowseSF2_entries_data, sizeof (BrowseSF2_entries_data[0]) * (BrowseSF2_entries_size + 16));
	if (!temp)
	{
		return -1;
	}
	BrowseSF2_entries_data = temp;
	BrowseSF2_entries_size += 16;
	return 0;
}

static void BrowseSF2_entries_clear (void)
{
	int i;
	for (i=0; i < BrowseSF2_entries_count; i++)
	{
		if (BrowseSF2_entries_data[i].file)
		{
			BrowseSF2_entries_data[i].file->unref (BrowseSF2_entries_data[i].file);
		}
		if (BrowseSF2_entries_data[i].dir)
		{
			BrowseSF2_entries_data[i].dir->unref (BrowseSF2_entries_data[i].dir);
		}
	}
	free (BrowseSF2_entries_data);
	BrowseSF2_entries_count = 0;
	BrowseSF2_entries_size = 0;
	BrowseSF2_entries_data = 0;
}

static void BrowseSF2_entries_append_parent (struct ocpdir_t *dir, const struct DevInterfaceAPI_t *API)
{
	if (BrowseSF2_entries_append())
	{
		return;
	}
	BrowseSF2_entries_data[BrowseSF2_entries_count].isparent = 1;
#ifdef _WIN32
	BrowseSF2_entries_data[BrowseSF2_entries_count].drive = 0;
#endif
	BrowseSF2_entries_data[BrowseSF2_entries_count].dir = dir;
	dir->ref (dir);
	BrowseSF2_entries_data[BrowseSF2_entries_count].file = 0;
	BrowseSF2_entries_count++;
}

static void BrowseSF2_entries_append_dir (struct ocpdir_t *dir, const struct DevInterfaceAPI_t *API)
{
	if (BrowseSF2_entries_append())
	{
		return;
	}
	BrowseSF2_entries_data[BrowseSF2_entries_count].isparent = 0;
#ifdef _WIN32
	BrowseSF2_entries_data[BrowseSF2_entries_count].drive = 0;
#endif
	BrowseSF2_entries_data[BrowseSF2_entries_count].dir = dir;
	dir->ref (dir);
	BrowseSF2_entries_data[BrowseSF2_entries_count].file = 0;
	BrowseSF2_entries_count++;
}

static void BrowseSF2_entries_append_file (struct ocpfile_t *file, const struct DevInterfaceAPI_t *API)
{
	if (BrowseSF2_entries_append())
	{
		return;
	}
	BrowseSF2_entries_data[BrowseSF2_entries_count].isparent = 0;
#ifdef _WIN32
	BrowseSF2_entries_data[BrowseSF2_entries_count].drive = 0;
#endif
	BrowseSF2_entries_data[BrowseSF2_entries_count].dir = 0;
	BrowseSF2_entries_data[BrowseSF2_entries_count].file = file;
	file->ref (file);
	BrowseSF2_entries_count++;
}

#ifdef _WIN32
static void BrowseSF2_entries_append_drive (const struct DevInterfaceAPI_t *API, char drive)
{
/*
	if (ref == DIRDB_CLEAR)
	{
		return;
	}
*/
	if (BrowseSF2_entries_append())
	{
		return;
	}
	BrowseSF2_entries_data[BrowseSF2_entries_count].isparent = 0;
	BrowseSF2_entries_data[BrowseSF2_entries_count].drive = drive;
	BrowseSF2_entries_data[BrowseSF2_entries_count].dir = 0;
	BrowseSF2_entries_data[BrowseSF2_entries_count].file = 0;
	BrowseSF2_entries_count++;
}
#endif

static const struct DevInterfaceAPI_t *cmp_API;
static int cmp(const void *a, const void *b)
{
	struct BrowseSF2_t *p1 = (struct BrowseSF2_t *)a;
	struct BrowseSF2_t *p2 = (struct BrowseSF2_t *)b;

	const char *n1;
	const char *n2;

	if (p1->isparent)
	{
		return -1;
	}
	if (p2->isparent)
	{
		return 1;
	}
	if (p1->dir && (!p2->dir))
	{
		return -1;
	}
	if (p2->dir && (!p1->dir))
	{
		return 1;
	}
#ifdef _WIN32
	if ((p1->drive) && (!p2->drive))
	{
		return -1;
	}
	if (p2->drive && (!p1->drive))
	{
		return 1;
	}
	if (p1->drive)
	{
		return p1->drive - p2->drive;
	}
#endif

	cmp_API->dirdb->GetName_internalstr (p1->file ? p1->file->dirdb_ref : p1->dir->dirdb_ref, &n1);
	cmp_API->dirdb->GetName_internalstr (p2->file ? p2->file->dirdb_ref : p2->dir->dirdb_ref, &n2);
	return strcmp (n1, n2);
}

static void BrowseSF2_entries_sort (const struct DevInterfaceAPI_t *API)
{
	cmp_API = API;
	qsort (BrowseSF2_entries_data, BrowseSF2_entries_count, sizeof (BrowseSF2_entries_data[0]), cmp);
	cmp_API = 0;
}

static void BrowseSF2_refresh_dir_add_dir (void *token, struct ocpdir_t *dir)
{
	const struct DevInterfaceAPI_t *API = token;
	const char *dirname = 0;
	API->dirdb->GetName_internalstr (dir->dirdb_ref, &dirname);
	if (!dirname)
	{
		return;
	}
	if ((strcmp (dirname, ".")) &&
	    (strcmp (dirname, "..")))
	{
		BrowseSF2_entries_append_dir (dir, API);
	}
}

static void BrowseSF2_refresh_dir_add_file (void *token, struct ocpfile_t *file)
{
	const struct DevInterfaceAPI_t *API = token;
	const char *filename = 0;
	size_t len;
	API->dirdb->GetName_internalstr (file->dirdb_ref, &filename);
	if (!filename)
	{
		return;
	}
	len = strlen (filename);
	if (len <= 4)
	{
		return;
	}
	if (!strcasecmp (filename + len - 4, ".sf2"))
	{
		BrowseSF2_entries_append_file (file, API);
	}
}

static void BrowseSF2_refresh_dir (struct ocpdir_t *dir, uint32_t old, int *fsel, const struct DevInterfaceAPI_t *API)
{
	int i;

	BrowseSF2_entries_clear ();

	ocpdirhandle_pt iter;
	if (dir->parent)
	{
		BrowseSF2_entries_append_parent (dir->parent, API);
	}
	iter = dir->readdir_start (dir, BrowseSF2_refresh_dir_add_file, BrowseSF2_refresh_dir_add_dir, (void *)API);
	while (dir->readdir_iterate (iter))
	{
	}
	dir->readdir_cancel (iter);

#ifdef _WIN32
	for (i=0; i < 26; i++)
	{
		if (API->dmDriveLetters[i])
		{
			BrowseSF2_entries_append_drive (API, i + 'A');
		}
	}
#endif
	BrowseSF2_entries_sort (API);
	for (i=0; i < BrowseSF2_entries_count; i++)
	{
		if (BrowseSF2_entries_data[i].file && (BrowseSF2_entries_data[i].file->dirdb_ref == old))
		{
			*fsel = i;
			return;
		}
		if (BrowseSF2_entries_data[i].dir && (BrowseSF2_entries_data[i].dir->dirdb_ref == old))
		{
			*fsel = i;
			return;
		}
	}
}

static int timidityConfigRunBrowseSF2 (const struct DevInterfaceAPI_t *API)
{
	int fsel = 0;
	struct ocpdir_t *currentdir;
	char *currentpath = 0;
	if (sf2_manual_dir)
	{
		currentdir = sf2_manual_dir;
	} else {
		currentdir = API->configAPI->DataHomeDir;
	}
	if (!currentdir)
	{
		return 0;
	}
	currentdir->ref(currentdir);
#ifdef _WIN32
	API->dirdb->GetFullname_malloc (currentdir->dirdb_ref, &currentpath, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH);
#else
	API->dirdb->GetFullname_malloc (currentdir->dirdb_ref, &currentpath, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH);
#endif

	BrowseSF2_refresh_dir (currentdir, sf2_manual_dirdb_ref, &fsel, API);

	API->console->FrameLock ();

	while (1)
	{
		API->fsDraw();
		timidityBrowseSF2Draw (fsel, currentpath, API);

		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_DOWN:
					if ((fsel + 1) < (BrowseSF2_entries_count))
					{
						fsel++;
					}
					break;
				case KEY_UP:
					if (fsel)
					{
						fsel--;
					}
					break;
				case _KEY_ENTER:
					if (!BrowseSF2_entries_count)
					{
						break;
					}
#ifdef _WIN32
					if (BrowseSF2_entries_data[fsel].drive)
					{
						if (API->dmDriveLetters[BrowseSF2_entries_data[fsel].drive-'A'])
						{
							currentdir->unref (currentdir);
							currentdir = 0;

							currentdir = API->dmDriveLetters[BrowseSF2_entries_data[fsel].drive-'A']->cwd;
							currentdir->ref (currentdir);
							BrowseSF2_refresh_dir (currentdir, sf2_manual_dirdb_ref, &fsel, API);

							free (currentpath);
							currentpath = 0;
#ifdef _WIN32
							API->dirdb->GetFullname_malloc (currentdir->dirdb_ref, &currentpath, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH);
#else
							API->dirdb->GetFullname_malloc (currentdir->dirdb_ref, &currentpath, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH);
#endif

						}
						break;
					}
#endif
					if (BrowseSF2_entries_data[fsel].dir)
					{
						uint32_t dirdb_ref = currentdir->dirdb_ref;
						API->dirdb->Ref(dirdb_ref, dirdb_use_file);

						currentdir->unref (currentdir);
						currentdir = 0;

						currentdir = BrowseSF2_entries_data[fsel].dir;
						currentdir->ref (currentdir);

						BrowseSF2_refresh_dir (currentdir, dirdb_ref, &fsel, API);

						API->dirdb->Unref(dirdb_ref, dirdb_use_file);
						dirdb_ref = DIRDB_CLEAR;

						free (currentpath);
						currentpath = 0;

#ifdef _WIN32
						API->dirdb->GetFullname_malloc (currentdir->dirdb_ref, &currentpath, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH);
#else
						API->dirdb->GetFullname_malloc (currentdir->dirdb_ref, &currentpath, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH);
#endif
						break;
					}

					if (BrowseSF2_entries_data[fsel].file)
					{
						free (sf2_manual);
						sf2_manual = 0;

						if (sf2_manual_dir)
						{
							sf2_manual_dir->unref (sf2_manual_dir);
							sf2_manual_dir = 0;
						}

						if (sf2_manual_dirdb_ref != DIRDB_CLEAR)
						{
							API->dirdb->Unref(sf2_manual_dirdb_ref, dirdb_use_file);
							sf2_manual_dirdb_ref = DIRDB_CLEAR;
						}

						sf2_manual_dir = BrowseSF2_entries_data[fsel].file->parent;
						sf2_manual_dir->ref (sf2_manual_dir);

						sf2_manual_dirdb_ref = BrowseSF2_entries_data[fsel].file->dirdb_ref;
						API->dirdb->Ref(sf2_manual_dirdb_ref, dirdb_use_file);

#ifdef _WIN32
						API->dirdb->GetFullname_malloc(sf2_manual_dirdb_ref, &sf2_manual, DIRDB_FULLNAME_DRIVE | DIRDB_FULLNAME_BACKSLASH);
#else
						API->dirdb->GetFullname_malloc(sf2_manual_dirdb_ref, &sf2_manual, 0);
#endif

						API->configAPI->SetProfileString("timidity", "sf2manual", sf2_manual);
						API->configAPI->SetProfileString("timidity", "configfile", sf2_manual);

						currentdir->unref (currentdir);
						currentdir = 0;

						free (currentpath);
						currentpath = 0;

						BrowseSF2_entries_clear ();
						return 1;
					}
					break;

					/* pass-through, for file-entries only!!!??!!?? */
				case KEY_EXIT:
				case KEY_ESC:
					currentdir->unref (currentdir);
					currentdir = 0;

					free (currentpath);
					currentpath = 0;

					BrowseSF2_entries_clear ();
					return 0;
			}
		}
		API->console->FrameLock ();
	}
}

static void timidityConfigRun (void **token, const struct DevInterfaceAPI_t *API)
{
	int esel = 0;

	refresh_configfiles (API);

	DefaultReverbMode     = API->configAPI->GetProfileInt ("timidity", "reverbmode",       2, 10);
	DefaultReverbLevel    = API->configAPI->GetProfileInt ("timidity", "reverblevel",     40, 10);
	DefaultScaleRoom      = API->configAPI->GetProfileInt ("timidity", "scaleroom",       28, 10);
	DefaultOffsetRoom     = API->configAPI->GetProfileInt ("timidity", "offsetroom",      70, 10);
	DefaultPredelayFactor = API->configAPI->GetProfileInt ("timidity", "predelayfactor", 100, 10);
	DefaultDelayMode      = API->configAPI->GetProfileInt ("timidity", "delaymode",       -1, 10) + 1;
	DefaultDelay          = API->configAPI->GetProfileInt ("timidity", "delay",           25, 10);
	DefaultChorus         = API->configAPI->GetProfileInt ("timidity", "chorusenabled",    1, 10);
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
		API->fsDraw();
		timidityConfigDraw (esel, API);
		while (API->console->KeyboardHit())
		{
			int key = API->console->KeyboardGetChar();
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
							const char *configfile = API->configAPI->GetProfileString ("timidity", "configfile", "");
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
											dsel = i + 1 + global_timidity_count_or_none;
										}
									}
								}
								if (!dsel)
								{
									if (sf2_manual)
									{
#ifdef _WIN32
										if (!strcasecmp(configfile, sf2_manual))
#else
										if (!strcmp(configfile, sf2_manual))
#endif
										{
											dsel = 1 + global_timidity_count_or_none + sf2_files_count_or_none;
										}
									}
								}
							}
						}

						while (inner)
						{
							API->fsDraw();
							timidityConfigFileSelectDraw (dsel, API);
							while (inner && API->console->KeyboardHit())
							{
								int key = API->console->KeyboardGetChar();
								switch (key)
								{
									case KEY_DOWN:
										if ((dsel + 1) < (1 + global_timidity_count_or_none + sf2_files_count_or_none + 1))
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
											API->configAPI->SetProfileString ("timidity", "configfile", "");
											inner = 0;
										} else if (dsel < (global_timidity_count_or_none + 1))
										{
											if (global_timidity_count)
											{
												API->configAPI->SetProfileString ("timidity", "configfile", global_timidity_path[dsel - 1]);
												inner = 0;
											}
										} else if (dsel < (global_timidity_count_or_none + sf2_files_count_or_none + 1))
										{
											if (sf2_files_count)
											{
												API->configAPI->SetProfileString ("timidity", "configfile", sf2_files_path[dsel - 1 - global_timidity_count_or_none]);
												inner = 0;
											}
										} else if (dsel == global_timidity_count_or_none + sf2_files_count_or_none + 1)
										{
											if (timidityConfigRunBrowseSF2 (API))
											{
												inner = 0;
											}
										}
										break;
									case KEY_DELETE:
										if (dsel == global_timidity_count_or_none + sf2_files_count_or_none + 1)
										{
											if (sf2_manual)
											{
												free (sf2_manual);
												sf2_manual = 0;
											}
											if (sf2_manual_dirdb_ref != DIRDB_CLEAR)
											{
												API->dirdb->Unref (sf2_manual_dirdb_ref, dirdb_use_file);
												sf2_manual_dirdb_ref = DIRDB_CLEAR;
											}
											if (sf2_manual_dir)
											{
												sf2_manual_dir->unref (sf2_manual_dir);
												sf2_manual_dir = 0;
											}
										}
										break;
								}
							}
							API->console->FrameLock ();
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
					API->configAPI->StoreConfig();
					goto superexit;
					break;
			}
		}
		API->console->FrameLock ();
	}

superexit:
	reset_configfiles (API);

	API->configAPI->SetProfileInt ("timidity", "reverbmode",     DefaultReverbMode,     10);
	API->configAPI->SetProfileInt ("timidity", "reverblevel",    DefaultReverbLevel,    10);
	API->configAPI->SetProfileInt ("timidity", "scaleroom",      DefaultScaleRoom,      10);
	API->configAPI->SetProfileInt ("timidity", "offsetroom",     DefaultOffsetRoom,     10);
	API->configAPI->SetProfileInt ("timidity", "predelayfactor", DefaultPredelayFactor, 10);
	API->configAPI->SetProfileInt ("timidity", "delaymode",      DefaultDelayMode - 1,  10);
	API->configAPI->SetProfileInt ("timidity", "delay",          DefaultDelay,          10);
	API->configAPI->SetProfileInt ("timidity", "chorusenabled",  DefaultChorus,         10);
	API->configAPI->StoreConfig ();
}

static struct ocpfile_t *timidityconfig; // needs to overlay an dialog above filebrowser, and after that the file is "finished"   Special case of DEVv
static void timidityConfigRun (void **token, const struct DevInterfaceAPI_t *API);

OCP_INTERNAL int timidity_config_init (struct PluginInitAPI_t *API)
{
	timidityconfig = API->dev_file_create (
		API->dmSetup->basedir,
		"timidityconfig.dev",
		"TiMidity+ Configuration",
		"",
		0, /* token */
		0, /* Init */
		timidityConfigRun,
		0, /* Close */
		0  /* Destructor */
	);

	API->filesystem_setup_register_file (timidityconfig);

	return errOk;
}

OCP_INTERNAL void timidity_config_done (struct PluginCloseAPI_t *API)
{
	if (timidityconfig)
	{
		API->filesystem_setup_unregister_file (timidityconfig);
		timidityconfig->unref (timidityconfig);
		timidityconfig = 0;
	}
}
