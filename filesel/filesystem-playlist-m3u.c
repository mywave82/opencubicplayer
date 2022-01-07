/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-playlist.h"
#include "filesystem-playlist-m3u.h"
#include "mdb.h"
#include "modlist.h"
#include "pfilesel.h"
#include "stuff/compat.h"

static void path_detect_unix_windows (const char *path, int *unix_n, int *windows_n)
{
	if (  ( ((path[0] >= 'a') && (path[0] <= 'z')) ||
	        ((path[0] >= 'A') && (path[0] <= 'Z')) ) &&
	      (path[1] == ':') &&
	      (path[2] == '\\')  )
	{
		(*windows_n) += 10;
		(*unix_n) -= 10;
	}
	while (*path)
	{
		if ((*path) == '/')
		{
			(*unix_n)++;
		} else if ((*path) == '\\')
		{
			(*windows_n)++;
		}
		path++;
	}
}

static int get_m3u_dirdb_flags (char *buftail, int buftail_n)
{
	int unix_n = 0;
	int windows_n = 0;

	while (buftail_n > 0)
	{
		char *s1, *s2;
		/* find new-line */
		s1=memchr(buftail, '\n', buftail_n);
		s2=memchr(buftail, '\r', buftail_n);
		if (!s1)
		{
			if (!s2)
			{
				break;
			}
			s1=s2;
		} else if (s2)
		{
			if (s2<s1)
			{
				s1=s2;
			}
		}
		*s1=0; /* and terminate the line */

		if (buftail[0]=='#')
		{
			goto newline;
		}
		if (!buftail[0])
		{
			goto newline;
		}

		path_detect_unix_windows (buftail, &unix_n, &windows_n);
newline:
		*s1 = '\n';
		buftail_n-=(s1-buftail)+1;
		buftail=s1+1;
	}

	if (unix_n >= windows_n)
	{
		return DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME | DIRDB_RESOLVE_TILDE_USER;
	} else {
		return DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH;
	}
}

struct ocpdir_t *m3u_check (const struct ocpdirdecompressor_t *self, struct ocpfile_t *file, const char *filetype)
{
	char *buftail;
	int buftail_n;
	struct playlist_instance_t *iter;
	struct ocpfilehandle_t *fd = 0;
	char *readbuffer = 0;
	uint64_t filesize;
	int flags;

	if (strcasecmp (filetype, ".m3u"))
	{
		return 0;
	}

	/* check the cache for an active instance */
	for (iter = playlist_root; iter; iter = iter->next)
	{
		if (iter->head.dirdb_ref == file->dirdb_ref)
		{
			iter->head.ref (&iter->head);
			return &iter->head;
		}
	}

	iter = playlist_instance_allocate (file->parent, file->dirdb_ref);
	if (!iter)
	{
		goto out;
	}

	fd = file->open (file);
	if (!fd)
	{
		goto out;
	}
	filesize = fd->filesize (fd);
	if (filesize > 1024*1024)
	{
		fprintf (stderr, "M3U file too big\n!");
		goto out;
	}
	if (!filesize)
	{
		fprintf (stderr, "M3U file too small\n");
		goto out;
	}
	readbuffer = malloc (filesize);
	if (fd->read (fd, readbuffer, filesize) != filesize)
	{
		fprintf (stderr, "M3U file failed to read\n");
		goto out;
	}
	fd->unref (fd);
	fd = 0;

	buftail=readbuffer;
	buftail_n=filesize;

	flags = get_m3u_dirdb_flags (buftail, buftail_n);

	while (buftail_n > 0)
	{
		char *s1, *s2;
		/* find new-line */
		s1=memchr(buftail, '\n', buftail_n);
		s2=memchr(buftail, '\r', buftail_n);
		if (!s1)
		{
			if (!s2)
			{
				break;
			}
			s1=s2;
		} else if (s2)
		{
			if (s2<s1)
			{
				s1=s2;
			}
		}
		*s1=0; /* and terminate the line */

		if (buftail[0]=='#')
		{
			goto newline;
		}
		if (!buftail[0])
		{
			goto newline;
		}

		playlist_add_string (iter, strdup (buftail), flags);
newline:
		buftail_n-=(s1-buftail)+1;
		buftail=s1+1;
	}
out:

	free (readbuffer);
	readbuffer = 0;

	if (fd)
	{
		fd->unref (fd);
		fd = 0;
	}

	return &iter->head;
}

static struct ocpdirdecompressor_t m3udecompressor =
{
	"m3u",
	"M3U playlist",
	m3u_check
};

void filesystem_m3u_register (void)
{
	register_dirdecompressor (&m3udecompressor);
}
