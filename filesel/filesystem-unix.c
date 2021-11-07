/* OpenCP Module Player
 * copyright (c) 2020-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to provide Unix filesystem into the virtual drive FILE:
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "../boot/psetting.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-drive.h"
#include "filesystem-unix.h"
#include "stuff/compat.h"

struct dmDrive *dmFILE;

uint32_t cfConfigDir_dirdbref = DIRDB_NOPARENT;

struct unix_ocpdirhandle_t
{
	struct unix_ocpdir_t *owner;

	DIR *dir;
	/* these are used as we iterate the dir */
	void (*callback_file)(void *token, struct ocpfile_t *);
	void (*callback_dir)(void *token, struct ocpdir_t *);
	void *token;
};

struct unix_ocpdir_t
{
	struct ocpdir_t head;
};

struct unix_ocpfilehandle_t
{
	struct ocpfilehandle_t head;

	struct unix_ocpfile_t *owner;

	int fd;
	int eof;
	int error;
	uint64_t pos;
};

struct unix_ocpfile_t
{
	struct ocpfile_t head;

	uint64_t filesize;
};

static void unix_dir_ref (struct ocpdir_t *_s);

static void unix_dir_unref (struct ocpdir_t *_s);

static ocpdirhandle_pt unix_dir_readdir_start (struct ocpdir_t *_s,
                                               void (*callback_file)(void *token, struct ocpfile_t *),
                                               void (*callback_dir)(void *token, struct ocpdir_t *),
                                               void *token);

static void unix_dir_readdir_cancel (ocpdirhandle_pt h);

static int unix_dir_readdir_iterate (ocpdirhandle_pt h);

static struct ocpdir_t *unix_dir_readdir_dir (struct ocpdir_t *_s, uint32_t dirdb_ref);

static struct ocpfile_t *unix_dir_readdir_file (struct ocpdir_t *_s, uint32_t dirdb_ref);

static void unix_file_ref (struct ocpfile_t *_s);

static void unix_file_unref (struct ocpfile_t *_s);

static struct ocpfilehandle_t *unix_file_open (struct ocpfile_t *);

static uint64_t unix_file_filesize (struct ocpfile_t *);

static int unix_file_filesize_ready (struct ocpfile_t *);

static void unix_filehandle_ref (struct ocpfilehandle_t *_s);

static void unix_filehandle_unref (struct ocpfilehandle_t *_s);

static int unix_filehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos);

static int unix_filehandle_seek_cur (struct ocpfilehandle_t *_s, int64_t pos);

static int unix_filehandle_seek_end (struct ocpfilehandle_t *_s, int64_t pos);

static uint64_t unix_filehandle_getpos (struct ocpfilehandle_t *_s);

static int unix_filehandle_eof (struct ocpfilehandle_t *_s);

static int unix_filehandle_error (struct ocpfilehandle_t *_s);

static int unix_filehandle_read (struct ocpfilehandle_t *_s, void *dst, int len);

static uint64_t unix_filehandle_filesize (struct ocpfilehandle_t *);

static int unix_filehandle_filesize_ready (struct ocpfilehandle_t *);

static struct ocpdir_t *unix_dir_steal (struct ocpdir_t *parent, const uint32_t dirdb_node);

static struct ocpfile_t *unix_file_steal (struct ocpdir_t *parent, const uint32_t dirdb_node, uint64_t filesize);

static void unix_dir_ref (struct ocpdir_t *_s)
{
	struct unix_ocpdir_t *s = (struct unix_ocpdir_t *)_s;
	s->head.refcount++;
}

static void unix_dir_unref (struct ocpdir_t *_s)
{
	struct unix_ocpdir_t *s = (struct unix_ocpdir_t *)_s;
	s->head.refcount--;
	if (s->head.refcount <= 0)
	{
#if 0
		if (s->dir)
		{
			closedir (s->dir);
			s->dir = 0;
		}
#endif
		if (s->head.parent)
		{
			s->head.parent->unref (s->head.parent);
			s->head.parent = 0;
		}
		dirdbUnref (s->head.dirdb_ref, dirdb_use_dir);
		free (s);
	}
}

static ocpdirhandle_pt unix_dir_readdir_start (struct ocpdir_t *_s,
                                void (*callback_file)(void *token, struct ocpfile_t *),
                                void (*callback_dir)(void *token, struct ocpdir_t *),
                                void *token)
{
	struct unix_ocpdir_t *s = (struct unix_ocpdir_t *)_s;
	struct unix_ocpdirhandle_t *retval;
	char *path;

	dirdbGetFullname_malloc (s->head.dirdb_ref, &path, DIRDB_FULLNAME_NODRIVE|DIRDB_FULLNAME_ENDSLASH);
	if (!path)
	{
		fprintf (stderr, "[filesystem unix readdir_start]: dirdbGetFullname_malloc () failed #1\n");
		return 0;
	}

	retval = malloc (sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "[filesystem unix readdir_start] malloc() failed #1\n");
		free (path);
		return 0;
	}

	retval->dir = opendir (path);
	if (!retval->dir)
	{
		fprintf (stderr, "[filesystem unix readdir_start]: opendir (\"%s\") failed\n", path);
		free (path);
		free (retval);
		return 0;
	}
	free (path); path = 0;

	s->head.ref(&s->head);
	retval->owner = s;
	retval->callback_file = callback_file;
	retval->callback_dir = callback_dir;
	retval->token = token;

	return retval;
}

static void unix_dir_readdir_cancel (ocpdirhandle_pt _h)
{
	struct unix_ocpdirhandle_t *h = _h;
	struct unix_ocpdir_t *s = h->owner;

	closedir (h->dir); h->dir = 0;
	free (h);

	s->head.unref (&s->head);
}

static int unix_dir_readdir_iterate (ocpdirhandle_pt _h)
{
	struct unix_ocpdirhandle_t *h = _h;
	struct unix_ocpdir_t *s = h->owner;
	struct dirent *de;

again:
	de=readdir (h->dir);
	if (!de)
	{
		return 0;
	}

	if (!(strcmp (de->d_name, ".") && strcmp (de->d_name, "..")))
	{
		goto again;
	}

#ifdef HAVE_STRUCT_DIRENT_D_TYPE
	if (de->d_type==DT_DIR)
	{
		struct ocpdir_t *n = unix_dir_steal (&s->head, dirdbFindAndRef (s->head.dirdb_ref, de->d_name, dirdb_use_dir));
		h->callback_dir (h->token, n);
		n->unref (n);
		return 1;
	} else if ((de->d_type==DT_REG)||(de->d_type==DT_LNK)||(de->d_type==DT_UNKNOWN))
#endif
	{
		struct stat st;
		struct stat lst;
		uint32_t dirdb_ref = dirdbFindAndRef (s->head.dirdb_ref, de->d_name, dirdb_use_dir);
		char *path;

		dirdbGetFullname_malloc (dirdb_ref, &path, DIRDB_FULLNAME_NODRIVE);

		if (lstat (path, &lst))
		{
			free (path);
			dirdbUnref (dirdb_ref, dirdb_use_dir);
			return 1;
		}

		if (S_ISLNK(lst.st_mode))
		{
			if (stat (path, &st))
			{
				free (path);
				dirdbUnref (dirdb_ref, dirdb_use_dir);
				return 1;
			}
		} else {
			memcpy (&st, &lst, sizeof (st));
		}

		free (path);

		if (S_ISDIR(st.st_mode))
		{
			struct ocpdir_t *n = unix_dir_steal (&s->head, dirdb_ref);
			h->callback_dir (h->token, n);
			n->unref (n);
			return 1;
		}

		if (S_ISREG(st.st_mode))
		{
			struct ocpfile_t *n = unix_file_steal (&s->head, dirdbRef (dirdb_ref, dirdb_use_file), st.st_size);
			dirdbUnref (dirdb_ref, dirdb_use_dir);
			h->callback_file (h->token, n);
			n->unref (n);
			return 1;
		}

		dirdbUnref (dirdb_ref, dirdb_use_dir);

		return 1;
	}

	return 0;
}

static struct ocpdir_t *unix_dir_readdir_dir (struct ocpdir_t *_s, uint32_t dirdb_ref)
{
	struct unix_ocpdir_t *s = (struct unix_ocpdir_t *)_s;
	char *path;
	struct stat st;
	struct stat lst;

	dirdbGetFullname_malloc (dirdb_ref, &path, DIRDB_FULLNAME_NODRIVE|DIRDB_FULLNAME_ENDSLASH);
	if (!path)
	{
		fprintf (stderr, "[filesystem unix readdir_dir]: dirdbGetFullname_malloc () failed\n");
		return 0;
	}

	if (lstat (path, &lst))
	{
		free (path);
		return 0;
	}

	if (S_ISLNK(lst.st_mode))
	{
		if (stat (path, &st))
		{
			free (path);
			return 0;
		}
	} else {
		memcpy (&st, &lst, sizeof (st));
	}

	free (path);

	if (S_ISDIR(st.st_mode))
	{
		struct ocpdir_t *n = unix_dir_steal (&s->head, dirdbRef (dirdb_ref, dirdb_use_dir));
		return n;
	}

	return 0;
}

static struct ocpfile_t *unix_dir_readdir_file (struct ocpdir_t *_s, uint32_t dirdb_ref)
{
	struct unix_ocpdir_t *s = (struct unix_ocpdir_t *)_s;
	char *path;
	struct stat st;
	struct stat lst;

	dirdbGetFullname_malloc (dirdb_ref, &path, DIRDB_FULLNAME_NODRIVE);
	fprintf (stderr, "   unix_dir_readdir_file \"%s\"\n", path);
	if (!path)
	{
		fprintf (stderr, "[filesystem unix readdir_file]: dirdbGetFullname_malloc () failed\n");
		return 0;
	}

	if (lstat (path, &lst))
	{
		fprintf (stderr, "   lstat() failed\n");
		free (path);
		return 0;
	}

	if (S_ISLNK(lst.st_mode))
	{
		if (stat (path, &st))
		{
			fprintf (stderr, "   stat() failed\n");
			free (path);
			return 0;
		}
	} else {
		memcpy (&st, &lst, sizeof (st));
	}

	free (path);

	if (S_ISREG(st.st_mode))
	{
		struct ocpfile_t *n = unix_file_steal (&s->head, dirdbRef (dirdb_ref, dirdb_use_file), st.st_size);
		fprintf (stderr, "   unix_file_steal => %p\n", n);
		return n;
	}

	fprintf (stderr, "   not a REF file\n");
	return 0;
}

static void unix_file_ref (struct ocpfile_t *_s)
{
	struct unix_ocpfile_t *s = (struct unix_ocpfile_t *)_s;
	s->head.refcount++;
}

static void unix_file_unref (struct ocpfile_t *_s)
{
	struct unix_ocpfile_t *s = (struct unix_ocpfile_t *)_s;
	s->head.refcount--;
	if (!s->head.refcount)
	{
		dirdbUnref (s->head.dirdb_ref, dirdb_use_file);
		s->head.parent->unref (s->head.parent);
		s->head.parent = 0;
		free (s);
	}
}

static struct ocpfilehandle_t *unix_file_open (struct ocpfile_t *_s)
{
	struct unix_ocpfile_t *s = (struct unix_ocpfile_t *)_s;
	char *path;
	int fd;
	struct unix_ocpfilehandle_t *r;

	dirdbGetFullname_malloc (s->head.dirdb_ref, &path, DIRDB_FULLNAME_NODRIVE);

	fd = open (path, O_RDONLY);
	if (fd < 0)
	{
		//fprintf (stderr, "[filesystem] unable to open \"%s\": %s\n", path, strerror (errno));
	}
	free (path);
	if (fd < 0)
	{
		return 0;
	}

	r = calloc (1, sizeof (*r));
	if (!r)
	{ /* out of memory */
		close (fd);
		return 0;
	}
	r->head.refcount = 1;
	r->fd = fd;
	r->owner = s;
	s->head.ref(&s->head);
	ocpfilehandle_t_fill
	(
		&r->head,
		unix_filehandle_ref,
		unix_filehandle_unref,
		unix_filehandle_seek_set,
		unix_filehandle_seek_cur,
		unix_filehandle_seek_end,
		unix_filehandle_getpos,
		unix_filehandle_eof,
		unix_filehandle_error,
		unix_filehandle_read,
	        0, /* ioctl */
		unix_filehandle_filesize,
		unix_filehandle_filesize_ready,
	        0, /* filename_override */
		dirdbRef (s->head.dirdb_ref, dirdb_use_filehandle)
	);

	return &r->head;
}

static uint64_t unix_file_filesize (struct ocpfile_t *_s)
{
	struct unix_ocpfile_t *s = (struct unix_ocpfile_t *)_s;

	return s->filesize;
}

static int unix_file_filesize_ready (struct ocpfile_t *_s)
{
	return 1;
}

static void unix_filehandle_ref (struct ocpfilehandle_t *_s)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;
	s->head.refcount++;
}

static void unix_filehandle_unref (struct ocpfilehandle_t *_s)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;
	s->head.refcount--;
	if (s->head.refcount <= 0)
	{
		if (s->fd >= 0)
		{
			close (s->fd);
			s->fd = -1;
		}
		dirdbUnref (s->head.dirdb_ref, dirdb_use_filehandle);

		s->owner->head.unref(&s->owner->head);
		s->owner = 0;

		free (s);
	}
}

static int unix_filehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;
	off_t r;

	r = lseek (s->fd, pos, SEEK_SET);
	if (r == (off_t) -1)
	{
		s->error = 1;
		s->eof = 1;
		return -1;
	} else {
		s->pos = r;
	}

	s->error = 0;
	s->eof = (r >= s->owner->filesize);

	return 0;
}

static int unix_filehandle_seek_cur (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;
	off_t r;

	r = lseek (s->fd, pos, SEEK_CUR);
	if (r == (off_t) -1)
	{
		s->error = 1;
		s->eof = 1;
		return -1;
	} else {
		s->pos = r;
	}

	s->error = 0;
	s->eof = (r >= s->owner->filesize);

	return 0;
}

static int unix_filehandle_seek_end (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;
	off_t r;

	r = lseek (s->fd, pos, SEEK_END);
	if (r == (off_t) -1)
	{
		s->error = 1;
		s->eof = 1;
		return -1;
	} else {
		s->pos = r;
	}

	s->error = 0;
	s->eof = (r >= s->owner->filesize);

	return 0;
}

static uint64_t unix_filehandle_getpos (struct ocpfilehandle_t *_s)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;
	off_t r;

	r = lseek (s->fd, 0, SEEK_CUR);
	if (r == (off_t) -1)
	{
		s->error = 1;
		s->eof = 1;
		return 0;
	}

	return r;
}

static int unix_filehandle_eof (struct ocpfilehandle_t *_s)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;

	return s->eof;
}

static int unix_filehandle_error (struct ocpfilehandle_t *_s)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;

	return s->error;
}

static int unix_filehandle_read (struct ocpfilehandle_t *_s, void *dst, int len)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;
	int got = 0;

	while (len)
	{
		int res;
		res = read (s->fd, (uint8_t *)dst + got, len);
		if (res == 0)
		{
			s->eof = 1;
			return got;
		}
		if (res < 0)
		{
			s->eof = 1;
			s->error = 1;
			return got;
		}
		got += res;
		len -= res;
		s->pos += res;
	}

	s->eof = (s->pos >= s->owner->filesize);

	return got;
}

static uint64_t unix_filehandle_filesize (struct ocpfilehandle_t *_s)
{
	struct unix_ocpfilehandle_t *s = (struct unix_ocpfilehandle_t *)_s;

	return s->owner->filesize;
}

static int unix_filehandle_filesize_ready (struct ocpfilehandle_t *_s)
{
	return 1;
}

// steals the dirdb reference
static struct ocpdir_t *unix_dir_steal (struct ocpdir_t *parent, const uint32_t dirdb_node)
{
	struct unix_ocpdir_t *r;

	r = calloc (1, sizeof (*r));

	if (!r)
	{ /* out of memory */
		dirdbUnref (dirdb_node, dirdb_use_dir);
		return 0;
	}

	ocpdir_t_fill (&r->head,
	               unix_dir_ref,
	               unix_dir_unref,
	               parent,
	               unix_dir_readdir_start,
	               0,
	               unix_dir_readdir_cancel,
	               unix_dir_readdir_iterate,
	               unix_dir_readdir_dir,
	               unix_dir_readdir_file,
	               0,
	               dirdb_node,
	               1, /* refcount */
	               0, /* is_archive */
	               0  /* is_playlist */);

	if (parent)
	{
		parent->ref (parent);
	}
	return &r->head;
}

// steals the dirdb reference
static struct ocpfile_t *unix_file_steal (struct ocpdir_t *parent, const uint32_t dirdb_node, uint64_t filesize)
{
	struct unix_ocpfile_t *r;

	r = calloc (1, sizeof (*r));

	if (!r)
	{ /* out of memory */
		dirdbUnref (dirdb_node, dirdb_use_file);
		return 0;
	}

	ocpfile_t_fill
	(
		&r->head,
		unix_file_ref,
		unix_file_unref,
		parent,
		unix_file_open,
		unix_file_filesize,
		unix_file_filesize_ready,
		0, /* filename_override */
		dirdb_node,
		1, /* refcount */
	        0  /* is_nodetect */
	);

	parent->ref (parent);
	r->filesize = filesize;

	return &r->head;
}

struct ocpdir_t *file_unix_root (void)
{
	uint32_t dirdb_node = dirdbFindAndRef (DIRDB_NOPARENT, "file:" , dirdb_use_dir);
	return unix_dir_steal (0, dirdb_node);
}

void filesystem_unix_init (void)
{
	struct ocpdir_t *root = file_unix_root();
	struct ocpdir_t *newcwd;
	struct dmDrive *newdrive;
	char *currentpath;
	uint32_t newcurrentpath;

	dmFILE = RegisterDrive("file:", root, root);

	root->unref (root); root = 0;

	currentpath = getcwd_malloc ();
	newcurrentpath = dirdbResolvePathWithBaseAndRef(dmFILE->basedir->dirdb_ref, currentpath, DIRDB_RESOLVE_NODRIVE, dirdb_use_dir);
	free (currentpath);
	currentpath = 0;
	if (!filesystem_resolve_dirdb_dir (newcurrentpath, &newdrive, &newcwd))
	{
		if (newdrive == dmFILE)
		{
			if (dmFILE->cwd)
			{
				dmFILE->cwd->unref (dmFILE->cwd);
			}
			dmFILE->cwd = newcwd;
		} else {
			newcwd->unref (newcwd);
		}
	}
	dirdbUnref (newcurrentpath, dirdb_use_dir);

	cfConfigDir_dirdbref = dirdbResolvePathWithBaseAndRef (dmFILE->basedir->dirdb_ref, cfConfigDir, 0, dirdb_use_dir);
}

void filesystem_unix_done (void)
{
	dirdbUnref (cfConfigDir_dirdbref, dirdb_use_dir);
	cfConfigDir_dirdbref = DIRDB_NOPARENT;
}
