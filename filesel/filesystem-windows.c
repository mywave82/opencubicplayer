/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to provide Windows filesystem into the virtual drives A: -> Z:
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
#include <assert.h>
#include <ctype.h>
#include <errhandlingapi.h>
#include <fileapi.h>
#include <handleapi.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "boot/psetting.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-setup.h"
#include "filesel/filesystem-windows.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"

struct windows_ocpdir_t
{
	struct ocpdir_t head;
};

struct windows_ocpfile_t
{
	struct ocpfile_t head;

	uint64_t filesize;
};

struct windows_ocpfilehandle_t
{
	struct ocpfilehandle_t head;

	struct windows_ocpfile_t *owner;

	HANDLE fd;
	int eof;
	int error;
	uint64_t pos;
};

       struct dmDrive  *dmDriveLetters[26]       = {0};
       char             dmLastActiveDriveLetter  = 0;
static struct ocpdir_t *dmDriveRoots[26]         = {0};

static struct ocpfile_t *windows_file_steal (struct ocpdir_t *parent, const uint32_t dirdb_node, uint64_t filesize);

static struct ocpdir_t *windows_dir_steal (struct ocpdir_t *parent, const uint32_t dirdb_node);

static void windows_dir_ref (struct ocpdir_t *_s)
{
	struct windows_ocpdir_t *s = (struct windows_ocpdir_t *)_s;
	s->head.refcount++;
}

static void windows_dir_unref (struct ocpdir_t *_s)
{
	struct windows_ocpdir_t *s = (struct windows_ocpdir_t *)_s;
	s->head.refcount--;
	if (s->head.refcount <= 0)
	{
		if (s->head.parent)
		{
			s->head.parent->unref (s->head.parent);
			s->head.parent = 0;
		}
		dirdbUnref (s->head.dirdb_ref, dirdb_use_dir);
		free (s);
	}
}

struct windows_ocpdirhandle_t
{
	void(*callback_file)(void *token, struct ocpfile_t *);
	void(*callback_dir )(void *token, struct ocpdir_t *);
	void *token;

	struct windows_ocpdir_t *owner;

	int EndOfList;
	HANDLE FindHandle;
	WIN32_FIND_DATAA FindData;
};

static ocpdirhandle_pt windows_dir_readdir_start (struct ocpdir_t *_s,
                                                  void(*callback_file)(void *token, struct ocpfile_t *),
                                                  void(*callback_dir )(void *token, struct ocpdir_t *),
                                                  void *token)
{
	struct windows_ocpdir_t *s = (struct windows_ocpdir_t *)_s;
	struct windows_ocpdirhandle_t *r;
	char *path, *path2;

	dirdbGetFullname_malloc (s->head.dirdb_ref, &path, DIRDB_FULLNAME_ENDSLASH | DIRDB_FULLNAME_BACKSLASH);

	if (!path)
	{
		fprintf (stderr, "[filesystem windows readdir_start]: dirdbGetFullname_malloc () failed #1\n");
		return 0;
	}
	path2 = malloc (strlen(path) + 2);
	if (!path2)
	{
		fprintf (stderr, "[filesystem windows readdir_start] malloc() failed #1\n");
		free (path);
		return 0;
	}
	sprintf (path2, "%s*", path);
	free (path); path = 0;

	r = calloc (sizeof (*r), 1);
	if (!r)
	{
		fprintf (stderr, "[filesystem windows readdir_start] malloc() failed #2\n");
		free (path2);
		return 0;
	}

	r->callback_file = callback_file;
	r->callback_dir  = callback_dir;
	r->token         = token;

	r->FindHandle = FindFirstFile (path2, &r->FindData);

	if (r->FindHandle == INVALID_HANDLE_VALUE)
	{ /* no files found, invalid path */
		char *lpMsgBuf = NULL;
		if (FormatMessage (
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM     |
			FORMAT_MESSAGE_IGNORE_INSERTS,             /* dwFlags */
			NULL,                                      /* lpSource */
			GetLastError(),                            /* dwMessageId */
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
			(LPSTR) &lpMsgBuf,                         /* lpBuffer */
			0,                                         /* nSize */
			NULL                                       /* Arguments */
		))
		{

			fprintf (stderr, "[filesystem] FindFirstFile(\"%s\"): %s\n", path2, lpMsgBuf);
			LocalFree (lpMsgBuf);
		}
		free (path2);
		free (r);
		return 0;
	}

	assert (r->FindHandle);
	free (path2);

	r->owner = s;
	s->head.ref (&s->head);

	return r;
}

static int windows_dir_readdir_iterate (ocpdirhandle_pt h)
{
	struct windows_ocpdirhandle_t *r = (struct windows_ocpdirhandle_t *)h;
	struct windows_ocpdir_t *s = r->owner;

	if (!r->FindHandle)
	{
		return 0;
	}
	if (r->EndOfList)
	{
		return 0;
	}
	if (r->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (strcmp (r->FindData.cFileName, ".") && strcmp (r->FindData.cFileName, ".."))
		{
			struct ocpdir_t *n = windows_dir_steal (&s->head, dirdbFindAndRef (s->head.dirdb_ref, r->FindData.cFileName, dirdb_use_dir));
			r->callback_dir (r->token, n);
			n->unref (n);
		}
	} else {
		struct ocpfile_t *n = windows_file_steal (&s->head, dirdbFindAndRef (s->head.dirdb_ref, r->FindData.cFileName, dirdb_use_file), ((uint64_t)(r->FindData.nFileSizeHigh) << 32) | r->FindData.nFileSizeLow);
		r->callback_file (r->token, n);
		n->unref (n);
	}
	if (!FindNextFile (r->FindHandle, &r->FindData))
	{
		r->EndOfList = 1;
		return 0;
	}
	return 1;
}

static void windows_dir_readdir_cancel (ocpdirhandle_pt h)
{
	struct windows_ocpdirhandle_t *r = (struct windows_ocpdirhandle_t *)h;
	struct windows_ocpdir_t *s = r->owner;

	if (r->FindHandle)
	{
		FindClose (r->FindHandle);
		r->FindHandle = 0;
	}
	s->head.unref (&s->head);
	r->owner = 0;
	free (r);
}

static void windows_filehandle_ref (struct ocpfilehandle_t *_s)
{
	struct windows_ocpfilehandle_t *s = (struct windows_ocpfilehandle_t *)_s;
	s->head.refcount++;
}

static void windows_filehandle_unref (struct ocpfilehandle_t *_s)
{
	struct windows_ocpfilehandle_t *s = (struct windows_ocpfilehandle_t *)_s;
	s->head.refcount--;
	if (s->head.refcount <= 0)
	{
		if (s->fd)
		{
			CloseHandle (s->fd);
			s->fd = 0;
		}
		dirdbUnref (s->head.dirdb_ref, dirdb_use_filehandle);

		s->owner->head.unref(&s->owner->head);
		s->owner = 0;

		free (s);
	}
}

static int windows_filehandle_seek_set (struct ocpfilehandle_t *_s, int64_t pos)
{
	struct windows_ocpfilehandle_t *s = (struct windows_ocpfilehandle_t *)_s;
	LARGE_INTEGER request, reply;

	request.QuadPart = pos;
	reply.QuadPart = 0;

	if (!SetFilePointerEx (s->fd, request, &reply, 0 /* FILE_BEGIN */))
	{
		s->error = 1;
		s->eof = 1;
		return -1;
	} else {
		s->pos = reply.QuadPart;
	}

	s->error = 0;
	s->eof = (reply.QuadPart >= s->owner->filesize);

	return 0;
}

static uint64_t windows_filehandle_getpos (struct ocpfilehandle_t *_s)
{
	struct windows_ocpfilehandle_t *s = (struct windows_ocpfilehandle_t *)_s;

	return s->pos;
}

static int windows_filehandle_eof (struct ocpfilehandle_t *_s)
{
	struct windows_ocpfilehandle_t *s = (struct windows_ocpfilehandle_t *)_s;

	return s->eof;
}

static int windows_filehandle_error (struct ocpfilehandle_t *_s)
{
	struct windows_ocpfilehandle_t *s = (struct windows_ocpfilehandle_t *)_s;

	return s->error;
}

static uint64_t windows_filehandle_filesize (struct ocpfilehandle_t *_s)
{
	struct windows_ocpfilehandle_t *s = (struct windows_ocpfilehandle_t *)_s;

	return s->owner->filesize;
}

static int windows_filehandle_filesize_ready (struct ocpfilehandle_t *_s)
{
	return 1;
}

static int windows_filehandle_read (struct ocpfilehandle_t *_s, void *dst, int len)
{
	struct windows_ocpfilehandle_t *s = (struct windows_ocpfilehandle_t *)_s;
	DWORD bytesread = 0;

	if (!ReadFile (s->fd, dst, len, &bytesread, 0))
	{
		s->eof = 1;
		s->error = 1;
		return 0;
	}
	s->pos += bytesread;
	s->eof = (s->pos >= s->owner->filesize);

	return bytesread;
}

static void windows_file_ref (struct ocpfile_t *_s)
{
	struct windows_ocpfile_t *s = (struct windows_ocpfile_t *)_s;
	s->head.refcount++;
}

static void windows_file_unref (struct ocpfile_t *_s)
{
	struct windows_ocpfile_t *s = (struct windows_ocpfile_t *)_s;
	s->head.refcount--;
	if (!s->head.refcount)
	{
		dirdbUnref (s->head.dirdb_ref, dirdb_use_file);
		s->head.parent->unref (s->head.parent);
		s->head.parent = 0;
		free (s);
	}
}

static struct ocpfilehandle_t *windows_file_open (struct ocpfile_t *_s)
{
	struct windows_ocpfile_t *s = (struct windows_ocpfile_t *)_s;
	char *path;
	HANDLE fd;
	struct windows_ocpfilehandle_t *r;

	dirdbGetFullname_malloc (s->head.dirdb_ref, &path, DIRDB_FULLNAME_BACKSLASH);

	fd = CreateFile (path,                   /* lpFileName */
	                 GENERIC_READ,          /* dwDesiredAccess */
	                 FILE_SHARE_READ,       /* dwShareMode */
	                 0,                     /* lpSecurityAttributes */
	                 OPEN_EXISTING,         /* dwCreationDisposition */
	                 FILE_ATTRIBUTE_NORMAL, /* dwFlagsAndAttributes */
	                 0);                    /* hTemplateFile */
	if (fd == INVALID_HANDLE_VALUE)
	{
		char *lpMsgBuf = NULL;
		if (FormatMessage (
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM     |
			FORMAT_MESSAGE_IGNORE_INSERTS,             /* dwFlags */
			NULL,                                      /* lpSource */
			GetLastError(),                            /* dwMessageId */
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
			(LPSTR) &lpMsgBuf,                         /* lpBuffer */
			0,                                         /* nSize */
			NULL                                       /* Arguments */
		))
		{

			fprintf (stderr, "[filesystem] CreateFile(\"%s\"): %s\n", path, lpMsgBuf);
			LocalFree (lpMsgBuf);
		}
		free (path);
		return 0;
	}
	free (path);

	r = calloc (1, sizeof (*r));
	if (!r)
	{ /* out of memory */
		CloseHandle (fd);
		return 0;
	}
	r->fd = fd;
	r->owner = s;
	s->head.ref(&s->head);
	ocpfilehandle_t_fill
	(
		&r->head,
		windows_filehandle_ref,
		windows_filehandle_unref,
		_s,
		windows_filehandle_seek_set,
		windows_filehandle_getpos,
		windows_filehandle_eof,
		windows_filehandle_error,
		windows_filehandle_read,
		0, /* ioctl */
		windows_filehandle_filesize,
		windows_filehandle_filesize_ready,
		0, /* filename_override */
		dirdbRef (s->head.dirdb_ref, dirdb_use_filehandle),
		1
	);

	return &r->head;
}

static uint64_t windows_file_filesize (struct ocpfile_t *_s)
{
	struct windows_ocpfile_t *s = (struct windows_ocpfile_t *)_s;

	return s->filesize;
}

static int windows_file_filesize_ready (struct ocpfile_t *_s)
{
	return 1;
}

static void filesystem_windows_add_drive (const int index, const char DriveLetter)
{
	char drivename[13];
	snprintf (drivename, sizeof (drivename), "%c:", DriveLetter);

	if (!dmDriveRoots[index])
	{
		uint32_t dirdb_node = dirdbFindAndRef (DIRDB_NOPARENT, drivename, dirdb_use_dir);

		if (dirdb_node == DIRDB_CLEAR)
		{
			fprintf (stderr, "filesystem_windows_add_drive(\"%c:\"): dirdbFindAndRef() failed\n", DriveLetter);
			return;
		}

		dmDriveRoots[index] = windows_dir_steal (0, dirdb_node);
	}

#warning drive FIX-ME CWD, the active drive should not default to root
	dmDriveLetters[index] = RegisterDrive (drivename, dmDriveRoots[index], dmDriveRoots[index]);
}

static void filesystem_windows_remove_drive (const int index)
{
	int i;
	if (dmDriveLetters[index])
	{
/* If the drive we are removing is the current active drive, update it */
		if (dmCurDrive == dmDriveLetters[index])
		{
			for (i=0; i < 26; i++)
			{
				int j = (i + 2) % 26;
				if (j == index)
				{
					continue;
				}
				if (dmDriveLetters[j])
				{
					dmCurDrive = dmDriveLetters[j];
					dmLastActiveDriveLetter = dmCurDrive->drivename[0];
					break;
				}
			}
			if (i == 26)
			{
				dmCurDrive = dmSetup;
			}
		} else if ((dmLastActiveDriveLetter - 'A') == index)
		{
			dmLastActiveDriveLetter = 0;
		}
		UnregisterDrive (dmDriveLetters[index]);
		dmDriveLetters[index] = 0;
	}
}

void filesystem_windows_refresh_drives (void)
{
	char i;
	DWORD DrivesMask = GetLogicalDrives();
	for (i='A'; i <= 'Z'; i++)
	{
		if (DrivesMask & (1 << (i - 'A')))
		{
			if (!dmDriveLetters[i - 'A'])
			{
				filesystem_windows_add_drive (i - 'A', i);
			}
		} else {
			if (dmDriveLetters[i - 'A'])
			{
				filesystem_windows_remove_drive (i - 'A');
			}
		}
	}
}

// steals the dirdb reference
static struct ocpfile_t *windows_file_steal (struct ocpdir_t *parent, const uint32_t dirdb_node, uint64_t filesize)
{
	struct windows_ocpfile_t *r;

	r = calloc (1, sizeof (*r));

	if (!r)
	{ /* out of memory */
		dirdbUnref (dirdb_node, dirdb_use_file);
		return 0;
	}

	ocpfile_t_fill
	(
		&r->head,
		windows_file_ref,
		windows_file_unref,
		parent,
		windows_file_open,
		windows_file_filesize,
		windows_file_filesize_ready,
		0, /* filename_override */
		dirdb_node,
		1, /* refcount */
	        0, /* is_nodetect */
		COMPRESSION_NONE
	);

	parent->ref (parent);
	r->filesize = filesize;

	return &r->head;
}

// steals the dirdb reference
static struct ocpdir_t *windows_dir_steal (struct ocpdir_t *parent, const uint32_t dirdb_node)
{
	struct windows_ocpdir_t *r;

	r = calloc (1, sizeof (*r));

	if (!r)
	{ /* out of memory */
		dirdbUnref (dirdb_node, dirdb_use_dir);
		return 0;
	}

	ocpdir_t_fill (&r->head,
	               windows_dir_ref,
	               windows_dir_unref,
	               parent,
	               windows_dir_readdir_start,
	               0, /* windows_readflatdir_start */
	               windows_dir_readdir_cancel,
	               windows_dir_readdir_iterate,
	               0, /* windows_dir_readdir_dir */
	               0, /* windows_dir_readdir_file */
	               0, /* charset_override_API */
	               dirdb_node,
	               1, /* refcount */
	               0, /* is_archive */
	               0, /* is_playlist */
	               COMPRESSION_NONE);

	if (parent)
	{
		parent->ref (parent);
	}
	return &r->head;
}

static struct ocpdir_t *filesystem_windows_resolve_dir (const char *path)
{
	uint32_t dirdb_ref;
	struct dmDrive *drive = 0;
	struct ocpdir_t *dir = 0;

	dirdb_ref = dirdbResolvePathWithBaseAndRef (DIRDB_CLEAR, path, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_WINDOWS_SLASH, dirdb_use_dir);
	if (!filesystem_resolve_dirdb_dir (dirdb_ref, &drive, &dir))
	{
		if (drive->drivename[1] != ':')
		{
			dir->unref (dir);
			dir = 0;
		}
	}
	dirdbUnref (dirdb_ref, dirdb_use_dir);
	return dir;
}

struct dmDrive *filesystem_windows_init (void)
{
	char path[4096];
	DWORD length;
	uint32_t newcurrentpath;
	int driveindex;
	char i;

	struct ocpdir_t *newcwd;
	struct dmDrive *newdrive;

	filesystem_windows_refresh_drives();

	length = GetCurrentDirectory (sizeof (path), path);
	if (length >= sizeof (path)) /* buffer was not big enough */
	{
		goto failed;
	}
	if ((path[0] == '\\' || path[1] == '\\')) // network share
	{
		goto failed;
	}
	if (path[1] != ':') // not a drive????
	{
		goto failed;
	}
	driveindex = toupper (path[0]) - 'A';
	if (!dmDriveLetters[driveindex])
	{
		goto failed;
	}

	if (!(configAPI.HomeDir       = filesystem_windows_resolve_dir    (configAPI.HomePath       ))) { fprintf (stderr, "Unable to resolve cfHome=%s\n",       configAPI.HomePath);       goto failed; }
	if (!(configAPI.ConfigHomeDir = filesystem_windows_resolve_dir    (configAPI.ConfigHomePath ))) { fprintf (stderr, "Unable to resolve cfConfigHome=%s\n", configAPI.ConfigHomePath); goto failed; }
	if (!(configAPI.DataHomeDir   = filesystem_windows_resolve_dir    (configAPI.DataHomePath   ))) { fprintf (stderr, "Unable to resolve cfDataHome=%s\n",   configAPI.DataHomePath);   goto failed; }
	if (!(configAPI.DataDir       = filesystem_windows_resolve_dir    (configAPI.DataPath       ))) { fprintf (stderr, "Unable to resolve cfData=%s\n",       configAPI.DataPath);       goto failed; }
	if (!(configAPI.TempDir       = filesystem_windows_resolve_dir    (configAPI.TempPath       ))) { fprintf (stderr, "Unable to resolve cfTemp=%s\n",       configAPI.TempPath);       goto failed; }

	newcurrentpath = dirdbResolvePathWithBaseAndRef (dmDriveRoots[driveindex]->dirdb_ref, path, DIRDB_RESOLVE_WINDOWS_SLASH | DIRDB_RESOLVE_DRIVE, dirdb_use_dir);
	if (newcurrentpath == DIRDB_CLEAR)
	{
		goto failed;
	}

	if (!filesystem_resolve_dirdb_dir (newcurrentpath, &newdrive, &newcwd))
	{
		newdrive->cwd->unref (newdrive->cwd);
		newdrive->cwd = newcwd;
	}
	dirdbUnref (newcurrentpath, dirdb_use_dir);

	if (newdrive)
	{
		dmLastActiveDriveLetter = newdrive->drivename[0];
	}

	return newdrive;

failed:
	for (i='C'; i <= 'Z'; i++)
	{
		if (dmDriveLetters[i-'A'])
		{
			return dmDriveLetters[i-'A'];
		}
	}
	for (i='A'; i < 'C'; i++)
	{
		if (dmDriveLetters[i-'A'])
		{
			return dmDriveLetters[i-'A'];
		}
	}

	return 0;
}

void filesystem_windows_done (void)
{
	int i;
	for (i='A'; i <= 'Z'; i++)
	{
		if (dmDriveLetters[i - 'A'])
		{
			filesystem_windows_remove_drive (i - 'A');
			dmDriveLetters[i - 'A'] = 0;
		}
	}
	dmLastActiveDriveLetter = 0;
}
