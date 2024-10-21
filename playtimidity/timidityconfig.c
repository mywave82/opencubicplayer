/* OpenCP Module Player
 * copyright (c) 2022-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
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

static char *user_timidity_path = 0;
static int   have_user_timidity = 0;
static int   have_default_timidity = 0;
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
	free (user_timidity_path);
	global_timidity_count = 0;
	global_timidity_path = 0;
	sf2_files_count = 0;
	sf2_files_path = 0;
	have_user_timidity = 0;
	user_timidity_path = 0;
	have_default_timidity = 0;
	default_timidity_path[0] = 0;
}

static void try_user (const struct DevInterfaceAPI_t *API, const char *filename)
{
	struct ocpfile_t *f;

	f = ocpdir_readdir_file (API->configAPI->HomeDir, filename, API->dirdb);
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

static void try_global (const char *path)
{
#ifdef _WIN32
	DWORD st;

	st = GetFileAttributes (path);

	if (st == INVALID_FILE_ATTRIBUTES)
	{
		return;
	}
	if (st & FILE_ATTRIBUTE_DIRECTORY)
	{
		return;
	}
#else
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
#endif
	PRINT ("Found global config %s%s", path, have_user_timidity?", with possible user overrides":" (no user overrides found earlier)\n");
	if (!have_default_timidity)
	{
		snprintf (default_timidity_path, sizeof (default_timidity_path), "%s", path);
		have_default_timidity = 1;
	}
}

static void scan_config_directory (const char *dpath)
{
#ifdef _WIN32
	HANDLE FindHandle;
	WIN32_FIND_DATAA FindData;
	char path[BUFSIZ];

	FindHandle = FindFirstFile (dpath, &FindData);
	if (FindHandle == INVALID_HANDLE_VALUE)
	{
		PRINT ("Unable to scan directory %s for global configuration files\n", dpath);
		return;
	}

	do
	{
		snprintf (path, sizeof (path), "%s%s%s", dpath, PATH_STRING, FindData.cFileName);

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

static void scan_sf2_directory (const char *dpath)
{
#ifdef _WIN32
	HANDLE FindHandle;
	WIN32_FIND_DATAA FindData;
	char path[BUFSIZ];

	FindHandle = FindFirstFile (dpath, &FindData);
	if (FindHandle == INVALID_HANDLE_VALUE)
	{
		PRINT ("Unable to scan directory %s for global sf2 fonts\n", dpath);
		return;
	}

	do
	{
		snprintf (path, sizeof (path), "%s%s%s", dpath, PATH_STRING, FindData.cFileName);

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
#endif
}

static int mystrcmp(const void *a, const void *b)
{
	return strcmp (*(char **)a, *(char **)b);
}

static void refresh_configfiles (const struct DevInterfaceAPI_t *API)
{
	reset_configfiles ();

#ifdef _WIN32
	try_user (API, "timidity.cfg");
	try_user (API, "_timidity.cfg");
#endif
	try_user (API, "timidity.cfg");

#ifdef __W32
	try_global ("C:\\WINDOWS\\timidity.cfg");
	try_global ("C:\\timidity\\timidity.cfg");
	try_global ("C:\\TiMidity++\\timidity.cfg");
	scan_config_directory ("C:\\timidity\\*");
	scan_config_directory ("C:\\TiMidity++\\*");
	scan_config_directory ("C:\\timidity\\soundfonts\\*");
	scan_config_directory ("C:\\TiMidity++\\soundfonts\\*");
	scan_config_directory ("C:\\timidity\\musix\\*");
	scan_config_directory ("C:\\TiMidity++\\musix\\*");
	scan_sf2_directory ("C:\\timidity\\soundfonts\\*");
	scan_sf2_directory ("C:\\TiMidity++\\soundfonts\\*");
	scan_sf2_directory ("C:\\timidity\\musix\\*");
	scan_sf2_directory ("C:\\TiMidity++\\musix\\*");
#else
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
#endif

	if (global_timidity_count >= 2)
	{
		qsort (global_timidity_path, global_timidity_count, sizeof (global_timidity_path[0]), mystrcmp);
	}
	if (sf2_files_count >= 2)
	{
		qsort (sf2_files_path, sf2_files_count, sizeof (sf2_files_path[0]), mystrcmp);
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
		dot = 0;
	} else if (contentsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 4;
	} else if (contentsel >= (contentheight - half))
	{ /* we are at the bottom part */
		skip = contentheight - (mlHeight - LINES_NOT_AVAILABLE);
		dot = mlHeight - LINES_NOT_AVAILABLE - 1 + 4;
	} else {
		skip = contentsel - half;
		dot = skip * (mlHeight - LINES_NOT_AVAILABLE) / (contentheight - (mlHeight - LINES_NOT_AVAILABLE)) + 4;
	}

	API->console->DisplayFrame (mlTop++, mlLeft++, mlHeight, mlWidth, DIALOG_COLOR_FRAME, "Select TiMidity++ configuration file", dot, 3, 0);
	mlWidth -= 2;
	mlHeight -= 2;
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x07, mlWidth, " Please select a new configuration file using the arrow keys and press");
	API->console->DisplayPrintf (mlTop++, mlLeft, 0x0f, mlWidth, " <ENTER>%0.7o when done, or %0.15o<ESC>%0.7o to cancel.");

	mlTop++; // 2: horizontal bar

	for (mlLine = 4; (mlLine - 1) < mlHeight; mlLine++)
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
				API->console->DisplayPrintf (mlTop++, mlLeft, (dsel==0)?0x8c:0x0c, mlWidth, " No global configuration file found");
			}
			mlTop++;
			continue;
		}
		if (masterindex == 3)
		{
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, 0x03, "Global configuration files:", mlWidth);
			continue;
		}
		if ((masterindex == 4) && (!global_timidity_count))
		{
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, 0x0c, " No configuration files found", mlWidth);
			continue;
		}
		if ((masterindex >= 4) && (masterindex < (global_timidity_count + 4)))
		{
			API->console->DisplayPrintf (mlTop++, mlLeft, (dsel==(masterindex - 4 + 1))?0x8f:0x0f, mlWidth, " %.*S", mlWidth - 1, global_timidity_path[masterindex - 4]);
			continue;
		}
		if (masterindex == (global_timidity_count + 5))
		{
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, 0x03, " Global SF2 files:", mlWidth);
			continue;
		}
		if ((masterindex == (global_timidity_count + 6)) && (!sf2_files_count))
		{
			API->console->Driver->DisplayStr  (mlTop++, mlLeft, 0x0c, " No soundfonts found", mlWidth);
			continue;
		}
		if ((masterindex >= (global_timidity_count + 6)) && (masterindex < (global_timidity_count + sf2_files_count + 6)))
		{
			API->console->DisplayPrintf (mlTop++, mlLeft, (dsel==(masterindex - 6 + 1))?0x8f:0x0f, mlWidth, " %.*S", mlWidth - 1, sf2_files_path[masterindex - global_timidity_count - 6]);
			continue;
		}
		mlTop++;
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
		if ((strlen(configfile) > 4) && !strcmp (configfile + strlen (configfile) - 4, ".sf2"))
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
											dsel = i + 1 + global_timidity_count;
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
											API->configAPI->SetProfileString ("timidity", "configfile", "");
										} else if (dsel < (global_timidity_count + 1))
										{
											API->configAPI->SetProfileString ("timidity", "configfile", global_timidity_path[dsel - 1]);
										} else {
											API->configAPI->SetProfileString ("timidity", "configfile", sf2_files_path[dsel - 1 - global_timidity_count]);
										}
										inner = 0;
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
	reset_configfiles ();

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
