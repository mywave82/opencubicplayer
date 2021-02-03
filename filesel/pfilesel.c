/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
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
#include "filesystem-drive.h"
#include "filesystem-bzip2.h"
#include "filesystem-gzip.h"
#include "filesystem-playlist.h"
#include "filesystem-playlist-m3u.h"
#include "filesystem-playlist-pls.h"
#include "filesystem-setup.h"
#include "filesystem-tar.h"
#include "filesystem-unix.h"
#include "filesystem-zip.h"
#include "mdb.h"
#include "modlist.h"
#include "pfilesel.h"
#include "stuff/compat.h"
#include "stuff/framelock.h"
#include "stuff/poutput.h"
#include "stuff/utf-8.h"

#include "pfilesel-charset.c"

static char fsScanDir(int pos);
static struct modlist *currentdir=NULL;
static struct modlist *playlist=NULL;

#define dirdbcurdirpath (dmCurDrive->cwd->dirdb_ref)
static char *curmask;

struct dmDrive *dmCurDrive=0;

struct preprocregstruct *plPreprocess = 0;

static int fsSavePlayList(const struct modlist *ml);

static struct interfacestruct *plInterfaces;

struct fsReadDir_token_t
{
#if 0
	struct dmDrive *drive;
#endif
	struct modlist *ml;
	const char     *mask;

	unsigned long   opt;

	int             cancel_recursive;
	char           *parent_displaydir;
};

static void fsReadDir_file (void *_token, struct ocpfile_t *file)
{
	struct fsReadDir_token_t *token = _token;
	char *curext;
	char *childpath = 0;
#ifndef FNM_CASEFOLD
	char *childpath_upper;
	char *iterate;
#endif

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
								while (ekbhit())
								{
									if (egetch() == ' ')
									{
										token->cancel_recursive = 1;
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

#ifndef FNM_CASEFOLD
	if ((childpath_upper = strdup(childpath)))
	{
		for (iterate = childpath_upper; *iterate; iterate++)
			*iterate = toupper(*iterate);
	} else {
		perror("pfilesel.c: strdup() failed");
		goto out;
	}

	if ((fnmatch(token->mask, childpath_upper, 0))||(!fsIsModule(curext)))
	{
		free (childpath_upper);
		goto out;
	}
	free(childpath_upper);
#else
	if ((fnmatch(token->mask, childpath, FNM_CASEFOLD))||(!fsIsModule(curext)))
	{
		goto out;
	}
#endif

	modlist_append_file (token->ml, file); /* modlist_append() will do refcount on the file */
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
#ifndef FNM_CASEFOLD
	char *mask_upper;
	int i;
#endif
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
	token.mask = strdup (mask);
	for (i=0; token.mask[i]; i++)
	{
		token.mask[i] = toupper (token.mask[i]);
	}
#else
	token.mask = mask;
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
				if (egetch() == ' ')
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

static char fsTypeCols[256]; /* colors */
const char *fsTypeNames[256] = {0}; /* type description */

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
	modlist_append_file (playlist, file); /* modlist_append calls file->ref (file); for us */
}

static void addfiles_dir (void *token, struct ocpdir_t *dir)
{
}

static int initRootDir(const char *sec)
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
			if (!(filename=cfGetProfileString2(sec, "CommandLine_Files", buffer, NULL)))
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

#ifdef __W32__
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
					ekbhit();
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
		if (!(filename=cfGetProfileString2(sec, "CommandLine_Files", buffer, NULL)))
		{
			break;
		}

#ifdef __W32__
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
				char *childpath, *curext;

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
	currentpath2 = cfGetProfileString2(sec, "fileselector", "path", ".");
	if (strlen (currentpath2) && strcmp (currentpath2, "."))
	{
		uint32_t newcurrentpath;
		struct ocpdir_t *cwd = 0;

		struct dmDrive *dmNewDrive=0;

		newcurrentpath = dirdbResolvePathWithBaseAndRef(dmFILE->cwd->dirdb_ref, currentpath2, DIRDB_RESOLVE_DRIVE, dirdb_use_pfilesel);

		if (!filesystem_resolve_dirdb_dir (newcurrentpath, &dmNewDrive, &cwd) == 0)
		{
			dmCurDrive = dmNewDrive;
			assert (dmCurDrive->cwd);
			dmCurDrive->cwd->unref (dmCurDrive->cwd);
			dmCurDrive->cwd = cwd;
		}

		dirdbUnref (newcurrentpath, dirdb_use_pfilesel);
	}

	return 1;
}

static struct modlistentry *nextplay=NULL;
typedef enum {NextPlayNone, NextPlayBrowser, NextPlayPlaylist} NextPlay;
static NextPlay isnextplay = NextPlayNone;
/* These guys has with rendering todo and stuff like that */
static unsigned short dirwinheight;
static char quickfind[12];
static char quickfindpos;
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
int fsScanMIF=1;
int fsScrType=0;
int fsEditWin=1;
int fsColorTypes=1;
int fsInfoMode=0;
int fsPutArcs=1;
int fsWriteModInfo=1;
static int fsPlaylistOnly=0;

int fsFilesLeft(void)
{
	return (isnextplay!=NextPlayNone)||playlist->num;
}

/* returns zero if fsReadDir() fails */
/* pos = 0, move cursor to the top
 * pos = 1, maintain current cursor position
 * pos = 2, move cursor one up
 */
static char fsScanDir (int pos)
{
	unsigned int op=0;
	switch (pos)
	{
		case 0:
			op=0;
			break;
		case 1:
			op=currentdir->pos;
			break;
		case 2:
			op=currentdir->pos?(currentdir->pos-1):0;
			break;
	}
	modlist_clear (currentdir);
	nextplay=0;

	if (!fsReadDir (currentdir, dmCurDrive->cwd, curmask, RD_PUTDRIVES | RD_PUTSUBS | (fsScanArcs?RD_ARCSCAN:0)))
	{
		return 0;
	}

	modlist_sort(currentdir);
	currentdir->pos=(op>=currentdir->num)?(currentdir->num-1):op;
	quickfindpos=0;
	scanposf=fsScanNames?0:~0;

	adbMetaCommit ();

	return 1;
}

void fsRescanDir(void)
{
	fsScanDir(1);
	conSave();
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

	if (!(info->flags1&MDB_VIRTUAL))
	{
		if (m->file)
		{
			*filehandle = m->file->open (m->file);
		}

		if (*filehandle)
		{
			if (!mdbInfoRead(m->mdb_ref)&&*filehandle)
			{
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

	if (!(info->flags1&MDB_VIRTUAL))
	{
		if (m->file)
		{
			*filehandle = m->file->open (m->file);
		}

		if (*filehandle)
		{
			if (!mdbInfoRead(m->mdb_ref)&&*filehandle)
			{
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

int fsPreInit(void)
{
	int i;

	const char *sec=cfGetProfileString(cfConfigSec, "fileselsec", "fileselector");

	const char *modexts;
	int extnum;

	curmask = strdup ("*");

	adbMetaInit(); /* archive database cache - ignore failures */

	if (!mdbInit())
		return 0;

	if (!dirdbInit())
		return 0;

	/* this on the other hand is VERY nice */

	for (i=0; i<256; i++)
	{
		char secname[20];
		sprintf(secname, "filetype %d", i);

		fsTypeCols[i]=cfGetProfileInt(secname, "color", 7, 10);
		fsTypeNames[i]=cfGetProfileString(secname, "name", "");
	}

	/* what are these?????
	dmTree=0;
	dmReloc=0;
	dmMaxNodes=0;
	dmNumNodes=0;
	*/

	modexts=cfGetProfileString2(sec, "fileselector", "modextensions", "MOD XM S3M MID MTM DMF ULT 669 NST WOW OKT PTM AMS MDL");
	extnum=cfCountSpaceList(modexts, 3);
	for (i=0; i<extnum; i++)
	{
		char t[4];
		cfGetSpaceListEntry(t, &modexts, 3);
		strupr(t);
		fsRegisterExt(t);
	}
	fsRegisterExt("DEV");

	fsScrType=cfGetProfileInt2(cfScreenSec, "screen", "screentype", 7, 10)&7;
	fsColorTypes=cfGetProfileBool2(sec, "fileselector", "typecolors", 1, 1);
	fsEditWin=cfGetProfileBool2(sec, "fileselector", "editwin", 1, 1);
	fsWriteModInfo=cfGetProfileBool2(sec, "fileselector", "writeinfo", 1, 1);
	fsScanMIF=cfGetProfileBool2(sec, "fileselector", "scanmdz", 1, 1);
	fsScanInArc=cfGetProfileBool2(sec, "fileselector", "scaninarcs", 1, 1);
	fsScanNames=cfGetProfileBool2(sec, "fileselector", "scanmodinfo", 1, 1);
	fsScanArcs=cfGetProfileBool2(sec, "fileselector", "scanarchives", 1, 1);
	fsListRemove=cfGetProfileBool2(sec, "fileselector", "playonce", 1, 1);
	fsListScramble=cfGetProfileBool2(sec, "fileselector", "randomplay", 1, 1);
	fsPutArcs=cfGetProfileBool2(sec, "fileselector", "putarchives", 1, 1);
	fsLoopMods=cfGetProfileBool2(sec, "fileselector", "loop", 1, 1);
	fsListRemove=cfGetProfileBool("commandline_f", "r", fsListRemove, 0);
	fsListScramble=!cfGetProfileBool("commandline_f", "o", !fsListScramble, 1);
	fsLoopMods=cfGetProfileBool("commandline_f", "l", fsLoopMods, 0);
	fsPlaylistOnly=!!cfGetProfileString("commandline", "p", 0);

	filesystem_drive_init ();

	filesystem_unix_init ();
	dmCurDrive=dmFILE;

	filesystem_bzip2_register ();
	filesystem_gzip_register ();
	filesystem_m3u_register ();
	filesystem_pls_register ();
	filesystem_setup_register ();
	filesystem_tar_register ();
	filesystem_zip_register ();

	currentdir=modlist_create();
	playlist=modlist_create();

	if (!initRootDir(sec))
		return 0;

	return 1;
}

int fsLateInit(void)
{
	if (plVidType == vidModern)
	{
		fsScrType=8;
	}

	return 1;
}

struct interfacestruct *CurrentVirtualInterface;
static int VirtualInterfaceInit (struct moduleinfostruct *info, struct ocpfilehandle_t *fi)
{
	char name[128];
	int res;

	fi->seek_set (fi, 0);
	res = fi->read (fi, name, sizeof (name) - 1);
	if (res <= 0)
	{
		fi->seek_set (fi, 0);
		return 0;
	}
	name[res] = 0;
	fi->seek_set (fi, 0);

	for (CurrentVirtualInterface = plInterfaces; CurrentVirtualInterface; CurrentVirtualInterface = CurrentVirtualInterface->next)
	{
		if (!strcmp (CurrentVirtualInterface->name, name))
		{
			int res = CurrentVirtualInterface->Init (info, fi);
			if (!res)
			{
				CurrentVirtualInterface = 0;
				return 0;
			}
			return 1;
		}
	}
	return 0;
}

static interfaceReturnEnum VirtualInterfaceRun (void)
{
	if (CurrentVirtualInterface && CurrentVirtualInterface->Run)
	{
		return CurrentVirtualInterface->Run ();
	}
	return interfaceReturnNextAuto; /* good choice? */
}

static void VirtualInterfaceClose (void)
{
	if (CurrentVirtualInterface && CurrentVirtualInterface->Close)
	{
		return CurrentVirtualInterface->Close ();
	}
}

static struct interfacestruct VirtualInterface = {VirtualInterfaceInit, VirtualInterfaceRun, VirtualInterfaceClose, "VirtualInterface", NULL};

int fsInit(void)
{
	/*
	if (!mifInit())  .mdz tag cache/reader
		return 0;
	*/

	plRegisterInterface (&VirtualInterface);

	if (!fsScanDir(0))
		return 0;

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

	filesystem_unix_done ();
	filesystem_drive_done ();
	dmCurDrive = 0;

	adbMetaClose();
	mdbClose();
	/*
	mifClose();
	*/
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

static void displayfile(const unsigned int y, unsigned int x, unsigned int width, /* TODO const*/ struct modlistentry *m, const unsigned char sel)
{
	unsigned char col;
	unsigned short sbuf[CONSOLE_MAX_X];
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
		mdbGetModuleInfo(&mi, m->mdb_ref);
#if 0
		if (mi.flags1&MDB_PLAYLIST)
		{
			col=0x0f;
		}
#endif
	} else {
		memset(&mi, 0, sizeof(mi));
		col=0x0f;
	}
	if (sel==1)
	{
		col|=0x80;
	}

	if (fsInfoMode==4)
	{
		//writestring(sbuf, 0, col, "", width);
		if (sel==2)
		{
			displaystr (y, x + 0,         0x07, "->", 2);
			displaystr (y, x + width - 2, 0x07, "<-", 2);
		} else {
			displaystr (y, x + 0,          col, "  ", 2);
			displaystr (y, x + width - 2,  col, "  ", 2);
		}

		if (m->file)
		{
			if (mi.modtype==0xFF)
				col&=~0x08;
			else if (fsColorTypes)
			{
				col&=0xF8;
				col|=fsTypeCols[mi.modtype&0xFF];
			}
		}

		if (m->dir && !strcmp (m->utf8_8_dot_3, ".."))
		{
			displaystr_utf8 (y, x + 2, col, m->utf8_8_dot_3, width - 13);
		} else {
			char *temp = 0;

			if (m->file)
			{
				dirdbGetName_internalstr (m->file->dirdb_ref, &temp);
			} else if (m->dir)
			{
				dirdbGetName_internalstr (m->dir->dirdb_ref, &temp);
			}
			displaystr_utf8 (y, x + 2, col, temp ? temp : "???", width - 13);
		}
		if (m->dir)
		{
			if (m->flags&MODLIST_FLAG_DRV)
			{
				displaystr (y, x + width - 7, col, "<DRV>", 5);
			} else if (m->dir->is_playlist)
			{
				displaystr (y, x + width - 7, col, "<PLS>", 5);
			} else if (m->dir->is_archive)
			{
				displaystr (y, x + width - 7, col, "<ARC>", 5);
			} else {
				displaystr (y, x + width - 7, col, "<DIR>", 5);
			}
		} else { /* m->file */
			char buffer[20];

			if (mi.size<1000000000)
			{
				snprintf (buffer, sizeof (buffer), "%9"PRId32, mi.size);
				displaystr (y, x + width - 11, (mi.flags1&MDB_BIGMODULE)?((col&0xF0)|0x0C):col, buffer, 9);
			} else {
				snprintf (buffer, sizeof (buffer), "%8"PRId32, mi.size);
				displaystr (y, x + width - 10, col, buffer, 8);
			}
		}
	} else {
		if (sel==2)
		{
			displaystr (y, x + 0,         0x07, "->", 2);
			displaystr (y, x + width - 2, 0x07, "<-", 2);
		} else {
			displaystr (y, x + 0,          col, "  ", 2);
			displaystr (y, x + width - 2,  col, "  ", 2);
		}
		if (width > 88)
		{
			displaystr_utf8 (y, x + 2, col, m->utf8_16_dot_3, 20);
			displaystr (y, x + 22, col, "  ", 2);
			x += 24;
			width -= 26;
		} else {
			displaystr_utf8 (y, x + 2, col, m->utf8_8_dot_3, 12);
			displaystr (y, x + 14, col, "  ", 2);
			x += 16;
			width -= 18;
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
#warning TODO, replace writestring() with displaystr(), but remember the spaces
			if (mi.modtype==0xFF)
				col&=~0x08;
			else if (fsColorTypes)
			{
				col&=0xF8;
				col|=fsTypeCols[mi.modtype&0xFF];
			}

			writestring(sbuf, 0, col, "", width);

			if (width>=100) /* 132 or bigger screen this will imply */
			{
				if (fsInfoMode&1)
				{
					if (mi.comment[0])
						writestring(sbuf, 0, col, mi.comment, 63);
					if (mi.style[0])
						writestring(sbuf, 69, col, mi.style, 31);
				} else { /* 32 + PAD2 + 2 + PAD1 + 6 + PAD2 + 32 + PAD1 + 11 + PAD2 + 9 => 100         + 16 prefix + 2 suffix  => 118 GRAND TOTAL WIDTH */
					if (mi.modname[0])
						writestring(sbuf, 0, col, mi.modname, 32);
					if (mi.channels)
						writenum(sbuf, 34, col, mi.channels, 10, 2, 1);
				        if (mi.playtime)
				        {
						writenum(sbuf, 37, col, mi.playtime/60, 10, 3, 1);
						writestring(sbuf, 40, col, ":", 1);
						writenum(sbuf, 41, col, mi.playtime%60, 10, 2, 0);
					}
					if (mi.composer[0])
						writestring(sbuf, 45, col, mi.composer, 32);
					if (mi.date)
					{
						if (mi.date&0xFF)
						{
							writestring(sbuf, 80, col, ".", 3);
							writenum(sbuf, 78, col, mi.date&0xFF, 10, 2, 1);
						}
						if (mi.date&0xFFFF)
						{
							writestring(sbuf, 83, col, ".", 3);
							writenum(sbuf, 81, col, (mi.date>>8)&0xFF, 10, 2, 1);
						}
						if (mi.date>>16)
						{
							writenum(sbuf, 84, col, mi.date>>16, 10, 4, 1);
							if (!((mi.date>>16)/100))
								writestring(sbuf, 85, col, "'", 1);
						}
					}
					if (mi.size<1000000000)
						writenum(sbuf, 90, (mi.flags1&MDB_BIGMODULE)?((col&0xF0)|0x0C):col, mi.size, 10, 9, 1);
					else
						writenum(sbuf, 91, col, mi.size, 16, 8, 0);
				}
			} else switch (fsInfoMode)
			{
				case 0:
					writestring(sbuf, 0, col, mi.modname, 32);
					if (mi.channels)
						writenum(sbuf, 34, col, mi.channels, 10, 2, 1);
					if (mi.size<1000000000)
						writenum(sbuf, 38, (mi.flags1&MDB_BIGMODULE)?((col&0xF0)|0x0C):col, mi.size, 10, 9, 1);
					else
						writenum(sbuf, 39, col, mi.size, 16, 8, 0);
					break;
				case 1:
					if (mi.composer[0])
						writestring(sbuf, 0, col, mi.composer, 32);
					if (mi.date)
					{
						if (mi.date&0xFF)
						{
							writestring(sbuf, 39, col, ".", 3);
							writenum(sbuf, 37, col, mi.date&0xFF, 10, 2, 1);
						}
						if (mi.date&0xFFFF)
						{
							writestring(sbuf, 42, col, ".", 3);
							writenum(sbuf, 40, col, (mi.date>>8)&0xFF, 10, 2, 1);
						}
						if (mi.date>>16)
						{
							writenum(sbuf, 43, col, mi.date>>16, 10, 4, 1);
							if (!((mi.date>>16)/100))
								writestring(sbuf, 44, col, "'", 1);
						}
					}

					break;
				case 2:
					if (mi.comment[0])
						writestring(sbuf, 0, col, mi.comment, width);
					break;
				case 3:
					if (mi.style[0])
						writestring(sbuf, 0, col, mi.style, 31);
					if (mi.playtime)
					{
						writenum(sbuf, 41, col, mi.playtime/60, 10, 3, 1);
						writestring(sbuf, 44, col, ":", 1);
						writenum(sbuf, 45, col, mi.playtime%60, 10, 2, 0);
					}
					break;
			}
			displaystrattr(y, x, sbuf, width);
		}
	}
}

static void fsShowDirBottom80File (int Y, int selecte, uint16_t *sbuf, const struct modlistentry *mle, struct moduleinfostruct *mi, const char *modtype, const char *npath)
{
	writestring (sbuf, 0, 0x07, "  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa   \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa   title: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", plScrWidth - 13 );
	writestring (sbuf, plScrWidth - 13 , 0x07, "   type: \xfa\xfa\xfa\xfa ", 80);

	if (mle->file)
	{
		writenum(sbuf, 15, 0x0F, mi->size, 10, 10, 1);

		if (mi->flags1&MDB_BIGMODULE)
			writestring(sbuf, 25, 0x0F, "!", 1);
	}
	if (mi->modname[0])
	{
		int w=plScrWidth - 48;
		int l=sizeof(mi->modname);
		if (l>w)
			l=w;
		writestring(sbuf, 35, /*(selecte==0)?0x8F:*/0x0F, mi->modname, l);
		writestring(sbuf, 35+l, /*(selecte==0)?0x8F:*/0x0F, "", w-l);
	}
	if (selecte==0)
		markstring(sbuf, 35, plScrWidth - 48);
	if (*modtype)
		writestring(sbuf, plScrWidth - 4, /*(selecte==1)?0x8F:*/0x0F, modtype, 4);
	if (selecte==1)
		markstring(sbuf, plScrWidth - 4, 4);

	displaystrattr (Y + 0, 0, sbuf, plScrWidth);
	displaystr_utf8 (Y + 2, 2, 0x0F, mle->utf8_8_dot_3, 12);

	writestring(sbuf, 0, 0x07, "   composer: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", plScrWidth - 35);
	writestring(sbuf, plScrWidth - 35, 0x07, "   date:     \xfa\xfa.\xfa\xfa.\xfa\xfa\xfa\xfa            ", 35);

	if (mi->date)
	{
		if (mi->date&0xFF)
		{
			writestring(sbuf, plScrWidth - 20, 0x0F, ".", 3);
			writenum(sbuf, plScrWidth - 22, 0x0F, mi->date&0xFF, 10, 2, 1);
		}
		if (mi->date&0xFFFF)
		{
			writestring(sbuf, plScrWidth - 17, 0x0F, ".", 3);
			writenum(sbuf, plScrWidth - 19, 0x0F, (mi->date>>8)&0xFF, 10, 2, 1);
		}
		if (mi->date>>16)
		{
			writenum(sbuf, plScrWidth - 16, 0x0F, mi->date>>16, 10, 4, 1);
			if (!((mi->date>>16)/100))
				writestring(sbuf, plScrWidth - 15, 0x0F, "'", 1);
		}

	}
	if (selecte==6)
		markstring(sbuf, plScrWidth - 22, 10);
	if (mi->composer[0])
	{ /* we pad up here */
		int w=plScrWidth - 47;
		int l=sizeof(mi->composer);
		if (l>w)
			l=w;
		writestring(sbuf, 13, 0x0F, mi->composer, l);
		writestring(sbuf, 13+l, 0x0F, "", w-l);
	}

	if (selecte==4)
		markstring(sbuf, 13, plScrWidth - 48);

	displaystrattr (Y + 1, 0, sbuf, plScrWidth);

	writestring(sbuf, 0, 0x07, "   style:    \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", plScrWidth - 35);
	writestring(sbuf, plScrWidth - 35, 0x07, "   playtime: \xfa\xfa\xfa:\xfa\xfa   channels: \xfa\xfa ", 35);

	if (mi->channels)
		writenum(sbuf, plScrWidth - 3, 0x0F, mi->channels, 10, 2, 1);
	if (selecte==2)
		markstring(sbuf, plScrWidth - 3, 2);
	if (mi->playtime)
	{
		writenum(sbuf, plScrWidth - 22, 0x0F, mi->playtime/60, 10, 3, 1);
		writestring(sbuf, plScrWidth - 19, 0x0F, ":", 1);
		writenum(sbuf, plScrWidth - 18, 0x0F, mi->playtime%60, 10, 2, 0);

	}
	if (selecte==3)
		markstring(sbuf, plScrWidth - 22, 6);
	if (mi->style[0])
	{ /* we pad up here */
		int w=plScrWidth - 48;
		int l=sizeof(mi->style);
		if (l>w)
			l=w;
		writestring(sbuf, 13, 0x0F, mi->style, l);
		writestring(sbuf, 13+l, 0x0F, "", w-l);
	}

	if (selecte==5)
		markstring(sbuf, 13, plScrWidth - 48);

	displaystrattr (Y + 2, 0, sbuf, plScrWidth);

	writestring(sbuf, 0, 0x07, "   comment:  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", plScrWidth - 4);
	writestring(sbuf, plScrWidth - 4, 0x07, "    ", 4);

	if (mi->comment[0])
	{ /* we pad up here */
		int w=plScrWidth - 17;
		int l=sizeof(mi->comment);
		if (l>w)
			l=w;
		writestring(sbuf, 13, 0x0F, mi->comment, l);
		writestring(sbuf, 13+l, 0x0F, "", w-l);
	}
	if (selecte==7)
		markstring(sbuf, 13, plScrWidth - 17);
	displaystrattr (Y + 3, 0, sbuf, plScrWidth);

	displaystr (Y + 4, 0, 0x07, "   long: ", 9);

	displaystr_utf8_overflowleft (Y + 4, 10, 0x0F, npath ? npath : "", plScrWidth - 10);
}

static void fsShowDirBottom132File (int Y, int selecte, uint16_t *sbuf, const struct modlistentry *mle, struct moduleinfostruct *mi, const char *modtype, const char *npath)
{
	writestring(sbuf, 0, 0x07, "  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa    \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa      title:    ", 42);

	fillstr(sbuf, 42, 0x07, 0xfa, plScrWidth - 100);
	writestring(sbuf, plScrWidth - 59, 0x07, "       type: \xfa\xfa\xfa\xfa     channels: \xfa\xfa      playtime: \xfa\xfa\xfa:\xfa\xfa   ", 59);

	if (mle->file)
	{
		writenum(sbuf, 16, 0x0F, mi->size, 10, 10, 1);

		if (mi->flags1&MDB_BIGMODULE)
			writestring(sbuf, 25, 0x0F, "!", 1);
	}
	if (mi->modname[0])
	{ /* we pad up here */
		int w=plScrWidth - 100;
		int l=sizeof(mi->modname);
		if (l>w)
			l=w;
		writestring(sbuf, 42, 0x0F, mi->modname, l);
		writestring(sbuf, 42+l, 0x0F, "", w-l);
	}
	if (selecte==0)
		markstring(sbuf, 42, plScrWidth - 100);
	if (*modtype)
		writestring(sbuf, plScrWidth - 46, 0x0F, modtype, 4);
	if (selecte==1)
		markstring(sbuf, plScrWidth - 46, 4);
	if (mi->channels)
		writenum(sbuf, plScrWidth - 27, 0x0F, mi->channels, 10, 2, 1);
	if (selecte==2)
		markstring(sbuf, plScrWidth - 27, 2);

	if (mi->playtime)
	{
		writenum(sbuf, plScrWidth - 9, 0x0F, mi->playtime/60, 10, 3, 1);
		writestring(sbuf, plScrWidth - 6, 0x0F, ":", 1);
		writenum(sbuf, plScrWidth - 5, 0x0F, mi->playtime%60, 10, 2, 0);
	}

	if (selecte==3)
		markstring(sbuf, plScrWidth - 9, 6);

	displaystrattr (Y + 0, 0, sbuf, plScrWidth);
	displaystr_utf8 (Y + 0, 2, 0x0F, mle->utf8_8_dot_3, 12);

	writestring(sbuf, 0, 0x07, "                                composer: ", 42);
	fillstr(sbuf, 42, 0x07, 0xfa, plScrWidth - 100);
	writestring(sbuf, plScrWidth - 58, 0x07, "     style: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa               ", 58);

	if (mi->composer[0])
	{ /* we pad up here */
		int w=plScrWidth - 100;
		int l=sizeof(mi->composer);
		if (l>w)
			l=w;
		writestring(sbuf, 42, 0x0F, mi->composer, l);
		writestring(sbuf, 42+l, 0x0F, "", w-l);
	}

	if (selecte==4)
		markstring(sbuf, 42, plScrWidth - 100);
	if (mi->style[0])
		writestring(sbuf, plScrWidth - 46, 0x0F, mi->style, 31);
	if (selecte==5)
		markstring(sbuf, plScrWidth - 46, 31);

	displaystrattr (Y + 1, 0, sbuf, plScrWidth);

	writestring(sbuf, 0, 0x07, "                                date:     \xfa\xfa.\xfa\xfa.\xfa\xfa\xfa\xfa     comment: ", 66);
	fillstr(sbuf, 66, 0x07, 0xfa,  plScrWidth - 69);
	writestring(sbuf, plScrWidth - 3, 0x07, "   ", 3);

	if (mi->date)
	{
		if (mi->date&0xFF)
		{
			writestring(sbuf, 44, 0x0F, ".", 3);
			writenum(sbuf, 42, 0x0F, mi->date&0xFF, 10, 2, 1);
		}
		if (mi->date&0xFFFF)
		{
			writestring(sbuf, 47, 0x0F, ".", 3);
			writenum(sbuf, 45, 0x0F, (mi->date>>8)&0xFF, 10, 2, 1);
		}
		if (mi->date>>16)
		{
			writenum(sbuf, 48, 0x0F, mi->date>>16, 10, 4, 1);
			if (!((mi->date>>16)/100))
				writestring(sbuf, 49, 0x0F, "'", 1);
		}
	}

	if (selecte==6)
		markstring(sbuf, 42, 10);
	if (mi->comment[0])
	{ /* we pad up here */
		int w=plScrWidth - 69;
		int l=sizeof(mi->comment);
		if (l>w)
			l=w;
		writestring(sbuf, 66, 0x0F, mi->comment, l);
		writestring(sbuf, 66+l, 0x0F, "", w-l);
	}
	if (selecte==7)
		markstring(sbuf, 66, plScrWidth - 69);
	displaystrattr (Y + 2, 0, sbuf, plScrWidth);

	displaystr (Y + 3, 0, 0x07, "    long: ", 10);

	displaystr_utf8_overflowleft (Y + 3, 10, 0x0F, npath ? npath : "", plScrWidth - 10);
}

static void fsShowDirBottom80Dir (int Y, int selectd, uint16_t *sbuf, const struct modlistentry *mle, const char *npath)
{
	writestring(sbuf, 0, 0x07, "  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa", plScrWidth);
	displaystrattr (Y + 0, 0, sbuf, plScrWidth);
	displaystr_utf8 (Y + 0, 2, 0x0F, mle->utf8_16_dot_3, 20);

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
		displaystr (Y + 1, 0, 0x07, "", plScrWidth);
	}

	displaystr (Y + 2, 0, 0x07, "", plScrWidth);

	displaystr (Y + 3, 0, 0x07, "", plScrWidth);

	displaystr (Y + 4, 0, 0x07, "   long: ", 9);

	displaystr_utf8_overflowleft (Y + 4, 10, 0x0F, npath ? npath : "", plScrWidth - 10);
}

static void fsShowDirBottom132Dir (int Y, int selectd, uint16_t *sbuf, const struct modlistentry *mle, const char *npath)
{
	writestring(sbuf, 0, 0x07, "  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa", plScrWidth);
//	fillstr(sbuf, 42, 0x07, 0xfa, plScrWidth - 100);
	displaystrattr (Y + 0, 0, sbuf, plScrWidth);
	displaystr_utf8 (Y + 0, 2, 0x0F, mle->utf8_16_dot_3, 20);

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
		displaystr (Y + 1, 0, 0x07, "", plScrWidth);
	}

	displaystr (Y + 2, 0, 0x07, "", plScrWidth);

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

	uint16_t sbuf[CONSOLE_MAX_X];

	if (currentdir->num>dirwinheight)
		vrelpos=dirwinheight*currentdir->pos/currentdir->num;
	if (playlist->num>dirwinheight)
		prelpos=dirwinheight*playlist->pos/playlist->num;

	make_title("file selector ][");
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
	fillstr(sbuf, 0, 0x07, 0xc4, CONSOLE_MAX_X);
	if (!playlistactive)
		fillstr(sbuf, plScrWidth-15, 0x07, 0xc2, 1);
	displaystrattr(2, 0, sbuf, plScrWidth);

	if (fsEditWin||(selecte>=0)||(selectd>=0))
	{
		int Y=dirwinheight+3;
		char *npath = 0;

		fillstr(sbuf, 0, 0x07, 0xc4, CONSOLE_MAX_X);
		if (!playlistactive)
			fillstr(sbuf, plScrWidth-15, 0x07, 0xc1, 1);
		displaystrattr (Y, 0, sbuf, plScrWidth);

		/* fill npath */
		if (mle->file)
		{
			const char *modtype="";
			struct moduleinfostruct mi;

			mdbGetModuleInfo(&mi, mle->mdb_ref);
			modtype=mdbGetModTypeString(mi.modtype);

			dirdbGetFullname_malloc (mle->file->dirdb_ref, &npath, 0);
			if (plScrWidth>=132)
			{
				fsShowDirBottom132File (Y + 1, selecte, sbuf, mle, &mi, modtype, npath);
			} else {
				fsShowDirBottom80File (Y + 1, selecte, sbuf, mle, &mi, modtype, npath);
			}
		} else if (mle->dir)
		{
			dirdbGetFullname_malloc (mle->dir->dirdb_ref, &npath, 0);
			if (plScrWidth>=132)
			{
				fsShowDirBottom132Dir (Y + 1, selectd, sbuf, mle, npath);
			} else {
				fsShowDirBottom80Dir (Y + 1, selectd, sbuf, mle, npath);
			}
		}

		free (npath); npath = 0;
	}

	fillstr(sbuf, 0, 0x17, 0, CONSOLE_MAX_X);
	writestring(sbuf, 0, 0x17, " quickfind: [\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa]    press F1 for help, or ALT-C for basic setup ", 74);
	writestring(sbuf, 13, 0x1F, quickfind, quickfindpos);

	displaystrattr(plScrHeight-1, 0, sbuf, plScrWidth);

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
		const char *fsInfoModes[]= {"name and size","composer","comment","style and playtime","long filenames"};
		uint16_t sbuf[CONSOLE_MAX_X];
		const char *modename = plGetDisplayTextModeName();
		int i;

superbreak:
		make_title("file selector setup");

		displaystr( 1,  0, 0x07, "1:  screen mode: ", 17);
		displaystr( 1, 17, 0x0f, modename, plScrWidth - 17);
		/*
		displaystr( 1,  0, 0x07, "1:  screen mode (if driver supports it TODO): ", 45);
		displaystr( 1, 45, 0x0F, (fsScrType&4)?"132x":" 80x", 4);
		displaystr( 1, 49, 0x0F, ((fsScrType&3)==0)?"25":((fsScrType&3)==1)?"30":((fsScrType&3)==2)?"50":"60", plScrWidth - 49);
		*/
		displaystr( 2,  0, 0x07, "2:  scramble module list order: ", 32);
		displaystr( 2, 32, 0x0F, fsListScramble?"on":"off", plScrWidth - 32);
		displaystr( 3,  0, 0x07, "3:  remove modules from playlist when played: ", 46);
		displaystr( 3, 46, 0x0F, fsListRemove?"on":"off", plScrWidth - 46);
		displaystr( 4,  0, 0x07, "4:  loop modules: ", 18);
		displaystr( 4, 18, 0x0F, fsLoopMods?"on":"off", plScrWidth - 18);
		displaystr( 5,  0, 0x07, "5:  scan module informatin: ", 28);
		displaystr( 5, 28, 0x0F, fsScanNames?"on":"off", plScrWidth - 28);
		displaystr( 6,  0, 0x04, "6:  scan module information files: ", 35);
		displaystr( 6, 35, 0x0F, fsScanMIF?"on":"off", plScrWidth - 35);
		displaystr( 7,  0, 0x07, "7:  scan archive contents: ", 27);
		displaystr( 7, 27, 0x0F, fsScanArcs?"on":"off", plScrWidth - 27);
		displaystr( 8,  0, 0x07, "8:  scan module information in archives: ", 41);
		displaystr( 8, 41, 0x0F, fsScanInArc?"on":"off", plScrWidth - 41);
		displaystr( 9,  0, 0x07, "9:  save module information to disk: ", 37);
		displaystr( 9, 37, 0x0F, fsWriteModInfo?"on":"off", plScrWidth - 37);
		displaystr(10,  0, 0x07, "A:  edit window: ", 17);
		displaystr(10, 17, 0x0F, fsEditWin?"on":"off", plScrWidth - 17);
		displaystr(11,  0, 0x07, "B:  module type colors: ", 24);
		displaystr(11, 24, 0x0F, fsColorTypes?"on":"off", plScrWidth - 24);
		displaystr(12,  0, 0x07, "C:  module information display mode: ", 37);
		displaystr(12, 37, 0x0F, fsInfoModes[fsInfoMode], plScrWidth - 37);
		displaystr(13,  0, 0x07, "D:  put archives: ", 18);
		displaystr(13, 18, 0x0F, fsPutArcs?"on":"off", plScrWidth - 18);

		fillstr(sbuf, 0, 0x00, 0, plScrWidth);
		writestring(sbuf, 0, 0x07, "+-: Target framerate: ", 22);
		writenum(sbuf, 22, 0x0f, fsFPS, 10, 3, 1);
		writestring(sbuf, 25, 0x07, ", actual framerate: ", 20);
		writenum(sbuf, 45, 0x0f, LastCurrent=fsFPSCurrent, 10, 3, 1);
		displaystrattr(14, 0, sbuf, plScrWidth);

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
			while (!ekbhit()&&(LastCurrent==fsFPSCurrent))
				framelock();
			if (!ekbhit())
				continue;
		}

		c=egetch();

		switch (c)
		{
			case '1': stored = 0; _plDisplaySetupTextMode(); break;
			/*case '1': stored = 0; fsScrType=(fsScrType+1)&7; break;*/
			case '2': stored = 0; fsListScramble=!fsListScramble; break;
			case '3': stored = 0; fsListRemove=!fsListRemove; break;
			case '4': stored = 0; fsLoopMods=!fsLoopMods; break;
			case '5': stored = 0; fsScanNames=!fsScanNames; break;
			case '6': stored = 0; fsScanMIF=!fsScanMIF; break;
			case '7': stored = 0; fsScanArcs=!fsScanArcs; break;
			case '8': stored = 0; fsScanInArc=!fsScanInArc; break;
			case '9': stored = 0; fsWriteModInfo=!fsWriteModInfo; break;
			case 'a': case 'A': stored = 0; fsEditWin=!fsEditWin; break;
			case 'b': case 'B': stored = 0; fsColorTypes=!fsColorTypes; break;
			case 'c': case 'C': stored = 0; fsInfoMode=(fsInfoMode+1)%5; break;
			case 'd': case 'D': stored = 0; fsPutArcs=!fsPutArcs; break;
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
				cfSetProfileBool(sec, "scanmdz", fsScanMIF);
				cfSetProfileBool(sec, "scanarchives", fsScanArcs);
				cfSetProfileBool(sec, "scaninarcs", fsScanInArc);
				cfSetProfileBool(sec, "writeinfo", fsWriteModInfo);
				cfSetProfileBool(sec, "editwin", fsEditWin);
				cfSetProfileBool(sec, "typecolors", fsColorTypes);
				/*cfSetProfileInt(sec, "", fsInfoMode);*/
				cfSetProfileBool(sec, "putarchives", fsPutArcs);
				cfSetProfileInt("screen", "fps", fsFPS, 10);
				cfStoreConfig();
				stored = 1;
				break;
			}
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
static int fsEditModType(unsigned char *oldtype)
{
	static int state = 0;
	/* 0 - new / idle
	 * 1 - in edit
	 * 2 - in keyboard help
	 */

	static unsigned char index[256];
	static int length=0;
	static int curindex=0;
	int offset=0;

	int i;
	const char *temp;

	const int Height=20;
	const int iHeight=Height-1;
	const int Width=18;
	int Top=(plScrHeight-Height)/2;
	int Left=(plScrWidth-Width)/2;
	const int Mid = 7;

	static int editcol=0;

	//int InKeyboardHelp = 0;

	if (state == 0) /* new edit / first time */
	{
		length = 0;
		curindex = 0;
		for (i=0;i<256;i++)
		{
			temp=mdbGetModTypeString(i);
			if ((temp[0])||(i==mtUnRead))
			{
				index[length]=i;
				if (i==*oldtype)
				{
					curindex=length;
				}
				length++;
			}
		}
		state = 1;
	}

	for (i=0;i<Height;i++)
		displayvoid(Top+i, Left, Width);
	displaystr(Top, Left, 0x04, "\xda", 1);
	for (i=1;i<Width;i++)
	{
		displaystr(Top, Left+i, 0x04, "\xc4", 1);
		displaystr(Top+Height, Left+i, 0x04, "\xc4", 1);
	}
	displaystr(Top, Left+Mid, 0x04, "\xc2", 1);
	displaystr(Top, Left+Width, 0x04, "\xbf", 1);

	for (i=1;i<Height;i++)
	{
		displaystr(Top+i, Left, 0x04, "\xb3", 1);
		displaystr(Top+i, Left+Mid, 0x04, "\xb3", 1);
		displaystr(Top+i, Left+Width, 0x04, "\xb3", 1);
	}
	displaystr(Top+Height, Left, 0x04, "\xc0", 1);
	displaystr(Top+Height, Left+Mid, 0x04, "\xc1", 1);
	displaystr(Top+Height, Left+Width, 0x04, "\xd9", 1);

	if (length>iHeight)
	{
		if (curindex<=(iHeight/2))
		{
			offset=0;
		} else if (curindex>=(length-iHeight/2))
		{
			offset=length-iHeight;
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
		unsigned char col;
		if ((!editcol)&&((offset+i)==curindex))
			col=0x80;
		else
			col=0;
		displaystr(Top+i+1, Left+1, col, "      ", 6);
		if ((offset+i)>=length)
			break;
		col|=fsTypeCols[index[offset+i]&0xFF];
		displaystr(Top+i+1, Left+2, col, mdbGetModTypeString(index[offset+i]), 4);
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
	while (ekbhit())
	{
		switch(egetch())
		{
			case KEY_RIGHT:
				editcol = fsTypeCols[index[curindex]&0xFF];
				break;
			case KEY_LEFT:
				if (editcol)
				{
					char secname[20];
					fsTypeCols[index[curindex]&0xff]=editcol;
					snprintf(secname, sizeof(secname), "filetype %d", index[curindex]);
					cfSetProfileInt(secname, "color", editcol, 10);
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
					if ((curindex+1)<length)
						curindex++;
				}
				break;
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
					char secname[20];
					fsTypeCols[index[curindex]&0xff]=editcol;
					sprintf(secname, "filetype %d", index[curindex]);
					cfSetProfileInt(secname, "color", editcol, 10);
					cfStoreConfig();
					editcol=0;
				} else {
					*oldtype = index[curindex];
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

/* s might not be zero-terminated */
static int fsEditString(unsigned int y, unsigned int x, unsigned int w, unsigned int l, char *s)
{
	static int state = 0;
	/* 0 - new / idle
	 * 1 - in edit
	 * 2 - in keyboard help
	 */

	static char *str = 0;

	static unsigned int curpos;
	static unsigned int cmdlen;
	static int insmode;
	unsigned int scrolled = 0;

	if (state == 0)
	{
		str = malloc(l+1);
		insmode=1;

		strncpy(str, s, l);
		str[l] = 0;

		curpos=strlen(str);
		cmdlen=strlen(str);

		setcurshape(1);

		state = 1;
	}

	while ((curpos-scrolled)>=w)
	{
		scrolled+=8;
	}

	while (scrolled && ((curpos - scrolled + 8) < w))
	{
		scrolled-=8;
	}

	displaystr(y, x, 0x8F, str+scrolled, w);
	setcur(y, x+curpos-scrolled);

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

	while (ekbhit())
	{
		uint16_t key=egetch();
		if ((key>=0x20)&&(key<=0xFF))
		{
			if (insmode)
			{
				if (cmdlen<l)
				{
					memmove(str+curpos+1, str+curpos, cmdlen-curpos+1);
					str[curpos]=key;
					curpos++;
					cmdlen++;
				}
			} else if (curpos==cmdlen)
			{
				if (cmdlen<l)
				{
					str[curpos++]=key;
					str[curpos]=0;
					cmdlen++;
				}
			} else
				str[curpos++]=key;
		} else switch (key)
		{
			case KEY_LEFT:
				if (curpos)
					curpos--;
				break;
			case KEY_RIGHT:
				if (curpos<cmdlen)
					curpos++;
				break;
			case KEY_HOME:
				curpos=0;
				break;
			case KEY_END:
				curpos=cmdlen;
				break;
			case KEY_INSERT:
				insmode=!insmode;
				setcurshape(insmode?1:2);
				break;
			case KEY_DELETE:
				if (curpos!=cmdlen)
				{
					memmove(str+curpos, str+curpos+1, cmdlen-curpos);
					cmdlen--;
				}
				break;
			case KEY_BACKSPACE:
				if (curpos)
				{
					memmove(str+curpos-1, str+curpos, cmdlen-curpos+1);
					curpos--;
					cmdlen--;
				}
				break;
			case KEY_ESC:
				setcurshape(0);
				free (str);
				state = 0;
				return 0;
			case _KEY_ENTER:
				setcurshape(0);
				strncpy(s, str, l);
				free (str);
				state = 0;
				return 0;
			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp(KEY_RIGHT, "Move cursor right");
				cpiKeyHelp(KEY_LEFT, "Move cursor left");
				cpiKeyHelp(KEY_HOME, "Move cursor home");
				cpiKeyHelp(KEY_END, "Move cursor to the end");
				cpiKeyHelp(KEY_INSERT, "Toggle insert mode");
				cpiKeyHelp(KEY_DELETE, "Remove character at cursor");
				cpiKeyHelp(KEY_BACKSPACE, "Remove character left of cursor");
				cpiKeyHelp(KEY_ESC, "Cancel changes");
				cpiKeyHelp(_KEY_ENTER, "Submit changes");
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
		convnum(*chan, str, 10, 2, 0);
		setcurshape(2);
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

	while (ekbhit())
	{
		uint16_t key=egetch();
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
		convnum((*playtime)/60, str, 10, 3, 0);
		str[3]=':';
		convnum((*playtime)%60, str+4, 10, 2, 0);

		curpos=(str[0]!='0')?0:(str[1]!='0')?1:2;

		setcurshape(2);

		state = 1;
	}

	displaystr(y, x, 0x8F, str, 6);
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

	while (ekbhit())
	{
		uint16_t key=egetch();
		switch (key)
		{
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
				curpos="\x01\x02\x04\x05\x05\x06\x06"[curpos];
				break;
			case KEY_BACKSPACE:
			case KEY_LEFT: /*left*/
				curpos="\x00\x00\x01\x02\x02\x04\x05"[curpos];
				if (key==8)
					str[curpos]='0';
				break;
			case KEY_ESC:
				setcurshape(0);
				state = 0;
				return 0;
			case _KEY_ENTER:
				*playtime=((((str[0]-'0')*10+str[1]-'0')*10+str[2]-'0')*6+str[4]-'0')*10+str[5]-'0';
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

		convnum((*date)&0xFF, str, 10, 2, 0);
		str[2]='.';
		convnum(((*date)>>8)&0xFF, str+3, 10, 2, 0);
		str[5]='.';
		convnum((*date)>>16, str+6, 10, 4, 0);

		setcurshape(2);
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

	while (ekbhit())
	{
		uint16_t key=egetch();
		switch (key)
		{
			case '\'':
				if (curpos==6)
				{
					str[6]=str[7]='0';
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
				/* fall-through */
			case KEY_RIGHT:
				curpos="\x01\x03\x03\x04\x06\x06\x07\x08\x09\x0A\x0A"[curpos];
				break;
			case KEY_BACKSPACE:
			case KEY_LEFT:
				curpos="\x00\x00\x01\x01\x03\x04\x04\x06\x07\x08\x09"[curpos];
				if (key==KEY_BACKSPACE)
					str[curpos]='0';
				break;
			case KEY_ESC:
				setcurshape(0);
				state = 0;
				return 0;
			case _KEY_ENTER:
				*date=((str[0]-'0')*10+str[1]-'0')|(((str[3]-'0')*10+str[4]-'0')<<8)|(((((str[6]-'0')*10+str[7]-'0')*10+str[8]-'0')*10+str[9]-'0')<<16);
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

	if (!mdbGetModuleInfo(&mdbEditBuf, me->mdb_ref))
		return 1;

	if (plScrWidth>=132)
	{
		switch (editfilepos)
		{
			default:
			case 0:
				retval = fsEditString(plScrHeight-5, 42, plScrWidth - 100, sizeof(mdbEditBuf.modname), mdbEditBuf.modname);
				break;
			case 1:
			{
				uint8_t modtype = mdbEditBuf.modtype; /* avoid unaligned pointer access */
				retval = fsEditModType(&modtype);
				mdbEditBuf.modtype = modtype;
				break;
			}
			case 2:
				retval = fsEditChan(plScrHeight-5, plScrWidth - 27, &mdbEditBuf.channels);
				break;
			case 3:
			{
				uint16_t playtime = mdbEditBuf.playtime; /* avoid unaligned pointer access */
				retval = fsEditPlayTime(plScrHeight-5, plScrWidth - 9, &playtime);
				mdbEditBuf.playtime = playtime;
				break;
			}
			case 4:
				retval = fsEditString(plScrHeight-4, 42, plScrWidth - 100, sizeof(mdbEditBuf.composer), mdbEditBuf.composer);
				break;
			case 5:
				retval = fsEditString(plScrHeight-4, plScrWidth - 46, 31, sizeof(mdbEditBuf.style), mdbEditBuf.style);
				break;
			case 6:
			{
				uint32_t date = mdbEditBuf.date; /* avoid unaligned pointer access */
				retval = fsEditDate(plScrHeight-3, 42, &date);
				mdbEditBuf.date = date;
				break;
			}
			case 7:
				retval = fsEditString(plScrHeight-3, 66, plScrWidth - 69, sizeof(mdbEditBuf.comment), mdbEditBuf.comment);
				break;
		}
	} else {
		switch (editfilepos)
		{
			default:
			case 0:
				retval = fsEditString(plScrHeight-6, 35, plScrWidth - 48, sizeof(mdbEditBuf.modname), mdbEditBuf.modname);
				break;
			case 1:
				{
				uint8_t modtype = mdbEditBuf.modtype; /* avoid unaligned pointer access */
				retval = fsEditModType(&modtype);
				mdbEditBuf.modtype = modtype;
				break;
			}
			case 2:
				retval = fsEditChan(plScrHeight-4, plScrWidth - 3, &mdbEditBuf.channels);
				break;
			case 3:
			{
				uint16_t playtime = mdbEditBuf.playtime; /* avoid unaligned pointer access */
				retval = fsEditPlayTime(plScrHeight-4, plScrWidth - 22, &playtime);
				mdbEditBuf.playtime = playtime;
				break;
			}
			case 4:
				retval = fsEditString(plScrHeight-5, 13, plScrWidth - 48, sizeof(mdbEditBuf.composer), mdbEditBuf.composer);
				break;
			case 5:
				retval = fsEditString(plScrHeight-4, 13, plScrWidth - 48, sizeof(mdbEditBuf.style), mdbEditBuf.style);
				break;
			case 6:
				{
					uint32_t date = mdbEditBuf.date; /* avoid unaligned pointer access */
					retval = fsEditDate(plScrHeight-5, plScrWidth - 22, &date);
					mdbEditBuf.date = date;
					break;
				}
			case 7:
				retval = fsEditString(plScrHeight-3, 13, plScrWidth - 17, sizeof(mdbEditBuf.comment), mdbEditBuf.comment);
				break;
		}
	}
/*
	if (editfilepos==1)
	{
		typeidx[4]=0;
		mdbEditBuf.modtype=mdbReadModType(typeidx);
	}*/
	if (!retval)
	{
		if (!mdbWriteModuleInfo(me->mdb_ref, &mdbEditBuf))
		{
			return -1;
		}
		return 0;
	}
	return 1;
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
	}

	fsScanDir(0);
	return 0;
}

void fsDraw(void)
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

	plSetTextMode(fsScrType);
	fsScrType=plScrType;

	isnextplay=NextPlayNone;

	quickfindpos=0;

	if (fsPlaylistOnly)
		return 0;

	while (1)
	{
		signed int firstv, firstp;
		uint16_t c;
		int curscanned=0;
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

		if (!ekbhit()&&fsScanNames)
		{
			if (curscanned||(mdbInfoRead(m->mdb_ref)))
			{
				while (((!win)||(scanposp>=playlist->num)) && (scanposf<currentdir->num))
				{
					struct modlistentry *scanm;
					if ((scanm=modlist_get(currentdir, scanposf++)))
					{
						if (scanm->file)
						{
							if (!mdbInfoRead(scanm->mdb_ref))
							{
								mdbScan(scanm->file, scanm->mdb_ref);
								break;
							}
						}
					}
				}
				while (((win)||(scanposf>=currentdir->num)) && (scanposp<playlist->num))
				{
					struct modlistentry *scanm;
					if ((scanm=modlist_get(playlist, scanposp++)))
					{
						if (scanm->file)
						{
							if (!mdbInfoRead(scanm->mdb_ref))
							{
								mdbScan(scanm->file, scanm->mdb_ref);
								break;
							}
						}
					}
				}
				framelock();
			} else {
				if (!curscanned)
				{
					if (m->file)
					{
						mdbScan(m->file, m->mdb_ref);
					}
					curscanned=1;
				}
			}
			continue;
		} else while (ekbhit())
		{
			c=egetch();
			curscanned=0;

			if (!editmode)
			{
				if (((c>=32)&&(c<=255)&&(c!=KEY_CTRL_BS))||(c==KEY_BACKSPACE))
				{
					if (c==KEY_BACKSPACE)
					{
						if (quickfindpos)
							quickfindpos--;
						if ((quickfindpos==8)&&(quickfind[8]=='.'))
							while (quickfindpos&&(quickfind[quickfindpos-1]==' '))
								quickfindpos--;
					} else
						if (quickfindpos<12)
						{
							if ((c=='.')&&(quickfindpos&&(*quickfind!='.')))
							{
								while (quickfindpos<9)
									quickfind[(int)quickfindpos++]=' ';
								quickfind[8]='.';
							} else
								if (quickfindpos!=8)
									quickfind[(int)quickfindpos++]=toupper(c);
						}
					memcpy(quickfind+quickfindpos, "        .   "+quickfindpos, 12-quickfindpos);
					if (!quickfindpos)
						continue;
					if (!win)
						currentdir->pos=modlist_fuzzyfind(currentdir, quickfind);
					else
						playlist->pos=modlist_fuzzyfind(playlist, quickfind);
					continue;
				}
			}

			quickfindpos=0;

			switch (c)
			{
#ifdef DOS32
				case 0xF8: /* : screen shot */
					TextScreenshot(fsScrType);
					break;
#endif

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
						cpiKeyHelp(KEY_DELETE, "Remove file/directory from the playlist");
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
				case KEY_ESC:
					return 0;
				case KEY_ALT_R:
					if (m->file)
					{
						if (!mdbGetModuleInfo(&mdbEditBuf, m->mdb_ref))
							return -1;
						mdbEditBuf.modtype = mtUnRead;
						if (!mdbWriteModuleInfo(m->mdb_ref, &mdbEditBuf))
							return -1;
					}
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
					break;
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
						if (m->file)
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

#if 0
							dmCurDrive = m->drive;
#else
							dmCurDrive = ocpdir_get_drive (m->dir);
#endif
							m->dir->ref (m->dir);
							dmCurDrive->cwd->unref (dmCurDrive->cwd);
							dmCurDrive->cwd = m->dir;

							fsScanDir(0);
							i = modlist_find (currentdir, olddirpath);
							if (i >= 0)
							{
								currentdir->pos = i;
							}
							dirdbUnref(olddirpath, dirdb_use_pfilesel);
						} else if (m->file)
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
							if (plScrWidth>=132)
								editfilepos="\x00\x01\x02\x03\x00\x01\x04\x05"[editfilepos];
							else
								editfilepos="\x00\x01\x06\x06\x00\x04\x00\x05"[editfilepos];
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
							if (plScrWidth>=132)
								editfilepos="\x04\x05\x05\x05\x06\x07\x06\x07"[editfilepos];
							else
								editfilepos="\x04\x06\x07\x07\x05\x07\x03\x07"[editfilepos];
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
							if (plScrWidth>=132)
								editfilepos="\x01\x02\x03\x03\x05\x05\x07\x07"[editfilepos];
							else
								editfilepos="\x01\x01\x02\x02\x06\x03\x06\x07"[editfilepos];
						}
					}
					break;
				case KEY_INSERT:
				/*case 0x5200: // add*/
					if (editmode)
						break;
					if (win)
					{
						/*if (!*/modlist_append (playlist, m)/*)
							return -1*/;
						/*playlist->pos=playlist->num-1; */
						scanposp=fsScanNames?0:~0;
					} else {
						if (m->dir)
						{
							if (!(fsReadDir (playlist, m->dir, curmask, RD_PUTRSUBS)))
							{
								return -1;
							}
						} else if (m->file)
						{
							modlist_append (playlist, m);
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
							if (plScrWidth>=132)
								editfilepos="\x00\x00\x01\x02\x04\x04\x06\x06"[editfilepos];
							else
								editfilepos="\x00\x00\x03\x05\x04\x05\x04\x07"[editfilepos];
						}
					}
					break;
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
								modlist_append (playlist, me);
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
    case 0x2500:  // alt-k TODO keys.... alt-k is now in use by key-helper
      if (editmode||win)
        break;
      if (m.mdb_ref<=0xFFFC)
        if (fsQueryKill(m))
          if (!fsScanDir(2))
            return -1;
      break;


#ifndef WIN32
    case 0x3200:  // alt-m TODO keys !!!!!!!! STRANGE THINGS HAPPENS IF YOU ENABLE HIS UNDER W32!!
      if (editmode||win)
        break;
      if (m.mdb_ref<=0xFFFC)
        if (fsQueryMove(m))
          if (!fsScanDir(1))
            return -1;
      break;
#endif

    case 0x3000: // alt-b TODO keys
      if (m.mdb_ref<=0xFFFC)
      {
        mdbGetModuleInfo(mdbEditBuf, m.mdb_ref);
        mdbEditBuf.flags1^=MDB_BIGMODULE;
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
		fprintf(stderr, "[filesel] file: is the only supported transport currently\n");
		free (dr);
		free (di);
		free (fn);
		free (ext);
		return 0;
	}

	free (dr); dr = 0;
	makepath_malloc (&newpath, NULL, di, fn, ext);
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
#ifdef __W32__
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
void plRegisterInterface(struct interfacestruct *interface)
{
	interface->next = plInterfaces;
	plInterfaces = interface;
}

void plUnregisterInterface(struct interfacestruct *interface)
{
	struct interfacestruct *curr = plInterfaces;

	if (curr == interface)
	{
		plInterfaces = interface->next;
		return;
	}

	while (curr)
	{
		if (curr->next == interface)
		{
			curr->next = curr->next->next;
			return;
		}
		curr = curr->next;
	}

	fprintf(stderr, __FILE__ ": Failed to unregister interface %s\n", interface->name);
}

struct interfacestruct *plFindInterface(const char *name)
{
	struct interfacestruct *curr = plInterfaces;

	while (curr)
	{
		if (!strcmp(curr->name, name))
			return curr;
		curr = curr->next;
	}
	fprintf(stderr, __FILE__ ": Unable to find interface: %s\n", name);
	return NULL;
}

void plRegisterPreprocess(struct preprocregstruct *r)
{
	r->next=plPreprocess;
	plPreprocess=r;
}

void plUnregisterPreprocess(struct preprocregstruct *r)
{
	struct preprocregstruct *curr = plPreprocess;

	if (curr == r)
	{
		plPreprocess = r->next;
		return;
	}

	while (curr)
	{
		if (curr->next == r)
		{
			curr->next = curr->next->next;
			return;
		}
		curr = curr->next;
	}

	fprintf(stderr, __FILE__ ": Failed to unregister a preprocregstruct %p\n", r);
}
