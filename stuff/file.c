#include "config.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
# include <windows.h>
# include <fileapi.h>
# include <windows.h>
#endif
#include "types.h"

#include "stuff/file.h"

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
	struct osfile_cacheline_t cache;
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
	f->h = CreateFile (path,                                        /* lpFileName */
	                   GENERIC_READ | GENERIC_WRITE,                /* dwDesiredAccess */
	                   dolock?0:(FILE_SHARE_READ|FILE_SHARE_WRITE), /* dwShareMode (exclusive access) */
	                   0,                                           /* lpSecurityAttributes */
	                   mustcreate?CREATE_NEW:OPEN_ALWAYS,           /* dwCreationDisposition */
	                   FILE_ATTRIBUTE_NORMAL,                       /* dwFlagsAndAttributes */
	                   0);                                          /* hTemplateFile */
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
				fprintf(stderr, "CreateFile(%s): %s", path, lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
			free (f);
		}
		return 0;
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

void osfile_close (struct osfile_t *f)
{
	if (!f)
	{
		return;
	}
#ifdef _WIN32
	CloseHandle (f->h);
#else
	close (f->fd);
#endif
	free (f->cache.data);
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
#ifdef _WIN32
	LARGE_INTEGER newpos;
	newpos.QuadPart = (uint64_t)pos;
	if (!SetFilePointerEx (mdbHandle, newpos, 0, FILE_BEGIN))
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

void osfile_purge_readaheadcache (struct osfile_t *f)
{
	if (!f)
	{
		return;
	}
	free (f->cache.data);
	f->cache.data = 0;
	f->cache.size = 0;
	f->cache.fill = 0;
	f->cache.offset = 0;
}

int64_t osfile_write (struct osfile_t *f, const void *data, uint64_t size)
{
#ifdef _WIN32
	int64_t retval = 0;
	ssize_t res;
	if (!f)
	{
		return -1;
	}
	if (f->cache.data)
	{
		osfile_purge_readaheadcache (f);
	}
	if (f->realpos != targetpos)
	{
		LARGE_INTEGER newpos;
		newpos.QuadPart = (uint64_t)f->pos;
		if (!SetFilePointerEx (mdbHandle, newpos, 0, FILE_BEGIN))
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
		f->realpos = f->pos;
	}

	{
		DWORD NumberOfBytesWritten = 0;
		BOOL Result;

		Result = WriteFile (f->h, data, size, &NumberOfBytesWritten, 0);
		if ((!Result) || (NumberOfBytesWritten != size))
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
				fprintf(stderr, "Failed to write %lu bytes into %s: %s", (unsigned long int)size, f->pathname, lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
			f->realpos += NumberOfBytesWritten;
			return -1;
		}
		f->pos += size;
		f->realpos += size;
		data += size;
		size -= size;
		retval += size;
	}
	return retval;
#else
	int64_t retval = 0;
	ssize_t res;
	if (!f)
	{
		return -1;
	}
	if (f->cache.data)
	{
		osfile_purge_readaheadcache (f);
	}
	if (f->realpos != f->pos)
	{
		if (lseek (f->fd, f->pos, SEEK_SET) == (off_t) -1)
		{
			fprintf (stderr, "Failed to lseek %s: %s\n", f->pathname, strerror (errno));
			return -1;
		}
		f->realpos = f->pos;
	}

	while (size)
	{
		res = write (f->fd, data, size);
		if (res <= 0) /* write should never return zero, so might aswell add it here */
		{
			if ((errno == EAGAIN) || (errno == EINTR))
			{
				continue;
			}
			fprintf (stderr, "Failed to write %lu bytes into %s: %s\n", (unsigned long int)size, f->pathname, strerror (errno));
			return -1;
		}
		f->pos += res;
		f->realpos += res;
		data += res;
		size -= res;
		retval += res;
	}
	return retval;
#endif
}

static int osfile_allocate_readaheadcache (struct osfile_t *f)
{
	if (!f)
	{
		return -1;
	}
	if (f->cache.data)
	{
		return 0;
	}
	f->cache.size = 256 * 1024;
	f->cache.data = malloc (f->cache.size);
	if (!f->cache.data)
	{
		fprintf (stderr, "osfile_allocate_readaheadcache: malloc() failed\n");
		f->cache.size = 0;
		return -1;
	}
	f->cache.fill = 0;
	f->cache.offset = 0;
	return 0;
}

static int osfile_fill_cache (struct osfile_t *f)
{
#ifdef _WIN32
	size_t need = f->cache.size - f->cache.fill;
	off_t targetpos = f->cache.offset + f->cache.fill;

	if (f->realpos != targetpos)
	{
		LARGE_INTEGER newpos;
		newpos.QuadPart = (uint64_t)targetpos;
		if (!SetFilePointerEx (mdbHandle, newpos, 0, FILE_BEGIN))
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
		if (!ReadFile (f->handle, f->cache.data + f->cache.fill, need, &NumberOfBytesRead, 0))
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
		f->cache.fill += NumberOfBytesRead;
		f->realpos += NumberOfBytesRead;
		return 0;
	}
#else
	int res;
	size_t need = f->cache.size - f->cache.fill;
	off_t targetpos = f->cache.offset + f->cache.fill;

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
		res = read (f->fd, f->cache.data + f->cache.fill, need);
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
		f->cache.fill += res;
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
	if (!f->cache.data)
	{
		if (osfile_allocate_readaheadcache (f))
		{
			return -1;
		}
	}
	while (size)
	{
		uint64_t available;
		uint64_t reloffset;

		if ( (f->pos < f->cache.offset) || /* position header is behind real-position - reset cache content */
		     ((f->cache.offset + f->cache.fill) <= f->pos) ) /* cache-ends before real-position, content in cache is useless */
		{
			f->cache.offset = f->pos;
			f->cache.fill = 0;
			if (osfile_fill_cache (f))
			{
				return -1;
			}
		}

		reloffset = f->pos - f->cache.offset;
		available = f->cache.fill - reloffset;
		if (!available) // EOF
		{
			return retval;
		}
		if (available > size)
		{
			available = size;
		}
		memcpy (data, f->cache.data + reloffset, available);
		f->pos += available;
		data += available;
		size -= available;
		retval += available;
	}
	return retval;
}
