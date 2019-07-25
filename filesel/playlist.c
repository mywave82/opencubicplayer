/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * playlist related functions (used by .pls and .m3u parsers)
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
#include <ctype.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "adb.h"
#include "dirdb.h"
#include "gendir.h"
#include "mdb.h"
#include "modlist.h"
#include "pfilesel.h"
#include "playlist.h"
#include "stuff/compat.h"

void fsAddPlaylist(struct modlist *ml, const char *path, const char *mask, unsigned long opt, const char *source)
{
	const struct dmDrive *dmDrive=0;
	char fullpath[PATH_MAX+1];
	struct stat st;
	struct modlistentry retval;
	char *s3;

	if (source[0]!='/')
	{
		if ((s3=index(source, '/')))
			if (s3[-1]==':')
			{
				if (!(dmDrive=dmFindDrive(source)))
				{
					*s3=0;
					fprintf(stderr, "[playlist] Drive/Protocol not supported (%s)\n", source);
					/* Drive/Protocol not supported */
					return;
				}
				source+=strlen(dmDrive->drivename);
				if ((source[0]!='/')||strstr(source, "/../"))
				{ /* doesnt catch /.. suffix, but shouldn' be a issue */
					fprintf(stderr, "[playlist] Relative paths in fullpath not possible\n");
					return;
				}
			}
	}
	if (!dmDrive)
		dmDrive=dmFindDrive("file:");

	if (strcmp(dmDrive->drivename, "file:"))
	{
		fprintf(stderr, "[playlist], API for getting handlers via dmDrive needed. TODO\n");
		return;
	}

	gendir(path, source, fullpath); /* path's doesn't need to reflect dmDrive, if drive is given, path must be full */
	if ((s3=rindex(fullpath, '/')))
		s3++;
	else
		s3=fullpath;

	memset(&st, 0, sizeof(st));
	memset(&retval, 0, sizeof(retval));

	if (stat(fullpath, &st)<0)
	{
		fprintf(stderr, "[playlist] stat() failed for %s\n", fullpath);
		return;
	}

	retval.drive=dmDrive;
	strncpy(retval.name, s3, NAME_MAX);
	retval.name[NAME_MAX]=0;

	retval.dirdbfullpath = dirdbResolvePathWithBaseAndRef(dmDrive->basepath, fullpath);
	fs12name(retval.shortname, s3);

	if (S_ISREG(st.st_mode))
	{
/*
		if (isarchivepath(fullpath))
		{
			retval.flags=MODLIST_FLAG_ARC;
			strncat(retval.fullname, "/", PATH_MAX-strlen(retval.fullname)-1);
		} else */{
			char *curext;
			getext_malloc (fullpath, &curext);

#ifndef FNM_CASEFOLD
			{
				char *name_upper;
				char *iterate;

				if ((name_upper = strdup(retval.name)))
				{
					for (iterate = name_upper; *iterate; iterate++)
						*iterate = toupper(*iterate);
				} else {
					perror("pfsm3u.c: strdup() failed");
					free (curext);
					dirdbUnref(retval.dirdbfullpath);
					return;
				}

				if (fnmatch(mask, name_upper, 0)||(!fsIsModule(curext)))
				{
					free (curext);
					free(name_upper);
					dirdbUnref(retval.dirdbfullpath);
					return;
				}
				free(name_upper);
			}
#else
			if ((fnmatch(mask, retval.name, FNM_CASEFOLD))||(!fsIsModule(curext)))
			{
				free (curext);
				dirdbUnref(retval.dirdbfullpath);
				return;
			}
#endif
			free (curext);
			retval.mdb_ref=mdbGetModuleReference(retval.shortname, st.st_size);
			retval.adb_ref=0xffffffff;
			retval.flags=MODLIST_FLAG_FILE;
		}
	} else if (S_ISDIR(st.st_mode))
	{
/*
		if ((opt&RD_PUTSUBS))
		{
			retval.flags=MODLIST_FLAG_DIR;
			strncat(retval.fullname, "/", PATH_MAX-strlen(retval.fullname)-1);
		} else if ((opt&RD_PUTRSUBS))
		{
			strncat(retval.fullname, "/", PATH_MAX-strlen(retval.fullname)-1);
			fsReadDir(ml,drive, retval.fullname, mask, opt);
			return;
		} else*/
		{
			dirdbUnref(retval.dirdbfullpath);
			return;
		}
	} else {
		dirdbUnref(retval.dirdbfullpath);
		return;
	}
	retval.Read=dosfile_Read;
	retval.ReadHeader=dosfile_ReadHeader;
	retval.ReadHandle=dosfile_ReadHandle;
	modlist_append(ml, &retval);
	dirdbUnref(retval.dirdbfullpath);
}
