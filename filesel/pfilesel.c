/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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

#include "adb.h"
#include "boot/psetting.h"
#include "cphlpfs.h"
#include "dirdb.h"
#include "gendir.h"
#include "mdb.h"
#include "cpiface/cpiface.h"
#include "pfilesel.h"
#include "playlist.h"
#include "stuff/compat.h"
#include "stuff/framelock.h"
#include "stuff/poutput.h"

#include "modlist.h"

static char fsScanDir(int pos);
static struct modlist *currentdir=NULL;
static struct modlist *playlist=NULL;

uint32_t dirdbcurdirpath=DIRDB_NOPARENT;
char curmask[NAME_MAX+1]="*";

struct dmDrive *dmDrives=0;
struct dmDrive *dmCurDrive=0;

struct dmDrive *dmFILE;

struct preprocregstruct *plPreprocess = 0;

static void fsSavePlayList(const struct modlist *ml);

struct dmDrive *RegisterDrive(const char *dmDrive)
{
	struct dmDrive *ref = dmDrives;

	while (ref)
	{
		if (!strcmp(ref->drivename, dmDrive))
			return ref;
		ref = ref->next;
	}

	ref=calloc(1, sizeof(struct dmDrive));
	strcpy(ref->drivename, dmDrive);
	ref->basepath=dirdbFindAndRef(DIRDB_NOPARENT, ref->drivename);
	ref->currentpath=ref->basepath;
	dirdbRef(ref->currentpath);
	ref->next=dmDrives;
	dmDrives=ref;

	return ref;
}

struct dmDrive *dmFindDrive(const char *drivename) /* to get the correct drive from a given string */
{
	struct dmDrive *cur=dmDrives;
	while (cur)
	{
		if (!strncasecmp(cur->drivename, drivename, strlen(cur->drivename)))
			return cur;
		cur=cur->next;
	}
	return NULL;
}

int dosfile_Read(struct modlistentry *entry, char **mem, size_t *size)
{
	int fd;
	ssize_t result;
	char *path;

	dirdbGetFullname_malloc (entry->dirdbfullpath, &path, DIRDB_FULLNAME_NOBASE);

	if (!(*size=_filelength(path)))
	{
		free (path);
		return -1;
	}
	if ((fd=open(path, O_RDONLY))<0)
	{
		fprintf (stderr, "Failed to open %s: %s\n", path, strerror (errno));
		free (path);
		return -1;
	}
	*mem=malloc(*size);
redo:
	result=read(fd, *mem, *size);
	if (result<0)
	{
		if (errno==EAGAIN)
			goto redo;
		if (errno==EINTR)
			goto redo;
		fprintf (stderr, "Failed to read %s: %s\n", path, strerror (errno));
		free(*mem);
		close(fd);
		free (path);
		return -1;
	}
	if (result!=(ssize_t)*size) /* short read ???? */
	{
		fprintf (stderr, "Failed to read entire file, only for %d of %d bytes\n", (int)result, (int)*size);
		free(*mem);
		close(fd);
		free (path);
		return -1;
	}
	close(fd);
	free (path);
	return 0;
}

int dosfile_ReadHeader(struct modlistentry *entry, char *mem, size_t *size) /* size is prefilled with max data, and mem is preset*/
{
	int fd, result;
	char *path;

	dirdbGetFullname_malloc (entry->dirdbfullpath, &path, DIRDB_FULLNAME_NOBASE);

	if (!(*size=_filelength(path)))
	{
		free (path);
		return -1;
	}
	if ((fd=open(path, O_RDONLY))<0)
	{
		fprintf (stderr, "Failed to open %s: %s\n", path, strerror (errno));
		free (path);
		return -1;
	}
redo:
	result=read(fd, mem, *size);
	if (result<0)
	{
		if (errno==EAGAIN)
			goto redo;
		if (errno==EINTR)
			goto redo;
		fprintf (stderr, "Failed to read %s: %s\n", path, strerror (errno));
		close(fd);
		free (path);
		return -1;
	}
	*size=result;
	close(fd);
	free (path);
	return 0;
}

FILE *dosfile_ReadHandle(struct modlistentry *entry)
{
	FILE *retval;
	char *path;
	dirdbGetFullname_malloc (entry->dirdbfullpath, &path, DIRDB_FULLNAME_NOBASE);

	if ((retval=fopen(path, "r")))
	{
		fcntl(fileno(retval), F_SETFD, 1<<FD_CLOEXEC);
	}
	free (path);
	return retval;
}

static int dosReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt);

static void dosReadDirChild(struct modlist *ml,
                            struct modlist *dl,
                            const struct dmDrive *drive,
                            const char *parentpath,
                            const char *childpath,
                            int d_type,
                            const char *mask,
                            unsigned long opt)
{
	struct modlistentry retval;

	char curext[NAME_MAX+1];
	char path[PATH_MAX+1];

	memset(&retval, 0, sizeof(struct modlistentry));
	retval.drive=drive;
	strncpy(retval.name, childpath, NAME_MAX);
	retval.name[NAME_MAX]=0;

	snprintf(path, PATH_MAX+1, "%s%s", parentpath, childpath);
	retval.dirdbfullpath=dirdbResolvePathWithBaseAndRef(drive->basepath, path);

	fs12name(retval.shortname, childpath);

#ifdef HAVE_STRUCT_DIRENT_D_TYPE
	if (d_type==DT_DIR)
	{
		if (!(opt&(RD_PUTRSUBS|RD_PUTSUBS)))
			goto out;
		retval.flags=MODLIST_FLAG_DIR;
		if (strlen(path)<PATH_MAX)
		{
			strcat(path, "/");
			if (opt&RD_PUTRSUBS)
				fsReadDir(dl, drive, retval.dirdbfullpath, mask, opt);
		}
		if (!(opt&RD_PUTSUBS))
			goto out;
	} else if ((d_type==DT_REG)||(d_type==DT_LNK)||(d_type==DT_UNKNOWN))
#endif
	{
		struct stat st;
		struct stat lst;
		if (lstat(path, &lst))
			goto out;
		if (S_ISLNK(lst.st_mode))
		{
			if (stat(path, &st))
				goto out;
		} else
			memcpy(&st, &lst, sizeof(st));
		if (S_ISREG(st.st_mode))
		{
			_splitpath(path, 0, 0, 0, curext);
			if (isarchiveext(curext))
			{
				retval.flags=MODLIST_FLAG_ARC;
				if (strlen(path)<PATH_MAX)
					strcat(path, "/");
			} else {
#ifndef FNM_CASEFOLD
				char *mask_upper;
				char *childpath_upper;
				char *iterate;

				if ((mask_upper = strdup(mask)))
				{
					for (iterate = mask_upper; *iterate; iterate++)
						*iterate = toupper(*iterate);
				} else {
					perror("pfilesel.c: strdup() failed");
					goto out;
				}

				if ((childpath_upper = strdup(childpath)))
				{
					for (iterate = childpath_upper; *iterate; iterate++)
						*iterate = toupper(*iterate);
				} else {
					perror("pfilesel.c: strdup() failed");
					goto out;
				}

				if ((fnmatch(mask_upper, childpath_upper, 0))||(!fsIsModule(curext)))
				{
					free(childpath_upper);
					free(mask_upper);
					goto out;
				}
				free(childpath_upper);
				free(mask_upper);
#else
				if ((fnmatch(mask, childpath, FNM_CASEFOLD))||(!fsIsModule(curext)))
					goto out;
#endif
				retval.mdb_ref=mdbGetModuleReference(retval.shortname, st.st_size);
				retval.adb_ref=0xffffffff;
				retval.flags=MODLIST_FLAG_FILE;
			}
		} else if (S_ISDIR(st.st_mode))
		{
			if (!(opt&(RD_PUTRSUBS|RD_PUTSUBS)))
				goto out;
			if (S_ISLNK(lst.st_mode)&&(opt&RD_SUBNOSYMLINK))
				goto out;
			retval.flags=MODLIST_FLAG_DIR;
			if (strlen(path)<PATH_MAX)
			{
				strcat(path, "/");
				if (opt&RD_PUTRSUBS)
					fsReadDir(dl, drive, retval.dirdbfullpath, mask, opt);
			}
			if (!(opt&RD_PUTSUBS))
				goto out;
		} else
			goto out;
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
	} else
		goto out;
#else
	}
#endif

	retval.Read=dosfile_Read;
	retval.ReadHeader=dosfile_ReadHeader;
	retval.ReadHandle=dosfile_ReadHandle;
	modlist_append(ml, &retval); /* this call no longer can fail */
out:
	dirdbUnref(retval.dirdbfullpath);
	return;
}

static int dosReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t dirdbpath, const char *mask, unsigned long opt)
{
	DIR *dir;
	char *path;
	struct modlist *tl;

	if (drive!=dmFILE)
		return 1;

	tl = modlist_create();

	dirdbGetFullname_malloc (dirdbpath, &path, DIRDB_FULLNAME_NOBASE|DIRDB_FULLNAME_ENDSLASH);
	if ((dir=opendir(path)))
	{
		struct dirent *de;
		while ((de=readdir(dir)))
		if (strcmp(de->d_name, "."))
		if (strcmp(de->d_name, ".."))
		if (((strlen(path)+strlen(de->d_name)+4)<PATH_MAX))
		{
			char *ext;
			getext_malloc (de->d_name, &ext);
			int res = isarchiveext(ext);
			free (ext);

			if (res)
			{
				if ((opt&RD_PUTSUBS)&&(fsPutArcs/*||!(opt&RD_ARCSCAN)*/))
				{
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
					dosReadDirChild(ml, ml, drive, path, de->d_name, de->d_type, mask, opt);
#else
					dosReadDirChild(ml, ml, drive, path, de->d_name, 0, mask, opt);
#endif
				}
				if (fsScanArcs)
				{
					uint32_t dirdbnewpath = dirdbFindAndRef(dirdbpath, de->d_name);
					if (!(fsReadDir(tl, drive, dirdbnewpath, mask, opt&~(RD_PUTRSUBS|RD_PUTSUBS))))
					{
						dirdbUnref(dirdbnewpath);
						closedir(dir);
						modlist_sort(tl);
						modlist_append_modlist(ml, tl);
						modlist_free(tl);
						free (path);
						return 0;
					}
					dirdbUnref(dirdbnewpath);
				}
			} else {
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
				dosReadDirChild(tl, ml, drive, path, de->d_name, de->d_type, mask, opt);
#else
				dosReadDirChild(tl, ml, drive, path, de->d_name, 0, mask, opt);
#endif
			}
		}
		closedir(dir);
	}
	modlist_sort(tl);
	modlist_append_modlist(ml, tl);
	modlist_free(tl);
	free (path);
	return 1;
}

/* TODO's
 *
 * ReadDir stuff
 * dmDrive stuff
 *
 * rename currentdir to viewlist again:
 */

static char fsTypeCols[256]; /* colors */
const char *(fsTypeNames[256]) = {0}; /* type description */

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
int fsIsModule(const char *ext)
{
	char **e;

	if (*ext++!='.')
		return 0;
	for (e=moduleextensions; *e; e++)
		if (!strcasecmp(ext, *e))
			return 1;
	return 0;
}

static int initRootDir(const char *sec)
{
	int count;

	int len = 4096; /* PATH_MAX on many systems */
	char *currentpath, *currentpath2;
	uint32_t newcurrentpath;

	dmFILE = RegisterDrive("file:");

	currentdir=modlist_create();
	playlist=modlist_create();

	currentpath = malloc(len);
	while (1)
	{
		/* since get_current_dir_name() is not POSIX, we need to do this dance to support unknown lengths paths */
		if (!getcwd (currentpath, len))
		{
			if (errno == ENAMETOOLONG)
			{
				len += 4096;
				currentpath = realloc (currentpath, len);
				continue;
			}
			fprintf (stderr, "getcwd() failed, using / instead: %s\n", strerror (errno));
			strcpy (currentpath, "/");
		}
		break;
	}
	newcurrentpath = dirdbResolvePathWithBaseAndRef(dmFILE->basepath, currentpath);

	dirdbUnref(dmFILE->currentpath);
	dmFILE->currentpath = newcurrentpath;
	dmCurDrive=dmFILE;

	for (count=0;;count++)
	{
		char buffer[32];
		const char *filename;
		sprintf(buffer, "file%d", count);
		if (!(filename=cfGetProfileString2(sec, "CommandLine_Files", buffer, NULL)))
			break;
		fsAddPlaylist(playlist, currentpath, "*", 0, filename);
	}
	for (count=0;;count++)
	{
		char buffer[32];
		const char *filename;
		uint32_t dirdbfullpath;

		sprintf(buffer, "playlist%d", count);
		if (!(filename=cfGetProfileString2(sec, "CommandLine_Files", buffer, NULL)))
			break;
		dirdbfullpath = dirdbFindAndRef(dmFILE->currentpath, filename);
		fsReadDir(playlist, dmFILE, dirdbfullpath, "*", 0); /* ignore errors */
		dirdbUnref(dirdbfullpath);
	}

	/* change dir */
	gendir_malloc (currentpath, cfGetProfileString2(sec, "fileselector", "path", "."), &currentpath2);
	free (currentpath);

	newcurrentpath = dirdbResolvePathWithBaseAndRef(dmFILE->basepath, currentpath2);
	free (currentpath2);

	dirdbUnref(dmFILE->currentpath);
	dmFILE->currentpath = newcurrentpath;

	dirdbcurdirpath=dmFILE->currentpath;
	dirdbRef(dmFILE->currentpath);

	return 1;
}

static void doneRootDir(void)
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
}

static struct modlistentry *nextplay=NULL;
typedef enum {NextPlayNone, NextPlayBrowser, NextPlayPlaylist} NextPlay;
static NextPlay isnextplay = NextPlayNone;
/* These guys has with rendering todo and stuff like that */
static unsigned short dirwinheight;
static char quickfind[12];
static char quickfindpos;
static short editpos=0;
static short editmode=0;
static unsigned int scanposf, scanposp;

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

static char fsScanDir(int pos)
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
	modlist_remove(currentdir, 0, currentdir->num);

	if (!fsReadDir(currentdir, dmCurDrive, dirdbcurdirpath, curmask, RD_PUTSUBS|(fsScanArcs?RD_ARCSCAN:0)))
		return 0;
	modlist_sort(currentdir);
	currentdir->pos=(op>=currentdir->num)?(currentdir->num-1):op;
	quickfindpos=0;
	scanposf=fsScanNames?0:~0;

	adbUpdate();

	return 1;
}

void fsRescanDir(void)
{
	fsScanDir(1);
	conSave();
}

int fsGetPrevFile(char *path, struct moduleinfostruct *info, FILE **file)
{
	struct modlistentry *m;
	int retval=0;
	int pick;

	switch (isnextplay)
	{
		default:
			return fsGetNextFile(path, info, file);
		case NextPlayNone:
			if (!playlist->num)
			{
				fprintf(stderr, "BUG in pfilesel.c: fsGetNextFile() invalid NextPlayPlaylist #2\n");
				return retval;
			}
			if (fsListScramble)
				return fsGetNextFile(path, info, file);
			if (playlist->pos)
				playlist->pos--;
			else
				playlist->pos = playlist->num - 1;
			if (playlist->pos)
				pick = playlist->pos-1;
			else
				pick = playlist->num - 1;
			m=modlist_get(playlist, pick);
			break;
	}

	mdbGetModuleInfo(info, m->mdb_ref);

	dirdbGetFullName(m->dirdbfullpath, path, 0);

	if (!(info->flags1&MDB_VIRTUAL)) /* this should equal to if (m->ReadHandle) */
	{
		if (!(*file=m->ReadHandle(m)))
			goto errorout;
		/* strcpy(path, m->fullname); WTF WTF TODO */ /* arc's change the path */
	} else
		*file=NULL;

	if (!mdbInfoRead(m->mdb_ref)&&*file)
	{
		mdbReadInfo(info, *file);
		fseek(*file, 0, SEEK_SET);
		mdbWriteModuleInfo(m->mdb_ref, info);
		mdbGetModuleInfo(info, m->mdb_ref);
	}

	retval=1;
errorout:
	if (fsListRemove)
		modlist_remove(playlist, pick, 1);
	return retval;
}

int fsGetNextFile(char *path, struct moduleinfostruct *info, FILE **file)
{
	struct modlistentry *m;
	unsigned int pick=0;
	int retval=0;

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
			m=modlist_get(playlist, pick);
			break;
		case NextPlayNone:
			if (!playlist->num)
			{
				fprintf(stderr, "BUG in pfilesel.c: fsGetNextFile() invalid NextPlayPlaylist #2\n");
				return retval;
			}
			if (fsListScramble)
				pick=rand()%playlist->num;
			else
				pick = playlist->pos;
			m=modlist_get(playlist, pick);
			break;
		default:
			fprintf(stderr, "BUG in pfilesel.c: fsGetNextFile() Invalid isnextplay\n");
			return retval;
	}

	mdbGetModuleInfo(info, m->mdb_ref);

	dirdbGetFullName(m->dirdbfullpath, path, 0);

	if (!(info->flags1&MDB_VIRTUAL)) /* this should equal to if (m->ReadHandle) */
	{
		if (!(*file=m->ReadHandle(m)))
			goto errorout;
		/* strcpy(path, m->fullname); WTF WTF TODO */ /* arc's change the path */
	} else
		*file=NULL;

	if (!mdbInfoRead(m->mdb_ref)&&*file)
	{
		mdbReadInfo(info, *file);
		fseek(*file, 0, SEEK_SET);
		mdbWriteModuleInfo(m->mdb_ref, info);
		mdbGetModuleInfo(info, m->mdb_ref);
	}

	retval=1;
errorout:
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
				modlist_remove(playlist, pick, 1);
			else {
				if (!fsListScramble)
					if ( (pick=playlist->pos+1)>=playlist->num)
						pick=0;
				playlist->pos=pick;
			}
	}
	return retval;
}

void fsForceRemove(const char *path)
{
	uint32_t handle;
	handle = dirdbResolvePathAndRef(path);
	if (handle == DIRDB_NO_MDBREF)
		return;
	modlist_remove_all_by_path(playlist, handle);
	dirdbUnref(handle);
}

int fsPreInit(void)
{
	int i;

	const char *sec=cfGetProfileString(cfConfigSec, "fileselsec", "fileselector");

	const char *modexts;
	int extnum;

	if (!adbInit()) /* archive database cache */
		return 0;

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
	if (!initRootDir(sec))
		return 0;

	RegisterDrive("setup:");

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

int fsInit(void)
{
	/*
	if (!mifInit())  .mdz tag cache/reader
		return 0;
	*/

	if (!fsScanDir(0))
		return 0;

	return 1;
}

void fsClose(void)
{
	doneRootDir();
	adbClose();
	mdbClose();
	/*

	mifClose();
	delete dmTree;
	delete dmReloc;
	*/
	if (moduleextensions)
	{
		int i;
		for (i=0;moduleextensions[i];i++)
			free(moduleextensions[i]);
		free(moduleextensions);
		moduleextensions=0;
	}
	/*
	delete playlist.files;
	playlist.files=0;
	delete viewlist.files;
	viewlist.files=0;
	*/

	if (dirdbcurdirpath != DIRDB_NOPARENT)
	{
		dirdbUnref(dirdbcurdirpath); /* due to curpath */
		dirdbcurdirpath = DIRDB_NOPARENT;
	}

	{
		struct dmDrive *drive = dmDrives, *next;
		while (drive)
		{
			next=drive->next;
			dirdbUnref(drive->basepath);
			dirdbUnref(drive->currentpath);
			free(drive);
			drive=next;
		}
		dmDrives=0;
	}
	dirdbClose();
}

static void displayfile(const unsigned int y, const unsigned int x, const unsigned int width, /* TODO const*/ struct modlistentry *m, const unsigned char sel)
{
	unsigned char col;
	unsigned short sbuf[CONSOLE_MAX_X-15];
	struct moduleinfostruct mi;

	if (width == 14)
	{
		unsigned short _sbuf[14];
		if (sel==2)
			writestring(_sbuf, 0, 0x07, "\x1A            \x1B", 14);
		else
			writestring(_sbuf, 0, (sel==1)?0x8F:0x0F, "", 14);
		writestring(_sbuf, 1, (sel==1)?0x8F:0x0F, m->shortname, 12);
		displaystrattr(y, x, _sbuf, 14);
		return;
	}

	if (m->flags&MODLIST_FLAG_FILE)
	{
		col=0x07;
		mdbGetModuleInfo(&mi, m->mdb_ref);
		if (mi.flags1&MDB_PLAYLIST)
		{
			col=0x0f;
			m->flags|=MODLIST_FLAG_DIR;
			/* TODO, register in dirdb as both DIR and FILE */
		}
	} else {
		memset(&mi, 0, sizeof(mi));
		col=0x0f;
	}
	if (sel==1)
		col|=0x80;
	writestring(sbuf, 0, col, "", width);
	if (sel==2)
	{
		writestring(sbuf, 0, 0x07, "->", 2);
		writestring(sbuf, width-2, 0x07, "<-", 2);
	}


	if (fsInfoMode==4)
	{
		if (!(m->flags&(MODLIST_FLAG_DIR|MODLIST_FLAG_DRV|MODLIST_FLAG_ARC)))
		{
			if (mi.modtype==0xFF)
				col&=~0x08;
			else if (fsColorTypes)
			{
				col&=0xF8;
				col|=fsTypeCols[mi.modtype&0xFF];
			}
		}

		writestring(sbuf, 2, col, m->name, width-13);
		if (mi.flags1&MDB_PLAYLIST)
			writestring(sbuf, width-7, col, "<PLS>", 5);
		else if (m->flags&MODLIST_FLAG_DIR)
			writestring(sbuf, width-7, col, "<DIR>", 5);
		else if (m->flags&MODLIST_FLAG_DRV)
			writestring(sbuf, width-7, col, "<DRV>", 5);
		else if (m->flags&MODLIST_FLAG_ARC)
			writestring(sbuf, width-7, col, "<ARC>", 5);
		else {
			if (mi.size<1000000000)
				writenum(sbuf, width-11, (mi.flags1&MDB_BIGMODULE)?((col&0xF0)|0x0C):col, mi.size, 10, 9, 1);
			else
				writenum(sbuf, width-10, col, mi.size, 16, 8, 0);
		}
	} else {
		writestring(sbuf, 2, col, m->shortname, 12);

		if (mi.flags1&MDB_PLAYLIST)
			writestring(sbuf, 16, col, "<PLS>", 5);
		else if (m->flags&MODLIST_FLAG_DIR)
			writestring(sbuf, 16, col, "<DIR>", 5);
		else if (m->flags&MODLIST_FLAG_DRV)
			writestring(sbuf, 16, col, "<DRV>", 5);
		else if (m->flags&MODLIST_FLAG_ARC)
			writestring(sbuf, 16, col, "<ARC>", 5);
		else {
			if (mi.modtype==0xFF)
				col&=~0x08;
			else if (fsColorTypes)
			{
				col&=0xF8;
				col|=fsTypeCols[mi.modtype&0xFF];
			}

			if (width>=117) /* 132 or bigger screen this will imply */
			{
				if (fsInfoMode&1)
				{
					if (mi.comment[0])
						writestring(sbuf, 16, col, mi.comment, 63);
					if (mi.style[0])
						writestring(sbuf, 84, col, mi.style, 31);
				} else {
					if (mi.modname[0])
						writestring(sbuf, 16, col, mi.modname, 32);
					if (mi.channels)
						writenum(sbuf, 50, col, mi.channels, 10, 2, 1);
				        if (mi.playtime)
				        {
						writenum(sbuf, 53, col, mi.playtime/60, 10, 3, 1);
						writestring(sbuf, 56, col, ":", 1);
						writenum(sbuf, 57, col, mi.playtime%60, 10, 2, 0);
					}
					if (mi.composer[0])
						writestring(sbuf, 61, col, mi.composer, 32);
					if (mi.date)
					{
						if (mi.date&0xFF)
						{
							writestring(sbuf, 96, col, ".", 3);
							writenum(sbuf, 94, col, mi.date&0xFF, 10, 2, 1);
						}
						if (mi.date&0xFFFF)
						{
							writestring(sbuf, 99, col, ".", 3);
							writenum(sbuf, 97, col, (mi.date>>8)&0xFF, 10, 2, 1);
						}
						if (mi.date>>16)
						{
							writenum(sbuf, 100, col, mi.date>>16, 10, 4, 1);
							if (!((mi.date>>16)/100))
								writestring(sbuf, 101, col, "'", 1);
						}
					}
					if (mi.size<1000000000)
						writenum(sbuf, 106, (mi.flags1&MDB_BIGMODULE)?((col&0xF0)|0x0C):col, mi.size, 10, 9, 1);
					else
						writenum(sbuf, 107, col, mi.size, 16, 8, 0);
				}

			} else switch (fsInfoMode)
			{
				case 0:
					writestring(sbuf, 16, col, mi.modname, 32);
					if (mi.channels)
						writenum(sbuf, 50, col, mi.channels, 10, 2, 1);
					if (mi.size<1000000000)
						writenum(sbuf, 54, (mi.flags1&MDB_BIGMODULE)?((col&0xF0)|0x0C):col, mi.size, 10, 9, 1);
					else
						writenum(sbuf, 55, col, mi.size, 16, 8, 0);
					break;
				case 1:
					if (mi.composer[0])
						writestring(sbuf, 16, col, mi.composer, 32);
					if (mi.date)
					{
						if (mi.date&0xFF)
						{
							writestring(sbuf, 55, col, ".", 3);
							writenum(sbuf, 53, col, mi.date&0xFF, 10, 2, 1);
						}
						if (mi.date&0xFFFF)
						{
							writestring(sbuf, 58, col, ".", 3);
							writenum(sbuf, 56, col, (mi.date>>8)&0xFF, 10, 2, 1);
						}
						if (mi.date>>16)
						{
							writenum(sbuf, 59, col, mi.date>>16, 10, 4, 1);
							if (!((mi.date>>16)/100))
								writestring(sbuf, 60, col, "'", 1);
						}
					}

					break;
				case 2:
					if (mi.comment[0])
						writestring(sbuf, 16, col, mi.comment, 47);
					break;
				case 3:
					if (mi.style[0])
						writestring(sbuf, 16, col, mi.style, 31);
					if (mi.playtime)
					{
						writenum(sbuf, 57, col, mi.playtime/60, 10, 3, 1);
						writestring(sbuf, 60, col, ":", 1);
						writenum(sbuf, 61, col, mi.playtime%60, 10, 2, 0);
					}
					break;
			}
		}
	}
	displaystrattr(y, x, sbuf, width);
}

static void fsShowDir(unsigned int firstv, unsigned int selectv, unsigned int firstp, unsigned int selectp, int selecte, const struct modlistentry *mle, int playlistactive)
{
	unsigned int i;

	unsigned int vrelpos= ~0;
	unsigned int prelpos= ~0;

	uint16_t sbuf[CONSOLE_MAX_X];
	const char *tmppos;
	char npath[PATH_MAX+1];

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

		if (len>plScrWidth)
		{
			displaystr(1, 0, 0x0F, temppath+len-plScrWidth, plScrWidth);
		} else {
			displaystr(1, 0, 0x0F, temppath, len);
		}
		free (temppath);
	}
	fillstr(sbuf, 0, 0x07, 0xc4, CONSOLE_MAX_X);
	if (!playlistactive)
		fillstr(sbuf, plScrWidth-15, 0x07, 0xc2, 1);
	displaystrattr(2, 0, sbuf, plScrWidth);

	if (fsEditWin||(selecte>=0))
	{
		int first=dirwinheight+3;
/*
		char longfile[270];
		struct modinfoentry *m1=0, *m2=0, *m3=0;
*/
		const char *modtype="";
/*
		longptr   mle->fullpath
		mle.name  mle->shortname
		char *longptr;
*/
		struct moduleinfostruct mi;

		if (mle->flags&MODLIST_FLAG_FILE)
		{
			mdbGetModuleInfo(&mi, mle->mdb_ref);
			modtype=mdbGetModTypeString(mi.modtype);
		} else {
			memset(&mi, 0, sizeof(mi));
		}

		fillstr(sbuf, 0, 0x07, 0xc4, CONSOLE_MAX_X);
		if (!playlistactive)
			fillstr(sbuf, plScrWidth-15, 0x07, 0xc1, 1);
		displaystrattr(first, 0, sbuf, plScrWidth);

		if (plScrWidth>=132)
		{
			writestring(sbuf, 0, 0x07, "  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa    \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa      title:    ", 42);

			fillstr(sbuf, 42, 0x07, 0xfa, plScrWidth - 100);
			writestring(sbuf, plScrWidth - 59, 0x07, "       type: \xfa\xfa\xfa\xfa     channels: \xfa\xfa      playtime: \xfa\xfa\xfa:\xfa\xfa   ", 59);

			writestring(sbuf, 2, 0x0F, mle->shortname, 12);

			if (mle->flags&MODLIST_FLAG_FILE)
			{
				writenum(sbuf, 15, 0x0F, mi.size, 10, 10, 1);

				if (mi.flags1&MDB_BIGMODULE)
					writestring(sbuf, 25, 0x0F, "!", 1);
			}
			if (mi.modname[0])
			{ /* we pad up here */
				int w=plScrWidth - 100;
				int l=sizeof(mi.modname);
				if (l>w)
					l=w;
				writestring(sbuf, 42, 0x0F, mi.modname, l);
				writestring(sbuf, 42+l, 0x0F, "", w-l);
			}
			if (selecte==0)
				markstring(sbuf, 42, plScrWidth - 100);
			if (*modtype)
				writestring(sbuf, plScrWidth - 46, 0x0F, modtype, 4);
			if (selecte==1)
				markstring(sbuf, plScrWidth - 46, 4);
			if (mi.channels)
				writenum(sbuf, plScrWidth - 27, 0x0F, mi.channels, 10, 2, 1);
			if (selecte==2)
				markstring(sbuf, plScrWidth - 27, 2);

			if (mi.playtime)
			{
				writenum(sbuf, plScrWidth - 9, 0x0F, mi.playtime/60, 10, 3, 1);
				writestring(sbuf, plScrWidth - 6, 0x0F, ":", 1);
				writenum(sbuf, plScrWidth - 5, 0x0F, mi.playtime%60, 10, 2, 0);
			}

			if (selecte==3)
				markstring(sbuf, plScrWidth - 9, 6);

			displaystrattr(first+1, 0, sbuf, plScrWidth);

			writestring(sbuf, 0, 0x07, "                                composer: ", 42);
			fillstr(sbuf, 42, 0x07, 0xfa, plScrWidth - 100);
			writestring(sbuf, plScrWidth - 58, 0x07, "     style: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa               ", 58);

			if (mi.composer[0])
			{ /* we pad up here */
				int w=plScrWidth - 100;
				int l=sizeof(mi.composer);
				if (l>w)
					l=w;
				writestring(sbuf, 42, 0x0F, mi.composer, l);
				writestring(sbuf, 42+l, 0x0F, "", w-l);
			}

			if (selecte==4)
				markstring(sbuf, 42, plScrWidth - 100);
			if (mi.style[0])
				writestring(sbuf, plScrWidth - 46, 0x0F, mi.style, 31);
			if (selecte==5)
				markstring(sbuf, plScrWidth - 46, 31);

			displaystrattr(first+2, 0, sbuf, plScrWidth);

			writestring(sbuf, 0, 0x07, "                                date:     \xfa\xfa.\xfa\xfa.\xfa\xfa\xfa\xfa     comment: ", 66);
			fillstr(sbuf, 66, 0x07, 0xfa,  plScrWidth - 69);
			writestring(sbuf, plScrWidth - 3, 0x07, "   ", 3);

			if (mi.date)
			{
				if (mi.date&0xFF)
				{
					writestring(sbuf, 44, 0x0F, ".", 3);
					writenum(sbuf, 42, 0x0F, mi.date&0xFF, 10, 2, 1);
				}
				if (mi.date&0xFFFF)
				{
					writestring(sbuf, 47, 0x0F, ".", 3);
					writenum(sbuf, 45, 0x0F, (mi.date>>8)&0xFF, 10, 2, 1);
				}
				if (mi.date>>16)
				{
					writenum(sbuf, 48, 0x0F, mi.date>>16, 10, 4, 1);
					if (!((mi.date>>16)/100))
						writestring(sbuf, 49, 0x0F, "'", 1);
				}
			}

			if (selecte==6)
				markstring(sbuf, 42, 10);
			if (mi.comment[0])
			{ /* we pad up here */
				int w=plScrWidth - 69;
				int l=sizeof(mi.comment);
				if (l>w)
					l=w;
				writestring(sbuf, 66, 0x0F, mi.comment, l);
				writestring(sbuf, 66+l, 0x0F, "", w-l);
			}
			if (selecte==7)
				markstring(sbuf, 66, plScrWidth - 69);
			displaystrattr(first+3, 0, sbuf, plScrWidth);

			writestring(sbuf, 0, 0x07, "    long: ", plScrWidth);
			dirdbGetFullName(mle->dirdbfullpath, npath, 0);
			tmppos=npath;
			if (strlen(tmppos)>=(plScrWidth - 10))
				tmppos+=strlen(tmppos)-(plScrWidth - 10);
			writestring(sbuf, 10, 0x0F, tmppos, plScrWidth - 10);

			displaystrattr(first+4, 0, sbuf, plScrWidth);
		} else {
			writestring(sbuf, 0, 0x07, "  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa   \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa   title: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", plScrWidth - 13 );
			writestring(sbuf, plScrWidth - 13 , 0x07, "  type: \xfa\xfa\xfa\xfa ", 80);

			writestring(sbuf, 2, 0x0F, mle->shortname, 12);

			if (mle->flags&MODLIST_FLAG_FILE)
			{
				writenum(sbuf, 15, 0x0F, mi.size, 10, 10, 1);

				if (mi.flags1&MDB_BIGMODULE)
					writestring(sbuf, 25, 0x0F, "!", 1);
			}
			if (mi.modname[0])
			{
				int w=plScrWidth - 48;
				int l=sizeof(mi.modname);
				if (l>w)
					l=w;
				writestring(sbuf, 35, /*(selecte==0)?0x8F:*/0x0F, mi.modname, l);
				writestring(sbuf, 35+l, /*(selecte==0)?0x8F:*/0x0F, "", w-l);
			}
			if (selecte==0)
				markstring(sbuf, 35, plScrWidth - 48);
			if (*modtype)
				writestring(sbuf, plScrWidth - 5, /*(selecte==1)?0x8F:*/0x0F, modtype, 4);
			if (selecte==1)
				markstring(sbuf, plScrWidth - 5, 4);

			displaystrattr(first+1, 0, sbuf, plScrWidth);

			writestring(sbuf, 0, 0x07, "   composer: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", plScrWidth - 35);
			writestring(sbuf, plScrWidth - 35, 0x07, "   date:     \xfa\xfa.\xfa\xfa.\xfa\xfa\xfa\xfa            ", 35);

			if (mi.date)
			{
				if (mi.date&0xFF)
				{
					writestring(sbuf, plScrWidth - 20, 0x0F, ".", 3);
					writenum(sbuf, plScrWidth - 22, 0x0F, mi.date&0xFF, 10, 2, 1);
				}
				if (mi.date&0xFFFF)
				{
					writestring(sbuf, plScrWidth - 17, 0x0F, ".", 3);
					writenum(sbuf, plScrWidth - 19, 0x0F, (mi.date>>8)&0xFF, 10, 2, 1);
				}
				if (mi.date>>16)
				{
					writenum(sbuf, plScrWidth - 16, 0x0F, mi.date>>16, 10, 4, 1);
					if (!((mi.date>>16)/100))
						writestring(sbuf, plScrWidth - 15, 0x0F, "'", 1);
				}

			}
			if (selecte==6)
				markstring(sbuf, plScrWidth - 22, 10);
			if (mi.composer[0])
			{ /* we pad up here */
				int w=plScrWidth - 47;
				int l=sizeof(mi.composer);
				if (l>w)
					l=w;
				writestring(sbuf, 13, 0x0F, mi.composer, l);
				writestring(sbuf, 13+l, 0x0F, "", w-l);
			}

			if (selecte==4)
				markstring(sbuf, 13, plScrWidth - 47);

			displaystrattr(first+2, 0, sbuf, plScrWidth);

			writestring(sbuf, 0, 0x07, "   style:    \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", plScrWidth - 35);
			writestring(sbuf, plScrWidth - 35, 0x07, "   playtime: \xfa\xfa\xfa:\xfa\xfa   channels: \xfa\xfa ", 35);

			if (mi.channels)
				writenum(sbuf, plScrWidth - 3, 0x0F, mi.channels, 10, 2, 1);
			if (selecte==2)
				markstring(sbuf, plScrWidth - 3, 2);
			if (mi.playtime)
			{
				writenum(sbuf, plScrWidth - 22, 0x0F, mi.playtime/60, 10, 3, 1);
				writestring(sbuf, plScrWidth - 19, 0x0F, ":", 1);
				writenum(sbuf, plScrWidth - 18, 0x0F, mi.playtime%60, 10, 2, 0);

			}
			if (selecte==3)
				markstring(sbuf, plScrWidth - 22, 6);
			if (mi.style[0])
			{ /* we pad up here */
				int w=plScrWidth - 49;
				int l=sizeof(mi.style);
				if (l>w)
					l=w;
				writestring(sbuf, 13, 0x0F, mi.style, l);
				writestring(sbuf, 13+l, 0x0F, "", w-l);
			}

			if (selecte==5)
				markstring(sbuf, 13, plScrWidth - 49);

			displaystrattr(first+3, 0, sbuf, plScrWidth);

			writestring(sbuf, 0, 0x07, "   comment:  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", plScrWidth - 4);
			writestring(sbuf, plScrWidth - 4, 0x07, "    ", 4);

			if (mi.comment[0])
			{ /* we pad up here */
				int w=plScrWidth - 17;
				int l=sizeof(mi.comment);
				if (l>w)
					l=w;
				writestring(sbuf, 13, 0x0F, mi.comment, l);
				writestring(sbuf, 13+l, 0x0F, "", w-l);
			}
			if (selecte==7)
				markstring(sbuf, 13, plScrWidth - 17);
			displaystrattr(first+4, 0, sbuf, plScrWidth);

			writestring(sbuf, 0, 0x07, "   long: ", plScrWidth);
			dirdbGetFullName(mle->dirdbfullpath, npath, 0);
			tmppos=npath;
			if (strlen(tmppos)>=(plScrWidth - 9))
				tmppos+=strlen(tmppos)-(plScrWidth - 9);
			writestring(sbuf, 9, 0x0F, tmppos, plScrWidth - 9);

			displaystrattr(first+5, 0, sbuf, plScrWidth);
		}
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
				displayfile(i+3, 0, plScrWidth-15, m, ((firstv+i)!=selectv)?0:(selecte<0)?1:2);
			}

			if (/*((firstp+i)<0)||*/((firstp+i)>=playlist->num))
				displayvoid(i+3, plScrWidth-14, 14);
			else {
				m=modlist_get(playlist, firstp+i);
				displayfile(i+3, plScrWidth-14, 14, m, ((firstp+i)!=selectp)?0:(selecte<0)?1:2);
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

	plSetTextMode(fsScrType);
	while (1)
	{
		const char *fsInfoModes[]= {"name and size","composer","comment","style and playtime","long filenames"};
		uint16_t sbuf[128];
		const char *modename = plGetDisplayTextModeName();

		make_title("file selector setup");

		displaystr(1, 0, 0x07, "1:  screen mode: ",17);
		displaystr(1, 17, 0x0f, modename, 64);
		/*displaystr(1, 0, 0x07, "1:  screen mode (if driver supports it TODO): ", 45);
		displaystr(1, 45, 0x0F, (fsScrType&4)?"132x":" 80x", 4);
		displaystr(1, 49, 0x0F, ((fsScrType&3)==0)?"25":((fsScrType&3)==1)?"30":((fsScrType&3)==2)?"50":"60", 69);*/
		displaystr(2, 0, 0x07, "2:  scramble module list order: ", 32);
		displaystr(2, 32, 0x0F, fsListScramble?"on":"off", 48);
		displaystr(3, 0, 0x07, "3:  remove modules from playlist when played: ", 46);
		displaystr(3, 46, 0x0F, fsListRemove?"on":"off", 34);
		displaystr(4, 0, 0x07, "4:  loop modules: ", 18);
		displaystr(4, 18, 0x0F, fsLoopMods?"on":"off", 62);
		displaystr(5, 0, 0x07, "5:  scan module informatin: ", 28);
		displaystr(5, 28, 0x0F, fsScanNames?"on":"off", 52);
		displaystr(6, 0, 0x04, "6:  scan module information files: ", 35);
		displaystr(6, 35, 0x0F, fsScanMIF?"on":"off", 45);
		displaystr(7, 0, 0x07, "7:  scan archive contents: ", 27);
		displaystr(7, 27, 0x0F, fsScanArcs?"on":"off", 53);
		displaystr(8, 0, 0x07, "8:  scan module information in archives: ", 41);
		displaystr(8, 41, 0x0F, fsScanInArc?"on":"off", 39);
		displaystr(9, 0, 0x07, "9:  save module information to disk: ", 37);
		displaystr(9, 37, 0x0F, fsWriteModInfo?"on":"off", 42);
		displaystr(10, 0, 0x07, "A:  edit window: ", 17);
		displaystr(10, 17, 0x0F, fsEditWin?"on":"off", 63);
		displaystr(11, 0, 0x07, "B:  module type colors: ", 24);
		displaystr(11, 24, 0x0F, fsColorTypes?"on":"off", 56);
		displaystr(12, 0, 0x07, "C:  module information display mode: ", 37);
		displaystr(12, 37, 0x0F, fsInfoModes[fsInfoMode], 43);
		displaystr(13, 0, 0x07, "D:  put archives: ", 18);
		displaystr(13, 18, 0x0F, fsPutArcs?"on":"off", 43);

		fillstr(sbuf, 0, 0x00, 0, 128);
		writestring(sbuf, 0, 0x07, "+-: Target framerate: ", 22);
		writenum(sbuf, 22, 0x0f, fsFPS, 10, 3, 1);
		writestring(sbuf, 25, 0x07, ", actual framerate: ", 20);
		writenum(sbuf, 45, 0x0f, LastCurrent=fsFPSCurrent, 10, 3, 1);
		displaystrattr(14, 0, sbuf, 128);

		displaystr(16, 0, 0x07, "ALT-S (or CTRL-S if in X) to save current setup to ocp.ini", 58);
		displaystr(plScrHeight-1, 0, 0x17, "  press the number of the item you wish to change and ESC when done", plScrWidth);

		displaystr(17, 0, 0x03, (stored?"ocp.ini saved":""), 13);

		while (!ekbhit()&&(LastCurrent==fsFPSCurrent))
			framelock();
		if (!ekbhit())
			continue;

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
			case 27:
				    return;
			case KEY_ALT_K:
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
				cpiKeyHelpDisplay();
				break;
		}
	}
}

static struct moduleinfostruct mdbEditBuf;

static unsigned char fsEditModType(unsigned char oldtype)
{
	unsigned char index[256];
	int length=0;
	int curindex=0;

	int i;
	const char *temp;
	int done=0;

	const int Height=20;
	const int iHeight=Height-1;
	const int Width=18;
	int Top=(plScrHeight-Height)/2;
	int Left=(plScrWidth-Width)/2;
	const int Mid = 7;

	int editcol=0;

	for (i=0;i<256;i++)
	{
		temp=mdbGetModTypeString(i);
		if ((temp[0])||(i==mtUnRead))
		{
			index[length]=i;
			if (i==oldtype)
				curindex=length;
			length++;
		}
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

	while (ekbhit())
		egetch();
	while (!done)
	{
		int offset;
		if (length>iHeight)
		{
			if (curindex<=(iHeight/2))
				offset=0;
			else if (curindex>=(length-iHeight/2))
				offset=length-iHeight;
			else
				offset=curindex-iHeight/2;
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
						done=1;
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
					} else
						return index[curindex];
					break;
				case KEY_ALT_K:
					cpiKeyHelp(KEY_RIGHT, "Edit color");
					cpiKeyHelp(KEY_LEFT, "Edit color");
					cpiKeyHelp(KEY_UP, "Select another filetype / change color");
					cpiKeyHelp(KEY_DOWN, "Select another filetype / change color");
					cpiKeyHelp(KEY_ESC, "Abort edit");
					cpiKeyHelp(_KEY_ENTER, "Select the highlighted filetype");
					cpiKeyHelpDisplay();
					break;
			}
		}
	}
	return oldtype;
}

static int fsEditString(unsigned int y, unsigned int x, unsigned int w, unsigned int l, char *s)
{
	char str[PATH_MAX+NAME_MAX+1];
	char *p=str;

	unsigned int curpos;
	unsigned int cmdlen;
	int insmode=1;
	unsigned int scrolled=0;

	/* ASSERT point */
	if (l>=sizeof(str))
		l=sizeof(str)-1;

	strcpy(str, s);
	str[l]=0;

	curpos=strlen(p);
	cmdlen=strlen(p);

	setcurshape(1);

	while (1)
	{
		displaystr(y, x, 0x8F, p+scrolled, w);
		setcur(y, x+curpos-scrolled);
		while (!ekbhit())
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
						memmove(p+curpos+1, p+curpos, cmdlen-curpos+1);
						p[curpos]=key;
						curpos++;
						cmdlen++;
					}
				} else if (curpos==cmdlen)
				{
					if (cmdlen<l)
					{
						p[curpos++]=key;
						p[curpos]=0;
						cmdlen++;
					}
				} else
					p[curpos++]=key;
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
						memmove(p+curpos, p+curpos+1, cmdlen-curpos);
						cmdlen--;
					}
					break;
				case KEY_BACKSPACE:
					if (curpos)
					{
						memmove(p+curpos-1, p+curpos, cmdlen-curpos+1);
						curpos--;
						cmdlen--;
					}
					break;
				case KEY_ESC:
					setcurshape(0);
					return 0;
				case _KEY_ENTER:
					setcurshape(0);
					strncpy(s, str, l);
					return 1;
				case KEY_ALT_K:
					cpiKeyHelp(KEY_RIGHT, "Move cursor right");
					cpiKeyHelp(KEY_LEFT, "Move cursor left");
					cpiKeyHelp(KEY_HOME, "Move cursor home");
					cpiKeyHelp(KEY_END, "Move cursor to the end");
					cpiKeyHelp(KEY_INSERT, "Toggle insert mode");
					cpiKeyHelp(KEY_DELETE, "Remove character at cursor");
					cpiKeyHelp(KEY_BACKSPACE, "Remove character left of cursor");
					cpiKeyHelp(KEY_ESC, "Cancel changes");
					cpiKeyHelp(_KEY_ENTER, "Submit changes");
					cpiKeyHelpDisplay();
					break;

			}
			while ((curpos-scrolled)>=w)
				scrolled+=8;
			/*
			while ((curpos-scrolled)<0)
				scrolled-=8;
			*/
		}
	}
}

static void fsEditChan(int y, int x, uint8_t *chan)
{
	int curpos=0;
	char str[3];
	convnum(*chan, str, 10, 2, 0);

	setcurshape(2);

	while (1)
	{
		displaystr(y, x, 0x8F, str, 2);
		setcur(y, x+curpos);
		while (!ekbhit())
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
					return;
				case _KEY_ENTER:
					*chan=(str[0]-'0')*10+str[1]-'0';
					setcurshape(0);
					return;
				case KEY_ALT_K:
					cpiKeyHelp(KEY_RIGHT, "Move cursor right");
					cpiKeyHelp(KEY_LEFT, "Move cursor left");
					cpiKeyHelp(KEY_BACKSPACE, "Move cursor right");
					cpiKeyHelp(KEY_ESC, "Cancel changes");
					cpiKeyHelp(_KEY_ENTER, "Submit changes");
					cpiKeyHelpDisplay();
					break;
			}
		}
	}
}

static void fsEditPlayTime(int y, int x, uint16_t *playtime)
{
	char str[7];
	int curpos;

	convnum((*playtime)/60, str, 10, 3, 0);
	str[3]=':';
	convnum((*playtime)%60, str+4, 10, 2, 0);

	curpos=(str[0]!='0')?0:(str[1]!='0')?1:2;

	setcurshape(2);

	while (1)
	{
		displaystr(y, x, 0x8F, str, 6);
		setcur(y, x+curpos);
		while (!ekbhit())
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
					return;
				case _KEY_ENTER:
					*playtime=((((str[0]-'0')*10+str[1]-'0')*10+str[2]-'0')*6+str[4]-'0')*10+str[5]-'0';
					setcurshape(0);
					return;
				case KEY_ALT_K:
					cpiKeyHelp(KEY_RIGHT, "Move cursor right");
					cpiKeyHelp(KEY_LEFT, "Move cursor left");
					cpiKeyHelp(KEY_BACKSPACE, "Move cursor right");
					cpiKeyHelp(KEY_ESC, "Cancel changes");
					cpiKeyHelp(_KEY_ENTER, "Submit changes");
					cpiKeyHelpDisplay();
					break;
			}
		}
	}
}

static void fsEditDate(int y, int x, uint32_t *date)
{
	char str[11];
	int curpos=0;
	convnum((*date)&0xFF, str, 10, 2, 0);
	str[2]='.';
	convnum(((*date)>>8)&0xFF, str+3, 10, 2, 0);
	str[5]='.';
	convnum((*date)>>16, str+6, 10, 4, 0);

	setcurshape(2);

	while (1)
	{
		displaystr(y, x, 0x8F, str, 10);
		setcur(y, x+curpos);
		while (!ekbhit())
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
					return;
				case _KEY_ENTER:
					*date=((str[0]-'0')*10+str[1]-'0')|(((str[3]-'0')*10+str[4]-'0')<<8)|(((((str[6]-'0')*10+str[7]-'0')*10+str[8]-'0')*10+str[9]-'0')<<16);
					setcurshape(0);
					return;
				case KEY_ALT_K:
					cpiKeyHelp(KEY_RIGHT, "Move cursor right");
					cpiKeyHelp(KEY_LEFT, "Move cursor left");
					cpiKeyHelp(KEY_BACKSPACE, "Move cursor right");
					cpiKeyHelp(KEY_ESC, "Cancel changes");
					cpiKeyHelp(_KEY_ENTER, "Submit changes");
					cpiKeyHelpDisplay();
					break;
			}
		}
	}
}

static int fsEditFileInfo(struct modlistentry *me)
{
	if (!mdbGetModuleInfo(&mdbEditBuf, me->mdb_ref))
		return 1;

	if (plScrWidth>=132)
		switch (editpos)
		{
			case 0:
				fsEditString(plScrHeight-5, 42, plScrWidth - 100, sizeof(mdbEditBuf.modname), mdbEditBuf.modname);
				break;
			case 1:
				mdbEditBuf.modtype = fsEditModType(mdbEditBuf.modtype);
				break;
			case 2:
				fsEditChan(plScrHeight-5, plScrWidth - 27, &mdbEditBuf.channels);
				break;
			case 3:
				fsEditPlayTime(plScrHeight-5, plScrWidth - 9, &mdbEditBuf.playtime);
				break;
			case 4:
				fsEditString(plScrHeight-4, 42, plScrWidth - 100, sizeof(mdbEditBuf.composer), mdbEditBuf.composer);
				break;
			case 5:
				fsEditString(plScrHeight-4, plScrWidth - 46, 31, sizeof(mdbEditBuf.style), mdbEditBuf.style);
				break;
			case 6:
				fsEditDate(plScrHeight-3, 42, &mdbEditBuf.date);
				break;
			case 7:
				fsEditString(plScrHeight-3, 66, plScrWidth - 69, sizeof(mdbEditBuf.comment), mdbEditBuf.comment);
				break;
		} else switch (editpos)
		{
			case 0:
				fsEditString(plScrHeight-6, 35, plScrWidth - 48, sizeof(mdbEditBuf.modname), mdbEditBuf.modname);
				break;
			case 1:
				/*fsEditString(plScrHeight-6, plScrWidth - 5, 4, 4, typeidx);*/
				mdbEditBuf.modtype = fsEditModType(mdbEditBuf.modtype);
				break;
			case 2:
				fsEditChan(plScrHeight-4, plScrWidth - 3, &mdbEditBuf.channels);
				break;
			case 3:
				fsEditPlayTime(plScrHeight-4, plScrWidth - 22, &mdbEditBuf.playtime);
				break;
			case 4:
				fsEditString(plScrHeight-5, 13, plScrWidth - 47, sizeof(mdbEditBuf.composer), mdbEditBuf.composer);
				break;
			case 5:
				fsEditString(plScrHeight-4, 13, plScrWidth - 49, sizeof(mdbEditBuf.style), mdbEditBuf.style);
				break;
			case 6:
				fsEditDate(plScrHeight-5, plScrWidth - 22, &mdbEditBuf.date);
				break;
			case 7:
				fsEditString(plScrHeight-3, 13, plScrWidth - 17, sizeof(mdbEditBuf.comment), mdbEditBuf.comment);
				break;
		}
/*
	if (editpos==1)
	{
		typeidx[4]=0;
		mdbEditBuf.modtype=mdbReadModType(typeidx);
	}*/
	if (!mdbWriteModuleInfo(me->mdb_ref, &mdbEditBuf))
		return 0;
	return 1;
}

static char fsEditViewPath(void)
{
#warning fsEditString API should have a dynamic version, FIXME
	char path[128*1024];

	{
		char *temppath;
		dirdbGetFullname_malloc (dirdbcurdirpath, &temppath, DIRDB_FULLNAME_ENDSLASH);
		free (temppath);
		snprintf(path, sizeof(path), "%s%s", temppath, curmask);
	}

	if (fsEditString(1, 0, plScrWidth, sizeof(path), path))
	{
		struct dmDrive *drives;
		char *drive;
		char *path;
		char *name;
		char *ext;
		uint32_t newcurrentpath;

		splitpath_malloc(path, &drive, &path, &name, &ext);
		for (drives = dmDrives; drives; drives = drives->next)
		{
			if (strcasecmp(drive, drives->drivename))
			{
				continue;
			}
			dmCurDrive=drives;
			if (strlen(path))
			{
				newcurrentpath = dirdbResolvePathWithBaseAndRef(dmCurDrive->basepath, path);
				dirdbUnref(dirdbcurdirpath);
				dirdbUnref(dmCurDrive->currentpath);
				dirdbcurdirpath = dmCurDrive->currentpath = newcurrentpath;
				dirdbRef(dirdbcurdirpath);
			}
			if (strlen(name)+strlen(ext)<=PATH_MAX)
			{
				strcpy(curmask, name);
				strcat(curmask, ext);
			}
			break;
		}
		free (drive);
		free (path);
		free (name);
		free (ext);

		if (!fsScanDir(0))
			return 0;
	}
	return 1;
}

signed int fsFileSelect(void)
{
	int win=0;
	unsigned long i;
	int curscanned=0;
	struct modlistentry *m;

	plSetTextMode(fsScrType);
	fsScrType=plScrType;

	isnextplay=NextPlayNone;

	quickfindpos=0;

	if (fsPlaylistOnly)
		return 0;

	while (1)
	{
		int firstv, firstp;
		uint16_t c;
		struct modlist *curlist;

		dirwinheight=plScrHeight-4;
		if (fsEditWin||editmode)
			dirwinheight-=(plScrWidth>=132)?5:6;

		if (!playlist->num)
		{
			win=0;
			playlist->pos=0;
		} else {
			if (playlist->pos>=playlist->num)
				playlist->pos=playlist->num-1;
			/*
			if (playlist->pos<0)
				playlist->pos=0;
			*/
		}
		if (!currentdir->num)
		{ /* this should never happen */
			currentdir->pos=0;
		} else {
			if (currentdir->pos>=currentdir->num)
				currentdir->pos=currentdir->num-1;
			/*
			if (currentdir->pos<0)
				currentdir->pos=0;
			*/
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
		if (!win)
			m=modlist_getcur(currentdir);
		else
			m=modlist_getcur(playlist);

		fsShowDir(firstv, win?(unsigned)~0:currentdir->pos, firstp, win?playlist->pos:(unsigned)~0, editmode?editpos:~0, m, win);

		if (!ekbhit()&&fsScanNames)
		{
			if (curscanned||(mdbInfoRead(m->mdb_ref)))
			{
				while (((!win)||(scanposp>=playlist->num)) && (scanposf<currentdir->num))
				{
					struct modlistentry *scanm;
					if ((scanm=modlist_get(currentdir, scanposf++)))
						if ((scanm->flags&MODLIST_FLAG_FILE)&&(!(scanm->flags&MODLIST_FLAG_VIRTUAL)))
							if (!mdbInfoRead(scanm->mdb_ref))
							{
								mdbScan(scanm);
								break;
							}
				}
				while (((win)||(scanposf>=currentdir->num)) && (scanposp<playlist->num))
				{
					struct modlistentry *scanm;
					if ((scanm=modlist_get(playlist, scanposp++)))
						if ((scanm->flags&MODLIST_FLAG_FILE)&&(!(scanm->flags&MODLIST_FLAG_VIRTUAL)))
							if (!mdbInfoRead(scanm->mdb_ref))
							{
								mdbScan(scanm);
								break;
							}
				}
				framelock();
			} else {
				curscanned=1;
				if ((m->flags&MODLIST_FLAG_FILE)&&((!(m->flags&MODLIST_FLAG_VIRTUAL))||fsScanInArc)) /* dirty hack to stop scanning in .tar.gz while scrolling */
					mdbScan(m);
			}
			continue;
		}
		c=egetch();
		curscanned=0;
#ifdef DOS32
		if(c==0xF8) /* : screen shot */
		{
			TextScreenshot(fsScrType);
			continue;
		}
#endif

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

		quickfindpos=0;

		curlist=(win?playlist:currentdir);
		m=modlist_getcur(curlist); /* this is not actually needed, m should be preserved from above logic */

		switch (c)
		{
			case KEY_ALT_K:
				cpiKeyHelp(KEY_ESC, "Exit");
				cpiKeyHelp(KEY_CTRL_BS, "Stop filescanning");
				cpiKeyHelp(KEY_ALT_S, "Stop filescanning");
				cpiKeyHelp(KEY_TAB, "Toggle between filelist and playlist");
				cpiKeyHelp(KEY_SHIFT_TAB, "Toggle between lists and editwindow");
				cpiKeyHelp(KEY_ALT_E, "Toggle between lists and editwindow");
				cpiKeyHelp(KEY_ALT_I, "Cycle file-list mode (fullname, title, time etc)");
				cpiKeyHelp(KEY_ALT_C, "Show setup dialog");
				cpiKeyHelp(KEY_ALT_P, "Save playlist");
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
				cpiKeyHelpDisplay();
				break;
			case KEY_ESC:
				return 0;
			case KEY_ALT_R:
				if (m->flags&MODLIST_FLAG_FILE)
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
				fsSavePlayList(playlist);
				break;
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
				if (!fsEditViewPath())
					return -1;
				break;
			case _KEY_ENTER:
				if (editmode)
					if (m->flags&MODLIST_FLAG_FILE)
					{
						if (!fsEditFileInfo(m))
							return -1;
						break;
					}
				if (win)
				{
					if (!playlist->num)
						break;
					isnextplay=NextPlayPlaylist;
					return 1;
				} else {
					if (m->flags&(MODLIST_FLAG_DIR|MODLIST_FLAG_DRV|MODLIST_FLAG_ARC))
					{
						uint32_t olddirpath;
						unsigned int i;

						olddirpath = dmCurDrive->currentpath;
						dirdbRef(olddirpath);
						dirdbUnref(dirdbcurdirpath);

						dirdbcurdirpath=m->dirdbfullpath;
						dirdbRef(dirdbcurdirpath);

						dmCurDrive=(struct dmDrive *)m->drive;
						dirdbUnref(dmCurDrive->currentpath);
						dmCurDrive->currentpath = m->dirdbfullpath;
						dirdbRef(dmCurDrive->currentpath);

						fsScanDir(0);
						for (i=0;i<currentdir->num;i++)
						{
							if (currentdir->files[i]->dirdbfullpath==olddirpath)
								break;
						}
						dirdbUnref(olddirpath);
						if (i<currentdir->num)
							currentdir->pos=i;
					} else if (m->flags&(MODLIST_FLAG_FILE|MODLIST_FLAG_VIRTUAL))
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
					if (plScrWidth>=132)
						editpos="\x00\x01\x02\x03\x00\x01\x04\x05"[editpos];
					else
						editpos="\x00\x01\x06\x06\x00\x04\x00\x05"[editpos];
				else if (!win)
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
					if (plScrWidth>=132)
						editpos="\x04\x05\x05\x05\x06\x07\x06\x07"[editpos];
					else
						editpos="\x04\x06\x07\x07\x05\x07\x03\x07"[editpos];
				else if (!win)
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
				{
					int add = editmode?1:dirwinheight;
			/*case 0x5100: //pgdn*/
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
					if (plScrWidth>=132)
						editpos="\x01\x02\x03\x03\x05\x05\x07\x07"[editpos];
					else
						editpos="\x01\x01\x02\x02\x06\x03\x06\x07"[editpos];
				}
				break;
			case KEY_INSERT:
			/*case 0x5200: // add*/
				if (editmode)
					break;
				if (win)
				{
					/*if (!*/modlist_append(playlist, m)/*)
						return -1*/;
					/*playlist->pos=playlist->num-1; */
					scanposp=fsScanNames?0:~0;
				} else {
					if (m->flags&MODLIST_FLAG_DIR)
					{
						if (!(fsReadDir(playlist, m->drive, m->dirdbfullpath, curmask, RD_PUTRSUBS)))
							return -1;
					} else if (m->flags&MODLIST_FLAG_FILE)
					{
						/*if (!*/modlist_append(playlist, m)/*)
							return -1*/;
						scanposp=fsScanNames?0:~0;
					}
				}
				break;
			case KEY_LEFT:
			/*case 0x4B00: // left*/
				if (editmode)
				{
					if (plScrWidth>=132)
						editpos="\x00\x00\x01\x02\x04\x04\x06\x06"[editpos];
					else
						editpos="\x00\x00\x03\x05\x04\x05\x04\x07"[editpos];
				}
				break;
			case KEY_DELETE:
			/*case 0x5300: // del*/
				if (editmode)
					break;
				if (win)
					modlist_remove(playlist, playlist->pos, 1);
				else {
					long f;

					if (m->flags&MODLIST_FLAG_DIR)
					{
						struct modlist *tl = modlist_create();
						struct modlistentry *me;
						int f;
						if (!(fsReadDir(tl, m->drive, m->dirdbfullpath, curmask, RD_PUTRSUBS)))
							return -1;
						for (i=0;i<tl->num;i++)
						{
							me=modlist_get(tl, i);
							if ((f=modlist_find(playlist, me->dirdbfullpath))>=0)
								modlist_remove(playlist, f, 1);
						}
						modlist_free(tl);
					} else if (m->flags&MODLIST_FLAG_FILE)
					{
						f=modlist_find(playlist, m->dirdbfullpath);
						if (f!=-1)
							modlist_remove(playlist, f, 1);
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
						if (me->flags&MODLIST_FLAG_FILE)
						{
							/*if (!*/modlist_append(playlist, me)/*)
								return -1*/;
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
				modlist_remove(playlist, 0, playlist->num);
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
  /*return 0; the above while loop doesn't go to this point */
}

static int stdReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
{
	struct modlistentry m;
	struct dmDrive *d;

	if (opt&RD_PUTSUBS)
	{
		uint32_t dirdbparent = dirdbGetParentAndRef(path);

		if (path!=drive->basepath)
		{
			memset(&m, 0, sizeof(struct modlistentry));
			m.drive=drive;
			strcpy(m.name, "/");
			strcpy(m.shortname, "/");
			m.flags=MODLIST_FLAG_DIR;
			m.dirdbfullpath=drive->basepath;
			modlist_append(ml, &m);

			if (dirdbparent!=DIRDB_NOPARENT)
			{
				memset(&m, 0, sizeof(struct modlistentry));
				m.drive=drive;
				strcpy(m.name, "..");
				strcpy(m.shortname, "..");
				m.flags=MODLIST_FLAG_DIR;
				m.dirdbfullpath=dirdbparent;
				modlist_append(ml, &m);
			}
		}

		if (dirdbparent!=DIRDB_NOPARENT)
			dirdbUnref(dirdbparent);

		for (d=dmDrives;d;d=d->next)
		{
			memset(&m, 0, sizeof(struct modlistentry));

			m.drive=d;
			strcpy(m.name, d->drivename);
			strncpy(m.shortname, d->drivename, 12);
			m.flags=MODLIST_FLAG_DRV;
			m.dirdbfullpath=d->currentpath;
			dirdbRef(m.dirdbfullpath);
			modlist_append(ml, &m);
			dirdbUnref(m.dirdbfullpath);
		}
	}
	return 1;
}

static void fsSavePlayList(const struct modlist *ml)
{
#warning fsEditString API should have a dynamic version, FIXME
	char path[128*1024];
	int mlTop=plScrHeight/2-2;
	unsigned int i;
	char *dr;
	char *di;
	char *fn;
	char *ext;
	char *newpath;
	FILE *f;

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

	{
		char *curdirpath;
		dirdbGetFullname_malloc (dirdbcurdirpath, &curdirpath, DIRDB_FULLNAME_ENDSLASH);
		snprintf (path, sizeof (path), "%s", curdirpath);
		free (curdirpath);
	}

	if (!fsEditString(mlTop+2, 5, plScrWidth-10, sizeof(path), path))
	{
		return;
	}

	splitpath_malloc(path, &dr, &di, &fn, &ext);
	if (!*ext)
	{
		free (ext);
		ext = strdup (".pls");
	}

	if (strcmp(dr, "file:"))
	{
		fprintf(stderr, "[filesel] file: is the only supported transport currently\n");
		free (dr);
		free (di);
		free (fn);
		free (ext);
		return;
	}

	makepath_malloc (&newpath, NULL, di, fn, ext);
	free (dr);
	free (fn);
	free (ext);

	if (!(f=fopen(newpath, "w")))
	{
		fprintf (stderr, "Failed to create file %s: %s\n", newpath, strerror (errno));
		free (di);
		free (newpath);
		return;
	}
	free (newpath);
	fprintf(f, "[playlist]\n");
	fprintf(f, "NumberOfEntries=%d\n", ml->num);

	for (i=0; i<ml->num; i++)
	{
		char *npath, *nnpath;
		struct modlistentry *m;
		fprintf(f, "File%d=",i+1);
		m=modlist_get(ml, i);
		if (m->drive!=dmFILE)
		{
			dirdbGetFullname_malloc (m->dirdbfullpath, &npath, 0);
			fputs (npath, f);
			free (npath);
		} else {
			dirdbGetFullname_malloc (m->dirdbfullpath, &npath, DIRDB_FULLNAME_NOBASE);
			genreldir_malloc (di, npath, &nnpath);
			fputs (nnpath, f);
			free (npath);
			free (nnpath);
		}
		fprintf(f, "\n");

	}
	free (di);
	fclose(f);

	fsScanDir(1);
}



#if 0



/* code for fullpath2 taken from bc3.1, converted and modified..., and redone for unix world.. anything left of the code??? */

static void reducepath(char *buf)
{
	char c;
	char *src,*dst;
	src=dst=buf;
	while (1)
	{
		c=*src++;
		if (!c||(c=='/'))
		{
			if ((dst[-1]=='.')&&(dst[-2]=='/'))
				dst-=2;
			else if ((dst[-1]=='.')&&(dst[-2]=='.')&&(dst[-3]=='/'))
			{
				/*if (dst[-4]!=':')   we don't have drive letters on this OS*/
				{
					dst-=3;
					while (*--dst!='/');
				}
			}
			if (!c)
			{
				if (dst[-1]=='/')
					dst--;
				/*if (dst[-1]==':')   still no drive letters on this OS
					*dst++='\\';*/
				*dst=0;
				break;
			} else
				*dst++='/';
		} /*else *dst++=toupper(c);   and we are case sentive */
	}
}

/* We can't relay on files having 8:3 format in unix, so we have redone this part quite heavily

static void convfilename12wc(char *c, const char *f, const char *e)
{
  int i;
  for (i=0; i<8; i++)
    *c++=(*f=='*')?'?':*f?*f++:' ';
  for (i=0; i<4; i++)
    *c++=(*e=='*')?'?':*e?*e++:' ';
  for (i=0; i<12; i++)
    c[i-12]=toupper(c[i-12]);
}

void fsConv12FileName(char *f, const char *c)
{
  int i;
  for (i=0; i<8; i++)
    if (c[i]==' ')
      break;
    else
      *f++=c[i];
  for (i=8; i<12; i++)
    if (c[i]==' ')
      break;
    else
      *f++=c[i];
  *f=0;
}

static void conv12filenamewc(char *f, const char *c)
{
  char *f0=f;
  short i;
  for (i=0; i<8; i++)
    if (c[i]==' ')
      break;
    else
      *f++=c[i];
  if (i==8)
  {
    for (i=7; i>=0; i--)
      if (c[i]!='?')
        break;
    if (++i<7)
    {
      f-=8-i;
      *f++='*';
    }
  }
  for (i=8; i<12; i++)
    if (c[i]==' ')
      break;
    else
      *f++=c[i];
  if (i==12)
  {
    for (i=11; i>=9; i--)
      if (c[i]!='?')
        break;
    if (++i<10)
    {
      f-=12-i;
      *f++='*';
    }
  }
  *f=0;
}

int fsMatchFileName12(const char *a, const char *b)
{
  int i;
  for (i=0; i<12; i++, a++, b++)
    if ((i!=8)&&(*b!='?')&&(*a!=*b))
      break;
  return i==12;
}*/

struct directorynode
{
  char name[_MAX_NAME+1];
  unsigned short parent;
};

static struct directorynode *dmTree;
static unsigned short *dmReloc;
static unsigned short dmMaxNodes;
static unsigned short dmNumNodes;
static unsigned char dmCurDrive;
static unsigned short dmDriveDirs[27];

static unsigned short dmGetPathReference(const char *p, const char *endp)
{
	const char *stp;
	unsigned short v;
	char subdir[_MAX_NAME+1];
	unsigned short *min;
	unsigned short num;
	unsigned short mn;

	if (endp[-1]=='/')
		endp--;
	stp=endp;
	while (((stp+_MAX_NAME-1)>endp)&&(stp>p)&&(stp[-1]!='/'))
		stp--;

	if (stp!=p)
	{
		v=dmGetPathReference(p, stp);
		if (v==0xFFFF)
			return 0xFFFF;
	} else
		v=0xFFFF;

	memcpy(subdir, stp, endp-stp);
	subdir[endp-stp]='\\';
	subdir[endp-stp+1]=0;
	strupr(subdir);

	min=dmReloc;
	num=dmNumNodes;

	while (num)
	{
		int res=strcmp(subdir, dmTree[min[num>>1]].name);
		if (!res)
		{
			res=dmTree[min[num>>1]].parent-v;
			if (!res)
				return min[num>>1];
		}
		if (res<0)
			num>>=1;
		else {
			min+=(num>>1)+1;
			num=(num-1)>>1;
		}
	}
	mn=min-dmReloc;

	if (dmNumNodes==dmMaxNodes)
	{
		void *n1, *n2;
		dmMaxNodes+=256;
		n1=realloc(dmTree, sizeof (*dmTree)*dmMaxNodes);
		n2=realloc(dmReloc, sizeof (*dmReloc)*dmMaxNodes);
		if (!n1||!n2)
			return 0xFFFF;
		dmTree=(directorynode *)n1;
		dmReloc=(unsigned short *)n2;
	}

	mymemmovew(dmReloc+mn+1, dmReloc+mn, dmNumNodes-mn);

	dmReloc[mn]=dmNumNodes;

	strcpy(dmTree[dmNumNodes].name, subdir);
	dmTree[dmNumNodes].parent=v;

	return dmNumNodes++;
}

unsigned short dmGetPathReference(const char *p)
{
	return dmGetPathReference(p, p+strlen(p));
}

static unsigned short dmGetParent(unsigned short ref)
{
	return (dmTree[ref].parent==0xFFFF)?ref:dmTree[ref].parent;
}

static unsigned short dmGetRoot(unsigned short ref)
{
	while (dmTree[ref].parent!=0xFFFF)
		ref=dmTree[ref].parent;
	return ref;
}

char *dmGetPathRel(char *path, unsigned short ref, unsigned short base)
{
	if ((ref==0xFFFF)||(ref==base))
	{
		*path=0;
		return path;
	}
	if (ref>=0xFFE0)
	{
		path[0]='@'+ref-0xFFE0;
		path[1]=':';
		path[2]=0;
		return path;
	}
	dmGetPathRel(path, dmTree[ref].parent, base);
	strcat(path, dmTree[ref].name);
	return path;
}

char *dmGetPath(char *path, unsigned short ref)
{
	return dmGetPathRel(path, ref, 0xFFFF);
}

static unsigned short dmConvertReference(unsigned short ref)
{
	unsigned char *drv;
	char path[_MAX_PATH];

	if (ref<0xFFE0)
		return ref;


	drv=ref-0xFFE0;

	if (dmDriveDirs[drv]<0xFFE0)
		return dmDriveDirs[drv];

  if (drv)
  {
    unsigned int savedrive, dum;
    _dos_getdrive(&savedrive);
    _dos_setdrive(drv, &dum);
    if (!getcwd(path, _MAX_PATH))
    {
      strcpy(path, "@:\\");
      path[0]+=drv;
    }
    _dos_setdrive(savedrive, &dum);
  }
  else
    strcpy(path, "@:\\");

  dmDriveDirs[drv]=dmGetPathReference(path);
  return dmDriveDirs[drv];
}

static unsigned short dmChangeDir(unsigned short dir)
{
  dir=dmConvertReference(dir);
  if (dir==0xFFFF)
    return 0xFFFF;

  char path[_MAX_PATH];
  dmGetPath(path, dir);
  char drive[_MAX_DRIVE];
  _splitpath(path, drive, 0, 0, 0);
  dmCurDrive=*drive-'@';
  dmDriveDirs[dmCurDrive]=dir;

  return dir;
}

unsigned short dmGetDriveDir(int drv)
{
  return dmDriveDirs[drv];
}

static unsigned short dmGetCurDir()
{
  return dmGetDriveDir(dmCurDrive);
}

static char dmFullPath(char *path)
{
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char name[_MAX_FNAME];
  char ext[_MAX_EXT];

  strupr(path);
  _splitpath(path, drive, dir, name, ext);

  if (!*drive||(*drive<'@')||(*drive>'Z'))
    *drive='@'+dmCurDrive;
  drive[1]=':';
  drive[2]=0;

  if (dir[0]!='\\')
  {
    char dir2[_MAX_DIR*2];

    unsigned short dr=dmConvertReference(dmDriveDirs[drive[0]-'@']);
    if (dr==0xFFFF)
      return 0;

    dmGetPath(path, dr);
    _splitpath(path, 0, dir2, 0, 0);
    strcat(dir2, dir);
    strcpy(dir, dir2);
  }
  _makepath(path, drive, dir, name, ext);
  reducepath(path);

  return 1;
}

struct mifentry
{
#define MIF_USED 1
#define MIF_DIRTY 2
  unsigned short flags;
  unsigned short size;
  char name[12];
};

static mifentry *mifData;
static unsigned long mifNum;
static char mifDirty;

static char mifInit()
{
  mifDirty=0;
  mifData=0;
  mifNum=0;

  char path[_MAX_PATH];
  strcpy(path, cfConfigDir);
  strcat(path, "CPMDZTAG.DAT");

  long f=open(path, O_RDONLY|O_BINARY);
  if (f<0)
    return 1;

  char sig[16];

  if (read(f, sig, 16)!=16)
  {
    close(f);
    return 1;
  }

  if (memcmp(sig, "MDZTagList\x1A\x00", 12))
  {
    close(f);
    return 1;
  }

  mifNum=*(unsigned long*)(sig+12);
  if (!mifNum)
  {
    close(f);
    return 1;
  }
  mifData=new mifentry[mifNum];
  if (!mifData)
    return 0;
  if (read(f, mifData, mifNum*sizeof(*mifData))!=(mifNum*sizeof(*mifData)))
  {
    delete mifData;
    mifNum=0;
    mifData=0;
    close(f);
    return 1;
  }
  close(f);
  return 1;
}

static void mifUpdate()
{
  if (!mifDirty||!fsWriteModInfo)
    return;
  mifDirty=0;

  char path[_MAX_PATH];
  strcpy(path, cfConfigDir);
  strcat(path, "CPMDZTAG.DAT");

  long f=open(path, O_WRONLY|O_BINARY|O_CREAT, S_IREAD|S_IWRITE);
  if (f<0)
    return;

  lseek(f, 0, SEEK_SET);
  write(f, "MDZTagList\x1A\x00", 12);
  write(f, &mifNum, 4);

  long i=0,j;

  while (i<mifNum)
  {
    if (!(mifData[i].flags&MIF_DIRTY))
    {
      i++;
      continue;
    }
    for (j=i; j<mifNum; j++)
      if (mifData[j].flags&MIF_DIRTY)
        mifData[j].flags&=~MIF_DIRTY;
      else
        break;
    lseek(f, 16+i*sizeof(*mifData), SEEK_SET);
    write(f, mifData+i, (j-i)*sizeof(*mifData));

    i=j;
  }
  lseek(f, 0, SEEK_END);
  close(f);
}

static void mifClose()
{
  mifUpdate();
  delete mifData;
}

static char mifTagged(const char *name, unsigned short size)
{
  long i;
  for (i=0; i<mifNum; i++)
    if ((mifData[i].flags&MIF_USED)&&(mifData[i].size==size))
      if (!memcmp(mifData[i].name, name, 12))
        return 1;
  return 0;
}

static char mifTag(const char *name, unsigned short size)
{
  long i;
  for (i=0; i<mifNum; i++)
    if (!(mifData[i].flags&MIF_USED))
      break;

  if (i==mifNum)
  {
    mifNum+=256;
    void *t=realloc(mifData, mifNum*sizeof(*mifData));
    if (!t)
      return 0;
    mifData=(mifentry*)t;
    memset(mifData+i, 0, (mifNum-i)*sizeof(*mifData));
    long j;
    for (j=i; j<mifNum; j++)
      mifData[j].flags|=MIF_DIRTY;
  }
  mifData[i].size=size;
  mifData[i].flags=MIF_USED|MIF_DIRTY;
  memcpy(mifData[i].name, name, 12);
  mifDirty=1;
  return 1;
}

char mifMemRead(const char *name, unsigned short size, char *ptr)
{
  if (!mifTag(name, size))
    return 0;

  char *endp=ptr+size;

  if ((ptr+7)>endp)
    return 1;
  if (memicmp(ptr, "MODINFO", 7))
    return 1;
  ptr+=7;
  short ver=0;
  while (ptr<endp)
  {
    if (!isdigit(*ptr))
      break;
    ver=ver*10+*ptr++-'0';
  }
  if (ver!=1)
    return 1;

  while (ptr<endp)
    if ((*ptr=='\r')||(*ptr=='\n'))
      break;
    else
      ptr++;
  while (ptr<endp)
    if ((*ptr=='\r')||(*ptr=='\n'))
      ptr++;
    else
      break;

  char close=0;
  char fname[12];
  unsigned long fsize;
  unsigned char flags=0;
  unsigned short mdb_ref=0xFFFF;
  while (1)
  {
    if (ptr==endp)
      close=1;

    if (close&&(mdb_ref!=0xFFFF))
    {
      if (!mdbWriteModuleInfo(mdb_ref, mdbEditBuf))
        return 0;
      mdb_ref=0xFFFF;
    }
    close=0;

    if (ptr==endp)
      return 1;

    if (flags==3)
    {
      mdb_ref=mdbGetModuleReference(fname, fsize);
      if (mdb_ref==0xFFFF)
        return 0;
      if (!mdbGetModuleInfo(mdbEditBuf, mdb_ref))
        return 0;
      flags=0;
    }

    char cmd[16];
    char arg[64];
    char *cmdp=cmd;
    char *argp=arg;

    while (ptr<endp)
      if ((*ptr==' ')||(*ptr=='\t'))
        ptr++;
      else
        break;

    while (ptr<endp)
      if (isspace(*ptr))
        break;
      else
        if ((cmd+15)>cmdp)
          *cmdp++=*ptr++;
        else
          ptr++;
    *cmdp=0;

    while (ptr<endp)
      if ((*ptr==' ')||(*ptr=='\t'))
        ptr++;
      else
        break;

    while (ptr<endp)
      if ((*ptr=='\r')||(*ptr=='\n'))
        break;
      else
        if ((arg+63)>argp)
          *argp++=*ptr++;
        else
          ptr++;
    while (argp>arg)
      if (isspace(argp[-1]))
        argp--;
      else
	break;
    *argp=0;

    while (ptr<endp)
      if ((*ptr=='\r')||(*ptr=='\n'))
        ptr++;
      else
        break;

    if (!stricmp(cmd, "MODULE"))
    {
      close=1;
      flags|=1;
      char fn[_MAX_FNAME];
      char ext[_MAX_EXT];
      strupr(arg);
      _splitpath(arg, 0, 0, fn, ext);
      fsConvFileName12(fname, fn, ext);
    }
    else
    if (!stricmp(cmd, "SIZE"))
    {
      close=1;
      flags|=2;
      fsize=strtoul(arg, 0, 10);
    }
    else
    if (!stricmp(cmd, "TYPE"))
      mdbEditBuf.modtype=mdbReadModType(arg);
    else
    if (!stricmp(cmd, "COMMENT"))
      strncpy(mdbEditBuf.comment, arg, 63);
    else
    if (!stricmp(cmd, "STYLE"))
      strncpy(mdbEditBuf.style, arg, 31);
    else
    if (!stricmp(cmd, "COMPOSER"))
      strncpy(mdbEditBuf.composer, arg, 32);
    else
    if (!stricmp(cmd, "TITLE"))
      strncpy(mdbEditBuf.modname, arg, 32);
    else
    if (!stricmp(cmd, "CHANNELS"))
      mdbEditBuf.channels=strtoul(arg, 0, 10);
    else
    if (!stricmp(cmd, "PLAYTIME"))
    {
      unsigned short min=0;
      argp=arg;
      while (isdigit(*argp))
        min=min*10+*argp++-'0';
      if ((argp[0]==':')&&isdigit(argp[1])&&isdigit(argp[2]))
        mdbEditBuf.playtime=min*60+(argp[1]-'0')*10+argp[2]-'0';
    }
    else
    if (!stricmp(cmd, "CDATE"))
    {
      argp=arg;
      if (isdigit(*argp))
      {
        unsigned char day=*argp++-'0';
        if (isdigit(*argp))
          day=day*10+*argp++-'0';
        if ((day<=31)&&(argp[0]=='.')&&isdigit(argp[1]))
        {
          argp++;
          unsigned char month=*argp++-'0';
          if (isdigit(*argp))
            month=month*10+*argp++-'0';
          if ((month<=12)&&(argp[0]=='.')&&isdigit(argp[1]))
          {
            argp++;
            unsigned short year=*argp++-'0';
            if (isdigit(*argp))
            {
              year=year*10+*argp++-'0';
              if (isdigit(*argp))
              {
                year=year*10+*argp++-'0';
                if (isdigit(*argp))
                  year=year*10+*argp++-'0';
              }
            }
            mdbEditBuf.date=(year<<16)|(month<<8)|day;
          }
        }
      }
    }
  }
}

static char mifRead(const char *name, unsigned short size, const char *path)
{
  short f=open(path, O_RDONLY|O_BINARY);
  if (f<0)
    return 1;
  char *buf=new char[size];
  if (!buf)
  {
    close(f);
    return 0;
  }
  read(f, buf, size);
  char stat=mifMemRead(name, size, buf);
  close(f);
  delete buf;
  return stat;
}

static void mifAppendInfo(short f, unsigned short mdb_ref)
{
  char buf[60];
  modinfoentry *m=&mdbData[mdb_ref];
  strcpy(buf, "MODULE ");
  fsConv12FileName(buf+strlen(buf), m->gen.name);
  strcat(buf, "\r\nSIZE ");
  ultoa(m->gen.size, buf+strlen(buf), 10);
  strcat(buf, "\r\n");
  write(f, buf, strlen(buf));
  if (m->gen.modtype!=0xFF)
  {
    strcpy(buf, "  TYPE ");
    strcat(buf, mdbGetModTypeString(m->gen.modtype));
    strcat(buf, "\r\n");
    write(f, buf, strlen(buf));
  }
  if (*m->gen.modname)
  {
    strcpy(buf, "  TITLE ");
    strcat(buf, m->gen.modname);
    strcat(buf, "\r\n");
    write(f, buf, strlen(buf));
  }
  if (m->gen.channels)
  {
    strcpy(buf, "  CHANNELS ");
    ultoa(m->gen.channels, buf+strlen(buf), 10);
    strcat(buf, "\r\n");
    write(f, buf, strlen(buf));
  }
  if (m->gen.playtime)
  {
    strcpy(buf, "  PLAYTIME ");
    ultoa(m->gen.playtime/60, buf+strlen(buf), 10);
    strcat(buf, ":00");
    buf[strlen(buf)-2]+=(m->gen.playtime%60)/10;
    buf[strlen(buf)-1]+=m->gen.playtime%10;
    strcat(buf, "\r\n");
    write(f, buf, strlen(buf));
  }
  if (m->gen.date)
  {
    strcpy(buf, "  CDATE ");
    ultoa(m->gen.date&0xFF, buf+strlen(buf), 10);
    strcat(buf, ".");
    ultoa((m->gen.date>>8)&0xFF, buf+strlen(buf), 10);
    strcat(buf, ".");
    ultoa(m->gen.date>>16, buf+strlen(buf), 10);
    strcat(buf, "\r\n");
    write(f, buf, strlen(buf));
  }
  if (m->gen.compref!=0xFFFF)
  {
    modinfoentry *m2=&mdbData[m->gen.compref];
    if (*m2->comp.composer)
    {
      strcpy(buf, "  COMPOSER ");
      strcat(buf, m2->comp.composer);
      strcat(buf, "\r\n");
      write(f, buf, strlen(buf));
    }
    if (*m2->comp.style)
    {
      strcpy(buf, "  STYLE ");
      strcat(buf, m2->comp.style);
      strcat(buf, "\r\n");
      write(f, buf, strlen(buf));
    }
  }
  if ((m->gen.comref!=0xFFFF)&&*mdbData[m->gen.comref].comment)
  {
    strcpy(buf, "  COMMENT ");
    strcat(buf, mdbData[m->gen.comref].comment);
    strcat(buf, "\r\n");
    write(f, buf, strlen(buf));
  }
}



struct modlist
{
  modlistentry *files;
  signed long num;
  signed long max;
  signed long pos;

  long fuzfirst;
  unsigned short fuzval;
  char fuzmask[12];

  modlist() { files=0; num=max=pos=0; }
  ~modlist() { delete files; }

  char insert(unsigned long before, const modlistentry *f, unsigned long n);
  char append(const modlistentry &f);
  void remove(unsigned long from, unsigned long n);
  void get(modlistentry *f, unsigned long from, unsigned long n) const;
  void getcur(modlistentry &f) const;
/* char copy(modlist &dest, unsigned long to, unsigned long from, unsigned long n); */

  void sort();

  long find(const modlistentry &f);
  long fuzzyfind(const char *c);
  long fuzzyfindnext();
};

int fsReadDir(modlist &ml, unsigned short dirref, const char *mask, unsigned long opt);

char modlist::insert(unsigned long before, const modlistentry *f, unsigned long n)
{
  if (before>num)
    before=num;

  if ((num+n)>10000)
    return 1;

  if (pos>=before)
    pos+=n;

  if ((num+n)>max)
  {
    max=(num+n+255)&~255;
    void *t=realloc(files, sizeof(*files)*max);
    if (!t)
      return 0;
    files=(modlistentry *)t;
  }

  mymemmovep(files+before+n, files+before, num-before);
  mymemmovep(files+before, f, n);
  num+=n;
  return 1;
}

char modlist::append(const modlistentry &f)
{
  return insert(num, &f, 1);
}

long modlist::find(const modlistentry &f)
{
  long i;
  for (i=0; i<num; i++)
    if (!memcmp(&files[i], &f, sizeof(*files)))
      break;
  return (i==num)?-1:i;
}

long modlist::fuzzyfind(const char *c)
{
  memcpy(fuzmask, c, 12);
  long i;
  fuzval=0;
  fuzfirst=0;
  for (i=0; i<num; i++)
  {
    char *cn=files[i].name;
    unsigned short cur=0;
    short j;
    for (j=0; j<8; j++)
      if (fuzmask[j]==cn[j])
        cur+=(fuzmask[j]==' ')?1:20;
    if (fuzmask[8]=='.')
      for (j=8; j<12; j++)
        if (fuzmask[j]==cn[j])
          cur+=(fuzmask[j]==' ')?1:20;
    if (cur>fuzval)
    {
      fuzval=cur;
      fuzfirst=i;
    }
  }
  return fuzfirst;
}

long modlist::fuzzyfindnext()
{
  long i;
  for (i=fuzfirst+1; i<num; i++)
  {
    char *cn=files[i].name;
    unsigned short cur=0;
    short j;
    for (j=0; j<8; j++)
      if (fuzmask[j]==cn[j])
        cur+=(fuzmask[j]==' ')?1:20;
    if (fuzmask[8]=='.')
      for (j=8; j<12; j++)
        if (fuzmask[j]==cn[j])
          cur+=(fuzmask[j]==' ')?1:20;
    if (cur==fuzval)
    {
      fuzfirst=i;
      return fuzfirst;
    }
  }
  fuzfirst=-1;
  return fuzzyfindnext();
}

static int dosReadDir(modlist &ml, unsigned short dirref, const char *mask, unsigned long opt)
{
  char path[_MAX_PATH];

  modlistentry m;

  if (opt&RD_PUTDSUBS)
  {
    unsigned int savedrive;
    _dos_getdrive(&savedrive);
#ifdef WIN32
    unsigned int disknum=26;
#else
    unsigned int disknum;
    _dos_setdrive(savedrive, &disknum);
#endif
    int i;
    unsigned int dummy;
/*    for (i=2; i<disknum; i++) */
    for (i=0; i<disknum; i++)
    {
      _dos_setdrive(i+1, &dummy);
      _dos_getdrive(&dummy);
      if (i!=(dummy-1))
        continue;

      fsConvFileName12(m.name, "A:", "");
      *m.name+=i;
      m.mdb_ref=0xFFFF;
      m.dirref=dmGetDriveDir(i+1);
      if (!mdbAppendNew(ml,m))
        return 0;
    }
    _dos_setdrive(savedrive, &dummy);
  }

  dmGetPath(path, dirref);
	if (!isarchivepath(path))
	{
		dmGetPath(path, dirref);
		if (opt&RD_PUTSUBS)
			strcat(path, "*.*");
		else
		{
			char curfile[_MAX_NAME];
			fsConv12FileName(curfile, mask);
			strcat(path, curfile);
		}
		find_t fi;

		char done=(char)_dos_findfirst(path, _A_SUBDIR
#ifdef DOS32
				|_A_RDONLY
#endif
				, &fi);
#ifdef WIN32
		char thispath[_MAX_PATH];
		strcpy(thispath, path);
#endif
		int dirsrc=!0, first=!0;    /* sorry for this, but i have no time invent some funny for/while/...-loops now. so i used this flags. */
		while(1)
		{
			if(!first) done=(char)_dos_findnext(&fi);
			first=0;

			if(done&&dirsrc)
			{
				/*        _dos_findclose(&fi); */
#ifdef WIN32
				done=_dos_findfirst(thispath, _A_NORMAL, &fi);
#endif
				dirsrc=0;
			}
			if(done) break;

			if (!strcmp(fi.name, ".")||!strcmp(fi.name, ".."))
				continue;

			char curname[_MAX_FNAME];
			char curext[_MAX_EXT];
			_splitpath(fi.name, 0, 0, curname, curext);
			fsConvFileName12(m.name, curname, curext);
			char isdir=!!(fi.attrib&_A_SUBDIR);
			if (isdir||isarchive(curext))
			{
				dmGetPath(path, dirref);
				strcat(path, fi.name);
				m.dirref=dmGetPathReference(path);
				if (m.dirref==0xFFFF)
					return 0;
				if (isdir)
				{
					if (opt&RD_PUTSUBS)
					{
						m.mdb_ref=0xFFFE;
						if (!ml.append(m))
							return 0;
					}
				}
				else
				{
					if ((opt&RD_PUTSUBS)&&(fsPutArcs||!(opt&RD_ARCSCAN)))
					{
						m.mdb_ref=0xFFFC;
						if (!ml.append(m))
							return 0;
					}
					if (opt&RD_ARCSCAN)
					{
						if (!fsReadDir(ml, m.dirref, mask, opt&~RD_PUTDSUBS))
							return 0;
					}
				}
				continue;
			}

			if ((fi.size<65536)&&!strcmp(curext, MIF_EXT)&&!mifTagged(m.name, fi.size)&&fsScanMIF)
			{
				dmGetPath(path, dirref);
				strcat(path, fi.name);
				if (!mifRead(m.name, fi.size, path))
					return 0;
			}

			if (!fsMatchFileName12(m.name, mask)||!fsIsModule(curext))
				continue;

			m.dirref=dirref;
			m.mdb_ref=mdbGetModuleReference(m.name, fi.size);
			if (m.mdb_ref==0xFFFF)
				return 0;
			if (!ml.append(m))
				return 0;
		}
	}

	return 1;
}



int fsReadDir(modlist &ml, unsigned short dirref, const char *mask, unsigned long opt)
{
  mdbreaddirregstruct *readdirs;
  for (readdirs=mdbReadDirs; readdirs; readdirs=readdirs->next)
    if (!readdirs->ReadDir(ml, dirref, mask, opt))
      return 0;
  return 1;
}


static modlistentry nextplay;
static unsigned char isnextplay;
static modlist playlist;
static modlist viewlist;
static char curmask[12];
static char curdirpath[_MAX_PATH];
static short dirwinheight;
static char quickfind[12];
static char quickfindpos;
static unsigned long scanpos;
static short editpos=0;
static short editmode=0;

static char fsExpandPath(char *dp, char *mask, const char *p)
{
  char path[_MAX_PATH];
  strcpy(path, p);
  if (!dmFullPath(path))
    return 0;
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char name[_MAX_FNAME];
  char ext[_MAX_EXT];
  _splitpath(path, drive, dir, name, ext);
  if (!(strchr(name, '*')||strchr(name, '?')||strchr(ext, '*')||strchr(ext, '?')))
  {
    find_t fi;
    if (!_dos_findfirst(path, _A_RDONLY|_A_SUBDIR, &fi)&&((fi.attrib&_A_SUBDIR)||isarchive(ext)))
    {
      _makepath(dp, drive, dir, name, ext);
      memcpy(mask, curmask, 12);
      return 1;
    }
    if (!*ext)
    {
      _makepath(path, drive, dir, name, ".*");
      if (!_dos_findfirst(path, _A_RDONLY, &fi)&&_dos_findnext(&fi))
      {
        _splitpath(fi.name, 0, 0, 0, ext);
        if (isarchive(ext))
        {
          _makepath(dp, drive, dir, name, ext);
          memcpy(mask, curmask, 12);
          return 1;
        }
      }
    }
  }

  char cmask[_MAX_NAME];
  conv12filenamewc(cmask, curmask);
  _makepath(dp, drive, dir, 0, 0);
  if (!*name)
    _splitpath(cmask, 0, 0, name, 0);
  if (!*ext)
    _splitpath(cmask, 0, 0, 0, ext);
  convfilename12wc(mask, name, ext);
  return 1;
}

static char fsScanDir(int pos)
{
  int op=0;
  switch (pos)
  {
  case 0: op=0; break;
  case 1: op=viewlist.pos; break;
  case 2: op=viewlist.pos?(viewlist.pos-1):0; break;
  }
  viewlist.remove(0, viewlist.num);

  if (!fsReadDir(viewlist, dmGetCurDir(), curmask, RD_PUTDSUBS|RD_PUTSUBS|(fsScanArcs?RD_ARCSCAN:0)))
    return 0;
  viewlist.sort();
  viewlist.pos=(op>=viewlist.num)?(viewlist.num-1):op;
  quickfindpos=0;
  scanpos=fsScanNames?0:0xFFFFFFFF;

  dmGetPath(curdirpath, dmGetCurDir());
  conv12filenamewc(curdirpath+strlen(curdirpath), curmask);

  adbUpdate();

  return 1;
}

static void fsSaveModInfo(const modlistentry &m)
{
  char path[_MAX_PATH];
  dmGetPath(path, m.dirref);
  if (isarchivepath(path))
    dmGetPath(path, dmGetParent(m.dirref));
  char n[_MAX_NAME];
  char fn[_MAX_FNAME];
  fsConv12FileName(n, m.name);
  _splitpath(n, 0, 0, fn, 0);
  _makepath(n, 0, 0, fn, MIF_EXT);
  strcat(path, n);
  short f=open(path, O_WRONLY|O_BINARY|O_TRUNC|O_CREAT, S_IREAD|S_IWRITE);
  if (f<0)
    return;
  write(f, "MODINFO1\r\n\r\n", 12);
  mifAppendInfo(f, m.mdb_ref);
  close(f);
}

static void fsSaveModInfoML(const modlist &ml)
{
  char path[_MAX_PATH];
  dmGetPath(path, dmGetCurDir());
  if (!fsEditPath(path))
    return;
  char dr[_MAX_DRIVE];
  char di[_MAX_DIR];
  char fn[_MAX_FNAME];
  char ext[_MAX_EXT];
  _splitpath(path, dr, di, fn, ext);
  if (!*ext)
    strcpy(ext, MIF_EXT);
  _makepath(path, dr, di, fn, ext);
  short f=open(path, O_WRONLY|O_BINARY|O_TRUNC|O_CREAT, S_IREAD|S_IWRITE);
  if (f<0)
    return;
  write(f, "MODINFO1\r\n\r\n", 12);
  long i;
  modlistentry m;
  for (i=0; i<ml.num; i++)
  {
    ml.get(&m, i, 1);
    if (m.mdb_ref<0xFFFC)
    {
      mifAppendInfo(f, m.mdb_ref);
      write(f, "\r\n", 2);
    }
  }
  close(f);
}

#ifndef WIN32
static int movefile(const char *dest, const char *src)
{
  if (!rename(src, dest))
    return 1;
  sbinfile fi,fo;
  if (fi.open(src, sbinfile::openro)||fo.open(dest, sbinfile::opencn))
    return 0;
  int len=fi.length();
  int max=len;
  char *buf=0;
  while (!buf)
  {
    buf=new char [max];
    if (!buf)
      max>>=1;
    if (max<65536)
      break;
  }
  if (!buf)
    return 0;
  while (len)
  {
    int l=(len>max)?max:len;
    if (!fi.eread(buf, l))
      return 0;
    if (!fo.ewrite(buf, l))
      return 0;
    len-=l;
  }
  delete buf;
  fo.close();
  fi.close();
  unlink(src);

  return 1;
}

static int movetoarc(const char *dest, const char *src)
{
  char path[_MAX_PATH];
  char path2[_MAX_PATH];
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char name[_MAX_FNAME];
  char ext[_MAX_EXT];
  _splitpath(dest, drive, dir, name, ext);
  _makepath(path, drive, dir, 0, 0);
  _splitpath(src, drive, dir, 0, 0);
  _makepath(path2, drive, dir, name, ext);
  path[strlen(path)-1]=0;
  _splitpath(path, 0, 0, 0, ext);

  adbregstruct *packers;
  for (packers=adbPackers; packers; packers=packers->next)
    if (!stricmp(ext, packers->ext))
    {
      if (stricmp(src, path2))
        if (rename(src, path2))
          return 0;
      conRestore();
      int r=packers->Call(adbCallMoveTo, path, path2, "");
      conSave();
      plSetTextMode(fsScrType);
      return r;
    }

  return 0;
}

static int movefromarc(const char *dest, const char *src)
{
  char path[_MAX_PATH];
  char path2[_MAX_PATH];
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char fname[_MAX_FNAME];
  char name[_MAX_NAME];
  char ext[_MAX_EXT];
  _splitpath(src, drive, dir, fname, ext);
  _makepath(name, 0, 0, fname, ext);
  _makepath(path, drive, dir, 0, 0);
  _splitpath(dest, drive, dir, 0, 0);
  _makepath(path2, drive, dir, 0, 0);
  path[strlen(path)-1]=0;
  _splitpath(path, 0, 0, 0, ext);

  adbregstruct *packers;
  for (packers=adbPackers; packers; packers=packers->next)
    if (!stricmp(ext, packers->ext))
    {
      conRestore();
      int r=packers->Call(adbCallMoveFrom, path, name, path2);
      conSave();
      plSetTextMode(fsScrType);
      if (!r)
        return 0;
      strcat(path2, name);
      if (stricmp(path2, dest))
        if (rename(path2, dest))
          return 0;
      return 1;
    }

  return 0;
}


static int movecrossarc(const char *dest, const char *src)
{
  char spath[_MAX_PATH];
  char dpath[_MAX_PATH];
  char path[_MAX_PATH];
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char fname[_MAX_FNAME];
  char sname[_MAX_NAME];
  char dname[_MAX_NAME];
  char ext[_MAX_EXT];
  char sext[_MAX_EXT];
  char dext[_MAX_EXT];
  _splitpath(src, drive, dir, fname, ext);
  _makepath(sname, 0, 0, fname, ext);
  _makepath(spath, drive, dir, 0, 0);
  _splitpath(dest, drive, dir, fname, ext);
  _makepath(dname, 0, 0, fname, ext);
  _makepath(dpath, drive, dir, 0, 0);
  spath[strlen(spath)-1]=0;
  dpath[strlen(dpath)-1]=0;
  _splitpath(spath, 0, 0, 0, sext);
  _splitpath(dpath, 0, 0, 0, dext);

  adbregstruct *spackers;
  for (spackers=adbPackers; spackers; spackers=spackers->next)
    if (!stricmp(sext, spackers->ext))
      break;
  adbregstruct *dpackers;
  for (dpackers=adbPackers; dpackers; dpackers=dpackers->next)
    if (!stricmp(dext, dpackers->ext))
      break;
  if (!spackers||!dpackers)
    return 0;

  conRestore();
  if (!spackers->Call(adbCallMoveFrom, spath, sname, cfTempDir))
  {
    conSave();
    plSetTextMode(fsScrType);
    return 0;
  }
  strcpy(spath, cfTempDir);
  strcat(spath, sname);
  strcpy(path, cfTempDir);
  strcat(path, dname);
  if (stricmp(spath, path))
    rename(spath, path);
  int r=dpackers->Call(adbCallMoveTo, dpath, path, "");
  conSave();
  plSetTextMode(fsScrType);
  return r;
}

static int fsQueryMove(modlistentry &m)
{
  char path[_MAX_PATH];
  char path2[_MAX_PATH];
  int srctype=0;
  dmGetPath(path, m.dirref);
  if (isarchivepath(path))
    srctype=1;
  if (m.mdb_ref==0xFFFC)
    srctype=2;

  char name[_MAX_NAME];
  fsConv12FileName(name, m.name);

  if (*fsDefMovePath)
    strcpy(path, fsDefMovePath);
  else
    dmGetPath(path, (m.mdb_ref==0xFFFC)?dmGetParent(m.dirref):m.dirref);
  if (!fsEditPath(path))
    return 0;
  dmFullPath(path);
  if (!*path)
    return 0;

  char ext[_MAX_EXT];
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  find_t ft;
  if (path[strlen(path)-1]!='\\')
  {
    _splitpath(path, 0, 0, 0, ext);
    if (isarchive(ext)&&(srctype!=2))
      strcat(path, "\\");
    else
#ifdef DOS32
      if (!_dos_findfirst(path, _A_NORMAL|_A_SUBDIR, &ft))
        if (ft.attrib&_A_SUBDIR)
          strcat(path, "\\");
#else
      if (!_dos_findfirst(path, _A_SUBDIR, &ft))
        if (ft.attrib&_A_SUBDIR)
          strcat(path, "\\");
#endif
  }
  if (path[strlen(path)-1]=='\\')
    strcat(path, name);

  _splitpath(path, drive, dir, 0, 0);
  _makepath(path2, drive, dir, 0, 0);
  path2[strlen(path2)-1]=0;
  _splitpath(path2, 0, 0, 0, ext);
  int desttype=0;
  if (isarchive(ext))
#ifdef DOS32
    if (_dos_findfirst(path2, _A_NORMAL|_A_SUBDIR, &ft))
#else
    if (_dos_findfirst(path2, _A_NORMAL, &ft))
#endif
      desttype=1;
    else
      if (!(ft.attrib&_A_SUBDIR))
        desttype=1;
  if ((desttype==1)&&(srctype==2))
    return 0;

  dmGetPath(path2, (m.mdb_ref==0xFFFC)?dmGetParent(m.dirref):m.dirref);
  strcat(path2, name);
  if ((desttype==0)&&(srctype!=1))
    return movefile(path, path2);
  if ((desttype==1)&&(srctype==0))
    return movetoarc(path, path2);
  if ((desttype==0)&&(srctype==1))
    return movefromarc(path, path2);
  if ((desttype==1)&&(srctype==1))
    return movecrossarc(path, path2);

  return 0;
}
#endif

static unsigned char fsQueryKill(modlistentry &m)
{
  displaystr(1, 0, 0xF0, "are you sure you want to delete this file?", 80);
  while (!ekbhit());
  if (toupper(egetch()&0xFF)!='Y')
    return 0;

  char path[_MAX_PATH];
  char name[_MAX_NAME];
  fsConv12FileName(name, m.name);
  dmGetPath(path, m.dirref);

  if ((m.mdb_ref!=0xFFFC)&&isarchivepath(path))
  {
    char ext[_MAX_EXT];
    path[strlen(path)-1]=0;
    _splitpath(path, 0, 0, 0, ext);

    adbregstruct *packers;
    for (packers=adbPackers; packers; packers=packers->next)
      if (!strcmp(ext, packers->ext))
      {
        conRestore();
        packers->Call(adbCallDelete, path, name, "");
        conSave();
        plSetTextMode(fsScrType);
      }

    return 1;
  }
  else
  {
    if (m.mdb_ref!=0xFFFC)
      strcat(path, name);
    else
      path[strlen(path)-1]=0;
    unlink(path);
    return 1;
  }
/* remove from lists... */
}

signed char fsFileSelect()
{
  plSetTextMode(fsScrType);

  isnextplay=0;

  short win=0;

  quickfindpos=0;
  long i;
  char curscanned=0;

  modlistentry m;
  while (1)
  {
    dirwinheight=plScrHeight-4;
    if (fsEditWin||editmode)
      dirwinheight-=(plScrWidth==132)?5:6;

    if (!playlist.num)
      win=0;

    if (playlist.pos>=playlist.num)
      playlist.pos=playlist.num-1;
    if (playlist.pos<0)
      playlist.pos=0;

    if (viewlist.pos>=viewlist.num)
      viewlist.pos=viewlist.num-1;
    if (viewlist.pos<0)
      viewlist.pos=0;

    short firstv=viewlist.pos-dirwinheight/2;

    if ((firstv+dirwinheight)>viewlist.num)
      firstv=viewlist.num-dirwinheight;
    if (firstv<0)
      firstv=0;

    short firstp=playlist.pos-dirwinheight/2;

    if ((firstp+dirwinheight)>playlist.num)
      firstp=playlist.num-dirwinheight;
    if (firstp<0)
      firstp=0;

    if (!win)
      viewlist.getcur(m);
    else
      playlist.getcur(m);

    fsShowDir(firstv, win?-1:viewlist.pos, firstp, win?playlist.pos:-1, editmode?editpos:-1, m);

    if (!ekbhit()&&fsScanNames)
    {
      if (curscanned||(m.mdb_ref>=0xFFFC)||mdbInfoRead(m.mdb_ref))
      {
        while (scanpos<viewlist.num)
        {
          viewlist.get(&m, scanpos++, 1);
          if ((m.mdb_ref<0xFFFC)&&!mdbInfoRead(m.mdb_ref))
          {
            mdbScan(m);
            break;
          }
        }
      }
      else
      {
        curscanned=1;
        mdbScan(m);
      }
     continue;
    }
    unsigned short c=egetch();
    curscanned=0;
    if ((c&0xFF)==0xE0)
      c&=0xFF00;
    if (c&0xFF)
      c&=0x00FF;
    DEBUGINT(c);

#ifdef DOS32
    if(c==0xF8) /* '°' : screen shot */
      {
	TextScreenshot(fsScrType);
	continue;
      }
#endif

    if ((c>32)&&(c<=255)&&(c!=0x7f)||(c==8))
    {
      if (c==8)
      {
        if (quickfindpos)
          quickfindpos--;
        if ((quickfindpos==8)&&(quickfind[8]=='.'))
          while (quickfindpos&&(quickfind[quickfindpos-1]==' '))
            quickfindpos--;
      }
      else
        if (quickfindpos<12)
          if ((c=='.')&&(quickfindpos&&(*quickfind!='.')))
          {
            while (quickfindpos<9)
              quickfind[quickfindpos++]=' ';
            quickfind[8]='.';
          }
          else
            if (quickfindpos!=8)
              quickfind[quickfindpos++]=toupper(c);
      memcpy(quickfind+quickfindpos, "        .   "+quickfindpos, 12-quickfindpos);
      if (!quickfindpos)
        continue;
      if (!win)
        viewlist.pos=viewlist.fuzzyfind(quickfind);
      else
        playlist.pos=playlist.fuzzyfind(quickfind);
      continue;
    }

    quickfindpos=0;

    modlist &curlist=(win?playlist:viewlist);
    curlist.getcur(m);

    switch (c)
    {
    case 27:
      return 0;
    case 0x7f:  /*c-bs*/
    case 0x1f00: /* alt-s */
      scanpos=0xFFFFFFFF;
      break;
    case 9:
      win=!win;
      break;
    case 0xF00:
    case 0x1200:
      editmode=!editmode;
      break;
    case 0x1700:
    case 0xa500:
      fsInfoMode=(fsInfoMode+1)&3;
      break;
    case 0x2E00:
      fsSetup();
      plSetTextMode(fsScrType);
      break;
    case 0x3b00:
      if (!fsHelp2())
        return -1;
      plSetTextMode(fsScrType);
      break;
    case 0x2C00:
      fsScrType=(fsScrType==0)?7:0;
      plSetTextMode(fsScrType);
      break;
    case 10:
      if (!fsEditViewPath())
        return -1;
      break;
    case 13:
      if (editmode)
        if (m.mdb_ref<0xFFFC)
        {
          if (!fsEditFileInfo(m.mdb_ref))
            return -1;
          break;
        }
      if (win)
      {
        nextplay=m;
        isnextplay=1;
        return 1;
      }
      else
      {
        if (m.mdb_ref<0xFFFC)
        {
          nextplay=m;
          isnextplay=1;
          return 1;
        }
        else
        {
          unsigned short parentdir=0xFFFF;
          if (!memcmp(m.name, "..", 2))
            parentdir=dmGetCurDir();
          if (dmChangeDir(m.dirref)==0xFFFF)
            return -1;
          if (!fsScanDir(0))
            return -1;
          if (parentdir!=0xFFFF)
          {
            for (i=viewlist.num-1; i>=0; i--)
            {
              viewlist.get(&m, i, 1);
              if ((m.mdb_ref<0xFFFC)||(m.dirref!=parentdir))
                continue;
              if (memcmp(m.name, "..", 2)&&(m.name[1]!=':'))
              {
                viewlist.pos=i;
                break;
              }
            }
          }
        }
      }
      break;
    case 0x4800: /* up */
      if (editmode)
        if (plScrWidth==132)
          editpos="\x00\x01\x02\x03\x00\x01\x04\x05"[editpos];
        else
          editpos="\x00\x01\x06\x06\x00\x04\x00\x05"[editpos];
      else
        curlist.pos--;
      break;
    case 0x5000: /* down */
      if (editmode)
        if (plScrWidth==132)
          editpos="\x04\x05\x05\x05\x06\x07\x06\x07"[editpos];
        else
          editpos="\x04\x06\x07\x07\x05\x07\x03\x07"[editpos];
      else
        curlist.pos++;
      break;
    case 0x4900: /* pgup */
      curlist.pos-=editmode?1:dirwinheight;
      break;
    case 0x5100: /* pgdn */
      curlist.pos+=editmode?1:dirwinheight;
      break;
    case 0x4700: /* home */
      if (editmode)
        break;
      curlist.pos=0;
      break;
    case 0x4F00: /* end */
      if (editmode)
        break;
      curlist.pos=curlist.num-1;
      break;

    case 0x4D00: /* right */
      if (editmode)
      {
        if (plScrWidth==132)
          editpos="\x01\x02\x03\x03\x05\x05\x07\x07"[editpos];
        else
          editpos="\x01\x01\x02\x02\x06\x03\x06\x07"[editpos];
      }
    case 0x5200: /* add */
      if (editmode)
        break;
      if (win)
      {
        if (!playlist.append(m))
          return -1;
/*        playlist.pos=playlist.num-1; */
      }
      else
      {
        if (m.mdb_ref==0xFFFC)
        {
          if (!fsReadDir(playlist, m.dirref, curmask, 0))
            return -1;
        }
        else
          if (m.mdb_ref<0xFFFC)
            if (!playlist.append(m))
              return -1;
      }
      break;

    case 0x4B00: /* left */
      if (editmode)
      {
        if (plScrWidth==132)
          editpos="\x00\x00\x01\x02\x04\x04\x06\x06"[editpos];
        else
          editpos="\x00\x00\x03\x05\x04\x05\x04\x07"[editpos];
      }
    case 0x5300: /* del */
      if (editmode)
        break;
      if (win)
        playlist.remove(playlist.pos, 1);
      else
      {
        long f;
        if (m.mdb_ref<0xFFFC)
        {
          f=playlist.find(m);
          if (f!=-1)
            playlist.remove(f, 1);
        }
        else
        if (m.mdb_ref==0xFFFC)
        {
          modlist tl;
          if (!fsReadDir(tl, m.dirref, curmask, 0))
            return -1;
          for (i=0; i<tl.num; i++)
          {
            tl.get(&m, i, 1);
            f=playlist.find(m);
            if (f!=-1)
              playlist.remove(f, 1);
          }
        }
      }
      break;

    case 0x9200:
    case 0x7400:
      if (editmode)
        break;
      for (i=0; i<viewlist.num; i++)
      {
        viewlist.get(&m, i, 1);
        if (m.mdb_ref<0xFFFC)
          if (!playlist.append(m))
            return -1;
      }
      break;

    case 0x9300:
    case 0x7300:
      if (editmode)
        break;
      playlist.remove(0, playlist.num);
      break;

    case 0x2500:  /* alt-k */
      if (editmode||win)
        break;
      if (m.mdb_ref<=0xFFFC)
        if (fsQueryKill(m))
          if (!fsScanDir(2))
            return -1;
      break;


#ifndef WIN32
    case 0x3200:  /* alt-m !!!!!!!! STRANGE THINGS HAPPENS IF YOU ENABLE HIS UNDER W32!! */
      if (editmode||win)
        break;
      if (m.mdb_ref<=0xFFFC)
        if (fsQueryMove(m))
          if (!fsScanDir(1))
            return -1;
      break;
#endif

    case 0x3000: /* alt-b */
      if (m.mdb_ref<=0xFFFC)
      {
        mdbGetModuleInfo(mdbEditBuf, m.mdb_ref);
        mdbEditBuf.flags1^=MDB_BIGMODULE;
        mdbWriteModuleInfo(m.mdb_ref, mdbEditBuf);
      }
      break;

    case 0x1900: /* alt-p */
      if (editmode)
        break;
      fsSavePlayList(playlist);
      break;

    case 0x1100: /* alt-w */
      if (m.mdb_ref<0xFFFC)
        fsSaveModInfo(m);
      break;
    case 0x1e00: /* alt-a */
      if (editmode)
        break;
      fsSaveModInfoML(curlist);
      break;

    case 0x8d00: /* ctrl-up */
      if (editmode||!win)
        break;
      if (!playlist.pos)
        break;
      playlist.remove(playlist.pos, 1);
      playlist.insert(playlist.pos-1, &m, 1);
      playlist.pos-=2;
      break;
    case 0x9100: /* ctrl-down */
      if (editmode||!win)
        break;
      if ((playlist.pos+1)>=playlist.num)
        break;
      playlist.remove(playlist.pos, 1);
      playlist.insert(playlist.pos+1, &m, 1);
      playlist.pos++;
      break;
    case 0x8400: /* ctrl-pgup */
      if (editmode||!win)
        break;
      i=(playlist.pos>dirwinheight)?dirwinheight:playlist.pos;
      playlist.remove(playlist.pos, 1);
      playlist.insert(playlist.pos-i, &m, 1);
      playlist.pos-=i+1;
      break;
    case 0x7600: /* ctrl-pgdown */
      if (editmode||!win)
        break;
      i=((playlist.num-1-playlist.pos)>dirwinheight)?dirwinheight:(playlist.num-1-playlist.pos);
      playlist.remove(playlist.pos, 1);
      playlist.insert(playlist.pos+i, &m, 1);
      playlist.pos+=i;
      break;
    case 0x7700: /* ctrl-home */
      if (editmode||!win)
        break;
      playlist.remove(playlist.pos, 1);
      playlist.insert(0, &m, 1);
      playlist.pos=0;
      break;
    case 0x7500: /* ctrl-end */
      if (editmode||!win)
        break;
      playlist.remove(playlist.pos, 1);
      playlist.insert(playlist.num, &m, 1);
      playlist.pos=playlist.num-1;
      break;
    }
  }
  /*return 0;  the above while loop doesn't go to this point */
}

/* use the god damn playlist instead
char fsAddFiles(const char *p)
{
  while (*p)
  {
    while (isspace(*p))
      p++;

    int i;
    char path[_MAX_PATH];

    for (i=0; (i<(_MAX_PATH-1))&&!isspace(*p)&&*p; i++)
      path[i]=*p++;
    path[i]=0;
    if ((*path=='-')||(*path=='/')||!*path)
      continue;
    if (*path=='@')
    {
      unsigned short olddir=dmGetCurDir();

      char nam[_MAX_FNAME];
      char ext[_MAX_EXT];
      char dir[_MAX_DIR];
      char drv[_MAX_DRIVE];

      _splitpath(path+1, drv, dir, nam, ext);
      _makepath(path, drv, dir, 0, 0);
      unsigned short dref=dmGetPathReference(path);
      if (dref==0xFFFF)
        return 0;
      dmGetPath(path, dref);
      dmChangeDir(dref);
      if (!*ext)
        strcpy(ext, ".M3U");
      _makepath(path+strlen(path), 0, 0, nam, ext);

      sbinfile lf;
      if (!lf.open(path, sbinfile::openro))
      {
        int len=lf.length();
        char *fbuf=new char [len+1];
        if (!fbuf)
          return 0;
        lf.read(fbuf, len);
        fbuf[len]=0;
        lf.close();
        len=fsAddFiles(fbuf);
        if (!len)
          return 0;
        delete fbuf;
      }
      dmChangeDir(olddir);
    }
    else
    {
      char path2[_MAX_PATH];
      unsigned short dref;
      char nmask[12];
      if (!fsExpandPath(path2, nmask, (*path=='@')?(path+1):path))
        return 0;
      dref=dmGetPathReference(path2);
      if (dref==0xFFFF)
        return 0;

      if (!fsReadDir(playlist, dref, nmask, (fsScanArcs?RD_ARCSCAN:0)))
        return 0;
    }
  }
  return 1;
}
*/

#endif

struct mdbreaddirregstruct fsReadDirReg = {stdReadDir MDBREADDIRREGSTRUCT_TAIL};
struct mdbreaddirregstruct dosReadDirReg = {dosReadDir MDBREADDIRREGSTRUCT_TAIL};

void fsConvFileName12(char *c, const char *f, const char *e)
/*  f=up to 8 chars, might end premature with a null
 *  e=up to 4 chars, starting with a ., migh premature with a null
 *  char c[12], will not be null terminated premature, but not after the 12, and no \0 exists then
 * f="hei" e=".gz"    -> {HEI     .GZ\0}
 * f="hello" e=".txt" -> {HELLO   .TXT}
 */
{
	int i;
	for (i=0; i<8; i++)
		*c++=*f?*f++:' ';
	for (i=0; i<4; i++)
		*c++=*e?*e++:' ';
	for (i=0; i<12; i++)
		c[i-12]=toupper(c[i-12]);
}

void convfilename12wc(char *c, const char *f, const char *e)
/* same as above, but * is expanded to ?
 * f="hei" e=".*" -> {HEI     .???}
 * f="*" e=".*"   -> {????????.???}
 */
{
	int i;
	for (i=0; i<8; i++)
		*c++=(*f=='*')?'?':*f?*f++:' ';
	for (i=0; i<4; i++)
		*c++=(*e=='*')?'?':*e?*e++:' ';
	for (i=0; i<12; i++)
		c[i-12]=toupper(c[i-12]);
}

/* broken due to the fact that we allow space
void fsConv12FileName(char *f, const char *c)
{
	int i;
	for (i=0; i<8; i++)
		if (c[i]==' ')
			break;
		else
			*f++=c[i];
	for (i=8; i<12; i++)
		if (c[i]==' ')
			break;
		else
			*f++=c[i];
	*f=0;
}
*/

/* broken due to the fact that we allow space, question-mask etc
static void conv12filenamewc(char *f, const char *c)
{
	char *f0=f;
	short i;
	for (i=0; i<8; i++)
		if (c[i]==' ')
			break;
		else
			*f++=c[i];
	if (i==8)
	{
		for (i=7; i>=0; i--)
			if (c[i]!='?')
				break;
		if (++i<7)
		{
			f-=8-i;
			*f++='*';
		}
	}
	for (i=8; i<12; i++)
		if (c[i]==' ')
			break;
		else
			*f++=c[i];
	if (i==12)
	{
		for (i=11; i>=9; i--)
			if (c[i]!='?')
				break;
		if (++i<10)
		{
			f-=12-i;
			*f++='*';
		}
	}
	*f=0;
}*/

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
