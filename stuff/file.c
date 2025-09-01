/* OpenCP Module Player
 * copyright (c) 2023-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Access host operating system files via a unified API
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef _WIN32
# include <windows.h>
# include <fileapi.h>
#else
# include <dirent.h>
# include <time.h>
#endif
#include "types.h"

#include "boot/psetting.h"
#include "stuff/compat.h"
#include "stuff/file.h"
#include "stuff/utf-16.h"

struct osfile_cacheline_t
{
	uint8_t *data;
	uint64_t size;
	uint64_t fill;
	uint64_t offset;
};

struct osfile_t
{
#ifdef _WIN32
	HANDLE h;
#else
	int fd;
#endif
	char *pathname;
	uint64_t pos; /* user-space */
	uint64_t realpos; /* kernel-space */
	struct osfile_cacheline_t readahead_cache;
	struct osfile_cacheline_t writeback_cache;
};

struct osfile_t *osfile_open_readwrite (const char *pathname, int dolock, int mustcreate)
{
	struct osfile_t *f;

	if (!pathname)
	{
		fprintf (stderr, "osfile_open_readwrite called with null\n");
		return 0;
	}

	f = calloc (1, sizeof (*f));
	if (!f)
	{
		fprintf (stderr, "osfile_open_readwrite (%s): Failed to allocate memory #1\n", pathname);
		return 0;
	}
	f->pathname = strdup (pathname);
	if (!f->pathname)
	{
		fprintf (stderr, "osfile_open_readwrite (%s): Failed to allocate memory #2\n", pathname);
		free (f);
		return 0;
	}

#ifdef _WIN32
	{
		uint16_t *wpathname;
		if (!(wpathname = utf8_to_utf16_LFN (pathname, 0)))
		{
			fprintf (stderr, "osfile_open_readwrite (%s): utf8 to utf16 convertion failed\n", pathname);
			free (f->pathname);
			free (f);
			return 0;
		}

		f->h = CreateFileW (wpathname,                                  /* lpFileName */
		                   GENERIC_READ | GENERIC_WRITE,                /* dwDesiredAccess */
		                   dolock?0:(FILE_SHARE_READ|FILE_SHARE_WRITE), /* dwShareMode (exclusive access) */
		                   0,                                           /* lpSecurityAttributes */
		                   mustcreate?CREATE_NEW:OPEN_ALWAYS,           /* dwCreationDisposition */
		                   FILE_ATTRIBUTE_NORMAL,                       /* dwFlagsAndAttributes */
		                   0);                                          /* hTemplateFile */

		free (wpathname);
		wpathname = 0;

		if (f->h == INVALID_HANDLE_VALUE)
		{
			if (!(mustcreate && (GetLastError() == ERROR_FILE_EXISTS)))
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
					fprintf(stderr, "CreateFileW(L\"%s\"): %s", f->pathname, lpMsgBuf);
					LocalFree (lpMsgBuf);
				}
			}
			free (f->pathname);
			free (f);
			return 0;
		}
	}
#else
	if ((f->fd = open(pathname,
	                  O_RDWR | O_CREAT | (mustcreate?O_EXCL:0)
# ifdef O_CLOEXEC
	                                                           | O_CLOEXEC
# endif
	                                                                      , S_IREAD|S_IWRITE)) < 0 )
	{
		if (!(mustcreate && (errno == EEXIST)))
		{
			fprintf (stderr, "open(%s): %s\n", pathname, strerror (errno));
		}
		free (f);
		return 0;
	}

	if (dolock)
	{
		if (flock (f->fd, LOCK_EX | LOCK_NB))
		{
			fprintf (stderr, "Failed to lock %s (more than one instance?)\n", pathname);
			close (f->fd);
			free (f);
			return 0;
		}
	}
#endif

	return f;
}

struct osfile_t *osfile_open_readonly (const char *pathname, int dolock)
{
	struct osfile_t *f;

	if (!pathname)
	{
		fprintf (stderr, "osfile_open_readonly called with null\n");
		return 0;
	}

	f = calloc (1, sizeof (*f));
	if (!f)
	{
		fprintf (stderr, "osfile_open_readonly (%s): Failed to allocate memory #1\n", pathname);
		return 0;
	}
	f->pathname = strdup (pathname);
	if (!f->pathname)
	{
		fprintf (stderr, "osfile_open_readonly (%s): Failed to allocate memory #2\n", pathname);
		free (f);
		return 0;
	}

#ifdef _WIN32
	{
		uint16_t *wpathname;
		if (!(wpathname = utf8_to_utf16_LFN (f->pathname, 0)))
		{
			fprintf (stderr, "osfile_open_readwrite (%s): utf8 to utf16 convertion failed\n", pathname);
			free (f->pathname);
			free (f);
			return 0;
		}

		f->h = CreateFileW (wpathname,                                  /* lpFileName */
				   GENERIC_READ,                                /* dwDesiredAccess */
				   dolock?0:(FILE_SHARE_READ),                  /* dwShareMode (exclusive access) */
				   0,                                           /* lpSecurityAttributes */
				   OPEN_EXISTING,                               /* dwCreationDisposition */
				   FILE_ATTRIBUTE_NORMAL,                       /* dwFlagsAndAttributes */
				   0);                                          /* hTemplateFile */

		free (wpathname);
		wpathname = 0;

		if (f->h == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() != ERROR_FILE_NOT_FOUND)
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
					fprintf(stderr, "CreateFileW(L\"%s\"): %s", f->pathname, lpMsgBuf);
					LocalFree (lpMsgBuf);
				}
			}
			free (f->pathname);
			free (f);
			return 0;
		}
	}
#else
	if ((f->fd = open(pathname,
	                  O_RDONLY
# ifdef O_CLOEXEC
	                                                           | O_CLOEXEC
# endif
	                                                                      , S_IREAD|S_IWRITE)) < 0 )
	{
		if (errno != ENOENT)
		{
			fprintf (stderr, "open(%s): %s\n", pathname, strerror (errno));
		}
		free (f->pathname);
		free (f);
		return 0;
	}

	if (dolock)
	{
		if (flock (f->fd, LOCK_EX | LOCK_NB))
		{
			fprintf (stderr, "Failed to lock %s (more than one instance?)\n", pathname);
			close (f->fd);
			free (f->pathname);
			free (f);
			return 0;
		}
	}
#endif

	return f;
}

uint64_t osfile_getfilesize (struct osfile_t *f)
{
#ifndef _WIN32
	struct stat st;
#else
	LARGE_INTEGER Size;
#endif
	if (!f)
	{
		return 0;
	}
#ifndef _WIN32
	if (fstat (f->fd, &st))
	{
		return 0;
	}
	return st.st_size;
#else
	if (!GetFileSizeEx (f->h, &Size))
	{
		return 0;
	} else {
		return Size.QuadPart;
	}
#endif
}

int64_t osfile_purge_writeback_cache (struct osfile_t *f)
{
	int64_t retval = 0;
#ifndef _WIN32
	ssize_t res;
#endif
	if (!f)
	{
		return -1;
	}

#ifdef _WIN32
	if (f->realpos != f->writeback_cache.offset)
	{
		LARGE_INTEGER newpos;
		newpos.QuadPart = (uint64_t)f->writeback_cache.offset;
		if (!SetFilePointerEx (f->h, newpos, 0, FILE_BEGIN))
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
				fprintf(stderr, "Failed to SetFilePointerEx(%s): %s", f->pathname, lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
			return -1;
		}
		f->realpos = f->writeback_cache.offset;
	}

	{
		DWORD NumberOfBytesWritten = 0;
		BOOL Result;

		Result = WriteFile (f->h, f->writeback_cache.data, f->writeback_cache.fill, &NumberOfBytesWritten, 0);
		if ((!Result) || (NumberOfBytesWritten != f->writeback_cache.fill))
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
				fprintf(stderr, "Failed to write %lu bytes into %s: %s", (unsigned long int)f->writeback_cache.fill, f->pathname, lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
			f->realpos += NumberOfBytesWritten;
			return -1;
		}
	}
#else
	if (f->realpos != f->writeback_cache.offset)
	{
		if (lseek (f->fd, f->writeback_cache.offset, SEEK_SET) == (off_t) -1)
		{
			fprintf (stderr, "Failed to lseek %s: %s\n", f->pathname, strerror (errno));
			return -1;
		}
		f->realpos = f->writeback_cache.offset;
	}

	while (f->writeback_cache.fill)
	{
		res = write (f->fd, f->writeback_cache.data, f->writeback_cache.fill);
		if (res <= 0) /* write should never return zero, so might aswell add it here */
		{
			if ((errno == EAGAIN) || (errno == EINTR))
			{
				continue;
			}
			fprintf (stderr, "Failed to write %lu bytes into %s: %s\n", (unsigned long int)f->writeback_cache.fill, f->pathname, strerror (errno));
			return -1;
		}
		if (res < f->writeback_cache.fill)
		{
			fprintf (stderr, "Partial write %lu of %lu bytes into %s\n", (unsigned long int)res, (unsigned long int)f->writeback_cache.fill, f->pathname);
			memmove (f->writeback_cache.data, f->writeback_cache.data + res, f->writeback_cache.fill - res);
			f->realpos += res;
			f->writeback_cache.offset += res;
			f->writeback_cache.fill -= res;

			return -1;
		}
		break;
	}
#endif
	retval += f->writeback_cache.fill;
	f->realpos += f->writeback_cache.fill;
	f->writeback_cache.offset += f->writeback_cache.fill;
	f->writeback_cache.fill = 0;

	return retval;
}

void osfile_close (struct osfile_t *f)
{
	if (!f)
	{
		return;
	}

	if (f->writeback_cache.fill)
	{
		osfile_purge_writeback_cache (f);
	}

#ifdef _WIN32
	CloseHandle (f->h);
#else
	close (f->fd);
#endif
	free (f->pathname);
	free (f->readahead_cache.data);
	free (f->writeback_cache.data);
	free (f);
}

uint64_t osfile_getpos (struct osfile_t *f)
{
	return f->pos;
}

void osfile_setpos (struct osfile_t *f, uint64_t pos)
{
	f->pos = pos;
}

void osfile_truncate_at (struct osfile_t *f, uint64_t pos)
{
	if (f->writeback_cache.fill)
	{
		osfile_purge_writeback_cache (f);
	}
#ifdef _WIN32
	LARGE_INTEGER newpos;
	newpos.QuadPart = (uint64_t)pos;
	if (!SetFilePointerEx (f->h, newpos, 0, FILE_BEGIN))
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
			fprintf(stderr, "Failed to SetFilePointerEx(%s): %s", f->pathname, lpMsgBuf);
			LocalFree (lpMsgBuf);
		}
		return;
	}
	f->realpos = pos;
	if (!SetEndOfFile (f->h))
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
			fprintf(stderr, "Failed to SetEndOfFile(%s): %s", f->pathname, lpMsgBuf);
			LocalFree (lpMsgBuf);
		}

	}
#else
	if (ftruncate (f->fd, pos))
	{
		fprintf (stderr, "osfile_truncate_at(%s, %" PRIu64 ") failed: %s\n", f->pathname, pos, strerror (errno));
	}
#endif
}

void osfile_purge_readahead_cache (struct osfile_t *f)
{
	if (!f)
	{
		return;
	}
	f->readahead_cache.fill = 0;
	f->readahead_cache.offset = 0;
}

static int osfile_allocate_writeback_cache (struct osfile_t *f)
{
	if (!f)
	{
		return -1;
	}
	if (f->writeback_cache.data)
	{
		return 0;
	}
	f->writeback_cache.size = 256 * 1024;
	f->writeback_cache.data = malloc (f->writeback_cache.size);
	if (!f->writeback_cache.data)
	{
		fprintf (stderr, "osfile_allocate_writeback_cache: malloc() failed\n");
		f->writeback_cache.size = 0;
		return -1;
	}
	f->writeback_cache.fill = 0;
	f->writeback_cache.offset = 0;
	return 0;
}

int64_t osfile_write (struct osfile_t *f, const void *data, uint64_t size)
{
	int64_t retval = 0;
	if (!f)
	{
		return -1;
	}
	if (!f->writeback_cache.data)
	{
		if (osfile_allocate_writeback_cache (f))
		{
			return -1;
		}
	}
	if (f->readahead_cache.fill)
	{
		osfile_purge_readahead_cache (f);
	}

	while (size)
	{
		uint64_t chunk;
		uint64_t available = f->writeback_cache.size - f->writeback_cache.fill;
		if (size > available)
		{
			chunk = available;
		} else {
			chunk = size;
		}
		if ((!f->writeback_cache.fill) || /* empty */
		    ((f->writeback_cache.offset + f->writeback_cache.fill) == f->pos)) /* we can append */
		{
			if (!f->writeback_cache.fill)
			{
				f->writeback_cache.offset = f->pos;
			}

			memcpy (f->writeback_cache.data + f->writeback_cache.fill, data, chunk);
			f->writeback_cache.fill += chunk;

			data += chunk;
			size -= chunk;
			f->pos += chunk;
			retval += chunk;
		} else {
			/* we could implement support for seek back + write support, but only OCP use-case is to skip only so we stick to the most simple solution: purge the cache */
			if (osfile_purge_writeback_cache (f) < 0)
			{
				return -1;
			}
		}

		if (f->writeback_cache.fill >= f->writeback_cache.size)
		{
			if (osfile_purge_writeback_cache (f) < 0)
			{
				return -1;
			}
		}
	}
	return retval;
}

static int osfile_allocate_readahead_cache (struct osfile_t *f)
{
	if (!f)
	{
		return -1;
	}
	if (f->readahead_cache.data)
	{
		return 0;
	}
	f->readahead_cache.size = 256 * 1024;
	f->readahead_cache.data = malloc (f->readahead_cache.size);
	if (!f->readahead_cache.data)
	{
		fprintf (stderr, "osfile_allocate_readahead_cache: malloc() failed\n");
		f->readahead_cache.size = 0;
		return -1;
	}
	f->readahead_cache.fill = 0;
	f->readahead_cache.offset = 0;
	return 0;
}

static int osfile_fill_readahead_cache (struct osfile_t *f)
{
#ifdef _WIN32
	size_t need = f->readahead_cache.size - f->readahead_cache.fill;
	off_t targetpos = f->readahead_cache.offset + f->readahead_cache.fill;

	if (f->realpos != targetpos)
	{
		LARGE_INTEGER newpos;
		newpos.QuadPart = (uint64_t)targetpos;
		if (!SetFilePointerEx (f->h, newpos, 0, FILE_BEGIN))
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
				fprintf(stderr, "Failed to SetFilePointerEx(%s): %s", f->pathname, lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
			return -1;
		}
		f->realpos = targetpos;
	}

	{
		DWORD NumberOfBytesRead = 0;
		if (!ReadFile (f->h, f->readahead_cache.data + f->readahead_cache.fill, need, &NumberOfBytesRead, 0))
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
				fprintf(stderr, "CreateFile(%s): %s", f->pathname, lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
			return -1;
		}
		f->readahead_cache.fill += NumberOfBytesRead;
		f->realpos += NumberOfBytesRead;
		return 0;
	}
#else
	int res;
	size_t need = f->readahead_cache.size - f->readahead_cache.fill;
	off_t targetpos = f->readahead_cache.offset + f->readahead_cache.fill;

	if (f->realpos != targetpos)
	{
		if (lseek (f->fd, targetpos, SEEK_SET) == (off_t) -1)
		{
			fprintf (stderr, "Failed to lseek %s: %s\n", f->pathname, strerror (errno));
			return -1;
		}
		f->realpos = targetpos;
	}

	while (1)
	{
		res = read (f->fd, f->readahead_cache.data + f->readahead_cache.fill, need);
		if (res < 0)
		{
			if ((errno == EAGAIN) || (errno == EINTR))
			{
				continue;
			}
			fprintf (stderr, "Failed to read from %s: %s\n", f->pathname, strerror (errno));
			return -1;
		}
		if (res == 0)
		{
			return 0; /* no more data to read - EOF */
		}
		f->readahead_cache.fill += res;
		f->realpos += res;
		return 0;
	}
#endif
}

int64_t osfile_read (struct osfile_t *f, void *data, uint64_t size) /* returns non-zero on error */
{
	int64_t retval = 0;
	if (!f)
	{
		return -1;
	}
	if (!f->readahead_cache.data)
	{
		if (osfile_allocate_readahead_cache (f))
		{
			return -1;
		}
	}
	if (f->writeback_cache.fill)
	{
		osfile_purge_writeback_cache (f);
	}
	while (size)
	{
		uint64_t available;
		uint64_t reloffset;

		if ( (f->pos < f->readahead_cache.offset) || /* position header is behind real-position - reset cache content */
		     ((f->readahead_cache.offset + f->readahead_cache.fill) <= f->pos) ) /* cache-ends before real-position, content in cache is useless */
		{
			f->readahead_cache.offset = f->pos;
			f->readahead_cache.fill = 0;
			if (osfile_fill_readahead_cache (f))
			{
				return -1;
			}
		}

		reloffset = f->pos - f->readahead_cache.offset;
		available = f->readahead_cache.fill - reloffset;
		if (!available) // EOF
		{
			return retval;
		}
		if (available > size)
		{
			available = size;
		}
		memcpy (data, f->readahead_cache.data + reloffset, available);
		f->pos += available;
		data += available;
		size -= available;
		retval += available;
	}
	return retval;
}

#ifdef _WIN32

struct osdir_iterate_internal_t
{
	HANDLE *d;
	WIN32_FIND_DATAW data;
	uint16_t *path;
	struct osdir_iterate_internal_t *child;
	int opened; /* used by delete, to delay the RemoveDirectory call */
};

static struct osdir_iterate_internal_t * osdir_iterate_opendir (const uint16_t *path)
{
	int len = wcslen (path);
	struct osdir_iterate_internal_t *i = calloc (sizeof (*i) + (len + 2 + 1) * sizeof (uint16_t), 1);
	if (!i)
	{
		return 0;
	}
	i->path = (uint16_t *)&(i[1]);
	wcscpy (i->path, path);

	if (i->path[len-1] != '\\')
	{
		i->path[len] = '\\';
		len++;
	}

	i->path[len] = '*';
	i->path[len+1] = 0;

	i->d = FindFirstFileW (i->path, &i->data);
	i->path[len] = 0;

	if (i->d == INVALID_HANDLE_VALUE)
	{
		free (i);
		return 0;
	}
	return i;
}

int osdir_size_start (struct osdir_size_t *r, const char *path)
{
	uint16_t *wpath;
	wpath = utf8_to_utf16_LFN (path, 0); /* osdir_iterate_opendir() keeps track of \ and * */

	if (!wpath)
	{
		return -1;
	}

	/* returns -1 on error, otherwize 0 */
	memset (r, 0, sizeof (*r));
	r->internal = osdir_iterate_opendir (wpath);
	free (wpath);
	if (!r->internal)
	{
		return -1;
	}
	return 0;
}

int osdir_size_iterate (struct osdir_size_t *r)
{
	/* returns 1 if more iterations are needed, otherwize 0 */
	int count = 0;
	struct osdir_iterate_internal_t *i, **p;

	if ((!r) || (!r->internal))
	{
		return 0;
	}

	p = (struct osdir_iterate_internal_t **)(&r->internal);
	i = r->internal;

	if ((i->d == INVALID_HANDLE_VALUE) && (!i->child))
	{
		free (i);
		r->internal = 0;
		return 0;
	}

	do
	{
		while (i->child)
		{
			p = &i->child;
			i = i->child;
		}

		if (i->d != INVALID_HANDLE_VALUE)
		{
			if ((i->data.cFileName[0] == '.') && (i->data.cFileName[1] == 0)) goto next;
			if ((i->data.cFileName[0] == '.') && (i->data.cFileName[1] == '.') && (i->data.cFileName[2] == 0)) goto next;

			if (i->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				size_t filelen = wcslen (i->path);
				size_t len = filelen + 1 + wcslen (i->data.cFileName) + 1;
				uint16_t *wtemp = malloc (len * sizeof (uint16_t));

				r->directories_n++;
				if (wtemp)
				{
					swprintf (wtemp, len, L"%ls%ls", i->path, i->data.cFileName);
					i->child = osdir_iterate_opendir (wtemp);
					free (wtemp);
				}
			} else {
				r->files_n++;
				r->files_size += (((uint_fast64_t)i->data.nFileSizeHigh) << 32) | i->data.nFileSizeLow;
				count++;
			}

next:
			if (!FindNextFileW (i->d, &i->data))
			{
				FindClose (i->d);
				i->d = INVALID_HANDLE_VALUE;
			}
		}

		if ((i->d == INVALID_HANDLE_VALUE) && (!i->child))
		{
			free (i);
			*p = 0;
			return 1;
		}

		count++;
	} while (count < 1024);

	return 1;
}

void osdir_size_cancel (struct osdir_size_t *r)
{
	struct osdir_iterate_internal_t *i, *n;

	if (!r || !r->internal)
	{
		return;
	}

	i = (struct osdir_iterate_internal_t *)(r->internal);
	n = i->child;
	while (i)
	{
		n = i->child;

		if (i->d != INVALID_HANDLE_VALUE)
		{
			FindClose (i->d);
			i->d = INVALID_HANDLE_VALUE;
		}
		free (i);

		i = n;
	}
	r->internal = 0;
}

int osdir_trash_available (const char *path)
{
#warning Windows does support Recycle Bin, but API is windows version dependent and not straight forward
	return 0;
}
int osdir_trash_perform (const char *path)
{
	return -1;
}

int osdir_delete_start   (struct osdir_delete_t *r, const char *path)
{
	uint16_t *wpath;
	wpath = utf8_to_utf16_LFN (path, 0); /* osdir_iterate_opendir() keeps track of \ and * */
	if (!wpath)
	{
		return -1;
	}

	/* returns -1 on error, otherwize 0 */
	memset (r, 0, sizeof (*r));
	r->internal = osdir_iterate_opendir (wpath);
	free (wpath);

	if (!r->internal)
	{
		return -1;
	}
	return 0;
}

int osdir_delete_iterate (struct osdir_delete_t *r)
{
	/* returns 1 if more iterations are needed, otherwize 0 */
	int count = 0;
	struct osdir_iterate_internal_t *i, **p;

	if ((!r) || (!r->internal))
	{
		return 0;
	}

	p = (struct osdir_iterate_internal_t **)(&r->internal);
	i = r->internal;

	if ((i->d == INVALID_HANDLE_VALUE) && (!i->child))
	{
		free (i);
		r->internal = 0;
		return 0;
	}

	do
	{
		while (i->child)
		{
			p = &i->child;
			i = i->child;
		}

		if (i->d != INVALID_HANDLE_VALUE)
		{
			if ((i->data.cFileName[0] == '.') && (i->data.cFileName[1] == 0)) goto next;
			if ((i->data.cFileName[0] == '.') && (i->data.cFileName[1] == '.') && (i->data.cFileName[2] == 0)) goto next;

			{
				size_t filelen = wcslen (i->path);
				size_t len = filelen + 1 + wcslen (i->data.cFileName) + 1;
				uint16_t *wtemp = malloc (len * sizeof (uint16_t));

				if (!wtemp) goto next;

				swprintf (wtemp, len, L"%ls%ls", i->path, i->data.cFileName);

				if (i->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (!i->opened)
					{
						i->opened = 1; /* delay the loop */
						i->child = osdir_iterate_opendir (wtemp);
						free (wtemp);
						count++;
						continue;
					} else {
						i->opened = 0;
						if (RemoveDirectoryW (wtemp))
						{
							r->removed_directories_n++;
						} else {
							r->failed_directories_n++;
						}
					}
				} else {
					if (DeleteFileW (wtemp))
					{
						r->removed_files_n++;
					} else {
						r->failed_files_n++;
					}
				}
				count++;
				free (wtemp);
			}

next:
			if (!FindNextFileW (i->d, &i->data))
			{
				FindClose (i->d);
				i->d = INVALID_HANDLE_VALUE;
			}
		}

		if ((i->d == INVALID_HANDLE_VALUE) && (!i->child))
		{
			free (i);
			*p = 0;
			return 1;
		}

		count++;
	} while (count < 64);

	return 1;

}

void osdir_delete_cancel (struct osdir_delete_t *r)
{
	struct osdir_iterate_internal_t *i, *n;

	if (!r || !r->internal)
	{
		return;
	}

	i = (struct osdir_iterate_internal_t *)(r->internal);
	n = i->child;
	while (i)
	{
		n = i->child;

		if (i->d != INVALID_HANDLE_VALUE)
		{
			FindClose (i->d);
			i->d = INVALID_HANDLE_VALUE;
		}
		free (i);

		i = n;
	}
	r->internal = 0;
}

#else

struct osdir_iterate_internal_t
{
	DIR *d;
	char *path; /* no need to free, concatted after this "parent" struct */
	struct osdir_iterate_internal_t *child;
};

static struct osdir_iterate_internal_t * osdir_iterate_opendir (const char *path)
{
	struct osdir_iterate_internal_t *i = calloc (sizeof (*i) + strlen (path) + 1, 1);
	if (!i)
	{
		return 0;
	}
	i->path = (char *)&(i[1]);
	strcpy (i->path, path);
	i->d = opendir (path);
	if (!i->d)
	{
		free (i);
		return 0;
	}
	return i;
}

int osdir_size_start (struct osdir_size_t *r, const char *path)
{
	memset (r, 0, sizeof (*r));
	r->internal = osdir_iterate_opendir (path);
	if (!r->internal)
	{
		return -1;
	}
	return 0;
}

int osdir_size_iterate (struct osdir_size_t *r)
{
	int count = 0;
	struct osdir_iterate_internal_t *i, **p;
	struct dirent *de;

	if (!r || !r->internal)
	{
		return 0;
	}

	p = (struct osdir_iterate_internal_t **)(&r->internal);
	i = r->internal;
	while (i->child)
	{
		p = &i->child;
		i = i->child;
	}

	while ((de = readdir (i->d)))
	{
		size_t len = strlen(i->path) + 1 + strlen (de->d_name) + 1;
		char *temp = malloc (len);
		struct stat st;

		if (!temp)
		{
			return 0;
		}
		snprintf (temp, len, "%s%s%s", i->path, (i->path[strlen(i->path) - 1] == '/') ? "" : "/", de->d_name);
		if (!lstat (temp, &st))
		{
			if ((st.st_mode & S_IFMT) == S_IFDIR)
			{
				if ((strcmp (de->d_name, ".")) &&(strcmp (de->d_name, "..")))
				{
					i->child = osdir_iterate_opendir (temp);
					r->directories_n++;
				}
				free (temp);
				temp = 0;
				return 1;
			} else {
				r->files_n++;
				if ((st.st_mode & S_IFMT) == S_IFREG)
				{
					r->files_size += st.st_size;
				}
				count++;
			}
		}
		free (temp);
		temp = 0;

		count++;
		if (count >= 1024)
		{
			return 1; /* we might need to repaint the screen */
		}
	}
	closedir (i->d);
	i->d = 0;

	free (i);
	*p = 0;

	return 1; /* back to parent, via possible repaint of the screen */
}

void osdir_size_cancel (struct osdir_size_t *r)
{
	struct osdir_iterate_internal_t *i, *n;

	if (!r || !r->internal)
	{
		return;
	}

	i = (struct osdir_iterate_internal_t *)(r->internal);
	n = i->child;
	while (i)
	{
		n = i->child;

		closedir (i->d);
		i->d = 0;

		free (i);

		i = n;
	}
	r->internal = 0;
}

#ifndef CFDATAHOMEDIR_OVERRIDE /* Do not include this code when making unit-tests */
int osdir_trash_available (const char *path)
{
	struct stat st1, st2;
	size_t len = strlen (configAPI.HomePath) + strlen (".local/share/Trash/") + 1;
	char *p = malloc (len);
	if (!p)
	{
		return 0;
	}
	snprintf (p, len, "%s.local/share/Trash/", configAPI.HomePath);
	if (stat (p, &st1))
	{
		free (p); p = 0;
		return 0;
	}
	free (p); p = 0;
	if (stat (path, &st2))
	{
		return 0;
	}
	if (st1.st_dev == st2.st_dev)
	{
		return 1;
	}
	return 0;
}

/* attempt to follow https://specifications.freedesktop.org/trash-spec/1.0/ */
int osdir_trash_perform (const char *path)
{
	const char *name;
	int namelen;
	size_t len;
	char *trash;
	char *tempinfo;
	char *tempfiles;
	int i;
	int fd;

	char *xdg_data_home;
	xdg_data_home = getenv("XDG_DATA_HOME");
	if (xdg_data_home)
	{
		len = strlen (xdg_data_home) + 6 + 1;
		trash = malloc (len);
		if (!trash)
		{
			return -1;
		}
		snprintf (trash, len, "%s/Trash", xdg_data_home);
	} else {
		len = strlen (configAPI.HomePath) + 19;
		trash = malloc (len);
		if (!trash)
		{
			return -1;
		}
		snprintf (trash, len, "%s.local/share/Trash", configAPI.HomePath);
	}

	len = strlen (path);
	if (len && (path[len-1] == '/'))
	{
		name = memrchr (path, '/', strlen(path) - 1);
		if (!name)
		{
			name = path;
		} else {
			name++;
		}
		namelen = strlen (name) - 1;
	} else {
		name = strrchr (path, '/');
		if (!name)
		{
			name = path;
		} else {
			name++;
		}
		namelen = strlen (name);
	}

	len = strlen (trash) + strlen ("/info/") + namelen + 32 + 1;
	tempinfo = malloc (len);
	tempfiles = malloc (len);
	if ((!tempinfo) || (!tempfiles))
	{
		free (trash);
		free (tempinfo);
		free (tempfiles);
		return -1;
	}
	snprintf (tempinfo,  len, "%s/info/%.*s",  trash, namelen, name);
	snprintf (tempfiles, len, "%s/files/%.*s", trash, namelen, name);
	i = 0;
	while (1)
	{
		fd = open (tempinfo, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		if (fd >= 0)
		{
			break;
		}
		if (errno == EINTR)
		{
			continue;
		}
		if (errno != EEXIST)
		{
			free (trash);
			free (tempinfo);
			free (tempfiles);
			return -1;
		}
		snprintf (tempinfo,  len, "%s/info/%.*s-%d",  trash, namelen, name, ++i);
		snprintf (tempfiles, len, "%s/files/%.*s-%d", trash, namelen, name, i);
	}
	do {} while ((write (fd, "[Trash Info]\nPath=", 18) < 0) && (errno == EINTR));
	{
		const char *c;
		for (c=path; *c && !((c[0] == '/') && (c[1] == 0)); c++) /* do not add final slash if present */
		{
			if ( ((*c >= '0') && (*c <= '9')) ||
			     ((*c >= 'A') && (*c <= 'Z')) ||
			     ((*c >= 'a') && (*c <= 'z')) )
			{
				do {} while ((write (fd, c, 1) < 0) && (errno == EINTR));
			} else {
				char c4[4];
				snprintf (c4, 4, "%%%02x", *(unsigned char *)c);
				do {} while ((write (fd, c4, 3) < 0) && (errno == EINTR));
			}
		}
	}

	do {} while ((write (fd, "\nDeletionDate=", 14) < 0) && (errno == EINTR));
	{
		char c32[32];
		struct tm *t;
		time_t T;

		time(&T);
		t = localtime (&T);

		snprintf (c32, 32, "%04u%02u%02uT%02u:%02u:%02u\n",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec);
		do {} while ((write (fd, c32, strlen (c32)) < 0) && (errno == EINTR));
	}
	do {} while ((close (fd) < 0) && (errno == EINTR));

	if (rename (path, tempfiles))
	{
		unlink (tempinfo);
		free (trash);
		free (tempinfo);
		free (tempfiles);
		return -1;
	}

	free (trash);
	free (tempinfo);
	free (tempfiles);

	return 0;
}

int osdir_delete_start   (struct osdir_delete_t *r, const char *path)
{
	memset (r, 0, sizeof (*r));
	r->internal = osdir_iterate_opendir (path);
	if (!r->internal)
	{
		return -1;
	}
	return 0;
}

int osdir_delete_iterate (struct osdir_delete_t *r)
{
	/* returns 1 if more iterations are needed, otherwize 0 */
	int count = 0;
	struct osdir_iterate_internal_t *i, **p;
	struct dirent *de;

	if (!r || !r->internal)
	{
		return 0;
	}

	p = (struct osdir_iterate_internal_t **)(&r->internal);
	i = r->internal;
	while (i->child)
	{
		p = &i->child;
		i = i->child;
	}

	while ((de = readdir (i->d)))
	{
		size_t len = strlen(i->path) + 1 + strlen (de->d_name) + 1;
		char *temp = malloc (len);
		struct stat st;

		if (!temp)
		{
			return 0;
		}
		snprintf (temp, len, "%s%s%s", i->path, (i->path[strlen(i->path) - 1] == '/') ? "" : "/", de->d_name);
		if (!lstat (temp, &st))
		{
			if ((st.st_mode & S_IFMT) == S_IFDIR)
			{
				if ((strcmp (de->d_name, ".")) &&(strcmp (de->d_name, "..")))
				{
					i->child = osdir_iterate_opendir (temp);
				}
				free (temp);
				temp = 0;
				return 1;
			} else {
				if (unlink (temp))
				{
					r->failed_files_n++;
				} else {
					r->removed_files_n++;
				}
				count++;
			}
		}
		free (temp);
		temp = 0;

		count++;
		if (count >= 64)
		{
			return 1; /* we might need to repaint the screen */
		}
	}
	closedir (i->d);
	i->d = 0;

	if (rmdir (i->path))
	{
		r->failed_directories_n++;
	} else {
		r->removed_directories_n++;
	}

	free (i);
	*p = 0;

	return 1; /* back to parent, via possible repaint of the screen */
}

void osdir_delete_cancel (struct osdir_delete_t *r)
{
	struct osdir_iterate_internal_t *i, *n;

	if (!r || !r->internal)
	{
		return;
	}

	i = (struct osdir_iterate_internal_t *)(r->internal);
	n = i->child;
	while (i)
	{
		n = i->child;

		closedir (i->d);
		i->d = 0;

		free (i);

		i = n;
	}
	r->internal = 0;
}
#endif

#endif
