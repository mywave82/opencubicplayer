/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * the File selector ][
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
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -changed some INI lookups to dllinfo lookups
 *    -fixed Long Filename lookup code a bit
 *    -no other changes, i won't touch this monster
 *  -fd981206   Felix Domke    <tmbinc@gmx.net>
 *    -edited for new binfile
 *    -no other changes, i won't touch this monster
 *     (we REALLY have to split up theis file!)
 *  -doj20020418 Dirk Jagdmann <doj@cubic.org>
 *    -added screenshot
 *  -ss040831   Stian Skjelstad <stian@nixia.no>
 *    -removed modlist->pathtothis, use curdirpath instead
 *    -updated fsEditPath and fsEditViewPath
 *  -ss040914   Stian Skjelstad <stian@nixia.no>
 *    -dirty hack to stop scanning files in arcs when moving the arrows
 *  -ss040915   Stian Skjelstad <stian@nixia.no>
 *    -Make sure that the dosfile_ReadHandle does not survive a fork
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -Make sure console is sane when we are done scanning a directory
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -setcurshape has new logic
 *  -ss050118   Stian Skjelstad <stian@nixia.no>
 *    -navigation in playlist
 *  -ss050124   Stian Skjelstad <stian@nixia.no>
 *    -fnmatch into place
 *    -minor changes to make filemask work
 *  -050528 Reinaert Albrecht <reinaert.albrecht@easynet.be>
 *    -recursive directory support (RD_PUTRSUBS)
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
# include <shlwapi.h>
#else
#include <fnmatch.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"

#include "adbmeta.h"
#include "boot/psetting.h"
#include "charsets.h"
#include "cphlpfs.h"
#include "cpiface/cpiface.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-ancient.h"
#include "filesystem-drive.h"
#include "filesystem-file-dev.h"
#include "filesystem-bzip2.h"
#include "filesystem-gzip.h"
#include "filesystem-pak.h"
#include "filesystem-playlist.h"
#include "filesystem-playlist-m3u.h"
#include "filesystem-playlist-pls.h"
#include "filesystem-setup.h"
#include "filesystem-rpg.h"
#include "filesystem-tar.h"
#include "filesystem-unix.h"
#include "filesystem-windows.h"
#include "filesystem-z.h"
#include "filesystem-zip.h"
#include "mdb.h"
#include "modlist.h"
#include "musicbrainz.h"
#include "pfilesel.h"
#include "stuff/compat.h"
#include "stuff/framelock.h"
#include "stuff/imsrtns.h"
#include "stuff/piperun.h"
#include "stuff/poutput.h"
#include "stuff/utf-8.h"

#include "pfilesel-charset.c"

static char fsScanDir(int pos);
static struct modlist *currentdir=NULL;
static struct modlist *playlist=NULL;

#define dirdbcurdirpath (dmCurDrive->cwd->dirdb_ref)
static char *curmask;

struct dmDrive *dmCurDrive=0;

static int fsSavePlayList(const struct modlist *ml);

static void fsDraw(void);

static struct interfacestruct *plInterfaces;

static struct DevInterfaceAPI_t DevInterfaceAPI;

struct fsReadDir_token_t
{
#if 0
	struct dmDrive *drive;
#endif
	struct modlist *ml;
	char           *mask;

	unsigned long   opt;

	int             cancel_recursive;
	char           *parent_displaydir;
};

static void fsReadDir_file (void *_token, struct ocpfile_t *file)
{
	struct fsReadDir_token_t *token = _token;
	char *curext;
	const char *childpath = 0;
#ifndef FNM_CASEFOLD
	char *childpath_upper;
#endif
	int ismod = 0;

	dirdbGetName_internalstr (file->dirdb_ref, &childpath);

	getext_malloc (childpath, &curext);
	if (!curext)
	{
		return;
	}

	if ((token->opt & RD_ARCSCAN) && (!token->cancel_recursive))
	{
		if (token->opt & (RD_PUTSUBS | RD_PUTRSUBS))
		{
			struct ocpdir_t *dir = ocpdirdecompressor_check (file, curext);
			if (dir)
			{
				if (token->opt & RD_PUTSUBS)
				{
					modlist_append_dir (token->ml, dir);
				}
				if (token->opt & RD_PUTRSUBS)
				{
					fsReadDir (token->ml, dir, token->mask, token->opt);
				}
				if ((!dir->is_playlist) && fsPutArcs && dir->readflatdir_start)
				{
					unsigned int mlTop=plScrHeight/2-2;
					unsigned int i;
					char *oldparent_displaydir;

					/* store the callers directory - we might be running recursive */
					oldparent_displaydir = token->parent_displaydir;
					token->parent_displaydir = 0;

					/* draw a box and information to the display... resize of the display will not be gracefull during a directory scan */
					displayvoid(mlTop+1, 5, plScrWidth-10);
					displayvoid(mlTop+2, 5, plScrWidth-10);
//					displayvoid(mlTop+3, 5, plScrWidth-10);
					displaystr(mlTop, 4, 0x04, "\xda", 1);
					for (i=5;i<(plScrWidth-5);i++)
					{
						displaystr(mlTop, i, 0x04, "\xc4", 1);
						displaystr(mlTop, plScrWidth-5, 0x04, "\xbf", 1);
						displaystr(mlTop+1, 4, 0x04, "\xb3", 1);
						displaystr(mlTop+2, 4, 0x04, "\xb3", 1);
						displaystr(mlTop+3, 4, 0x04, "\xb3", 1);
						displaystr(mlTop+1, plScrWidth-5, 0x04, "\xb3", 1);
						displaystr(mlTop+2, plScrWidth-5, 0x04, "\xb3", 1);
						displaystr(mlTop+3, plScrWidth-5, 0x04, "\xb3", 1);
						displaystr(mlTop+4, 4, 0x04, "\xc0", 1);
						for (i=5;i<(plScrWidth-5);i++)
						{
							displaystr(mlTop,   i, 0x04, "\xc4", 1);
							displaystr(mlTop+4, i, 0x04, "\xc4", 1);
						}
						displaystr(mlTop+4, plScrWidth-5, 0x04, "\xd9", 1);
					}
					displaystr (mlTop + 1, 5, 0x09, "Scanning content of the given file. Press space to cancel", plScrWidth - 10);
					dirdbGetFullname_malloc (dir->dirdb_ref, &token->parent_displaydir, DIRDB_FULLNAME_ENDSLASH);
					displaystr_utf8_overflowleft (mlTop + 3, 5, 0x0a, token->parent_displaydir, plScrWidth - 10);


					{ /* do the actual scan */
						ocpdirhandle_pt dh = dir->readflatdir_start (dir, fsReadDir_file, token); /* recycle the same token... */
						while (dir->readdir_iterate (dh) && (!token->cancel_recursive))
						{
							if (poll_framelock())
							{
								while (Console.KeyboardHit())
								{
									int key = Console.KeyboardGetChar();
									if ((key == ' ') || (key == KEY_EXIT))
									{
										token->cancel_recursive = 1;
									}
									if (key == VIRT_KEY_RESIZE)
									{
										fsScrType = plScrType;
										continue;
									}
								}
							}
						}
						dir->readdir_cancel (dh);
					}

					/* undo the filename displayed */
					free (token->parent_displaydir); token->parent_displaydir = oldparent_displaydir;
					if (token->parent_displaydir)
					{
						displaystr_utf8_overflowleft (mlTop + 3, 5, 0x0a, token->parent_displaydir, plScrWidth - 10);
					} else {
						displayvoid(mlTop+3, 5, plScrWidth-10);
					}
				}
				dir->unref (dir);
				free (curext);
				return;
			}
		}
	}

#ifdef _WIN32
	childpath_upper = strupr(strdup(childpath));
	if (!childpath_upper)
	{
		perror("pfilesel.c: strdup() failed");
		goto out;
	}
	if (!PathMatchSpec (childpath_upper, token->mask))
	{
		free (childpath_upper);
		goto out;
	}
	free (childpath_upper);
#else
#ifndef FNM_CASEFOLD
	childpath_upper = strupr(strdup(childpath));
	if (!childpath_upper)
	{
		perror("pfilesel.c: strdup() failed");
		goto out;
	}
	if (fnmatch(token->mask, childpath_upper, 0))
	{
		free (childpath_upper);
		goto out;
	}
	free(childpath_upper);
#else
	if (fnmatch(token->mask, childpath, FNM_CASEFOLD))
	{
		goto out;
	}
#endif
#endif
	ismod = fsIsModule(curext);
	if (ismod ||   // always include if file is an actual module
	    (fsShowAllFiles && (!(token->opt & RD_ISMODONLY)))) // force include if  fsShowAllFiles  is true, except if RD_ISMODONLY
	{
		modlist_append_file (token->ml, file, ismod, file->compression >= COMPRESSION_SOLID); /* modlist_append() will do refcount on the file */
	}
out:
	free (curext);
}

static void fsReadDir_dir  (void *_token, struct ocpdir_t *dir)
{
	struct fsReadDir_token_t *token = _token;

#warning What about RD_SUBNOSYMLINK ??.....

	if (token->opt & RD_PUTRSUBS)
	{
		fsReadDir (token->ml, dir, token->mask, token->opt);
	}
	if (token->opt & RD_PUTSUBS)
	{
		modlist_append_dir (token->ml, dir);
	}
}

int fsReadDir (struct modlist *ml, struct ocpdir_t *dir, const char *mask, unsigned long opt)
{
	struct fsReadDir_token_t token;
	ocpdirhandle_pt dh;

	if (opt & RD_PUTDRIVES)
	{
		struct dmDrive *d;
		for (d=dmDrives; d; d=d->next)
		{
			modlist_append_drive (ml, d);
		}

		if (dir->parent) /* we only add dotdot, if we add drives */
		{
			modlist_append_dotdot (ml, dir->parent);
		}

		opt &= ~RD_PUTDRIVES;
	}

	token.ml = ml;
	token.cancel_recursive = 0;
	token.parent_displaydir = 0;
#ifndef FNM_CASEFOLD
	token.mask = strupr(strdup (mask));
#else
	token.mask = (char *)mask;
#endif
	token.opt = opt;

	if ((opt & RD_PUTRSUBS) && dir->readflatdir_start)
	{
		dh = dir->readflatdir_start (dir, fsReadDir_file, &token);
	} else {
		dh = dir->readdir_start (dir, fsReadDir_file, fsReadDir_dir, &token);
	}

	if (!dh)
	{
#ifndef FNM_CASEFOLD
		free (token.mask);
#endif
		return 0;
	}
	while (dir->readdir_iterate (dh))
	{
#if 0
		if (poll_framelock())
		{
			while (ekbhit())
			{
				int key = egetch();
				if ((key == ' ') || (key == KEY_EXIT))
				{
					token.cancel_recursive = 1;
				}
			}
		}
#endif
	};
	dir->readdir_cancel (dh);
#ifndef FNM_CASEFOLD
	free (token.mask);
#endif
	return 1;
}

struct fsType
{
	struct moduletype modtype;
	uint8_t color; /* for the file-selector */
	const char **description;
	const char *interfacename;
	const struct cpifaceplayerstruct *cp;
};
struct fsType *fsTypes;
int fsTypesCount;

void fsTypeRegister (struct moduletype modtype, const char **description, const char *interfacename, const struct cpifaceplayerstruct *cp)
{
	int i;
	char m[5];
	m[0] = modtype.string.c[0];
	m[1] = modtype.string.c[1];
	m[2] = modtype.string.c[2];
	m[3] = modtype.string.c[3];
	m[4] = 0;

	for (i=0; i < fsTypesCount; i++)
	{
		if (fsTypes[i].modtype.integer.i == modtype.integer.i)
		{
			fprintf (stderr, "fsTypeRegister() modtype %s already registered\n", m);
			return;
		}
		if (strncmp (fsTypes[i].modtype.string.c, modtype.string.c, 4) > 0) break;
	}
	if (!(fsTypesCount & 0x3f))
	{
		void *t = realloc (fsTypes, sizeof (fsTypes[0]) * (fsTypesCount + 0x40));
		if (!t)
		{
			fprintf (stderr, "fsTypeRegister() realloc failed\n");
			return;
		}
		fsTypes = t;
	}
	memmove (fsTypes + i + 1, fsTypes + i, (fsTypesCount - i) * sizeof (fsTypes[0]));
	fsTypes[i].modtype = modtype;
	fsTypes[i].color = cfGetProfileInt ("fscolors", m, 7, 10);
	fsTypes[i].description = description;
	fsTypes[i].interfacename = interfacename;
	fsTypes[i].cp = cp;
	fsTypesCount++;
}

void fsTypeUnregister (struct moduletype modtype)
{
	int i;

	for (i=0; i < fsTypesCount; i++)
	{
		if (fsTypes[i].modtype.integer.i == modtype.integer.i)
		{
			memmove (fsTypes + i, fsTypes + i + 1, fsTypesCount - i - 1);
			fsTypesCount--;
			if (!fsTypesCount)
			{
				free (fsTypes);
				fsTypes = 0;
			}
			return;
		}
		if (strncmp (fsTypes[i].modtype.string.c, modtype.string.c, 4) > 0) break;
	}

}

uint8_t fsModTypeColor(struct moduletype modtype)
{
	int i;

	if (modtype.integer.i == 0) return 7;

	for (i=0; i < fsTypesCount; i++)
	{
		if (fsTypes[i].modtype.integer.i == modtype.integer.i)
		{
			return fsTypes[i].color;
		}
	}
	return 7;
}

static char **moduleextensions=0;

void fsRegisterExt(const char *ext)
{
	if (moduleextensions)
	{
		int n=0;
		char **e;
		for (e=moduleextensions; *e; e++, n++)
			if (!strcasecmp(ext, *e))
				return;
		moduleextensions=realloc(moduleextensions, (n+2)*sizeof(char *));
		moduleextensions[n]=strdup(ext);
		moduleextensions[n+1]=0;
	} else {
		moduleextensions=malloc(2*sizeof(char *));
		moduleextensions[0]=strdup(ext);
		moduleextensions[1]=0;
	}
}

/* This function tells if a file ends with a valid extension or not
 */
int fsIsModule (const char *ext)
{
	char **e;

	if (*ext++!='.')
		return 0;
	for (e=moduleextensions; *e; e++)
		if (!strcasecmp(ext, *e))
			return 1;
	return 0;
}

static void addfiles_file (void *token, struct ocpfile_t *file)
{
	char *curext = 0;
	const char *filename = 0;
	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	getext_malloc (filename, &curext);
	if (!curext)
	{
		return;
	}
	if (fsIsModule(curext))
	{
		modlist_append_file (playlist, file, 1, 0); /* modlist_append calls file->ref (file); for us */
	}
	free (curext);
}

static void addfiles_dir (void *token, struct ocpdir_t *dir)
{
}

static int initRootDir(const struct configAPI_t *configAPI, const char *sec)
{
	int count;
	const char *currentpath2 = 0;

	/* check for files from the command-line */
	{
		struct playlist_instance_t *playlist = 0;

		for (count=0;;count++)
		{
			char buffer[32];
			const char *filename;
			sprintf(buffer, "file%d", count);
			if (!(filename = configAPI->GetProfileString2(sec, "CommandLine_Files", buffer, NULL)))
			{
				break;
			}
			if (!playlist)
			{
				uint32_t dirdb_ref = dirdbFindAndRef (dmCurDrive->cwd->dirdb_ref, "VirtualPlaylist.VirtualPLS", dirdb_use_pfilesel);
				playlist=playlist_instance_allocate (dmCurDrive->cwd, dirdb_ref);
				dirdbUnref (dirdb_ref, dirdb_use_pfilesel);
				if (!playlist)
				{
					break;
				}
			}

#ifdef _WIN32
			playlist_add_string (playlist, strdup (filename), DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_WINDOWS_SLASH);
#else
			playlist_add_string (playlist, strdup (filename), DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_TILDE_USER);
#endif
		}
		if (playlist)
		{
			ocpdirhandle_pt dirhandle;
			dirhandle = playlist->head.readdir_start (&playlist->head, addfiles_file, addfiles_dir, 0);
			while (playlist->head.readdir_iterate (dirhandle))
			{
				if (poll_framelock())
				{
					Console.KeyboardHit();
				}
			}
			playlist->head.readdir_cancel (dirhandle);
			playlist->head.unref (&playlist->head);
		}
	}

	/* check for playlists from the command-line */
	for (count=0;;count++)
	{
		char buffer[32];
		const char *filename;
		uint32_t dirdb_ref;

		sprintf(buffer, "playlist%d", count);
		if (!(filename = configAPI->GetProfileString2(sec, "CommandLine_Files", buffer, NULL)))
		{
			break;
		}

#ifdef _WIN32
		dirdb_ref = dirdbResolvePathWithBaseAndRef (dmCurDrive->cwd->dirdb_ref, filename, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_WINDOWS_SLASH, dirdb_use_pfilesel);
#else
		dirdb_ref = dirdbResolvePathWithBaseAndRef (dmCurDrive->cwd->dirdb_ref, filename, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_TILDE_USER, dirdb_use_pfilesel);
#endif

		if (dirdb_ref != DIRDB_NOPARENT)
		{
			struct ocpfile_t *file = 0;
			filesystem_resolve_dirdb_file (dirdb_ref, 0, &file);
			dirdbUnref(dirdb_ref, dirdb_use_pfilesel); dirdb_ref = DIRDB_NOPARENT;

			if (file)
			{
				struct ocpdir_t *dir = 0;
				const char *childpath;
				char *curext;

				dirdbGetName_internalstr (file->dirdb_ref, &childpath);
				getext_malloc (childpath, &curext);
				if (curext)
				{
					dir = m3u_check (0, file, curext);
					if (!dir)
					{
						dir = pls_check (0, file, curext);
					}
					free (curext); curext = 0;
					if (dir)
					{
						if (!(fsReadDir (playlist, dir, curmask, RD_PUTRSUBS)))
						{
							// ignore errors
						}
						dir->unref (dir); dir = 0;
					}
					file->unref (file); file = 0;
				}
			}
		}
	}

	/* change dir, if a path= is given in [fileselector], we default to . */
	currentpath2 = configAPI->GetProfileString2(sec, "fileselector", "path", ".");
	if (strlen (currentpath2) && strcmp (currentpath2, "."))
	{
		uint32_t newcurrentpath = DIRDB_CLEAR;
		struct ocpdir_t *cwd = 0;

		struct dmDrive *dmNewDrive=0;
#ifdef _WIN32
		int driveindex = toupper(currentpath2[0]) - 'A';
		if ((driveindex >= 0) && (driveindex < 26) && dmDriveLetters[driveindex])
		{
			newcurrentpath = dirdbResolvePathWithBaseAndRef(dmDriveLetters[driveindex]->cwd->dirdb_ref, currentpath2, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH, dirdb_use_pfilesel);
		}
#else
		newcurrentpath = dirdbResolvePathWithBaseAndRef(dmFile->cwd->dirdb_ref, currentpath2, DIRDB_RESOLVE_DRIVE, dirdb_use_pfilesel);
#endif

		if (newcurrentpath != DIRDB_CLEAR)
		{
			if (!filesystem_resolve_dirdb_dir (newcurrentpath, &dmNewDrive, &cwd))
			{
				dmCurDrive = dmNewDrive;
				assert (dmCurDrive->cwd);
				dmCurDrive->cwd->unref (dmCurDrive->cwd);
				dmCurDrive->cwd = cwd;
#ifdef _WIN32
				if (dmCurDrive->drivename[1] == ':')
				{
					dmLastActiveDriveLetter = dmCurDrive->drivename[0];
				}
#endif
			}

			dirdbUnref (newcurrentpath, dirdb_use_pfilesel);
		}
	}

	return 1;
}

static struct modlistentry *nextplay=NULL;
typedef enum {NextPlayNone, NextPlayBrowser, NextPlayPlaylist} NextPlay;
static NextPlay isnextplay = NextPlayNone;
/* These guys has with rendering todo and stuff like that */
static unsigned short dirwinheight;
static char quickfind[24*6+1];
static char quickfindlen;
static short editfilepos=0;
static short editdirpos=0;
static short editmode=0;
static unsigned int scanposf, scanposp;
static int win = 0;

int fsListScramble=1;
int fsListRemove=1;
int fsLoopMods=1;
int fsScanNames=1;
int fsScanArcs=0;
int fsScanInArc=1;
int fsScrType=0;
int fsEditWin=1;
int fsColorTypes=1;
int fsInfoMode=0;
int fsPutArcs=1;
int fsWriteModInfo=1;
int fsShowAllFiles=0;
static int fsPlaylistOnly=0;

int fsFilesLeft(void)
{
	return (isnextplay!=NextPlayNone)||playlist->num;
}

/* returns zero if fsReadDir() fails */
/* op = 0, move cursor to the top
 * op = 1, maintain current cursor position
 */
static char fsScanDir (int op)
{
	uint32_t dirdb_ref = DIRDB_CLEAR;
	int pos;

	 /* if we are to maintain the old position, store both the dirdb reference and the position as fall-back */
	if (op == 1)
	{
		pos = currentdir->pos;
		if (currentdir->pos < currentdir->num)
		{
			if (currentdir->files[currentdir->sortindex[pos]].file)
			{
				dirdb_ref = currentdir->files[currentdir->sortindex[pos]].file->dirdb_ref;
			} else if (currentdir->files[currentdir->sortindex[pos]].dir)
			{
				dirdb_ref = currentdir->files[currentdir->sortindex[pos]].dir->dirdb_ref;
			}
		}
		if (dirdb_ref != DIRDB_CLEAR)
		{
			dirdbRef (dirdb_ref, dirdb_use_pfilesel);
		}
	}

	modlist_clear (currentdir);
	nextplay=0;

#ifdef _WIN32
	filesystem_windows_refresh_drives();
#endif

	if (!fsReadDir (currentdir, dmCurDrive->cwd, curmask, RD_PUTDRIVES | RD_PUTSUBS | (fsScanArcs?RD_ARCSCAN:0)))
	{
		if (dirdb_ref != DIRDB_CLEAR)
		{
			dirdbUnref (dirdb_ref, dirdb_use_pfilesel);
		}
		return 0;
	}

	modlist_sort(currentdir);

	/* if we are to maintain position, attempt one is to find the previous dirdb reference */
	if (op == 1)
	{
		int newpos = modlist_find (currentdir, dirdb_ref);
		/* if unable to find, fallback to the same position */
		if (newpos < 0)
		{
			newpos = pos;
			/* if position is out of range, adjust it */
			if (newpos >= currentdir->num)
			{
				newpos = currentdir->num ? currentdir->num - 1 : 0;
			}
		}
		currentdir->pos = newpos;
	} else {
		currentdir->pos = 0;
	}

	quickfind[0] = 0;
	quickfindlen = 0;
	scanposf=fsScanNames?0:~0;

	adbMetaCommit ();

	if (dirdb_ref != DIRDB_CLEAR)
	{
		dirdbUnref (dirdb_ref, dirdb_use_pfilesel);
	}

	return 1;
}

int fsGetPrevFile (struct moduleinfostruct *info, struct ocpfilehandle_t **filehandle)
{
	struct modlistentry *m;
	int retval=0;
	int pick;

	*filehandle = 0;

	switch (isnextplay)
	{
		default:
			return fsGetNextFile (info, filehandle);
		case NextPlayNone:
			if (!playlist->num)
			{
				fprintf(stderr, "BUG in pfilesel.c: fsGetNextFile() invalid NextPlayPlaylist #2\n");
				return retval;
			}
			if (fsListScramble)
				return fsGetNextFile (info, filehandle);
			if (playlist->pos)
				playlist->pos--;
			else
				playlist->pos = playlist->num - 1;
			if (playlist->pos)
				pick = playlist->pos-1;
			else
				pick = playlist->num - 1;
			m=modlist_get (playlist, pick);
			break;
	}

	mdbGetModuleInfo (info, m->mdb_ref);

	if (!(info->flags&MDB_VIRTUAL))
	{
		if (m->file)
		{
			struct ocpfilehandle_t *ancient;
			*filehandle = m->file->open (m->file);
			if ((ancient = ancient_filehandle (0, 0, *filehandle)))
			{
				(*filehandle)->unref (*filehandle);
				*filehandle = ancient;
			}
		}

		if (*filehandle)
		{
			if (!mdbInfoIsAvailable(m->mdb_ref)&&*filehandle)
			{
				m->flags |= MODLIST_FLAG_SCANNED;

				mdbReadInfo(info, *filehandle); /* detect info... */
				(*filehandle)->seek_set (*filehandle, 0);
				mdbWriteModuleInfo(m->mdb_ref, info);
				mdbGetModuleInfo(info, m->mdb_ref);
			}

			retval = 1;
		}
	} else {
		retval = 1;
	}

//errorout:
	if (fsListRemove)
	{
		modlist_remove(playlist, pick);
	}
	return retval;
}

int fsGetNextFile (struct moduleinfostruct *info, struct ocpfilehandle_t **filehandle)
{
	struct modlistentry *m;
	unsigned int pick=0;
	int retval=0;

	*filehandle = 0;

	switch (isnextplay)
	{
		case NextPlayBrowser:
			m=nextplay;
			break;
		case NextPlayPlaylist:
			if (!playlist->num)
			{
				fprintf(stderr, "BUG in pfilesel.c: fsGetNextFile() invalid NextPlayPlaylist #1\n");
				return retval;
			}
			pick = playlist->pos;
			m = modlist_get (playlist, pick);
			break;
		case NextPlayNone:
			if (!playlist->num)
			{
				fprintf(stderr, "BUG in pfilesel.c: fsGetNextFile() invalid NextPlayPlaylist #2\n");
				return retval;
			}
			if (fsListScramble)
				pick = rand() % playlist->num;
			else
				pick = playlist->pos;
			m=modlist_get (playlist, pick);
			break;
		default:
			fprintf(stderr, "BUG in pfilesel.c: fsGetNextFile() Invalid isnextplay\n");
			return retval;
	}

	mdbGetModuleInfo(info, m->mdb_ref);

	if (m->file)
	{
		struct ocpfilehandle_t *ancient;
		*filehandle = m->file->open (m->file);
		if ((ancient = ancient_filehandle (0, 0, *filehandle)))
		{
			(*filehandle)->unref (*filehandle);
			*filehandle = ancient;
		}
	}

	if (*filehandle)
	{
		if (!mdbInfoIsAvailable(m->mdb_ref)&&*filehandle)
		{
			mdbReadInfo(info, *filehandle); /* detect info... */
			(*filehandle)->seek_set (*filehandle, 0);
			mdbWriteModuleInfo(m->mdb_ref, info);
			mdbGetModuleInfo(info, m->mdb_ref);
		}

		retval = 1;
	}

//errorout:
	switch (isnextplay)
	{
		case NextPlayBrowser:
			isnextplay = NextPlayNone;
			break;
		case NextPlayPlaylist:
			isnextplay = NextPlayNone;
			/* and then run the same as bellow :-p */
		case NextPlayNone:
			if (fsListRemove)
			{
				modlist_remove(playlist, pick);
			} else {
				if (!fsListScramble)
					if ( (pick=playlist->pos+1)>=playlist->num)
						pick=0;
				playlist->pos=pick;
			}
	}
	return retval;
}

void fsForceRemove(const uint32_t dirdbref)
{
	modlist_remove_all_by_path(playlist, dirdbref);
}

static const char *DEVv_description[] =
{
	"Virtual files used for Open Cubic Player to change audio device",
	NULL
};

static const char *UNKN_description[] =
{
	"The format of the given file is unknown",
	NULL
};

int fsPreInit (const struct configAPI_t *configAPI)
{
	const char *sec = configAPI->GetProfileString (configAPI->ConfigSec, "fileselsec", "fileselector");

	struct moduletype mt;

	curmask = strdup ("*");

	adbMetaInit (configAPI); /* archive database cache - ignore failures */

	if (!mdbInit (configAPI))
	{
		fprintf (stderr, "mdb failed to initialize\n");
		return 0;
	}

	if (!dirdbInit (configAPI))
	{
		fprintf (stderr, "dirdb failed to initialize\n");
		return 0;
	}

	mt.integer.i = mtUnknown;
	fsTypeRegister (mt, UNKN_description, 0, 0);

	fsRegisterExt("DEV");
	mt.integer.i = MODULETYPE("DEVv");
	fsTypeRegister (mt, DEVv_description, "VirtualInterface", 0);

	fsScrType      =  configAPI->GetProfileInt2   (configAPI->ScreenSec, "screen",        "screentype",   7, 10);
	if ((fsScrType < 0) || (fsScrType > 8)) fsScrType = 8;
	fsColorTypes   =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "typecolors",   1, 1);
	fsEditWin      =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "editwin",      1, 1);
	fsWriteModInfo =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "writeinfo",    1, 1);
	fsScanInArc    =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "scaninarcs",   1, 1);
	fsScanNames    =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "scanmodinfo",  1, 1);
	fsScanArcs     =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "scanarchives", 1, 1);
	fsListRemove   =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "playonce",     1, 1);
	fsListScramble =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "randomplay",   1, 1);
	fsPutArcs      =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "putarchives",  1, 1);
	fsLoopMods     =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "loop",         1, 1);
	fsListRemove   =  configAPI->GetProfileBool   (                      "commandline_f", "r",            fsListRemove, 0);
	fsListScramble = !configAPI->GetProfileBool   (                      "commandline_f", "o",           !fsListScramble, 1);
	fsLoopMods     =  configAPI->GetProfileBool   (                      "commandline_f", "l",            fsLoopMods, 0);
	fsPlaylistOnly =!!configAPI->GetProfileString (                      "commandline",   "p",            0);
	fsShowAllFiles =  configAPI->GetProfileBool2  (sec,                  "fileselector",  "showallfiles", 0, 0);

	filesystem_drive_init ();

	filesystem_bzip2_register ();
	filesystem_gzip_register ();
	filesystem_m3u_register ();
	filesystem_pak_register ();
	filesystem_pls_register ();
	filesystem_setup_register ();
	filesystem_rpg_register ();
	filesystem_tar_register ();
	filesystem_Z_register ();
	filesystem_zip_register ();

#ifdef _WIN32
	dmCurDrive = filesystem_windows_init ();
	if (!dmCurDrive)
	{
		dmCurDrive = dmSetup;
	}
#else
	if (filesystem_unix_init ())
	{
		fprintf (stderr, "Failed to initialize unix filesystem\n");
		return 0;
	}
	dmCurDrive = dmFile;
#endif

	if (!musicbrainz_init (configAPI)) /* needs setup */
	{
		fprintf (stderr, "musicbrainz failed to initialize\n");
		return 0;
	}

	currentdir=modlist_create();
	playlist=modlist_create();

	return 1;
}

void fsLateClose(void)
{
	struct moduletype mt;

	mt.integer.i = mtUnknown;
	fsTypeUnregister (mt);

	mt.integer.i = MODULETYPE("DEVv");
	fsTypeUnregister (mt);
}

int fsLateInit (const struct configAPI_t *configAPI)
{
	const char *sec=configAPI->GetProfileString (configAPI->ConfigSec, "fileselsec", "fileselector");

	if (!initRootDir(configAPI, sec))
		return 0;

	return 1;
}

static struct IOCTL_DevInterface *CurrentVirtualDevice;
static struct ocpfilehandle_t *CurrentVirtualDeviceFile;
static struct DevInterfaceAPI_t DevInterfaceAPI =
{
	&configAPI,
	&dirdbAPI,
	&Console,
	&PipeProcess,
#ifdef _WIN32
	&dmLastActiveDriveLetter,
	dmDriveLetters,
#else
	0, /* dmFile */
#endif
	cpiKeyHelp,
	cpiKeyHelpClear,
	cpiKeyHelpDisplay,
	fsDraw
};

static int VirtualInterfaceInit (struct moduleinfostruct *info, struct ocpfilehandle_t *fi, const struct cpifaceplayerstruct *cp)
{
	CurrentVirtualDevice = 0;
	if (fi->ioctl (fi, IOCTL_DEVINTERFACE, &CurrentVirtualDevice))
	{
		CurrentVirtualDevice = 0;
		return 0;
	}
	if (CurrentVirtualDevice)
	{
		if (!CurrentVirtualDevice->Init (CurrentVirtualDevice, info, &DevInterfaceAPI))
		{
			CurrentVirtualDevice = 0;
			return 0;
		}
		CurrentVirtualDeviceFile = fi;
		CurrentVirtualDeviceFile->ref (CurrentVirtualDeviceFile);
	}
	return 1;
}

static interfaceReturnEnum VirtualInterfaceRun (void)
{
	if (CurrentVirtualDevice)
	{
		return CurrentVirtualDevice->Run (CurrentVirtualDevice, &DevInterfaceAPI);
	}
	return interfaceReturnNextAuto;
}

static void VirtualInterfaceClose (void)
{
	if (CurrentVirtualDevice)
	{
		CurrentVirtualDevice->Close (CurrentVirtualDevice, &DevInterfaceAPI);
		CurrentVirtualDeviceFile->unref (CurrentVirtualDeviceFile);
		CurrentVirtualDeviceFile = 0;
		CurrentVirtualDevice = 0;
	}
}

static struct interfacestruct VirtualInterface = {VirtualInterfaceInit, VirtualInterfaceRun, VirtualInterfaceClose, "VirtualInterface", NULL};

int fsInit(void)
{
#ifndef _WIN32
	DevInterfaceAPI.dmFile = dmFile;
#endif
	plRegisterInterface (&VirtualInterface);

	return 1;
}

void fsClose(void)
{
	if (currentdir)
	{
		modlist_free(currentdir);
		currentdir=NULL;
	}
	if (playlist)
	{
		modlist_free(playlist);
		playlist=NULL;
	}

	musicbrainz_done();

#ifdef _WIN32
	filesystem_windows_done ();
#else
	filesystem_unix_done ();
#endif
	filesystem_drive_done ();
	dmCurDrive = 0;

	adbMetaClose();
	mdbClose();
	if (moduleextensions)
	{
		int i;
		for (i=0;moduleextensions[i];i++)
			free(moduleextensions[i]);
		free(moduleextensions);
		moduleextensions=0;
	}

	dirdbClose();

	free (curmask); curmask = 0;

	plUnregisterInterface (&VirtualInterface);
}

static void displayfile(const unsigned int y, unsigned int x, unsigned int width, struct modlistentry *m, const unsigned char sel)
{
	unsigned char col;
	struct moduleinfostruct mi;

	if (width == 14)
	{
		if (sel==2)
		{
			displaystr (y, x,    0x07, "\x1A", 1);
			displaystr (y, x+13, 0x07, "\x1B", 1);
		} else if (sel==1)
		{
			displaystr (y, x,    0x8f, " ", 1);
			displaystr (y, x+13, 0x8f, " ", 1);
		} else {
			displaystr (y, x,    0x0f, " ", 1);
			displaystr (y, x+13, 0x0f, " ", 1);
		}
		displaystr_utf8 (y, x + 1, (sel==1)?0x8f:0x0f, m->utf8_8_dot_3, 12);
		return;
	}

	if (m->file)
	{
		col=0x07;
		if (m->flags & MODLIST_FLAG_ISMOD)
		{
			mdbGetModuleInfo(&mi, m->mdb_ref);
		} else {
			memset (&mi, 0, sizeof (mi));
			mi.size = m->file->filesize (m->file);
		}
	} else {
		memset (&mi, 0, sizeof (mi));
		col=0x0f;
	}
	if (sel==1)
	{
		col|=0x80;
	}

	if (sel==2)
	{
		displaystr (y, x + 0,         0x07, "->", 2);
		displaystr (y, x + width - 2, 0x07, "<-", 2);
	} else {
		displaystr (y, x + 0,          col, "  ", 2);
		displaystr (y, x + width - 2,  col, "  ", 2);
	}
	x += 2;
	width -= 4;

	/* Mode 4 does not care about width it uses all space for filename/dirname + size/dir-type */
	if (fsInfoMode==4)
	{
		if (m->file)
		{
			if (mi.modtype.integer.i == mtUnRead)
				col&=~0x08;
			else if (fsColorTypes)
			{
				col&=0xF8;
				col|=fsModTypeColor(mi.modtype);
			}
		}

		if (m->dir && !strcmp (m->utf8_8_dot_3, ".."))
		{
			displaystr_utf8 (y, x, col, m->utf8_8_dot_3, width - 5);
		} else {
			const char *temp = 0;

			if (m->file)
			{
				dirdbGetName_internalstr (m->file->dirdb_ref, &temp);
			} else if (m->dir)
			{
				dirdbGetName_internalstr (m->dir->dirdb_ref, &temp);
			}
			displaystr_utf8 (y, x, col, temp ? temp : "???", width - 2);
		}
		if (m->dir)
		{
			if (m->flags&MODLIST_FLAG_DRV)
			{
				displaystr (y, x + width - 5, col, "<DRV>", 5);
			} else if (m->dir->is_playlist)
			{
				displaystr (y, x + width - 5, col, "<PLS>", 5);
			} else if (m->dir->is_archive)
			{
				displaystr (y, x + width - 5, col, "<ARC>", 5);
			} else {
				displaystr (y, x + width - 5, col, "<DIR>", 5);
			}
		} else { /* m->file */
			char buffer[22];

			snprintf (buffer, sizeof (buffer), " %"PRIu64, mi.size);
			displaystr (y, x + width - strlen(buffer), (mi.flags&MDB_BIGMODULE)?((col&0xF0)|0x0C):col, buffer, strlen (buffer));
		}
		return;
	} else {
		if (width > 84)
		{
			displaystr_utf8 (y, x, col, m->utf8_16_dot_3, 22); /* two extra spaces at the end */
			x += 22;
			width -= 22;
		} else {
			displaystr_utf8 (y, x, col, m->utf8_8_dot_3, 14); /* two extra spaces at the end */
			x += 14;
			width -= 14;
		}

		if (m->dir)
		{
			if (m->flags&MODLIST_FLAG_DRV)
			{
				displaystr (y, x, col, "<DRV>", width);
			} else if (m->dir->is_playlist)
			{
				displaystr (y, x, col, "<PLS>", width);
			} else if (m->dir->is_archive)
			{
				displaystr (y, x, col, "<ARC>", width);
			} else {
				displaystr (y, x, col, "<DIR>", width);
			}
		} else { /* m->file */
			char temp[16];
			if (mi.modtype.integer.i == mtUnRead)
			{
				col&=~0x08;
			} else if (fsColorTypes)
			{
				col&=0xF8;
				col|=fsModTypeColor(mi.modtype);
			}

			if (width>=79) /* implies 120 or bigger screens */
			{
				if (fsInfoMode&1)
				{
					int w1 = (width - 1) / 2;
					displaystr_utf8 (y, x +  0, col, mi.comment, w1);
					displaystr      (y, x + w1, col, "", 1);
					displaystr_utf8 (y, x + w1 + 1, col, mi.style, width - w1 - 1);
				} else { /* 32 + PAD2 + 2 + PAD1 + 6 + PAD2 + 32 + PAD1 + 11 + PAD2 + 9 => 100         + 16 prefix + 2 suffix  => 118 GRAND TOTAL WIDTH */
					int w1 = (width - 4 - 8 - 11 - 11) / 2;
					displaystr_utf8 (y, x +  0, col, mi.title, w1);
					if (mi.channels)
					{
						snprintf (temp, sizeof (temp), " %2d ", mi.channels);
						displaystr (y, x + w1, col, temp, 4); // include padding before and after
					} else {
						displaystr (y, x + w1, col, "", 4);
					}
				        if (mi.playtime)
				        {
						snprintf (temp, sizeof (temp), "%3d:%02d", mi.playtime / 60, mi.playtime % 60);
						displaystr (y, x + w1 + 4, col, temp, 8); // include two padding
					} else {
						displaystr (y, x + w1 + 4, col, "", 8);
					}
					displaystr_utf8 (y, x + w1 + 4 + 8, col, mi.composer, width - w1 - 4 - 8 - 11 - 11);
					if (mi.date)
					{
						if (mi.date&0xFF)
						{
							snprintf (temp, sizeof (temp), " %02d.", mi.date & 0xff);
						} else {
							snprintf (temp, sizeof (temp), "    ");
						}
						if (mi.date&0xFFFF)
						{
							snprintf (temp + 4, sizeof (temp) - 4, "%02d.", (mi.date >> 8)&0xff);
						} else {
							snprintf (temp + 4, sizeof (temp) - 4, "   ");
						}
						if (mi.date>>16)
						{
							snprintf (temp + 7, sizeof (temp) - 7, "%4d", (mi.date >> 16));
							if ((mi.date>>16) <= 100)
							{
								temp[8] = '\'';
							}
						}
						displaystr (y, x + width - 11 - 11, col, temp, 11); // include one padding
					} else {
						displaystr (y, x + width - 11 - 11, col, "", 11); // include one padding
					}
					if (mi.size<10000000000ll)
					{
						snprintf (temp, sizeof (temp), "%11"PRIu64, mi.size);
					} else {
						snprintf (temp, sizeof (temp), "x%10"PRIx64, saturate(mi.size, 0, (uint64_t)0xffffffffffll));
					}
					displaystr (y, x + width - 11, (mi.flags&MDB_BIGMODULE)?((col&0xF0)|0x0C):col, temp, 11);
				}
			} else switch (fsInfoMode)
			{
				case 0:
					displaystr_utf8 (y, x +  0, col, mi.title, width - 4 - 11);
					if (mi.channels)
					{
						snprintf (temp, sizeof (temp), " %2d ", mi.channels);
						displaystr (y, x + width - 4 - 11, col, temp, 4); // include padding before and after
					} else {
						displaystr (y, x + width - 4 - 11, col, "", 4); // include one padding
					}

					if (mi.size<100000000000ll)
					{
						snprintf (temp, sizeof (temp), "%11"PRIu64, mi.size);
					} else {
						snprintf (temp, sizeof (temp), "x%10"PRIx64, saturate(mi.size, 0, (uint64_t)0xffffffffffffll));
					}
					displaystr (y, x + width - 11, (mi.flags&MDB_BIGMODULE)?((col&0xF0)|0x0C):col, temp, 11);
					break;
				case 1:
					displaystr_utf8 (y, x +  0, col, mi.composer, width - 11);

					if (mi.date)
					{
						if (mi.date&0xFF)
						{
							snprintf (temp, sizeof (temp), " %02d.", mi.date & 0xff);
						} else {
							snprintf (temp, sizeof (temp), "    ");
						}
						if (mi.date&0xFFFF)
						{
							snprintf (temp + 4, sizeof (temp) - 4, "%02d.", (mi.date >> 8)&0xff);
						} else {
							snprintf (temp + 4, sizeof (temp) - 4, "   ");
						}
						if (mi.date>>16)
						{
							snprintf (temp + 7, sizeof (temp) - 7, "%4d", (mi.date >> 16));
							if ((mi.date>>16) <= 100)
							{
								temp[8] = '\'';
							}
						}
						displaystr (y, x + width - 11, col, temp, 11);
					} else {
						displaystr (y, x + width - 11, col, "", 11);
					}

					break;
				case 2:
					displaystr_utf8 (y, x + 0, col, mi.comment, width);
					break;
				case 3:
					displaystr_utf8 (y, x + 0, col, mi.style, width - 7);
					if (mi.playtime)
					{
						snprintf (temp, sizeof (temp), " %3d:%02d", mi.playtime / 60, mi.playtime % 60);
						displaystr (y, x + width - 7, col, temp, 7);
					} else {
						displaystr (y, x + width - 7, col, "", 7);
					}
					break;
			}
		}
	}
}

static void fsShowDirBottom80File (int Y, int selecte, const struct modlistentry *mle, struct moduleinfostruct *mi, const char *npath)
{
	const int leftwidth = (plScrWidth - 10 - 9 - 1) / 2;
	const int rightwidth = plScrWidth - 10 - 9 - 1 - leftwidth;

	displayvoid (Y + 0, 0, 2);
	if (mle && mle->utf8_8_dot_3[0])
	{
		displaystr_utf8 (Y + 0, 2, 0x0F, mle->utf8_8_dot_3, 12);
	} else {
		displaystr (Y + 0, 2, 0x07, "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa", 12);
	}
	displayvoid (Y + 0, 14, 1);
	if (mle && mle->file)
	{
		char temp[13];
		snprintf (temp, sizeof (temp), "  %" PRIu64 "%c", mi->size, (mi->flags&MDB_BIGMODULE) ? '!' : ' ');
		displaystr (Y + 0, 15, 0x0f, temp, 12);
	} else {
		displaystr (Y + 0, 15, 0x07, " \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 12);
	}
	displaystr (Y + 0, 27, 0x07, " title: ", 8);
	if (mi->title[0])
	{
		displaystr_utf8 (Y + 0, 35, (selecte==0)?0x8f:0x0f, mi->title, plScrWidth - 35 - 13);
	} else {
		displaychr (Y + 0, 35, (selecte==0)?0x87:0x07, '\xfa', plScrWidth - 35 - 13);
	}
	displaystr (Y + 0, plScrWidth - 13, 0x07, "  type: ", 8);
	if (mi->modtype.string.c[0])
	{
		displaystr (Y + 0, plScrWidth - 5, (selecte==1)?0x8f:0x0f, mi->modtype.string.c, 4);
	} else {
		displaystr (Y + 0, plScrWidth - 5, (selecte==1)?0x87:0x07, "\xfa\xfa\xfa\xfa", 4);
	}
	displayvoid (Y + 0, plScrWidth - 1, 1);


	displaystr (Y + 1, 0, 0x07, "composer: ", 10);
	if (mi->composer[0])
	{
		displaystr_utf8 (Y + 1, 10, (selecte==4)?0x8f:0x0f, mi->composer, plScrWidth - 10 - 19);
	} else {
		displaychr (Y + 1, 10, (selecte==4)?0x87:0x07, '\xfa', plScrWidth - 10 - 19);
	}


	displaystr (Y + 1, plScrWidth - 19, 0x07, "  date: ", 8);
	if (mi->date)
	{
		char temp[11];

		if (mi->date & 0xff)
		{
			snprintf (temp, 3, "%02d", saturate(mi->date & 0xff, 0, 99));
		} else {
			temp[0] = '.';
			temp[1] = '.';
		}
		temp[2] = '.';
		if (mi->date & 0xffff)
		{
			snprintf (temp+3, 3, "%02d", saturate((mi->date >> 8) & 0xff, 0, 99));
		} else {
			temp[3] = '.';
			temp[4] = '.';
		}
		temp[5] = '.';
		if (mi->date >> 16)
		{
			snprintf (temp+6, 5, "%4d", saturate(mi->date >> 16, 0, 9999));
			if ((mi->date>>16) <= 100)
			{
				temp[7] = '\'';
			}
		} else {
			temp[6] = '.';
			temp[7] = '.';
			temp[8] = '.';
			temp[9] = '.';
			temp[10] = 0;
		}
		displaystr (Y + 1, plScrWidth - 11, (selecte==6)?0x8f:0x0f, temp, 10);
	} else {
		displaystr (Y + 1, plScrWidth - 11, (selecte==6)?0x87:0x07, "\xfa\xfa.\xfa\xfa.\xfa\xfa\xfa\xfa", 10);
	}
	displayvoid (Y + 1, plScrWidth - 1, 1);

	displaystr (Y + 2, 0, 0x07, "  artist: ", 10);
	if (mi->artist[0])
	{
		displaystr_utf8 (Y + 2, 10, (selecte==8)?0x8f:0x0f, mi->artist, leftwidth);
	} else {
		displaychr (Y + 2, 10, (selecte==8)?0x87:0x07, '\xfa', leftwidth);
	}
	displaystr (Y + 2, 10 + leftwidth, 0x07, "  album: ", 9);
	if (mi->album[0])
	{
		displaystr_utf8 (Y + 2, 10 + leftwidth + 9, (selecte==9)?0x8f:0x0f, mi->album, rightwidth);
	} else {
		displaychr (Y + 2, 10 + leftwidth + 9, (selecte==9)?0x87:0x07, '\xfa', rightwidth);
	}
	displayvoid (Y + 2, plScrWidth - 1, 1);

	displaystr (Y + 3, 0, 0x07, "   style: ", 10);
	if (mi->style[0])
	{
		displaystr_utf8 (Y + 3, 10, (selecte==5)?0x8f:0x0f, mi->style, plScrWidth - 10 - 33);
	} else {
		displaychr (Y + 3, 10, (selecte==5)?0x87:0x07, '\xfa', plScrWidth - 10 - 33);
	}
	displaystr (Y + 3, plScrWidth - 33, 0x07, "  playtime: ", 12);
	if (mi->playtime)
	{
		char temp[7];
		snprintf (temp, sizeof (temp), "%3d:%02d", saturate(mi->playtime / 60, 0, 999), mi->playtime % 60);
		displaystr (Y + 3, plScrWidth - 21, (selecte==3)?0x8f:0x0f, temp, 6);
	} else {
		displaystr (Y + 3, plScrWidth - 21, (selecte==3)?0x87:0x07, "\xfa\xfa\xfa:\xfa\xfa", 6);
	}
	displaystr (Y + 3, plScrWidth - 15, 0x07, "  channels: ", 12);
	if (mi->channels)
	{
		char temp[3];
		snprintf (temp, sizeof (temp), "%2d", saturate(mi->channels, 0, 99));
		displaystr (Y + 3, plScrWidth - 3, (selecte==2)?0x8f:0x0f, temp, 2);
	} else {
		displaystr (Y + 3, plScrWidth - 3, (selecte==2)?0x87:0x07, "\xfa\xfa", 2);
	}
	displayvoid (Y + 3, plScrWidth - 1, 1);


	displaystr (Y + 4, 0, 0x07, " comment: ", 10);
	if (mi->comment[0])
	{
		displaystr_utf8 (Y + 4, 10, (selecte==7)?0x8f:0x0f, mi->comment, plScrWidth - 11);
	} else {
		displaychr (Y + 4, 10, (selecte==7)?0x87:0x07, '\xfa', plScrWidth - 11);
	}
	displayvoid (Y + 4, plScrWidth - 1, 1);

#if 0
	displaystr (Y + 4, 0, 0x07, "   long: ", 10);
	displaystr_utf8_overflowleft (Y + 4, 10, 0x0F, npath ? npath : "", plScrWidth - 10);
#endif
}

static void fsShowDirBottom132File (int Y, int selecte, const struct modlistentry *mle, struct moduleinfostruct *mi, const char *npath)
{
	const int leftwidth = (plScrWidth - 37 - 10 - 35) / 2;
	const int rightwidth = plScrWidth - 37 - 10 - 35 - leftwidth;

	displayvoid (Y + 0, 0, 2);
	if (mle && mle->utf8_8_dot_3[0])
	{
		displaystr_utf8 (Y + 0, 2, 0x0F, mle->utf8_8_dot_3, 12);
	} else {
		displaystr (Y + 0, 2, 0x07, "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa", 12);
	}
	displayvoid (Y + 0, 14, 2);
	if (mle && mle->file)
	{
		char temp[13];
		snprintf (temp, sizeof (temp), "  %" PRIu64 "%c", mi->size, (mi->flags&MDB_BIGMODULE) ? '!' : ' ');
		displaystr (Y + 0, 16, 0x0f, temp, 12);
	} else {
		displaystr (Y + 0, 16, 0x07, " \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 12);
	}

	displaystr (Y + 0, 28, 0x07, "  title: ", 9);
	if (mi->title[0])
	{
		displaystr_utf8 (Y + 0, 37, (selecte==0)?0x8f:0x0f, mi->title, leftwidth);
	} else {
		displaychr (Y + 0, 37, (selecte==0)?0x87:0x07, '\xfa', leftwidth);
	}

	displaystr (Y + 0, 37 + leftwidth, 0x07, "  artist: ", 10);
	if (mi->artist[0])
	{
		displaystr_utf8 (Y + 0, 47 + leftwidth, (selecte==8)?0x8f:0x0f, mi->artist, rightwidth);
	} else {
		displaychr (Y + 0, 47 + leftwidth, (selecte==8)?0x87:0x07, '\xfa', rightwidth);
	}

	displaystr (Y + 0, plScrWidth - 35, 0x07, "  channels: ", 12);
	if (mi->channels)
	{
		char temp[3];
		snprintf (temp, sizeof (temp), "%2d", saturate(mi->channels, 0, 99));
		displaystr (Y + 0, plScrWidth - 35 + 12, (selecte==2)?0x8f:0x0f, temp, 2);
	} else {
		displaystr (Y + 0, plScrWidth - 35 + 12, (selecte==2)?0x87:0x07, "\xfa\xfa", 2);
	}

	displaystr (Y + 0, plScrWidth - 21, 0x07, "  playtime: ", 12);
	if (mi->playtime)
	{
		char temp[7];
		snprintf (temp, sizeof (temp), "%3d:%02d", saturate(mi->playtime / 60, 0, 999), mi->playtime % 60);
		displaystr (Y + 0, plScrWidth - 9, (selecte==3)?0x8f:0x0f, temp, 6);
	} else {
		displaystr (Y + 0, plScrWidth - 9, (selecte==3)?0x87:0x07, "\xfa\xfa\xfa:\xfa\xfa", 6);
	}
	displayvoid (Y + 0, plScrWidth - 3, 3);

	displaystr (Y + 1, 0, 0x07, "                           composer: ", 37);
	if (mi->composer[0])
	{
		displaystr_utf8 (Y + 1, 37, (selecte==4)?0x8f:0x0f, mi->composer, leftwidth);
	} else {
		displaychr (Y + 1, 37, (selecte==4)?0x8f:0x0f, '\xfa', leftwidth);
	}

	displaystr (Y + 1, 37 + leftwidth, 0x07, "   album: ", 10);
	if (mi->album[0])
	{
		displaystr_utf8 (Y + 1, 47 + leftwidth, (selecte==9)?0x8f:0x0f, mi->album, rightwidth);
	} else {
		displaychr (Y + 1, 47 + leftwidth, (selecte==9)?0x8f:0x0f, '\xfa', rightwidth);
	}

	displaystr (Y + 1, plScrWidth - 35, 0x07, "     type: ", 11);
	if (mi->modtype.string.c[0])
	{
		displaystr (Y + 1, plScrWidth - 35 + 11, (selecte==1)?0x8f:0x0f, mi->modtype.string.c, 4);
	} else {
		displaystr (Y + 1, plScrWidth - 35 + 11, (selecte==1)?0x87:0x07, "\xfa\xfa\xfa\xfa", 4);
	}

	displaystr(Y + 1, plScrWidth - 20, 0x07, " date: ", 7);
	if (mi->date)
	{
		char temp[11];

		if (mi->date & 0xff)
		{
			snprintf (temp, 3, "%02d", saturate(mi->date & 0xff, 0, 99));
		} else {
			temp[0] = '.';
			temp[1] = '.';
		}
		temp[2] = '.';
		if (mi->date & 0xffff)
		{
			snprintf (temp+3, 3, "%02d", saturate((mi->date >> 8) & 0xff, 0, 99));
		} else {
			temp[3] = '.';
			temp[4] = '.';
		}
		temp[5] = '.';
		if (mi->date >> 16)
		{
			snprintf (temp+6, 5, "%4d", saturate(mi->date >> 16, 0, 9999));
			if ((mi->date>>16) <= 100)
			{
				temp[7] = '\'';
			}
		} else {
			temp[6] = '.';
			temp[7] = '.';
			temp[8] = '.';
			temp[9] = '.';
			temp[10] = 0;
		}
		displaystr (Y + 1, plScrWidth - 21 + 8, (selecte==6)?0x8f:0x0f, temp, 10);
	} else {
		displaystr (Y + 1, plScrWidth - 21 + 8, (selecte==6)?0x87:0x07, "\xfa\xfa.\xfa\xfa.\xfa\xfa\xfa\xfa", 10);
	}
	displayvoid (Y + 1, plScrWidth - 3, 3);

	displaystr (Y + 2, 0, 0x07, "                            comment: ", 37);
	if (mi->comment[0])
	{
		displaystr_utf8 (Y + 2, 37, (selecte==7)?0x8f:0x0f, mi->comment, plScrWidth - 37 - 43);
	} else {
		displaychr (Y + 2, 37, (selecte==7)?0x8f:0x0f, '\xfa', plScrWidth - 37 - 43);

	}
	displaystr (Y + 2, plScrWidth - 43, 0x07, "  style: ", 9);
	if (mi->style[0])
	{
		displaystr_utf8 (Y + 2, plScrWidth - 43 + 9, (selecte==5)?0x8f:0x0f, mi->style, 43 - 9 - 3);
	} else {
		displaychr (Y + 2, plScrWidth - 43 + 9, (selecte==5)?0x8f:0x0f, '\xfa', 43 - 9 - 3);
	}
	displayvoid (Y + 2, plScrWidth - 3, 3);

	displaystr (Y + 3, 0, 0x07, "    long: ", 10);

	displaystr_utf8_overflowleft (Y + 3, 10, 0x0F, npath ? npath : "", plScrWidth - 10);
}

static void fsShowDirBottom180File (int Y, int selecte, const struct modlistentry *mle, struct moduleinfostruct *mi, const char *npath)
{
	//"   title: " 10    "  artist: " 10
	//"composer: " 10    "   album: " 10
	const int leftwidth = (plScrWidth - (28 + 55 + 10 + 10)) / 2;
	const int rightwidth = plScrWidth - (28 /* file: abcdef.mod FILESIZE */ + 55 /* style: foobar */+ 10 /* "   title: " */ + 10 /* "  artist: */) - leftwidth;

	displayvoid (Y + 0, 0, 2);
	if (mle && mle->utf8_8_dot_3[0])
	{
		displaystr_utf8 (Y + 0, 2, 0x0F, mle->utf8_8_dot_3, 12);
	} else {
		displaystr (Y + 0, 2, 0x07, "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa", 12);
	}
	displayvoid (Y + 0, 14, 2);
	if (mle && mle->file)
	{
		char temp[13];
		snprintf (temp, sizeof (temp), "  %" PRIu64 "%c", mi->size, (mi->flags&MDB_BIGMODULE) ? '!' : ' ');
		displaystr (Y + 0, 16, 0x0f, temp, 12);
	} else {
		displaystr (Y + 0, 16, 0x07, " \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 12);
	}

	displaystr (Y + 0, 28, 0x07, "   title: ", 10);
	if (mi->title[0])
	{
		displaystr_utf8 (Y + 0, 38, (selecte==0)?0x8f:0x0f, mi->title, leftwidth);
	} else {
		displaychr (Y + 0, 38, (selecte==0)?0x87:0x07, '\xfa', leftwidth);
	}
	displaystr (Y + 0, 28 + 10 + leftwidth, 0x07, "  artist: ", 10);
	if (mi->artist[0])
	{
		displaystr_utf8 (Y + 0, 28 + 10 + leftwidth + 10, (selecte==8)?0x8f:0x0f, mi->artist, rightwidth);
	} else {
		displaychr (Y + 0, 28 + 10 + leftwidth + 10, (selecte==8)?0x87:0x07, '\xfa', rightwidth);
	}

	displaystr (Y + 0, plScrWidth - 55, 0x07, "   type: ", 9);
	if (mi->modtype.string.c[0])
	{
		displaystr (Y + 0, plScrWidth - 46, (selecte==1)?0x8f:0x0f, mi->modtype.string.c, 4);
	} else {
		displaystr (Y + 0, plScrWidth - 46, (selecte==1)?0x87:0x07, "\xfa\xfa\xfa\xfa", 4);
	}
	displaystr (Y + 0, plScrWidth - 42, 0x07, "     channels: ", 15);
	if (mi->channels)
	{
		char temp[3];
		snprintf (temp, sizeof (temp), "%2d", saturate(mi->channels, 0, 99));
		displaystr (Y + 0, plScrWidth - 27, (selecte==2)?0x8f:0x0f, temp, 2);
	} else {
		displaystr (Y + 0, plScrWidth - 27, (selecte==2)?0x87:0x07, "\xfa\xfa", 2);
	}
	displaystr (Y + 0, plScrWidth - 25, 0x07, "      playtime: ", 16);
	if (mi->playtime)
	{
		char temp[7];
		snprintf (temp, sizeof (temp), "%3d:%02d", saturate(mi->playtime / 60, 0, 999), mi->playtime % 60);
		displaystr (Y + 0, plScrWidth - 9, (selecte==3)?0x8f:0x0f, temp, 6);
	} else {
		displaystr (Y + 0, plScrWidth - 9, (selecte==3)?0x87:0x07, "\xfa\xfa\xfa:\xfa\xfa", 6);
	}
	displayvoid (Y + 0, plScrWidth - 3, 3);


	displaystr (Y + 1, 0, 0x07, "                            composer: ", 38);
	if (mi->composer[0])
	{
		displaystr_utf8 (Y + 1, 38, (selecte==4)?0x8f:0x0f, mi->composer, leftwidth);
	} else {
		displaychr (Y + 1, 38, (selecte==4)?0x8f:0x0f, '\xfa', leftwidth);
	}
	displaystr (Y + 1, 38 + leftwidth, 0x07, "   album: ", 10);
	if (mi->album[0])
	{
		displaystr_utf8 (Y + 1, 28 + 10 + leftwidth + 10, (selecte==9)?0x8f:0x0f, mi->album, rightwidth);
	} else {
		displaychr (Y + 1, 28 + 10 + leftwidth + 10, (selecte==9)?0x87:0x07, '\xfa', rightwidth);
	}

	displaystr (Y + 1, plScrWidth - 55, 0x07, "  style: ", 9);
	if (mi->style[0])
	{
		displaystr_utf8 (Y + 1, plScrWidth - 46, (selecte==5)?0x8f:0x0f, mi->style, 43);
	} else {
		displaychr (Y + 1, plScrWidth - 46, (selecte==5)?0x8f:0x0f, '\xfa', 43);
	}
	displayvoid (Y + 1, plScrWidth - 3, 3);

	displaystr(Y + 2, 0, 0x07, "                                date: ", 38);
	if (mi->date)
	{
		char temp[11];

		if (mi->date & 0xff)
		{
			snprintf (temp, 3, "%02d", saturate(mi->date & 0xff, 0, 99));
		} else {
			temp[0] = '.';
			temp[1] = '.';
		}
		temp[2] = '.';
		if (mi->date & 0xffff)
		{
			snprintf (temp+3, 3, "%02d", saturate((mi->date >> 8) & 0xff, 0, 99));
		} else {
			temp[3] = '.';
			temp[4] = '.';
		}
		temp[5] = '.';
		if (mi->date >> 16)
		{
			snprintf (temp+6, 5, "%4d", saturate(mi->date >> 16, 0, 9999));
			if ((mi->date>>16) <= 100)
			{
				temp[7] = '\'';
			}
		} else {
			temp[6] = '.';
			temp[7] = '.';
			temp[8] = '.';
			temp[9] = '.';
			temp[10] = 0;
		}
		displaystr (Y + 2, 38, (selecte==6)?0x8f:0x0f, temp, 10);
	} else {
		displaystr (Y + 2, 38, (selecte==6)?0x87:0x07, "\xfa\xfa.\xfa\xfa.\xfa\xfa\xfa\xfa", 10);
	}
	displaystr (Y + 2, 48, 0x07, "       comment: ", 16);
	if (mi->comment[0])
	{
		displaystr_utf8 (Y + 2, 64, (selecte==7)?0x8f:0x0f, mi->comment, plScrWidth - 67);
	} else {
		displaychr (Y + 2, 64, (selecte==7)?0x8f:0x0f, '\xfa', plScrWidth - 67);

	}
	displayvoid (Y + 2, plScrWidth - 3, 3);

	displaystr (Y + 3, 0, 0x07, "    long: ", 10);

	displaystr_utf8_overflowleft (Y + 3, 10, 0x0F, npath ? npath : "", plScrWidth - 10);
}

static void fsShowDirBottom80Dir (int Y, int selectd, const struct modlistentry *mle, const char *npath)
{
	displayvoid (Y + 0, 0, 2);
	if (mle && mle->utf8_16_dot_3[0])
	{
		displaystr_utf8 (Y + 0, 2, 0x0f, mle->utf8_16_dot_3, plScrWidth - 2);
	} else {
		displaystr_utf8 (Y + 0, 2, 0x07, "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa", plScrWidth - 2);
	}

	if (mle->dir->charset_override_API)
	{
		const char *userstring = 0;
		const char *defaultlabel = 0;
		const char *defaultcharset = 0;
		uint8_t attr = 0x0f;

		displaystr (Y + 1, 0, 0x07, "  charset: ", 11);
		userstring = mle->dir->charset_override_API->get_byuser_string (mle->dir);
		if (!userstring)
		{
			attr = 0x0a;
			mle->dir->charset_override_API->get_default_string (mle->dir, &defaultlabel, &defaultcharset);
		}
		if (selectd == 0)
		{
			attr |= 0x80;
		}
		displaystr (Y + 1, 11, attr, userstring ? userstring : defaultlabel, plScrWidth - 13);
		displaystr (Y + 1, plScrWidth - 2, 0x07, "  ", 2);
	} else {
		displayvoid (Y + 1, 0, plScrWidth);
	}

	displayvoid (Y + 2, 0, plScrWidth);

	displayvoid (Y + 3, 0, plScrWidth);

	displaystr (Y + 4, 0, 0x07, "   long: ", 9);

	displaystr_utf8_overflowleft (Y + 4, 10, 0x0F, npath ? npath : "", plScrWidth - 10);
}

static void fsShowDirBottom132Dir (int Y, int selectd, const struct modlistentry *mle, const char *npath)
{
	displayvoid (Y + 0, 0, 2);
	if (mle && mle->utf8_16_dot_3[0])
	{
		displaystr_utf8 (Y + 0, 2, 0x0f, mle->utf8_16_dot_3, plScrWidth - 2);
	} else {
		displaystr_utf8 (Y + 0, 2, 0x07, "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa", plScrWidth - 2);
	}

	if (mle->dir->charset_override_API)
	{
		const char *userstring = 0;
		const char *defaultlabel = 0;
		const char *defaultcharset = 0;
		uint8_t attr = 0x0f;

		displaystr (Y + 1, 0, 0x07, "  charset: ", 11);
		userstring = mle->dir->charset_override_API->get_byuser_string (mle->dir);
		if (!userstring)
		{
			attr = 0x0a;
			mle->dir->charset_override_API->get_default_string (mle->dir, &defaultlabel, &defaultcharset);
		}
		if (selectd == 0)
		{
			attr |= 0x80;
		}
		displaystr (Y + 1, 11, attr, userstring ? userstring : defaultlabel, plScrWidth - 13);
		displaystr (Y + 1, plScrWidth - 2, 0x07, "  ", 2);
	} else {
		displayvoid (Y + 1, 0, plScrWidth);
	}

	displayvoid (Y + 2, 0, plScrWidth);

	displaystr (Y + 3, 0, 0x07, "    long: ", 10);

	displaystr_utf8_overflowleft (Y + 3, 10, 0x0F, npath ? npath : "", plScrWidth - 10);
}

/* selecte - which field in the file-meta-editor is active
 *   0: title     mi.modname
 *   1: type      mi.modtype
 *   2: channels  mi.channels
 *   3: playtime  mi.playtime
 *   4: composer  mi.composer
 *   5: style     mi.style
 *   6: date      mi.date
 *   7: comment   mi.comment
 */
static void fsShowDir(unsigned int firstv, unsigned int selectv, unsigned int firstp, unsigned int selectp, int selectd, int selecte, const struct modlistentry *mle, int playlistactive)
{
	unsigned int i;

	unsigned int vrelpos= ~0;
	unsigned int prelpos= ~0;

	if (currentdir->num>dirwinheight)
		vrelpos=dirwinheight*currentdir->pos/currentdir->num;
	if (playlist->num>dirwinheight)
		prelpos=dirwinheight*playlist->pos/playlist->num;

	make_title("file selector ][", 0);
	displayvoid(1, 0, plScrWidth);

	if (selectv==(unsigned)~0)
	{
		displaystr(1, 0, 0x0f, "playlist://", plScrWidth);
	} else {
		unsigned int len;
		char *temppath;

		dirdbGetFullname_malloc (dirdbcurdirpath, &temppath, DIRDB_FULLNAME_ENDSLASH);
		len = strlen (temppath) + strlen(curmask);
		temppath = realloc (temppath, len + 1);
		strcat(temppath, curmask);

		displaystr_utf8_overflowleft (1, 0, 0x0f, temppath, plScrWidth);

		free (temppath);
	}

	/* horizontal line below path:/ bar */
	if (playlistactive)
	{
		displaychr (2, 0,               0x07, 0xc4, plScrWidth);
	} else {
		displaychr (2, 0,               0x07, 0xc4, plScrWidth - 15);
		displaychr (2, plScrWidth - 15, 0x07, 0xc2, 1);
		displaychr (2, plScrWidth - 14, 0x07, 0xc4, 14);
	}

	if (fsEditWin||(selecte>=0)||(selectd>=0))
	{
		int Y=dirwinheight+3;
		char *npath = 0;

		/* horizontal line between file-list and the bottom editor */
		if (playlistactive)
		{
			displaychr (Y, 0,               0x07, 0xc4, plScrWidth - 17);
			displaystr (Y, plScrWidth - 17, 0x07, " ALT-E to edit \xc4\xc4", 17);
		} else {
			displaychr (Y, 0,               0x07, 0xc4, plScrWidth - 32);
			displaystr (Y, plScrWidth - 32, 0x07, " ALT-E to edit \xc4\xc4", 17);
			displaychr (Y, plScrWidth - 15, 0x07, 0xc1, 1);
			displaychr (Y, plScrWidth - 14, 0x07, 0xc4, 14);
		}

		/* fill npath */
		if (mle->file)
		{
			struct moduleinfostruct mi;

			if (mle->flags & MODLIST_FLAG_ISMOD)
			{
				mdbGetModuleInfo(&mi, mle->mdb_ref);
			} else {
				memset (&mi, 0, sizeof (mi));
				mi.size = mle->file->filesize (mle->file);
			}

			dirdbGetFullname_malloc (mle->file->dirdb_ref, &npath, 0);
			if (plScrWidth>=180)
			{
				fsShowDirBottom180File (Y + 1, selecte, mle, &mi, npath);
			} else if (plScrWidth>=132)
			{
				fsShowDirBottom132File (Y + 1, selecte, mle, &mi, npath);
			} else {
				fsShowDirBottom80File (Y + 1, selecte, mle, &mi, npath);
			}
		} else if (mle->dir)
		{
			dirdbGetFullname_malloc (mle->dir->dirdb_ref, &npath, 0);
			if (plScrWidth>=132)
			{
				fsShowDirBottom132Dir (Y + 1, selectd, mle, npath);
			} else {
				fsShowDirBottom80Dir (Y + 1, selectd, mle, npath);
			}
		}

		free (npath); npath = 0;
	}

	if (plScrWidth <= 90)
	{
		displaystr (plScrHeight-1, 0, 0x17, " quickfind: [            ]    press F1 for help, or ALT-C for basic setup", plScrWidth);
		displaystr_utf8_overflowleft (plScrHeight-1, 13, 0x1f, quickfind, 12);
	} else {
		displaystr (plScrHeight-1, 0, 0x17, " quickfind: [                        ]    press F1 for help, or ALT-C for basic setup", plScrWidth);
		displaystr_utf8_overflowleft (plScrHeight-1, 13, 0x1f, quickfind, 24);
	}

	for (i=0; i<dirwinheight; i++)
	{
		struct modlistentry *m;

		if (!playlistactive)
		{
			if (/*((firstv+i)<0)||*/((firstv+i)>=currentdir->num))
				displayvoid(i+3, 0, plScrWidth-15);
			else {
				m=modlist_get(currentdir, firstv+i);
				displayfile(i+3, 0, plScrWidth-15, m, ((firstv+i)!=selectv)?0:((selecte<0)&&(selectd<0))?1:2);
			}

			if (/*((firstp+i)<0)||*/((firstp+i)>=playlist->num))
				displayvoid(i+3, plScrWidth-14, 14);
			else {
				m=modlist_get(playlist, firstp+i);
				displayfile(i+3, plScrWidth-14, 14, m, ((firstp+i)!=selectp)?0:((selecte<0)&&(selectd<0))?1:2);
			}
			displaystr(i+3, plScrWidth-15, 0x07, (i==vrelpos)?(i==prelpos)?"\xdb":"\xdd":(i==prelpos)?"\xde":"\xb3", 1);
		} else
		{
			if (/*((firstp+i)<0)||*/((firstp+i)>=playlist->num))
				displayvoid(i+3, 0, plScrWidth);
			else {
				m=modlist_get(playlist, firstp+i);
				displayfile(i+3, 0, plScrWidth, m, ((firstp+i)!=selectp)?0:(selecte<0)?1:2);
			}
		}
	}
}


void fsSetup(void)
{
	int stored = 0;
	uint16_t c;
	int LastCurrent;
	int InKeyboardHelp = 0;

	plSetTextMode(fsScrType);
	while (1)
	{
		const char *fsInfoModes[] =
		{
			"title, channels and size",
			"composer and date",
			"comment",
			"style and playtime",
			"long filenames"
		};
		const char *modename = plGetDisplayTextModeName();
		int i;

superbreak:
		make_title("file selector setup", 0);

		display_nprintf ( 1, 0, 0x07, plScrWidth, "1:  screen mode: %.15o%s",                              modename);
		display_nprintf ( 2, 0, 0x07, plScrWidth, "2:  scramble module list order: %.15o%s",               fsListScramble?"on":"off");
		display_nprintf ( 3, 0, 0x07, plScrWidth, "3:  remove modules from playlist when played: %.15o%s", fsListRemove?"on":"off");
		display_nprintf ( 4, 0, 0x07, plScrWidth, "4:  loop modules: %.15o%s",                             fsLoopMods?"on":"off");
		display_nprintf ( 5, 0, 0x07, plScrWidth, "5:  scan module informatin: %.15o%s",                   fsScanNames?"on":"off");
		display_nprintf ( 6, 0, 0x07, plScrWidth, "6:  scan archive contents: %.15o%s",                    fsScanArcs?"on":"off");
		display_nprintf ( 7, 0, 0x07, plScrWidth, "7:  scan module information in archives: %.15o%s",      fsScanInArc?"on":"off");
		display_nprintf ( 8, 0, 0x07, plScrWidth, "8:  save module information to disk: %.15o%s",          fsWriteModInfo?"on":"off");
		display_nprintf ( 9, 0, 0x07, plScrWidth, "9:  edit window: %.15o%s",                              fsEditWin?"on":"off");
		display_nprintf (10, 0, 0x07, plScrWidth, "A:  module type colors: %.15o%s",                       fsColorTypes?"on":"off");
		display_nprintf (11, 0, 0x07, plScrWidth, "B:  module information display mode: %.15o%s",          fsInfoModes[fsInfoMode]);
		display_nprintf (12, 0, 0x07, plScrWidth, "C:  put archives: %.15o%s",                             fsPutArcs?"on":"off");
		display_nprintf (13, 0, 0x07, plScrWidth, "D:  show all files: %.15o%s",                           fsShowAllFiles?"on":"off");
		display_nprintf (14, 0, 0x07, plScrWidth, "+-: target framerate:%.15o%-4d%.7o, actual framerate: %.15o%d", fsFPS, LastCurrent=fsFPSCurrent);

		displayvoid (15, 0, plScrWidth);

		displaystr(16, 0, 0x07, "ALT-S (or CTRL-S if in X) to save current setup to ocp.ini", plScrWidth);
		displaystr(plScrHeight-1, 0, 0x17, "  press the number of the item you wish to change and ESC when done", plScrWidth);

		displaystr(17, 0, 0x03, (stored?"ocp.ini saved":""), plScrWidth);

		for (i=18; i < plScrHeight; i++)
		{
			displayvoid (i, 0, plScrWidth);
		}

		if (InKeyboardHelp)
		{
			InKeyboardHelp = cpiKeyHelpDisplay();
			framelock();
			continue;
		} else {
			while ( !Console.KeyboardHit() && (LastCurrent==fsFPSCurrent) )
			{
				framelock();
			}
			if (!Console.KeyboardHit())
			{
				continue;
			}
		}

		c = Console.KeyboardGetChar();

		switch (c)
		{
			case '1': stored = 0; plDisplaySetupTextMode(); break;
			/*case '1': stored = 0; fsScrType=(fsScrType+1)&7; break;*/
			case '2': stored = 0; fsListScramble=!fsListScramble; break;
			case '3': stored = 0; fsListRemove=!fsListRemove; break;
			case '4': stored = 0; fsLoopMods=!fsLoopMods; break;
			case '5': stored = 0; fsScanNames=!fsScanNames; break;
			case '6': stored = 0; fsScanArcs=!fsScanArcs; break;
			case '7': stored = 0; fsScanInArc=!fsScanInArc; break;
			case '8': stored = 0; fsWriteModInfo=!fsWriteModInfo; break;
			case '9': stored = 0; fsEditWin=!fsEditWin; break;
			case 'a': case 'A': stored = 0; fsColorTypes=!fsColorTypes; break;
			case 'b': case 'B': stored = 0; fsInfoMode=(fsInfoMode+1)%5; break;
			case 'c': case 'C': stored = 0; fsPutArcs=!fsPutArcs; break;
			case 'd': case 'D': stored = 0; fsShowAllFiles=!fsShowAllFiles; break;
			case '+': if (fsFPS<1000) fsFPS++; break;
			case '-': if (fsFPS>1) fsFPS--; break;
			case KEY_CTRL_S:
			case KEY_ALT_S:
			{
				const char *sec=cfGetProfileString(cfConfigSec, "fileselsec", "fileselector");

				cfSetProfileInt(cfScreenSec, "screentype", fsScrType, 10);
				cfSetProfileBool(sec, "randomplay", fsListScramble);
				cfSetProfileBool(sec, "playonce", fsListRemove);
				cfSetProfileBool(sec, "loop", fsLoopMods);
				cfSetProfileBool(sec, "scanmodinfo", fsScanNames);
				cfSetProfileBool(sec, "scanarchives", fsScanArcs);
				cfSetProfileBool(sec, "scaninarcs", fsScanInArc);
				cfSetProfileBool(sec, "writeinfo", fsWriteModInfo);
				cfSetProfileBool(sec, "editwin", fsEditWin);
				cfSetProfileBool(sec, "typecolors", fsColorTypes);
				/*cfSetProfileInt(sec, "", fsInfoMode);*/
				cfSetProfileBool(sec, "putarchives", fsPutArcs);
				cfSetProfileBool(sec, "showallfiles", fsShowAllFiles);
				cfSetProfileInt("screen", "fps", fsFPS, 10);
				cfStoreConfig();
				stored = 1;
				break;
			}
			case VIRT_KEY_RESIZE:
				fsScrType = plScrType;
				break;
			case KEY_EXIT:
			case KEY_ESC:
				    return;
			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp('1', "Toggle option 1");
				cpiKeyHelp('2', "Toggle option 2");
				cpiKeyHelp('3', "Toggle option 3");
				cpiKeyHelp('4', "Toggle option 4");
				cpiKeyHelp('5', "Toggle option 5");
				cpiKeyHelp('6', "Toggle option 6");
				cpiKeyHelp('7', "Toggle option 7");
				cpiKeyHelp('8', "Toggle option 8");
				cpiKeyHelp('9', "Toggle option 9");
				cpiKeyHelp('a', "Toggle option A");
				cpiKeyHelp('b', "Toggle option B");
				cpiKeyHelp('c', "Toggle option C");
				cpiKeyHelp('d', "Toggle option D");
				cpiKeyHelp('A', "Toggle option A");
				cpiKeyHelp('B', "Toggle option B");
				cpiKeyHelp('C', "Toggle option C");
				cpiKeyHelp('D', "Toggle option D");
				cpiKeyHelp('+', "Increase FPS");
				cpiKeyHelp('-', "Decrease FPS");
				cpiKeyHelp(KEY_ALT_S, "Store settings to ocp.ini");
				cpiKeyHelp(KEY_CTRL_S, "Store settings to ocp.ini (avoid this key if in curses)");
				InKeyboardHelp = 1;
				goto superbreak;
		}
	}
}

static struct moduleinfostruct mdbEditBuf;

/* recall until it returns zero */
static int fsEditModType (struct moduletype *oldtype, int _Bottom, int _Right)
{
	static int state = 0;
	/* 0 - new / idle
	 * 1 - in edit
	 * 2 - in keyboard help
	 */

	static int curindex=0; /* points into fsTypes[] or onto the entry past it for "unscanned" */
	int offset=0;

	int i;

	const int Height=23;
	const int iHeight=15;
	const int TopWidth=20;
	int Top;
	int Left;
	const int Mid = 9;
	int d;
	static int editcol=0;

	Top = _Bottom - Height;
	Left = _Right - 78;

	if (Top < 1)
	{
		Top = 1;
	}
	if (Left < 1)
	{
		Left = 1;
	}

	if (state == 0) /* new edit / first time */
	{
		curindex = fsTypesCount;
		for (i=0;i<fsTypesCount;i++)
		{
			if (fsTypes[i].modtype.integer.i == oldtype->integer.i)
			{
				curindex=i;
				break;
			}
		}
		state = 1;
	}

	displaystr(Top, Left, 0x04, "\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc2\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf", 21);

	for (i=1;i<Height-7;i++)
	{
		displaystr(Top+i, Left, 0x04, "\xb3", 1);
		displaystr(Top+i, Left+Mid, 0x04, "\xb3", 1);
		displaystr(Top+i, Left+TopWidth, 0x04, "\xb3", 1);
	}

	displaystr(Top+i, Left, 0x04, "\xc3\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc1\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc1\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf", 78);

	d = curindex < fsTypesCount;
	for (i=0; i < 6; i++)
	{
		displaystr(Top + Height - 6 + i, Left, 0x04, "\xb3", 1);

		if (d && (!fsTypes[curindex].description[i])) d = 0;
		if (d)
		{
			displaystr (Top + Height - 6 + i, Left + 1, 0x07, fsTypes[curindex].description[i], 76);
		} else {
			displayvoid (Top + Height - 6 + i, Left + 1, 76);
		}

		displaystr(Top + Height - 6 + i, Left + 77, 0x04, "\xb3", 1);
	}
	displaystr(Top+Height, Left, 0x04, "\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9", 78);

	if (fsTypesCount>=iHeight)
	{
		if (curindex<=(iHeight/2))
		{
			offset=0;
		} else if (curindex>=((fsTypesCount+1)-iHeight/2))
		{
			offset=fsTypesCount + 1 - iHeight;
		} else {
			offset=curindex-iHeight/2;
		}
	} else {
		offset=0;
	}

	for (i=1;i<16;i++)
	{
		unsigned char col;
		char buffer[11];
		col=i;
		if (editcol==i)
			col|=0x80;
		snprintf(buffer, sizeof(buffer), " color %2d ", i);
		displaystr(Top+i, Left+Mid+1, col, buffer, 10);
	}
	for (i=0;i<iHeight;i++)
	{
		char m[5];
		unsigned char col;

		if ((offset+i)==curindex)
		{
			displaystr (Top + i + 1, Left + 1, 0x07, "->    <-", 8);
		} else {
			displayvoid (Top + i + 1, Left + 1, 8);
		}

		if ((!editcol)&&((offset+i)==curindex))
		{
			col=0x80;
		} else {
			col=0;
		}

		if ((offset+i)>=fsTypesCount)
			break;
		col|=fsTypes[offset+i].color;
		m[0] = fsTypes[offset+i].modtype.string.c[0];
		m[1] = fsTypes[offset+i].modtype.string.c[1];
		m[2] = fsTypes[offset+i].modtype.string.c[2];
		m[3] = fsTypes[offset+i].modtype.string.c[3];
		m[4] = 0;
		displaystr(Top+i+1, Left+3, col, m, 4);
	}
	if (state == 2)
	{
		if (cpiKeyHelpDisplay())
		{
			framelock();
			return 1;
		}
		state = 1;
	}
	framelock();
	while (Console.KeyboardHit())
	{
		switch (Console.KeyboardGetChar())
		{
			case VIRT_KEY_RESIZE:
				fsScrType = plScrType;
				break;
			case KEY_RIGHT:
				if (curindex != fsTypesCount)
				{
					editcol = fsTypes[curindex].color;
				}
				break;
			case KEY_LEFT:
				if (editcol)
				{
					char m[5];
					m[0] = fsTypes[curindex].modtype.string.c[0];
					m[1] = fsTypes[curindex].modtype.string.c[1];
					m[2] = fsTypes[curindex].modtype.string.c[2];
					m[3] = fsTypes[curindex].modtype.string.c[3];
					m[4] = 0;
					fsTypes[curindex].color = editcol;
					cfSetProfileInt("fscolors", m, editcol, 10);
					cfStoreConfig();
					editcol=0;
				}
				break;
			case KEY_UP:
				if (editcol)
				{
					if (editcol>1)
						editcol--;
				} else {
					if (curindex)
						curindex--;
				}
				break;
			case KEY_DOWN:
				if (editcol)
				{
					if (editcol<15)
						editcol++;
				} else {
					if ((curindex+1)<=fsTypesCount)
						curindex++;
				}
				break;
			case KEY_EXIT:
			case KEY_ESC:
				if (editcol)
				{
					editcol=0;
				} else {
					state = 0;
					return 0;
				}
				break;
			case _KEY_ENTER:
				if (editcol)
				{
					char m[5];
					m[0] = fsTypes[curindex].modtype.string.c[0];
					m[1] = fsTypes[curindex].modtype.string.c[1];
					m[2] = fsTypes[curindex].modtype.string.c[2];
					m[3] = fsTypes[curindex].modtype.string.c[3];
					m[4] = 0;
					fsTypes[curindex].color = editcol;
					cfSetProfileInt("fscolors", m, editcol, 10);
					cfStoreConfig();
					editcol=0;
				} else {
					if (curindex != fsTypesCount)
					{
						*oldtype = fsTypes[curindex].modtype;
					} else {
						oldtype->integer.i = 0;
					}
					state = 0;
					return 0;
				}
				break;
			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp(KEY_RIGHT, "Edit color");
				cpiKeyHelp(KEY_LEFT, "Edit color");
				cpiKeyHelp(KEY_UP, "Select another filetype / change color");
				cpiKeyHelp(KEY_DOWN, "Select another filetype / change color");
				cpiKeyHelp(KEY_ESC, "Abort edit");
				cpiKeyHelp(_KEY_ENTER, "Select the highlighted filetype");
				state = 2;
				return 1;
		}
	}
	return 1;
}

static int fsEditChan(int y, int x, uint8_t *chan)
{
	static int state = 0;
	/* 0 - new / idle
	 * 1 - in edit
	 * 2 - in keyboard help
	 */
	static int curpos=0;
	static char str[3];

	if (state == 0)
	{
		curpos = 0;
		snprintf (str, sizeof (str), "%02d", saturate(*chan,0, 99));
		setcurshape(1);
		state = 1;
	}

	displaystr(y, x, 0x8F, str, 2);
	setcur(y, x+curpos);

	if (state == 2)
	{
		if (cpiKeyHelpDisplay())
		{
			framelock();
			return 1;
		}
		state = 1;
	}
	framelock();

	while (Console.KeyboardHit())
	{
		uint16_t key = Console.KeyboardGetChar();
		switch (key)
		{
			case ' ':
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				if (key==' ')
					key='0';
				if ((curpos==0)&&(key>='4'))
					break;
				if (curpos==0)
					str[1]='0';
				if ((curpos==1)&&(str[0]=='3')&&(key>'2'))
					break;
				if (curpos<2)
					str[curpos]=key;
			case KEY_RIGHT:
				curpos="\x01\x02\x02"[curpos];
				break;
			case KEY_BACKSPACE:
			case KEY_LEFT:
				curpos="\x00\x00\x01"[curpos];
				if (key==KEY_BACKSPACE)
					str[curpos]='0';
				break;
			case VIRT_KEY_RESIZE:
				fsScrType = plScrType;
				break;
			case KEY_EXIT:
			case KEY_ESC:
				setcurshape(0);
				state = 0;
				return 0;
			case _KEY_ENTER:
				*chan=(str[0]-'0')*10+str[1]-'0';
				setcurshape(0);
				state = 0;
				return 0;
			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp(KEY_RIGHT, "Move cursor right");
				cpiKeyHelp(KEY_LEFT, "Move cursor left");
				cpiKeyHelp(KEY_BACKSPACE, "Move cursor right");
				cpiKeyHelp(KEY_ESC, "Cancel changes");
				cpiKeyHelp(_KEY_ENTER, "Submit changes");
				state = 2;
				return 1;
		}
	}

	return 1;
}

static int fsEditPlayTime(int y, int x, uint16_t *playtime)
{
	static int state = 0;
	/* 0 - new / idle
	 * 1 - in edit
	 * 2 - in keyboard help
	 */
	static char str[7];
	static int curpos;

	if (state == 0)
	{
		snprintf (str, sizeof (str), "%03d:%02d", saturate((*playtime)/60, 0, 999), (*playtime)%60);

		curpos=(str[0]!='0')?0:(str[1]!='0')?1:2;

		setcurshape(1);

		state = 1;
	}

	displaystr(y, x, 0x8f, str, 6);
	setcur(y, x + curpos);

	if (state == 2)
	{
		if (cpiKeyHelpDisplay())
		{
			framelock();
			return 1;
		}
		state = 1;
	}
	framelock();

	while (Console.KeyboardHit())
	{
		uint16_t key = Console.KeyboardGetChar();
		switch (key)
		{
			case ':':
				curpos = 4;
				break;
			case ' ':
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				if (key==' ')
					key='0';
				if ((curpos==4)&&(key>'5'))
					break;
				if (curpos<6)
					str[curpos]=key;
				/* fall-through */
			case KEY_RIGHT:
				curpos="\x01\x02\x04\xff\x05\x05"[curpos];
				break;
			case KEY_BACKSPACE:
			case KEY_LEFT: /*left*/
				curpos="\x00\x00\x01\xff\x02\x04"[curpos];
				if (key==KEY_BACKSPACE)
					str[curpos]='0';
				break;
			case VIRT_KEY_RESIZE:
				fsScrType = plScrType;
				break;
			case KEY_EXIT:
			case KEY_ESC:
				setcurshape(0);
				state = 0;
				return 0;
			case _KEY_ENTER:
				*playtime=(str[0]-'0')*6000+
				          (str[1]-'0')* 600+
				          (str[2]-'0')*  60+
				          (str[4]-'0')*  10+
				          (str[5]-'0');
				setcurshape(0);
				state = 0;
				return 0;
			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp(KEY_RIGHT, "Move cursor right");
				cpiKeyHelp(KEY_LEFT, "Move cursor left");
				cpiKeyHelp(KEY_BACKSPACE, "Move cursor right");
				cpiKeyHelp(KEY_ESC, "Cancel changes");
				cpiKeyHelp(_KEY_ENTER, "Submit changes");
				state = 2;
				return 1;
		}
	}

	return 1;
}

static int fsEditDate(int y, int x, uint32_t *date)
{
	static int state = 0;
	/* 0 - new / idle
	 * 1 - in edit
	 * 2 - in keyboard help
	 */
	static char str[11];
	static int curpos;

	if (state == 0)
	{
		curpos = 0;

		snprintf (str, sizeof (str), "%02d.%02d.%04d", saturate((*date)&0xFF, 0, 99), saturate(((*date)>>8)&0xFF, 0, 99), saturate((*date)>>16, 0, 9999));
		if ((((*date)>>16) > 0) && (((*date)>>16) < 100))
		{
			str[6] = ' ';
			str[7] = '\'';
		}
		setcurshape(1);
		state = 1;
	}

	displaystr(y, x, 0x8F, str, 10);
	setcur(y, x+curpos);

	if (state == 2)
	{
		if (cpiKeyHelpDisplay())
		{
			framelock();
			return 1;
		}
		state = 1;
	}
	framelock();

	while (Console.KeyboardHit())
	{
		uint16_t key = Console.KeyboardGetChar();
		switch (key)
		{
			case '.':
				if (curpos <= 3)
				{
					curpos=3;
				} else if (curpos <= 6)
				{
					curpos = 6;
				}
				break;
			case '\'':
				if (curpos==6)
				{
					str[6]=' ';
					str[7]='\'';
					curpos=8;
				}
				break;
			case ' ':
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				if (key==' ')
					key='0';
				if ((curpos==0)&&(key>='4'))
					break;
				if (curpos==0)
					str[1]='0';
				if ((curpos==1)&&(str[0]=='3')&&(key>'1'))
					break;
				if ((curpos==3)&&(key>'1'))
					break;
				if (curpos==3)
					str[4]='0';
				if ((curpos==4)&&(str[3]=='1')&&(key>'2'))
					break;
				if (curpos<10)
					str[curpos]=key;
				if ((str[6]!=' ') && (str[7] == '\''))
				{
					str[7]='0';
				}
				/* fall-through */
			case KEY_RIGHT:
				curpos="\x01\x03\xff\x04\x06\xff\x07\x08\x09\x09"[curpos];
				break;
			case KEY_BACKSPACE:
			case KEY_LEFT:
				curpos="\x00\x00\xff\x01\x03\xff\x04\x06\x07\x08"[curpos];
				if (key==KEY_BACKSPACE)
					str[curpos]='0';
				break;
			case VIRT_KEY_RESIZE:
				fsScrType = plScrType;
				break;
			case KEY_EXIT:
			case KEY_ESC:
				setcurshape(0);
				state = 0;
				return 0;
			case _KEY_ENTER:
				*date=((str[0]-'0')*10+
				        str[1]-'0')|
				     (((str[3]-'0')*10+
				        str[4]-'0')<<8);
				if ((str[7]=='\'') && (str[8]=='0') && (str[9]=='0'))
				{ /* special case for '00 (year 2000 ) */
					*date += 100 << 16;
				} else if (str[7] == '\'')
				{
					*date +=(atoi (str + 8) << 16);
				} else {
					*date += (atoi (str + 6) << 16);
				}
				setcurshape(0);
				state = 0;
				return 0;
			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp(KEY_RIGHT, "Move cursor right");
				cpiKeyHelp(KEY_LEFT, "Move cursor left");
				cpiKeyHelp(KEY_BACKSPACE, "Move cursor right");
				cpiKeyHelp(KEY_ESC, "Cancel changes");
				cpiKeyHelp(_KEY_ENTER, "Submit changes");
				state = 2;
				return 1;
		}
	}
	return 1;
}

static int fsEditFileInfo(struct modlistentry *me)
{
	int retval;

	if (!(me->flags & MODLIST_FLAG_ISMOD))
		return 1;
	if (!mdbGetModuleInfo(&mdbEditBuf, me->mdb_ref))
		return 1;

	if (plScrWidth>=180)
	{
		const int leftwidth = (plScrWidth - (28 + 55 + 10 + 10)) / 2;
		const int rightwidth = plScrWidth - (28 /* file: abcdef.mod FILESIZE */ + 55 /* style: foobar */+ 10 /* "   title: " */ + 10 /* "  artist: */) - leftwidth;

		switch (editfilepos)
		{
			default:
			case 0:
				retval = EditStringUTF8z(plScrHeight - 5, 38, leftwidth, sizeof(mdbEditBuf.title), mdbEditBuf.title);
				break;
			case 1:
				retval = fsEditModType(&mdbEditBuf.modtype, plScrHeight - 5, plScrWidth - 46 + 4);
				break;
			case 2:
				retval = fsEditChan(plScrHeight - 5, plScrWidth - 27, &mdbEditBuf.channels);
				break;
			case 3:
			{
				uint16_t playtime = mdbEditBuf.playtime; /* avoid unaligned pointer access */
				retval = fsEditPlayTime(plScrHeight - 5, plScrWidth - 9, &playtime);
				mdbEditBuf.playtime = playtime;
				break;
			}
			case 4:
				retval = EditStringUTF8z(plScrHeight - 4, 38, leftwidth, sizeof(mdbEditBuf.composer), mdbEditBuf.composer);
				break;
			case 5:
				retval = EditStringUTF8z(plScrHeight - 4, plScrWidth - 46, 43, sizeof(mdbEditBuf.style), mdbEditBuf.style);
				break;
			case 6:
			{
				uint32_t date = mdbEditBuf.date; /* avoid unaligned pointer access */
				retval = fsEditDate(plScrHeight - 3, 38, &date);
				mdbEditBuf.date = date;
				break;
			}
			case 7:
				retval = EditStringUTF8z(plScrHeight - 3, 64, plScrWidth - 67, sizeof(mdbEditBuf.comment), mdbEditBuf.comment);
				break;
			case 8:
				retval = EditStringUTF8z(plScrHeight - 5, 28 + 10 + leftwidth + 10, rightwidth, sizeof(mdbEditBuf.artist), mdbEditBuf.artist);
				break;
			case 9:
				retval = EditStringUTF8z(plScrHeight-4, 28 + 10 + leftwidth + 10, rightwidth, sizeof(mdbEditBuf.album), mdbEditBuf.album);
				break;
		}
	} else if (plScrWidth>=132)
	{
		const int leftwidth = (plScrWidth - 37 - 10 - 35) / 2;
		const int rightwidth = plScrWidth - 37 - 10 - 35 - leftwidth;

		switch (editfilepos)
		{
			default:
			case 0:
				retval = EditStringUTF8z(plScrHeight - 5, 37, leftwidth, sizeof(mdbEditBuf.title), mdbEditBuf.title);
				break;
			case 1:
				retval = fsEditModType(&mdbEditBuf.modtype, plScrHeight - 4, plScrWidth - 35 + 11 + 4);
				break;
			case 2:
				retval = fsEditChan(plScrHeight - 5, plScrWidth - 35 + 12, &mdbEditBuf.channels);
				break;
			case 3:
			{
				uint16_t playtime = mdbEditBuf.playtime; /* avoid unaligned pointer access */
				retval = fsEditPlayTime(plScrHeight - 5, plScrWidth - 9, &playtime);
				mdbEditBuf.playtime = playtime;
				break;
			}
			case 4:
				retval = EditStringUTF8z(plScrHeight - 4, 37, leftwidth, sizeof(mdbEditBuf.composer), mdbEditBuf.composer);
				break;
			case 5:
				retval = EditStringUTF8z(plScrHeight - 3, plScrWidth - 43 + 9, 43 - 9 - 3, sizeof(mdbEditBuf.style), mdbEditBuf.style);
				break;
			case 6:
			{
				uint32_t date = mdbEditBuf.date; /* avoid unaligned pointer access */
				retval = fsEditDate(plScrHeight - 4, plScrWidth - 21 + 8, &date);
				mdbEditBuf.date = date;
				break;
			}
			case 7:
				retval = EditStringUTF8z(plScrHeight - 3, 37, plScrWidth - 37 - 43, sizeof(mdbEditBuf.comment), mdbEditBuf.comment);
				break;
			case 8:
				retval = EditStringUTF8z(plScrHeight - 5, 47 + leftwidth, rightwidth, sizeof(mdbEditBuf.artist), mdbEditBuf.artist);
				break;
			case 9:
				retval = EditStringUTF8z(plScrHeight - 4, 47 + leftwidth, rightwidth, sizeof(mdbEditBuf.album), mdbEditBuf.album);
				break;
		}
	} else {
		const int leftwidth = (plScrWidth - 10 - 9 - 1) / 2;
		const int rightwidth = plScrWidth - 10 - 9 - 1 - leftwidth;

		switch (editfilepos)
		{
			default:
			case 0:
				retval = EditStringUTF8z(plScrHeight - 6, 35, plScrWidth - 35 - 13, sizeof(mdbEditBuf.title), mdbEditBuf.title);
				break;
			case 1:
				retval = fsEditModType(&mdbEditBuf.modtype, plScrHeight - 6, plScrWidth - 1);
				break;
			case 2:
				retval = fsEditChan(plScrHeight - 3, plScrWidth - 3, &mdbEditBuf.channels);
				break;
			case 3:
			{
				uint16_t playtime = mdbEditBuf.playtime; /* avoid unaligned pointer access */
				retval = fsEditPlayTime(plScrHeight - 3, plScrWidth - 21, &playtime);
				mdbEditBuf.playtime = playtime;
				break;
			}
			case 4:
				retval = EditStringUTF8z(plScrHeight - 5, 10, plScrWidth - 10 - 19, sizeof(mdbEditBuf.composer), mdbEditBuf.composer);
				break;
			case 5:
				retval = EditStringUTF8z(plScrHeight - 3, 10, plScrWidth - 10 - 33, sizeof(mdbEditBuf.style), mdbEditBuf.style);
				break;
			case 6:
				{
					uint32_t date = mdbEditBuf.date; /* avoid unaligned pointer access */
					retval = fsEditDate(plScrHeight - 5, plScrWidth - 11, &date);
					mdbEditBuf.date = date;
					break;
				}
			case 7:
				retval = EditStringUTF8z(plScrHeight - 2, 10, plScrWidth - 11, sizeof(mdbEditBuf.comment), mdbEditBuf.comment);
				break;
			case 8:
				retval = EditStringUTF8z(plScrHeight - 4, 10, leftwidth, sizeof(mdbEditBuf.artist), mdbEditBuf.artist);
				break;
			case 9:
				retval = EditStringUTF8z(plScrHeight - 4, 10 + leftwidth + 9, rightwidth, sizeof(mdbEditBuf.album), mdbEditBuf.album);
				break;
		}
	}

	if (!retval)
	{
		if (!mdbWriteModuleInfo(me->mdb_ref, &mdbEditBuf))
		{
			return -1;
		}
		if (mdbEditBuf.modtype.integer.i == mtUnRead)
		{
			me->flags &= ~MODLIST_FLAG_SCANNED;
		}

		return 0;
	}
	return retval > 0;
}

static int fsEditDirInfo(struct modlistentry *me)
{
	int retval;

	/* our current only entry does not care
	if (plScrWidth>=132)
	{
	} else {
	}
	*/
	switch (editdirpos)
	{
		default:
		case 0:
			if (me->dir->charset_override_API)
			{
				retval = fsEditCharset(me->dir);
			} else {
				retval = 0;
			}
			break;
	}

	return retval;
}

static int fsEditViewPath(void)
{
	static int state = 0;
	static char *temppath;
	int retval;

	uint32_t dirdb_newpath = DIRDB_NOPARENT;
	struct dmDrive *newdrive = 0;
	struct ocpdir_t *newdir = 0;

	if (state == 0)
	{
		char *curdirpath;
		dirdbGetFullname_malloc (dirdbcurdirpath, &curdirpath, DIRDB_FULLNAME_ENDSLASH);

		temppath = malloc (strlen(curdirpath) + strlen(curmask) + 1);
		strcpy (temppath, curdirpath);
		strcat (temppath, curmask);
		free (curdirpath);

		state = 1;
	}

	retval = EditStringUTF8(1, 0, plScrWidth, &temppath);

	if (retval > 0)
	{
		return 1;
	}

	state = 0;

	if (retval < 0)
	{ /* abort */
		free (temppath);
		return 0;
	}

	/* split out curmask, and leave temppath to only contain the actual path */
	{
		char *drive;
		char *path;
		char *filename;

		splitpath_malloc (temppath, &drive, &path, &filename);

		if (!strlen(filename))
		{
			free (filename);
			filename = strdup ("*");
		}

		free (curmask);
		curmask = filename;

		/* this should not be needed, since it should always shrink or be the same size */
		free (temppath);
		temppath = malloc (strlen (drive) + strlen (path) + 1);
		if (!temppath)
		{
			return 0;
		}

		sprintf (temppath, "%s%s", drive, path);
		free (drive);
		free (path);
	}

	dirdb_newpath = dirdbResolvePathAndRef (temppath, dirdb_use_pfilesel);
	free (temppath);
	temppath = 0;
	filesystem_resolve_dirdb_dir (dirdb_newpath, &newdrive, &newdir);
	dirdbUnref (dirdb_newpath, dirdb_use_pfilesel);

	if (newdir)
	{
		dmCurDrive = newdrive;
		assert (dmCurDrive->cwd);
		dmCurDrive->cwd->unref (dmCurDrive->cwd);
		dmCurDrive->cwd = newdir;

#ifdef _WIN32
		if (dmCurDrive->drivename[1] == ':')
		{
			dmLastActiveDriveLetter = dmCurDrive->drivename[0];
		}
#endif
	}

	fsScanDir(0);
	return 0;
}

static void fsDraw(void)
{ /* used by medialib to have backdrop for the dialogs they display on the screen */
	signed int firstv, firstp;
	struct modlistentry *m;

	dirwinheight=plScrHeight-4;
	if (fsEditWin||editmode)
		dirwinheight-=(plScrWidth>=132)?5:6;

	if (!playlist->num)
	{
		win=0;
		playlist->pos=0;
	} else {
		if (playlist->pos>=playlist->num)
		{
			playlist->pos=playlist->num-1;
		}
	}
	if (!currentdir->num)
	{ /* this should never happen */
		currentdir->pos=0;
	} else {
		if (currentdir->pos>=currentdir->num)
		{
			currentdir->pos=currentdir->num-1;
		}
	}
	firstv=currentdir->pos-dirwinheight/2;

	if ((unsigned)(firstv+dirwinheight)>currentdir->num)
		firstv=currentdir->num-dirwinheight;
	if (firstv<0)
		firstv=0;
	firstp=playlist->pos-dirwinheight/2;

	if ((unsigned)(firstp+dirwinheight)>playlist->num)
		firstp=playlist->num-dirwinheight;
	if (firstp<0)
		firstp=0;

	m = modlist_getcur ( win ? playlist : currentdir );

	fsShowDir(firstv, win?(unsigned)~0:currentdir->pos, firstp, win?playlist->pos:(unsigned)~0, (editmode && m && m->dir) ? editdirpos : -1 , (editmode && m && m->file) ? editfilepos : -1 , m, win);

	/* we do not paint any of the edits from the fsFileSelect() state */
}

signed int fsFileSelect(void)
{
	int state = 0;
	/* state = 0 - Idle
	 * state = 1 - fsEditFileInfo()
	 * state = 2 - cpiKeyHelpDisplay()
	 * state = 3 - fsEditViewPath()
	 * state = 4 - fsSavePlayList()
	 * state = 5 - fsEditDirInfo()
	 */
	unsigned long i;

	if (!currentdir->num)
	{ /* this is true the very first time we execute */
		fsScanDir(0);
	}

	plSetTextMode(fsScrType);
	fsScrType=plScrType;

	isnextplay=NextPlayNone;

	quickfind[0] = 0;
	quickfindlen = 0;

	if (fsPlaylistOnly)
		return 0;

	while (1)
	{
		signed int firstv, firstp;
		uint16_t c;
		struct modlistentry *m;

superbreak:
		dirwinheight=plScrHeight-4;
		if (fsEditWin||editmode)
			dirwinheight-=(plScrWidth>=132)?5:6;

		if (!playlist->num)
		{
			win=0;
			playlist->pos=0;
		} else {
			if (playlist->pos>=playlist->num)
			{
				playlist->pos=playlist->num-1;
			}
		}
		if (!currentdir->num)
		{ /* this should never happen */
			currentdir->pos=0;
		} else {
			if (currentdir->pos>=currentdir->num)
			{
				currentdir->pos=currentdir->num-1;
			}
		}
		firstv=currentdir->pos-dirwinheight/2;

		if ((unsigned)(firstv+dirwinheight)>currentdir->num)
			firstv=currentdir->num-dirwinheight;
		if (firstv<0)
			firstv=0;
		firstp=playlist->pos-dirwinheight/2;

		if ((unsigned)(firstp+dirwinheight)>playlist->num)
			firstp=playlist->num-dirwinheight;
		if (firstp<0)
			firstp=0;

		m=modlist_getcur ( win ? playlist : currentdir );

		fsShowDir(firstv, win?(unsigned)~0:currentdir->pos, firstp, win?playlist->pos:(unsigned)~0, (editmode && m && m->dir) ? editdirpos : -1 , (editmode && m && m->file) ? editfilepos : -1 , m, win);

		if (state == 1)
		{
			int retval;
			retval = fsEditFileInfo(m);
			if (retval > 0)
			{
				goto superbreak;
			} else if (retval < 0)
			{
				return -1;
			}
			state = 0;
		} else if (state == 2)
		{
			if (cpiKeyHelpDisplay())
			{
				framelock();
				goto superbreak;
			}
			state = 0;
		} else if (state == 3)
		{
			if (fsEditViewPath())
			{
				framelock();
				goto superbreak;
			}
			state = 0;
			goto superbreak;
		} else if (state == 4)
		{
			if (fsSavePlayList(playlist))
			{
				framelock();
				goto superbreak;
			}
			state = 0;
			goto superbreak; /* need to reload m */
		} else if (state == 5)
		{
			int retval;
			retval = fsEditDirInfo(m);
			if (retval > 0)
			{
				framelock();
				goto superbreak;
			} else if (retval < 0)
			{
				return -1;
			}
			state = 0;
		}

		if (!Console.KeyboardHit() && fsScanNames)
		{
			int poll = 1;
			if ((m->file && (m->flags & MODLIST_FLAG_ISMOD)) && (!mdbInfoIsAvailable(m->mdb_ref)) && (!(m->flags&MODLIST_FLAG_SCANNED)))
			{
				mdbScan(m->file, m->mdb_ref);
				m->flags |= MODLIST_FLAG_SCANNED;
			}

			while (((!win)||(scanposp>=playlist->num)) && (scanposf<currentdir->num))
			{
				struct modlistentry *scanm;
				if ((scanm=modlist_get(currentdir, scanposf++)))
				{
					if (scanm->file && (scanm->flags & MODLIST_FLAG_ISMOD) && (!(scanm->flags & MODLIST_FLAG_SCANNED)))
					{
						if (!mdbInfoIsAvailable(scanm->mdb_ref))
						{
							mdbScan(scanm->file, scanm->mdb_ref);
							scanm->flags |= MODLIST_FLAG_SCANNED;

							if (poll_framelock())
							{
								poll = 0;
								break;
							}
						}
					}
				}
			}
			while (((win)||(scanposf>=currentdir->num)) && (scanposp<playlist->num))
			{
				struct modlistentry *scanm;
				if ((scanm=modlist_get(playlist, scanposp++)))
				{
					if (scanm->file && (scanm->flags & MODLIST_FLAG_ISMOD))
					{
						if (!mdbInfoIsAvailable(scanm->mdb_ref))
						{
							mdbScan(scanm->file, scanm->mdb_ref);
							scanm->flags |= MODLIST_FLAG_SCANNED;

							if (poll_framelock())
							{
								poll = 0;
								break;
							}
						}
					}
				}
			}
			if (poll)
			{
				framelock();
			}
			continue;
		} else while (Console.KeyboardHit())
		{
			c = Console.KeyboardGetChar();

			if (c == VIRT_KEY_RESIZE)
			{
				fsScrType = plScrType;
				continue;
			}

			if (!editmode)
			{
				if (((c>=32)&&(c<=255)&&(c!=KEY_CTRL_BS))||(c==KEY_BACKSPACE))
				{
					if (c==KEY_BACKSPACE)
					{
						if (quickfindlen)
						{
							int len = strlen (quickfind);
							int more;
							do
							{
								more = ((quickfind[len-1] & 0xc0) == 0x80);
								quickfind[len - 1] = 0;
								len --;
							} while (more && len);
							quickfindlen--;
						}
					} else {
						static uint8_t input_buffer[8];
						static int input_buffer_fill;
						uint32_t codepoint;
						int incr = 0;

						input_buffer[input_buffer_fill++] = c;
						input_buffer[input_buffer_fill] = 0x80; /* dummy follow... */
						codepoint = utf8_decode ((char *)input_buffer, input_buffer_fill + 1, &incr);
						if (incr > input_buffer_fill)
						{ /* we need more data */
							assert (input_buffer_fill < 6);
							continue;
						}
						input_buffer_fill = 0;

						if (quickfindlen < 24)
						{
							utf8_encode (quickfind + strlen (quickfind), codepoint);
							quickfindlen++;
						}
					}
					if (!quickfindlen)
						continue;
					if (!win)
						currentdir->pos=modlist_fuzzyfind(currentdir, quickfind);
					else
						playlist->pos=modlist_fuzzyfind(playlist, quickfind);
					continue;
				}
			}

			quickfind[0] = 0;
			quickfindlen = 0;

			switch (c)
			{
				case KEY_ALT_K:
					cpiKeyHelpClear();
					cpiKeyHelp(KEY_ESC, "Exit");
					cpiKeyHelp(KEY_CTRL_BS, "Stop filescanning");
					cpiKeyHelp(KEY_ALT_S, "Stop filescanning");
					cpiKeyHelp(KEY_TAB, "Toggle between filelist and playlist");
					cpiKeyHelp(KEY_SHIFT_TAB, "Toggle between lists and editwindow");
					cpiKeyHelp(KEY_ALT_E, "Toggle between lists and editwindow");
					cpiKeyHelp(KEY_ALT_I, "Cycle file-list mode (fullname, title, time etc)");
					cpiKeyHelp(KEY_ALT_C, "Show setup dialog");
					cpiKeyHelp(KEY_F(1), "Show help");
					cpiKeyHelp(KEY_ALT_R, "Rescan selected file");
					cpiKeyHelp(KEY_ALT_Z, "Toggle resolution if possible (mode 0 and 7)");
					cpiKeyHelp(KEY_ALT_ENTER, "Edit path");
					cpiKeyHelp(KEY_CTRL_ENTER, "Edit path");
					cpiKeyHelp(_KEY_ENTER, "Play selected file, or open selected directory/arc/playlist");
					cpiKeyHelp(KEY_UP, "Move cursor up");
					cpiKeyHelp(KEY_DOWN, "Move cursor down");
					cpiKeyHelp(KEY_PPAGE, "Move cursor a page up");
					cpiKeyHelp(KEY_NPAGE, "Move cursor a page down");
					cpiKeyHelp(KEY_HOME, "Move cursor home");
					cpiKeyHelp(KEY_END, "Move cursor end");
					if (editmode)
					{
						cpiKeyHelp(KEY_RIGHT, "Move cursor right");
						cpiKeyHelp(KEY_LEFT, "Move cursor left");
					} else {
						cpiKeyHelp(KEY_ALT_P, "Save playlist");
						cpiKeyHelp(KEY_INSERT, "Append file/directory into the playlist");
						cpiKeyHelp(KEY_RIGHT, "Append file/directory into the playlist");
						cpiKeyHelp(KEY_DELETE, "Remove file/directory from the playlist");
						cpiKeyHelp(KEY_LEFT, "Remove file/directory from the playlist");
						cpiKeyHelp(KEY_CTRL_RIGHT, "Append all files in current directory into the playlist");
						cpiKeyHelp(KEY_CTRL_LEFT, "Remove all files in current directory from the playlist");
						if (!win)
						{
							cpiKeyHelp(KEY_CTRL_UP, "Move the selected file up in playlist");
							cpiKeyHelp(KEY_CTRL_DOWN, "Move the selected file down in the playlist");
							cpiKeyHelp(KEY_CTRL_PGUP, "Move the selected file up one page in playlist");
							cpiKeyHelp(KEY_CTRL_PGDN, "Move the selected file down one page in the playlist");
						}
					}
					state = 2;
					goto superbreak;
				case KEY_EXIT:
				case KEY_ESC:
					return 0;
				case KEY_ALT_R:
					if ((m->file)&&(m->flags & MODLIST_FLAG_ISMOD))
					{
						if (!mdbGetModuleInfo(&mdbEditBuf, m->mdb_ref))
							return -1;
						mdbEditBuf.modtype.integer.i = mtUnRead;
						if (!mdbWriteModuleInfo(m->mdb_ref, &mdbEditBuf))
							return -1;
						m->flags &= ~MODLIST_FLAG_SCANNED;
					}
					break;
				case KEY_CTRL_BS:
				case KEY_ALT_S:
					scanposp=~0;
					scanposf=~0;
					break;
				case KEY_TAB:
					win=!win;
					break;
				case KEY_SHIFT_TAB:
				case KEY_ALT_E:
					editmode=!editmode;
				break;
				case KEY_ALT_I:
/* TODO-keys
				case KEY_ALT_TAB:*/
					fsInfoMode=(fsInfoMode+1)%5;
					break;
				case KEY_ALT_C:
					fsSetup();
					plSetTextMode(fsScrType);
					fsScrType=plScrType;
					fsScanDir(0);
					goto superbreak;
				case KEY_ALT_P:
					if (editmode)
						break;
					state = 4;
					goto superbreak;
				case KEY_F(1):
					if (!fsHelp2())
						return -1;
					plSetTextMode(fsScrType);
					break;
				case KEY_ALT_Z:
					fsScrType=(fsScrType==0)?7:0;
					plSetTextMode(fsScrType);
					break;
				case KEY_CTRL_ENTER:
				case KEY_ALT_ENTER:
					state = 3;
					goto superbreak;
				case _KEY_ENTER:
					if (editmode)
					{
						if (m->file && (m->flags & MODLIST_FLAG_ISMOD))
						{
							if (fsEditFileInfo(m))
							{
								state = 1;
								goto superbreak;
							}
							break;
						} else if (m->dir)
						{
							if (fsEditDirInfo(m))
							{
								state = 5;
								goto superbreak;
							}
							break;
						}
					}
					if (win)
					{
						if (!playlist->num)
							break;
						isnextplay=NextPlayPlaylist;
						return 1;
					} else {
						if (m->dir)
						{
							uint32_t olddirpath = dmCurDrive->cwd->dirdb_ref;
							unsigned int i;

							dirdbRef (olddirpath, dirdb_use_pfilesel);

							dmCurDrive = ocpdir_get_drive (m->dir);
							m->dir->ref (m->dir);
							dmCurDrive->cwd->unref (dmCurDrive->cwd);
							dmCurDrive->cwd = m->dir;

#ifdef _WIN32
							if (dmCurDrive->drivename[1] == ':')
							{
								dmLastActiveDriveLetter = dmCurDrive->drivename[0];
							}
#endif

							fsScanDir(0);
							i = modlist_find (currentdir, olddirpath);
							if (i >= 0)
							{
								currentdir->pos = i;
							}
							dirdbUnref(olddirpath, dirdb_use_pfilesel);
						} else if (m->file && (m->flags & MODLIST_FLAG_ISMOD))
						{
							nextplay=m;
							isnextplay=NextPlayBrowser;
							return 1;
						}
					}
					break;
				case KEY_UP:
				/*case 0x4800: // up*/
					if (editmode)
					{
						if (m && m->file)
						{
							if (plScrWidth>=180)
							{
								/*             /- Title
								 *             |    /- Type
								 *             |   |   /- Channels
								 *             |   |   |   /- Playtime
								 *             |   |   |   |   /- Composer
								 *             |   |   |   |   |   /- Style
								 *             |   |   |   |   |   |   /- Date
								 *             |   |   |   |   |   |   |   /- Comment
								 *             |   |   |   |   |   |   |   |   /- Artist
								 *             |   |   |   |   |   |   |   |   |   /- Album */
								editfilepos="\x00\x01\x02\x03\x00\x01\x04\x09\x08\x08"[editfilepos];
							} else if (plScrWidth>=132)
							{
								editfilepos="\x00\x02\x02\x03\x00\x01\x03\x04\x08\x08"[editfilepos];
							} else {
								editfilepos="\x00\x01\x09\x09\x00\x08\x01\x05\x04\x06"[editfilepos];
							}
						}
					} else if (!win)
					{
						if (currentdir->pos)
							currentdir->pos--;
					} else {
						if (playlist->pos)
							playlist->pos--;
					}
					break;
				case KEY_DOWN:
				/*case 0x5000: // down*/
					if (editmode)
					{
						if (m && m->file)
						{
							if (plScrWidth>=180)
							{
								/*             /- Title
								 *             |    /- Type
								 *             |   |   /- Channels
								 *             |   |   |   /- Playtime
								 *             |   |   |   |   /- Composer
								 *             |   |   |   |   |   /- Style
								 *             |   |   |   |   |   |   /- Date
								 *             |   |   |   |   |   |   |   /- Comment
								 *             |   |   |   |   |   |   |   |   /- Artist
								 *             |   |   |   |   |   |   |   |   |   /- Album */
								editfilepos="\x04\x05\x05\x05\x06\x07\x06\x07\x09\x07"[editfilepos];
							} else if (plScrWidth>=132)
							{
								editfilepos="\x04\x05\x01\x06\x07\x05\x05\x07\x09\x07"[editfilepos];
							} else {
								editfilepos="\x04\x06\x07\x07\x08\x07\x09\x07\x05\x03"[editfilepos];
							}
						}
					} else if (!win)
					{
						if ((currentdir->pos+1) < currentdir->num)
							currentdir->pos++;
					} else {
						if ((playlist->pos+1) < playlist->num)
							playlist->pos++;
					}
					break;
				case KEY_PPAGE:
				/*case 0x4900: //pgup*/
					{
						int sub = editmode?1:dirwinheight;
						if (!win)
						{
							if (currentdir->pos < sub)
								currentdir->pos = 0;
							else
								currentdir->pos -= sub;
						} else {
							if (playlist->pos < sub)
								playlist->pos = 0;
							else
								playlist->pos -= sub;
						}
					}
					break;
				case KEY_NPAGE:
				/*case 0x5100: //pgdn*/
					{
						int add = editmode?1:dirwinheight;
						if (!win)
						{
							if (currentdir->num <= (currentdir->pos + add))
								currentdir->pos = currentdir->num - 1;
							else
								currentdir->pos += add;
						} else {
							if (playlist->num <= (playlist->pos + add))
								playlist->pos = playlist->num - 1;
							else
								playlist->pos += add;
						}
					}
					break;
				case KEY_HOME:
				/*case 0x4700: //home*/
					if (editmode)
						break;
					if (!win)
						currentdir->pos=0;
					else
						playlist->pos=0;
					break;
				case KEY_END:
				/*case 0x4F00: //end*/
					if (editmode)
						break;
					if (!win)
						currentdir->pos=currentdir->num-1;
					else
						playlist->pos=playlist->num-1;
					break;
				case KEY_RIGHT:
				/*case 0x4D00: // right*/
					if (editmode)
					{
						if (m && m->file)
						{
							if (plScrWidth>=180)
							{
								/*             /- Title
								 *             |    /- Type
								 *             |   |   /- Channels
								 *             |   |   |   /- Playtime
								 *             |   |   |   |   /- Composer
								 *             |   |   |   |   |   /- Style
								 *             |   |   |   |   |   |   /- Date
								 *             |   |   |   |   |   |   |   /- Comment
								 *             |   |   |   |   |   |   |   |   /- Artist
								 *             |   |   |   |   |   |   |   |   |   /- Album */
								editfilepos="\x08\x02\x03\x03\x09\x05\x07\x07\x01\x05"[editfilepos];
							} else if (plScrWidth>=132)
							{
								editfilepos="\x08\x06\x03\x03\x09\x05\x06\x05\x02\x01"[editfilepos];
							} else {
								editfilepos="\x01\x01\x02\x02\x06\x03\x06\x07\x09\x09"[editfilepos];
							}
						}
						break;
					}
					/* fallthrough */
				case KEY_INSERT:
				/*case 0x5200: // add*/
					if (editmode)
						break;
					if (win)
					{
						if (m->flags & MODLIST_FLAG_ISMOD)
						{
							modlist_append (playlist, m);
						}
						/*playlist->pos=playlist->num-1; */
						scanposp=fsScanNames?0:~0;
					} else {
						if (m->dir)
						{
							if (!(fsReadDir (playlist, m->dir, curmask, RD_PUTRSUBS | RD_ISMODONLY)))
							{
								return -1;
							}
						} else if (m->file)
						{
							if (m->flags & MODLIST_FLAG_ISMOD)
							{
								modlist_append (playlist, m);
							}
							scanposp=fsScanNames?0:~0;
						}
					}
					break;
				case KEY_LEFT:
				/*case 0x4B00: // left*/
					if (editmode)
					{
						if (m && m->file)
						{
							if (plScrWidth>=180)
							{
								/*             /- Title
								 *             |    /- Type
								 *             |   |   /- Channels
								 *             |   |   |   /- Playtime
								 *             |   |   |   |   /- Composer
								 *             |   |   |   |   |   /- Style
								 *             |   |   |   |   |   |   /- Date
								 *             |   |   |   |   |   |   |   /- Comment
								 *             |   |   |   |   |   |   |   |   /- Artist
								 *             |   |   |   |   |   |   |   |   |   /- Album */
								editfilepos="\x00\x08\x01\x02\x04\x09\x06\x06\x00\x04"[editfilepos];
							} else if (plScrWidth>=132)
							{
								editfilepos="\x00\x09\x08\x02\x04\x07\x01\x07\x00\x04"[editfilepos];
							} else {
								editfilepos="\x00\x00\x03\x05\x04\x05\x04\x07\x08\x08"[editfilepos];
							}
						}
						break;
					}
					/* fall-through */
				case KEY_DELETE:
				/*case 0x5300: // del*/
					if (editmode)
						break;
					if (win)
					{
						modlist_remove(playlist, playlist->pos);
					} else {
						long f;

						if (m->dir)
						{
							struct modlist *tl = modlist_create();
							struct modlistentry *me;
							int f;
							if (!(fsReadDir (tl, m->dir, curmask, RD_PUTRSUBS)))
							{
								return -1;
							}
							for (i=0;i<tl->num;i++)
							{
								me=modlist_get (tl, i);
								assert(me->file);
								if ((f=modlist_find (playlist, me->file->dirdb_ref)) >= 0)
								{
									modlist_remove (playlist, f);
								}
							}
							modlist_free(tl);
						} else if (m->file)
						{
							if ((f = modlist_find (playlist, m->file->dirdb_ref)) >= 0)
							{
								modlist_remove (playlist, f);
							}
						}
					}
					break;
				case KEY_CTRL_RIGHT:
				/* case 0x7400: //ctrl-right */
				/* case 0x9200: //ctrl-insert TODO keys */
					{
						if (editmode)
							break;
						for (i=0; i<currentdir->num; i++)
						{
							struct modlistentry *me;
							me=modlist_get(currentdir, i);
							if (m->file)
							{
								if (me->flags & MODLIST_FLAG_ISMOD)
								{
									modlist_append (playlist, me);
								}
								scanposp=fsScanNames?0:~0;
							}
						}
						break;
					}
				case KEY_CTRL_LEFT:
				/* case 0x7300: //ctrl-left */
				/* case 0x9300: //ctrl-delete TODO keys */
					if (editmode)
						break;
					modlist_clear (playlist);
					break;
/*
    case 0x3000: // alt-b TODO keys
      if (m.mdb_ref<=0xFFFC)
      {
	mdbGetModuleInfo(mdbEditBuf, m.mdb_ref);
	mdbEditBuf.flags^=MDB_BIGMODULE;
	mdbWriteModuleInfo(m.mdb_ref, mdbEditBuf);
      }
      break;

    case 0x1100: // alt-w TODO keys
      if (m.mdb_ref<0xFFFC)
	fsSaveModInfo(m);
      break;
    case 0x1e00: // alt-a TODO keys
      if (editmode)
	break;
      fsSaveModInfoML(curlist);
      break;
*/
				case KEY_CTRL_UP:
				/* case 0x8d00: //ctrl-up */
					if (editmode||!win)
						break;
					if (!playlist->pos)
						break;
					modlist_swap(playlist, playlist->pos-1, playlist->pos);
					playlist->pos-=1;
					break;
				case KEY_CTRL_DOWN:
				/* case 0x9100: //ctrl-down  */
					if (editmode||!win)
						break;
					if ((playlist->pos+1)>=playlist->num)
						break;
					modlist_swap(playlist, playlist->pos, playlist->pos+1);
					playlist->pos++;
					break;
				case KEY_CTRL_PGUP:
				/* case 0x8400: //ctrl-pgup */
					if (editmode||!win)
						break;
					i=(playlist->pos>dirwinheight)?dirwinheight:playlist->pos;
					modlist_swap(playlist, playlist->pos, playlist->pos-i);
					playlist->pos-=i;
					break;
				case KEY_CTRL_PGDN:
				/* case 0x7600: //ctrl-pgdown */
					if (editmode||!win)
						break;
					i=((playlist->num-1-playlist->pos)>dirwinheight)?dirwinheight:(playlist->num-1-playlist->pos);
					modlist_swap(playlist, playlist->pos, playlist->pos+i);
					playlist->pos+=i;
					break;
/*    case 0x7700: //ctrl-home TODO keys
      if (editmode||!win)
	break;
      playlist.remove(playlist.pos, 1);
      playlist.insert(0, &m, 1);
      playlist.pos=0;
      break;
    case 0x7500: //ctrl-end TODO keys
      if (editmode||!win)
	break;
      playlist.remove(playlist.pos, 1);
      playlist.insert(playlist.num, &m, 1);
      playlist.pos=playlist.num-1;
      break;*/
			}
		}
	}
  /*return 0; the above while loop doesn't go to this point */
}

#warning we can add a dir->SaveFile API.....
static int fsSavePlayList(const struct modlist *ml)
{
	static int state = 0;
	static char *temppath;
	char *newpath;
	int mlTop=plScrHeight/2-2;
	unsigned int i;
	char *dr;
	char *di;
	char *fn;
	char *ext;
	FILE *f;
	int retval;

	if (state == 0)
	{
		dirdbGetFullname_malloc (dirdbcurdirpath, &temppath, DIRDB_FULLNAME_ENDSLASH);
		state = 1;
	}

	displayvoid(mlTop+1, 5, plScrWidth-10);
	displayvoid(mlTop+2, 5, plScrWidth-10);
	displayvoid(mlTop+3, 5, plScrWidth-10);
	displaystr(mlTop, 4, 0x04, "\xda", 1);
	for (i=5;i<(plScrWidth-5);i++)
		displaystr(mlTop, i, 0x04, "\xc4", 1);
	displaystr(mlTop, plScrWidth-5, 0x04, "\xbf", 1);
	displaystr(mlTop+1, 4, 0x04, "\xb3", 1);
	displaystr(mlTop+2, 4, 0x04, "\xb3", 1);
	displaystr(mlTop+3, 4, 0x04, "\xb3", 1);
	displaystr(mlTop+1, plScrWidth-5, 0x04, "\xb3", 1);
	displaystr(mlTop+2, plScrWidth-5, 0x04, "\xb3", 1);
	displaystr(mlTop+3, plScrWidth-5, 0x04, "\xb3", 1);
	displaystr(mlTop+4, 4, 0x04, "\xc0", 1);
	for (i=5;i<(plScrWidth-5);i++)
		displaystr(mlTop+4, i, 0x04, "\xc4", 1);
	displaystr(mlTop+4, plScrWidth-5, 0x04, "\xd9", 1);

	displaystr(mlTop+1, 5, 0x0b, "Store playlist, please give filename (.pls format):", 50);
	displaystr(mlTop+3, 5, 0x0b, "-- Abort with escape --", 23);

	retval = EditStringUTF8(mlTop+2, 5, plScrWidth-10, &temppath);
	if (retval > 0)
	{
		return 1;
	}

	if (retval < 0)
	{ /* abort */
		free (temppath);
		state = 0;
		return 0;
	}

	splitpath4_malloc(temppath, &dr, &di, &fn, &ext);
	if (!*ext)
	{
		free (ext);
		ext = strdup (".pls");
	}
	free (temppath); temppath = 0;

	if (strcmp(dr, "file:"))
	{
		fprintf(stderr, "[filesel] file: is the only supported drive currently\n");
		free (dr);
		free (di);
		free (fn);
		free (ext);
		return 0;
	}

	free (dr); dr = 0;
	newpath = malloc (strlen (di) + strlen (fn) + strlen (ext) + 1);
	sprintf (newpath, "%s%s%s", di, fn, ext);
	free (fn); fn = 0;
	free (ext); ext = 0;
	free (di); di = 0;

	if (!(f=fopen(newpath, "w")))
	{
		fprintf (stderr, "Failed to create file %s: %s\n", newpath, strerror (errno));
		free (newpath);
		return 0;
	}
	free (newpath); newpath = 0;
	fprintf(f, "[playlist]\n");
	fprintf(f, "NumberOfEntries=%d\n", ml->num);

	for (i=0; i<ml->num; i++)
	{
		char *npath;
		struct modlistentry *m;
		fprintf(f, "File%d=",i+1);
		m=modlist_get(ml, i);
		if (m->file)
		{
#ifdef _WIN32
			npath = dirdbDiffPath (dirdbcurdirpath, m->file->dirdb_ref, DIRDB_DIFF_WINDOWS_SLASH);
#else
			npath = dirdbDiffPath (dirdbcurdirpath, m->file->dirdb_ref, 0);
#endif
			if (npath)
			{
				fputs (npath, f);
				free (npath);
			}
		}
		fprintf(f, "\n");

	}
	fclose(f);
	fsScanDir(1);

	return 0;
}

int fsMatchFileName12(const char *a, const char *b)
{
	int i;
	for (i=0; i<12; i++, a++, b++)
		if ((i!=8)&&(*b!='?')&&(*a!=*b))
			break;
	return i==12;
}

static struct interfacestruct *plInterfaces = 0;
void plRegisterInterface(struct interfacestruct *_interface)
{
	_interface->next = plInterfaces;
	plInterfaces = _interface;
}

void plUnregisterInterface(struct interfacestruct *_interface)
{
	struct interfacestruct **curr = &plInterfaces;

	while (*curr)
	{
		if (*curr == _interface)
		{
			*curr = (*curr)->next;
			return;
		}
		curr = &((*curr)->next);
	}

	fprintf(stderr, __FILE__ ": Failed to unregister interface %s\n", _interface->name);
}

void plFindInterface(struct moduletype modtype, const struct interfacestruct **in, const struct cpifaceplayerstruct **cp)
{
	int i;
	*in = 0;
	*cp = 0;

	for (i=0; i < fsTypesCount; i++)
	{
		if (fsTypes[i].modtype.integer.i == modtype.integer.i)
		{
			struct interfacestruct *curr = plInterfaces;

			if (!fsTypes[i].interfacename)
			{
				return;
			}

			while (curr)
			{
				if (!strcmp(curr->name, fsTypes[i].interfacename))
				{
					*in = curr;
					*cp = fsTypes[i].cp;
					return;
				}
				curr = curr->next;
			}
			fprintf(stderr, __FILE__ ": Unable to find interface for filetype %s\n", modtype.string.c);
			return;
		}
	}
	fprintf(stderr, __FILE__ ": Unable to find moduletype: %4s\n", modtype.string.c);
	return;
}
