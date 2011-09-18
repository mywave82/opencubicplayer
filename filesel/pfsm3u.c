/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * *.M3u file-reader/parser
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
 *  -ss051231  Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
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

static int m3uReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
{
	char *s1, *s2;
#if 0
	char *s3;
#endif
	char newpath[PATH_MAX+1];

	char *readbuffer;

	char *buftail;
	int buftail_n;

	int fd;
	struct stat st;

#ifndef FNM_CASEFOLD
	char *mask_upper;
	char *iterate;
#endif

	if (drive!=dmFILE)
		return 1;

	dirdbGetFullName(path, newpath, DIRDB_FULLNAME_NOBASE); /* no file: */

	/* Does the file end in .M3U ? */
	s1=newpath+strlen(newpath)-4;
	if (s1<newpath)
	{
		return 1;
	}
	if (strcasecmp(s1, ".M3U"))
		return 1;

	/* Try to open the file */
	if ((fd=open(newpath, O_RDONLY))<0)
		return 1;

	(*rindex(newpath, '/'))=0; /* remove ....pls from path-name */

	if (fstat(fd, &st)<0)
	{
		close(fd);
		return 1;
	}
	/* regular file? */
	if (!S_ISREG(st.st_mode))
	{
		close(fd);
		return 1;
	}
	/* file too big? */
	if (st.st_size>(1024*1024))
	{
		fprintf(stderr, "[M3U] File too big\n");
		close(fd);
		return 1;
	}

	readbuffer=malloc(st.st_size);
	if (read(fd, readbuffer, st.st_size)!=st.st_size)
	{
		close(fd);
		return 1;
	}
	close(fd);

	buftail=readbuffer;
	buftail_n=st.st_size;

#ifndef FNM_CASEFOLD
	if ((mask_upper = strdup(mask)))
	{
		for (iterate = mask_upper; *iterate; iterate++)
			*iterate = toupper(*iterate);
	} else {
		perror("pfsm3u.c: strdup() failed");
		return 1;
	}
#endif

	while (buftail_n>0)
	{
		/* find new-line */
		s1=memchr(buftail, '\n', buftail_n);
		s2=memchr(buftail, '\r', buftail_n);
		if (!s1)
		{
			if (!s2)
				break;
			s1=s2;
		} else if (s2)
			if (s2<s1)
				s1=s2;
		*s1=0; /* and terminate the line */
#if 0
		s2=buftail;
#endif

		if (buftail[0]=='#')
			goto newline;
		if (!buftail[0])
			goto newline;
#ifndef FNM_CASEFOLD
		fsAddPlaylist(ml, newpath, mask_upper, opt, buftail);
#else
		fsAddPlaylist(ml, newpath, mask, opt, buftail);
#endif

newline:
		buftail_n-=(s1-buftail)+1;
		buftail=s1+1;
	}
#ifndef FNM_CASEFOLD
	free(mask_upper);
#endif
	free(readbuffer);
	return 1;
}

struct mdbreaddirregstruct m3uReadDirReg = {m3uReadDir MDBREADDIRREGSTRUCT_TAIL};
