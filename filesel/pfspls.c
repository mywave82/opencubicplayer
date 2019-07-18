/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * *.PLS file-reader/parser
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
#include "dirdb.h"
#include "gendir.h"
#include "mdb.h"
#include "modlist.h"
#include "playlist.h"
#include "pfilesel.h"
#include "stuff/compat.h"

static int plsReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
{
	char *s1, *s2;
#if 0
	char *s3;
#endif
	char *newpath;

	char *readbuffer;

	char *buftail;
	int buftail_n;

	int fd;
	struct stat st;

#ifndef FNM_CASEFOLD
	char *mask_upper;
	char *iterate;
#endif

	if (drive!=dmFILE)  /* we, only support file:// transport for now... TODO */
		return 1;

	dirdbGetFullname_malloc(path, &newpath, DIRDB_FULLNAME_NOBASE); /* no file: */

	/* Does the file end in .PLS ? */
	s1=newpath+strlen(newpath)-4;
	if ((s1<newpath) || strcasecmp(s1, ".PLS"))
	{
		free (newpath);
		return 1;
	}

	/* Try to open the file */
	if ((fd=open(newpath, O_RDONLY))<0)
	{
		fprintf (stderr, "failed to open (%s): %s\n", newpath, strerror (errno));
		free (newpath);
		return 1;
	}

	if (fstat(fd, &st)<0)
	{
		fprintf (stderr, "failed to fstat (%s): %s\n", newpath, strerror (errno));
		close(fd);
		free (newpath);
		return 1;
	}
	/* regular file? */
	if (!S_ISREG(st.st_mode))
	{
		close(fd);
		free (newpath);
		return 1;
	}
	/* file too big? */
	if (st.st_size>(1024*1024))
	{
		fprintf(stderr, "%s: File too big\n", newpath);
		close(fd);
		free (newpath);
		return 1;
	}

	readbuffer=malloc(st.st_size);
	if (read(fd, readbuffer, st.st_size)!=st.st_size)
	{
		fprintf (stderr, "Reading %s, gave only partial result\n", newpath);
		close(fd);
		free (newpath);
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
		perror("pfspls.c: strdup() failed");
		return 1;
	}
#endif

	(*rindex(newpath, '/'))=0; /* remove XXX.PLS from path-name */

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

		/* do we have a fileN= syntax? */
		if (strncasecmp(buftail, "file", 4))
			goto newline;
		if (!(s2=index(buftail, '=')))
			goto newline;
		/* skip the =, and check that the line has a length */
		if (!*(++s2))
			goto newline;
#ifndef FNM_CASEFOLD
		fsAddPlaylist(ml, newpath, mask_upper, opt, s2);
#else
		fsAddPlaylist(ml, newpath, mask, opt, s2);
#endif
newline:
		buftail_n-=(s1-buftail)+1;
		buftail=s1+1;
	}
#ifndef FNM_CASEFOLD
	free(mask_upper);
#endif
	free(readbuffer);
	free (newpath);
	return 1;
}

struct mdbreaddirregstruct plsReadDirReg = {plsReadDir MDBREADDIRREGSTRUCT_TAIL};
