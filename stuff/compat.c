/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Some functions not present in POSIX that is needed
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
 *  -ss040614   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss040709   Stian Skjelstad <stian@nixia.no>
 *    -added dos_clock since the linux one returnes cpu_time
 *  -ss040816   Stian Skjelstad <stian@nixia.no>
 *    -started on nasty hack to implement .tar.* files better
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "compat.h"

#ifdef __POCC__
#include <sys/types.h>
#endif /* for off_t */
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <stdarg.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef __W32__
#include <windows.h>
#endif /* __W32__ */

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#include <ctype.h>

#ifdef __MACOS__
#include <Threads.h>
#endif

time_t dos_clock(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec*0x10000+t.tv_usec*1024/15625;
}

void getext_malloc(const char *src, char **ext)
{
	const char *ref1;
	int len;

	/* clear the target points, for now */
	if (ext) *ext = 0;

	if ((ref1 = rindex (src, '/')))
	{
		src = ref1+1;
	}

	len = strlen(src);

	ref1 = rindex (src, '.');
	if (!ref1)
	{
		ref1 = src + len;
	}

	src = ref1;

	if (ext)
	{
		*ext = strdup(src);
		if (!*ext)
		{
			fprintf (stderr, "getext_malloc: *ext = strdup(\"%s\") failed\n", src);
			return;
		}
	}
}

int splitpath4_malloc(const char *src, char **drive, char **path, char **file, char **ext)
{
	/* returns non-zero on errors */
	const char *ref1;
	int len;

	/* clear the target points, so our error-path can free-up data */
	if (drive) *drive = 0;
	if (path) *path = 0;
	if (file) *file = 0;
	if (ext) *ext = 0;

	/* if src string starts with /, we do not have a drive string for sure */
	if (*src!='/')
	{
		/* if src does not contain a :, we do not have a drive string for sure */
		if ((ref1=strchr(src, ':')))
		{
			/* ref1 now points at the first occurance of : */

			/* the first occurance of /, should be the character after :, unless src only contains the drive */
			if (((ref1+1)==strchr(src, '/'))||(!ref1[1]))
			{
				if (drive)
				{
					len = ref1 - src + 1;
					*drive = malloc (len + 1);
					if (!*drive)
					{
						fprintf (stderr, "splitpath_malloc: *drive = malloc(%d) failed\n", len + 1);
						goto error_out;
					}
					memcpy (*drive, src, len);
					(*drive)[len] = 0;
				}
				src = ref1 + 1;
			}
		}
	}
	if (drive && !*drive)
	{
		*drive = strdup("");
		if (!*drive)
		{
			fprintf (stderr, "splitpath_malloc: *drive = strdup(\"\") failed\n");
			goto error_out;
		}
	}

	if (drive && *drive && *src!='/') /* Paths with drive should always start with / */
	{
		fprintf (stderr, "splitpath_malloc: PATH does not start with /\n");
		goto error_out;
	}

	if ((ref1=rindex(src, '/')))
	{
		if (path)
		{
			len = ref1 - src + 1;
			*path = malloc (len + 1);
			if (!*path)
			{
				fprintf (stderr, "splitpath_malloc: *path = malloc(%d) failed\n", len + 1);
				goto error_out;
			}
			memcpy (*path, src, len);
			(*path)[len] = 0;
		}
		src = ref1 + 1;
	}
	if (path && !*path)
	{
		*path = strdup("");
		if (!*path)
		{
			fprintf (stderr, "splitpath_malloc: *path = strdup(\"\") failed\n");
			goto error_out;
		}
	}


	len = strlen(src);

	ref1 = rindex (src, '.');
	if (!ref1)
	{
		ref1 = src + len;
	}
	if (file)
	{
		len = ref1 - src;
		*file = malloc (len + 1);
		if (!*file)
		{
			fprintf (stderr, "splitpath_malloc: *file = malloc(%d) failed\n", len + 1);
			goto error_out;
		}
		memcpy (*file, src, len);
		(*file)[len] = 0;
	}
	src = ref1;

	if (ext)
	{
		*ext = strdup(src);
		if (!*ext)
		{
			fprintf (stderr, "splitpath_malloc: *ext = strdup(\"%s\") failed\n", src);
			goto error_out;
		}
	}

	return 0;

error_out:
	/* free any data allocated, before we return an error */
	if (drive)
	{
		free(*drive);
		*drive = 0;
	}
	if (path)
	{
		free(*path);
		*path = 0;
	}
	if (file)
	{
		free (*file);
		 *file = 0;
	}
	if (ext)
	{
		free (*ext);
		*ext = 0;
	}
	return -1;
}

int splitpath_malloc(const char *src, char **drive, char **path, char **filename)
{
	/* returns non-zero on errors */
	const char *ref1;
	int len;

	/* clear the target points, so our error-path can free-up data */
	if (drive) *drive = 0;
	if (path) *path = 0;
	if (filename) *filename = 0;

	/* if src string starts with /, we do not have a drive string for sure */
	if (*src!='/')
	{
		/* if src does not contain a :, we do not have a drive string for sure */
		if ((ref1=strchr(src, ':')))
		{
			/* ref1 now points at the first occurance of : */

			/* the first occurance of /, should be the character after :, unless src only contains the drive */
			if (((ref1+1)==strchr(src, '/'))||(!ref1[1]))
			{
				if (drive)
				{
					len = ref1 - src + 1;
					*drive = malloc (len + 1);
					if (!*drive)
					{
						fprintf (stderr, "splitpath_malloc: *drive = malloc(%d) failed\n", len + 1);
						goto error_out;
					}
					memcpy (*drive, src, len);
					(*drive)[len] = 0;
				}
				src = ref1 + 1;
			}
		}
	}
	if (drive && !*drive)
	{
		*drive = strdup("");
		if (!*drive)
		{
			fprintf (stderr, "splitpath_malloc: *drive = strdup(\"\") failed\n");
			goto error_out;
		}
	}

	if (drive && *drive && *src!='/') /* Paths with drive should always start with / */
	{
		fprintf (stderr, "splitpath_malloc: PATH does not start with /\n");
		goto error_out;
	}

	if ((ref1=rindex(src, '/')))
	{
		if (path)
		{
			len = ref1 - src + 1;
			*path = malloc (len + 1);
			if (!*path)
			{
				fprintf (stderr, "splitpath_malloc: *path = malloc(%d) failed\n", len + 1);
				goto error_out;
			}
			memcpy (*path, src, len);
			(*path)[len] = 0;
		}
		src = ref1 + 1;
	}
	if (path && !*path)
	{
		*path = strdup("");
		if (!*path)
		{
			fprintf (stderr, "splitpath_malloc: *path = strdup(\"\") failed\n");
			goto error_out;
		}
	}


	if (filename)
	{
		*filename = strdup (src);
		if (!*filename)
		{
			fprintf (stderr, "splitpath_malloc: *filename = strdup(%s) failed\n", src);
			goto error_out;
		}
	}

	return 0;

error_out:
	/* free any data allocated, before we return an error */
	if (drive)
	{
		free(*drive);
		*drive = 0;
	}
	if (path)
	{
		free(*path);
		*path = 0;
	}
	if (filename)
	{
		free (*filename);
		 *filename = 0;
	}
	return -1;
}

int makepath_malloc(char **dst, const char *drive, const char *path, const char *file, const char *ext)
{
	/* returns non-zero on errors */
	unsigned int dstlen = 0;
	int pathadd = 0;

	*dst = 0;
	if (drive)
	{
		char *pos1, *pos2;
		dstlen+=strlen(drive);

		/* Assertion tests */
		if (strchr (drive, '/'))
		{
			fprintf (stderr, "makepath_malloc(): drive contains /\n");
			return -1;
		}
		if (!drive[0])
		{
			fprintf (stderr, "makepath_malloc(): drive is non-null, but zero bytes long\n");
			return -1;
		}
		if (drive[0] == ':')
		{
			fprintf (stderr, "makepath_malloc(): drive starts with :\n");
			return -1;
		}
		pos1 = index (drive, ':');
		pos2 = rindex (drive, ':');
		if (!pos1)
		{
			fprintf (stderr, "makepath_malloc(): drive does not contain:\n");
			return -1;
		}
		if (pos1 != pos2)
		{
			fprintf (stderr, "makepath_malloc(): drive contains multiple :\n");
			return -1;
		}
		if (pos1[1])
		{
			fprintf (stderr, "makepath_malloc(): drive does not end with :\n");
			return -1;
		}
	}

	if (path)
	{
		int pathlen = strlen(path);

		/* Assertion test */
		if ((path[0] != '/') && (path[0] != 0))
		{
			fprintf (stderr, "makepath_malloc(): path does not start with /\n"); /* we path to miss the final /, hence empty string is allowed */
			return -1;
		}

		dstlen += pathlen;
		if (path[pathlen-1] != '/')
		{
			dstlen += 1;
			pathadd++;
		}
	}

	if (file)
	{
		/* Assertion test */
		if (index (file, '/'))
		{
			fprintf (stderr, "makepath_malloc(): file contains /\n");
			return -1;
		}

		dstlen += strlen(file);
	}
	if (ext)
	{
		/* Assertion tests */
		if (index (ext, '/'))
		{
			fprintf (stderr, "makepath_malloc(): ext contains /\n");
			return -1;
		}
		if (ext[0] != '.')
		{
			fprintf (stderr, "makepath_malloc(): ext does not start with .\n");
			return -1;
		}

		dstlen += strlen (ext);
	}

	*dst = malloc (dstlen + 1);
	if (!*dst)
	{
		fprintf (stderr, "makepath_malloc: malloc(%d) failed\n", dstlen+1);
		return -1;
	}

	if (drive)
	{
		strcpy (*dst, drive);
	} else {
		(*dst)[0] = 0;
	}

	if (path)
	{
		strcat (*dst, path);
		if (pathadd)
		{
			strcat (*dst, "/");
		}
	}

	if (file)
	{
		strcat (*dst, file);
	}

	if (ext)
	{
		strcat (*dst, ext);
	}

	return 0;
}

#ifndef HAVE_STRUPR

char *strupr(char *src)
{
	char *retval = src;
	if (src)
	{
		while (*src)
		{
			*src=toupper(*src);
			src++;
		}
	}
	return retval;
}

#endif

#ifndef HAVE_MEMMEM
void *memmem(const void *haystack, size_t haystacklen,
             const void *needle, size_t needlelen)
{
	while (haystacklen>=needlelen)
	{
		if (!memcmp(haystack, needle, needlelen))
			return (void *)haystack;
		haystack++;
		haystacklen--;
	}
	return NULL;
}
#endif

#ifndef HAVE_VSNPRINTF
/* From glib-1.1.13:gstrfuncs.c
 * Modified by Masanao Izumo <mo@goice.co.jp>
 */
#ifndef VA_COPY
# if (defined (__GNUC__) && defined (__PPC__) && (defined (_CALL_SYSV) || defined (__WIN32__))) || defined (__WATCOMC__)
#  define VA_COPY(ap1, ap2)	  (*(ap1) = *(ap2))
# elif defined (VA_COPY_AS_ARRAY)
#  define VA_COPY(ap1, ap2)	  memmove ((ap1), (ap2), sizeof (va_list))
# else /* va_list is a pointer */
#  define VA_COPY(ap1, ap2)	  ((ap1) = (ap2))
# endif /* va_list is a pointer */
#endif /* !VA_COPY */

static int printf_string_upper_bound (const char* format,
			     va_list      args)
{
  int len = 1;

  while (*format)
    {
      int long_int = 0;
      int extra_long = 0;
      char c;

      c = *format++;

      if (c == '%')
	{
	  int done = 0;

	  while (*format && !done)
	    {
	      switch (*format++)
		{
		  char *string_arg;

		case '*':
		  len += va_arg (args, int);
		  break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		  /* add specified format length, since it might exceed the
		   * size we assume it to have.
		   */
		  format -= 1;
		  len += strtol (format, (char**) &format, 10);
		  break;
		case 'h':
		  /* ignore short int flag, since all args have at least the
		   * same size as an int
		   */
		  break;
		case 'l':
		  if (long_int)
		    extra_long = 1; /* linux specific */
		  else
		    long_int = 1;
		  break;
		case 'q':
		case 'L':
		  long_int = 1;
		  extra_long = 1;
		  break;
		case 's':
		  string_arg = va_arg (args, char *);
		  if (string_arg)
		    len += strlen (string_arg);
		  else
		    {
		      /* add enough padding to hold "(null)" identifier */
		      len += 16;
		    }
		  done = 1;
		  break;
		case 'd':
		case 'i':
		case 'o':
		case 'u':
		case 'x':
		case 'X':
#ifdef	G_HAVE_GINT64
		  if (extra_long)
		    (void) va_arg (args, gint64);
		  else
#endif	/* G_HAVE_GINT64 */
		    {
		      if (long_int)
			(void) va_arg (args, long);
		      else
			(void) va_arg (args, int);
		    }
		  len += extra_long ? 64 : 32;
		  done = 1;
		  break;
		case 'D':
		case 'O':
		case 'U':
		  (void) va_arg (args, long);
		  len += 32;
		  done = 1;
		  break;
		case 'e':
		case 'E':
		case 'f':
		case 'g':
#ifdef HAVE_LONG_DOUBLE
		  if (extra_long)
		    (void) va_arg (args, long double);
		  else
#endif	/* HAVE_LONG_DOUBLE */
		    (void) va_arg (args, double);
		  len += extra_long ? 64 : 32;
		  done = 1;
		  break;
		case 'c':
		  (void) va_arg (args, int);
		  len += 1;
		  done = 1;
		  break;
		case 'p':
		case 'n':
		  (void) va_arg (args, void*);
		  len += 32;
		  done = 1;
		  break;
		case '%':
		  len += 1;
		  done = 1;
		  break;
		default:
		  /* ignore unknow/invalid flags */
		  break;
		}
	    }
	}
      else
	len += 1;
    }

  return len;
}

int vsnprintf(char *buff, size_t bufsiz, const char *fmt, va_list ap)
{
    char *tmpbuf = buff;
    int ret;
    va_list ap2;

    VA_COPY(ap2, ap);
    init_mblock(&pool);
    tmpbuf = malloc(printf_string_upper_bound(fmt, ap));
    ret = vsprintf(tmpbuf, fmt, ap2);
    strncpy(buff, tmpbuf, bufsiz);
    buff[bufsiz-1] = '\0';
    free (tmpbuf);
    va_end(ap2);
    return ret;
}
#endif /* HAVE_VSNPRINTF */


#ifndef HAVE_SNPRINTF
int snprintf(char *buff, size_t bufsiz, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = vsnprintf(buff, bufsiz, fmt, ap);
    va_end(ap);
    return ret;
}
#endif /* HAVE_SNPRINTF */

#ifndef HAVE_STRERROR
#ifndef HAVE_ERRNO_H
char *strerror(int errnum) {
    static char s[32];
    sprintf(s, "ERROR %d", errnum);
    return s;
}
#else

char *strerror(int errnum)
{
    int i;
    static char s[32];
    struct {
	int id;
	char *str;
    } error_list[] = {
#ifdef EPERM
    {EPERM, "Not super-user"},
#endif /* EPERM */
#ifdef ENOENT
    {ENOENT, "No such file or directory"},
#endif /* ENOENT */
#ifdef ESRCH
    {ESRCH, "No such process"},
#endif /* ESRCH */
#ifdef EINTR
    {EINTR, "interrupted system call"},
#endif /* EINTR */
#ifdef EIO
    {EIO, "I/O error"},
#endif /* EIO */
#ifdef ENXIO
    {ENXIO, "No such device or address"},
#endif /* ENXIO */
#ifdef E2BIG
    {E2BIG, "Arg list too long"},
#endif /* E2BIG */
#ifdef ENOEXEC
    {ENOEXEC, "Exec format error"},
#endif /* ENOEXEC */
#ifdef EBADF
    {EBADF, "Bad file number"},
#endif /* EBADF */
#ifdef ECHILD
    {ECHILD, "No children"},
#endif /* ECHILD */
#ifdef EAGAIN
    {EAGAIN, "Resource temporarily unavailable"},
#endif /* EAGAIN */
#ifdef EWOULDBLOCK
    {EWOULDBLOCK, "Resource temporarily unavailable"},
#endif /* EWOULDBLOCK */
#ifdef ENOMEM
    {ENOMEM, "Not enough core"},
#endif /* ENOMEM */
#ifdef EACCES
    {EACCES, "Permission denied"},
#endif /* EACCES */
#ifdef EFAULT
    {EFAULT, "Bad address"},
#endif /* EFAULT */
#ifdef ENOTBLK
    {ENOTBLK, "Block device required"},
#endif /* ENOTBLK */
#ifdef EBUSY
    {EBUSY, "Mount device busy"},
#endif /* EBUSY */
#ifdef EEXIST
    {EEXIST, "File exists"},
#endif /* EEXIST */
#ifdef EXDEV
    {EXDEV, "Cross-device link"},
#endif /* EXDEV */
#ifdef ENODEV
    {ENODEV, "No such device"},
#endif /* ENODEV */
#ifdef ENOTDIR
    {ENOTDIR, "Not a directory"},
#endif /* ENOTDIR */
#ifdef EISDIR
    {EISDIR, "Is a directory"},
#endif /* EISDIR */
#ifdef EINVAL
    {EINVAL, "Invalid argument"},
#endif /* EINVAL */
#ifdef ENFILE
    {ENFILE, "File table overflow"},
#endif /* ENFILE */
#ifdef EMFILE
    {EMFILE, "Too many open files"},
#endif /* EMFILE */
#ifdef ENOTTY
    {ENOTTY, "Inappropriate ioctl for device"},
#endif /* ENOTTY */
#ifdef ETXTBSY
    {ETXTBSY, "Text file busy"},
#endif /* ETXTBSY */
#ifdef EFBIG
    {EFBIG, "File too large"},
#endif /* EFBIG */
#ifdef ENOSPC
    {ENOSPC, "No space left on device"},
#endif /* ENOSPC */
#ifdef ESPIPE
    {ESPIPE, "Illegal seek"},
#endif /* ESPIPE */
#ifdef EROFS
    {EROFS, "Read only file system"},
#endif /* EROFS */
#ifdef EMLINK
    {EMLINK, "Too many links"},
#endif /* EMLINK */
#ifdef EPIPE
    {EPIPE, "Broken pipe"},
#endif /* EPIPE */
#ifdef EDOM
    {EDOM, "Math arg out of domain of func"},
#endif /* EDOM */
#ifdef ERANGE
    {ERANGE, "Math result not representable"},
#endif /* ERANGE */
#ifdef ENOMSG
    {ENOMSG, "No message of desired type"},
#endif /* ENOMSG */
#ifdef EIDRM
    {EIDRM, "Identifier removed"},
#endif /* EIDRM */
#ifdef ECHRNG
    {ECHRNG, "Channel number out of range"},
#endif /* ECHRNG */
#ifdef EL2NSYNC
    {EL2NSYNC, "Level 2 not synchronized"},
#endif /* EL2NSYNC */
#ifdef EL3HLT
    {EL3HLT, "Level 3 halted"},
#endif /* EL3HLT */
#ifdef EL3RST
    {EL3RST, "Level 3 reset"},
#endif /* EL3RST */
#ifdef ELNRNG
    {ELNRNG, "Link number out of range"},
#endif /* ELNRNG */
#ifdef EUNATCH
    {EUNATCH, "Protocol driver not attached"},
#endif /* EUNATCH */
#ifdef ENOCSI
    {ENOCSI, "No CSI structure available"},
#endif /* ENOCSI */
#ifdef EL2HLT
    {EL2HLT, "Level 2 halted"},
#endif /* EL2HLT */
#ifdef EDEADLK
    {EDEADLK, "Deadlock condition."},
#endif /* EDEADLK */
#ifdef ENOLCK
    {ENOLCK, "No record locks available."},
#endif /* ENOLCK */
#ifdef ECANCELED
    {ECANCELED, "Operation canceled"},
#endif /* ECANCELED */
#ifdef ENOTSUP
    {ENOTSUP, "Operation not supported"},
#endif /* ENOTSUP */
#ifdef EDQUOT
    {EDQUOT, "Disc quota exceeded"},
#endif /* EDQUOT */
#ifdef EBADE
    {EBADE, "invalid exchange"},
#endif /* EBADE */
#ifdef EBADR
    {EBADR, "invalid request descriptor"},
#endif /* EBADR */
#ifdef EXFULL
    {EXFULL, "exchange full"},
#endif /* EXFULL */
#ifdef ENOANO
    {ENOANO, "no anode"},
#endif /* ENOANO */
#ifdef EBADRQC
    {EBADRQC, "invalid request code"},
#endif /* EBADRQC */
#ifdef EBADSLT
    {EBADSLT, "invalid slot"},
#endif /* EBADSLT */
#ifdef EDEADLOCK
    {EDEADLOCK, "file locking deadlock error"},
#endif /* EDEADLOCK */
#ifdef EBFONT
    {EBFONT, "bad font file fmt"},
#endif /* EBFONT */
#ifdef ENOSTR
    {ENOSTR, "Device not a stream"},
#endif /* ENOSTR */
#ifdef ENODATA
    {ENODATA, "no data (for no delay io)"},
#endif /* ENODATA */
#ifdef ETIME
    {ETIME, "timer expired"},
#endif /* ETIME */
#ifdef ENOSR
    {ENOSR, "out of streams resources"},
#endif /* ENOSR */
#ifdef ENONET
    {ENONET, "Machine is not on the network"},
#endif /* ENONET */
#ifdef ENOPKG
    {ENOPKG, "Package not installed"},
#endif /* ENOPKG */
#ifdef EREMOTE
    {EREMOTE, "The object is remote"},
#endif /* EREMOTE */
#ifdef ENOLINK
    {ENOLINK, "the link has been severed"},
#endif /* ENOLINK */
#ifdef EADV
    {EADV, "advertise error"},
#endif /* EADV */
#ifdef ESRMNT
    {ESRMNT, "srmount error"},
#endif /* ESRMNT */
#ifdef ECOMM
    {ECOMM, "Communication error on send"},
#endif /* ECOMM */
#ifdef EPROTO
    {EPROTO, "Protocol error"},
#endif /* EPROTO */
#ifdef EMULTIHOP
    {EMULTIHOP, "multihop attempted"},
#endif /* EMULTIHOP */
#ifdef EBADMSG
    {EBADMSG, "trying to read unreadable message"},
#endif /* EBADMSG */
#ifdef ENAMETOOLONG
    {ENAMETOOLONG, "path name is too long"},
#endif /* ENAMETOOLONG */
#ifdef EOVERFLOW
    {EOVERFLOW, "value too large to be stored in data type"},
#endif /* EOVERFLOW */
#ifdef ENOTUNIQ
    {ENOTUNIQ, "given log. name not unique"},
#endif /* ENOTUNIQ */
#ifdef EBADFD
    {EBADFD, "f.d. invalid for this operation"},
#endif /* EBADFD */
#ifdef EREMCHG
    {EREMCHG, "Remote address changed"},
#endif /* EREMCHG */
#ifdef ELIBACC
    {ELIBACC, "Can't access a needed shared lib."},
#endif /* ELIBACC */
#ifdef ELIBBAD
    {ELIBBAD, "Accessing a corrupted shared lib."},
#endif /* ELIBBAD */
#ifdef ELIBSCN
    {ELIBSCN, ".lib section in a.out corrupted."},
#endif /* ELIBSCN */
#ifdef ELIBMAX
    {ELIBMAX, "Attempting to link in too many libs."},
#endif /* ELIBMAX */
#ifdef ELIBEXEC
    {ELIBEXEC, "Attempting to exec a shared library."},
#endif /* ELIBEXEC */
#ifdef EILSEQ
    {EILSEQ, "Illegal byte sequence."},
#endif /* EILSEQ */
#ifdef ENOSYS
    {ENOSYS, "Unsupported file system operation"},
#endif /* ENOSYS */
#ifdef ELOOP
    {ELOOP, "Symbolic link loop"},
#endif /* ELOOP */
#ifdef ERESTART
    {ERESTART, "Restartable system call"},
#endif /* ERESTART */
#ifdef ESTRPIPE
    {ESTRPIPE, "if pipe/FIFO, don't sleep in stream head"},
#endif /* ESTRPIPE */
#ifdef ENOTEMPTY
    {ENOTEMPTY, "directory not empty"},
#endif /* ENOTEMPTY */
#ifdef EUSERS
    {EUSERS, "Too many users (for UFS)"},
#endif /* EUSERS */
#ifdef ENOTSOCK
    {ENOTSOCK, "Socket operation on non-socket"},
#endif /* ENOTSOCK */
#ifdef EDESTADDRREQ
    {EDESTADDRREQ, "Destination address required"},
#endif /* EDESTADDRREQ */
#ifdef EMSGSIZE
    {EMSGSIZE, "Message too long"},
#endif /* EMSGSIZE */
#ifdef EPROTOTYPE
    {EPROTOTYPE, "Protocol wrong type for socket"},
#endif /* EPROTOTYPE */
#ifdef ENOPROTOOPT
    {ENOPROTOOPT, "Protocol not available"},
#endif /* ENOPROTOOPT */
#ifdef EPROTONOSUPPORT
    {EPROTONOSUPPORT, "Protocol not supported"},
#endif /* EPROTONOSUPPORT */
#ifdef ESOCKTNOSUPPORT
    {ESOCKTNOSUPPORT, "Socket type not supported"},
#endif /* ESOCKTNOSUPPORT */
#ifdef EOPNOTSUPP
    {EOPNOTSUPP, "Operation not supported on socket"},
#endif /* EOPNOTSUPP */
#ifdef EPFNOSUPPORT
    {EPFNOSUPPORT, "Protocol family not supported"},
#endif /* EPFNOSUPPORT */
#ifdef EAFNOSUPPORT
    {EAFNOSUPPORT, "Address family not supported by"},
#endif /* EAFNOSUPPORT */
#ifdef EADDRINUSE
    {EADDRINUSE, "Address already in use"},
#endif /* EADDRINUSE */
#ifdef EADDRNOTAVAIL
    {EADDRNOTAVAIL, "Can't assign requested address"},
#endif /* EADDRNOTAVAIL */
#ifdef ENETDOWN
    {ENETDOWN, "Network is down"},
#endif /* ENETDOWN */
#ifdef ENETUNREACH
    {ENETUNREACH, "Network is unreachable"},
#endif /* ENETUNREACH */
#ifdef ENETRESET
    {ENETRESET, "Network dropped connection because"},
#endif /* ENETRESET */
#ifdef ECONNABORTED
    {ECONNABORTED, "Software caused connection abort"},
#endif /* ECONNABORTED */
#ifdef ECONNRESET
    {ECONNRESET, "Connection reset by peer"},
#endif /* ECONNRESET */
#ifdef ENOBUFS
    {ENOBUFS, "No buffer space available"},
#endif /* ENOBUFS */
#ifdef EISCONN
    {EISCONN, "Socket is already connected"},
#endif /* EISCONN */
#ifdef ENOTCONN
    {ENOTCONN, "Socket is not connected"},
#endif /* ENOTCONN */
#ifdef ESHUTDOWN
    {ESHUTDOWN, "Can't send after socket shutdown"},
#endif /* ESHUTDOWN */
#ifdef ETOOMANYREFS
    {ETOOMANYREFS, "Too many references: can't splice"},
#endif /* ETOOMANYREFS */
#ifdef ETIMEDOUT
    {ETIMEDOUT, "Connection timed out"},
#endif /* ETIMEDOUT */
#ifdef ECONNREFUSED
    {ECONNREFUSED, "Connection refused"},
#endif /* ECONNREFUSED */
#ifdef EHOSTDOWN
    {EHOSTDOWN, "Host is down"},
#endif /* EHOSTDOWN */
#ifdef EHOSTUNREACH
    {EHOSTUNREACH, "No route to host"},
#endif /* EHOSTUNREACH */
#ifdef EALREADY
    {EALREADY, "operation already in progress"},
#endif /* EALREADY */
#ifdef EINPROGRESS
    {EINPROGRESS, "operation now in progress"},
#endif /* EINPROGRESS */
#ifdef ESTALE
    {ESTALE, "Stale NFS file handle"},
#endif /* ESTALE */
    {0, NULL}};

    for(i = 0; error_list[i].str != NULL; i++)
	if(error_list[i].id == errnum)
	    return error_list[i].str;
    sprintf(s, "ERROR %d", errnum);
    return s;
}
#endif /* HAVE_ERRNO_H */
#endif /* HAVE_STRERROR */

#ifndef HAVE_USLEEP
#ifdef __W32__
int usleep(unsigned int usec)
{
    Sleep(usec / 1000);
    return 0;
}
#elif __MACOS__
int usleep(unsigned int /*usec*/)
{
    YieldToAnyThread();
    return 0;
}
#else
int usleep(unsigned int usec)
{
    struct timeval tv;
    tv.tv_sec  = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    select(0, NULL, NULL, NULL, &tv);
    return 0;
}
#endif /* __W32__ */
#endif /* HAVE_USLEEP */

#ifndef HAVE_STRDUP
char *strdup(const char *s)
{
    size_t len;
    char *p;

    len = strlen(s);
    if((p = (char *)safe_malloc(len + 1)) == NULL)
	return NULL;
    return strcpy(p, s);
}
#endif /* HAVE_STRDUP */


#ifndef HAVE_GETCWD
char *getcwd_malloc (void)
{
#ifdef MAXPATHLEN
	char path[MAXPATHLEN+1];
#elif defined(PATH_MAX)
	char path[PATH_MAX+1];
#elif defined(_MAX_PATH)
	char path[_MAX_PATH+1];
#else
	char path[4097];
#endif /* MAXPATHLEN */
	if(getwd(path) == NULL)
	{
		strcpy(path, "/");
	}
	return strdup(path);
}
#else
char *getcwd_malloc (void)
{
	int len = 4096; /* PATH_MAX on many systems */
	char *currentpath;

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
	return currentpath;
}
#endif


#ifndef HAVE_STRNCASECMP
int strncasecmp(char *s1, char *s2, unsigned int len) {
  int dif;
  while (len-- > 0) {
	if ((dif =
		 (unsigned char)tolower(*s1) - (unsigned char)tolower(*s2++)) != 0)
	  return(dif);
	if (*s1++ == '\0')
	  break;
  }
  return (0);
}
#endif /* HAVE_STRNCASECMP */

#ifndef HAVE_SYS_STAT_H
#ifdef __MACOS__
int stat(const char *filename, struct stat *st)
{
	Str255				pfilename;
	CInfoPBRec			pb;
	FSSpec				fss;

	c2pstrcpy(pfilename, filename);
	if (FSMakeFSSpec(0, 0, pfilename, &fss) == noErr)
	{
		pb.hFileInfo.ioNamePtr = fss.name;
		pb.hFileInfo.ioVRefNum = fss.vRefNum;
		pb.hFileInfo.ioFDirIndex = 0;
		pb.hFileInfo.ioDirID = fss.parID;
		if (PBGetCatInfoSync(&pb) == noErr)
		{
			st->st_mode = (pb.hFileInfo.ioFlAttrib & kioFlAttribDirMask) ? S_IFDIR : 0;
			st->st_dev = pb.hFileInfo.ioVRefNum;
			st->st_ino = pb.hFileInfo.ioDirID;
			st->st_size = pb.hFileInfo.ioFlLgLen;
			st->st_mtime = pb.hFileInfo.ioFlMdDat;
			st->st_ctime = pb.hFileInfo.ioFlCrDat;
			st->st_btime = pb.hFileInfo.ioFlBkDat;
			return 0;
		}
	}
	st->st_mode = 0;
	st->st_dev = 0;
	st->st_ino = 0;
	st->st_size = 0;
	st->st_mtime = st->st_ctime = st->st_btime = 0;
	errno = EIO;
	return -1;
}
#endif /* __MACOS__ */
#endif /* HAVE_SYS_STAT_H */

#ifndef HAVE_STRLCPY
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#include <string.h>

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}
#endif  /* strlcpy() */

#ifndef HAVE_STRLCAT
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#include <string.h>

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t
strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}
#endif  /* strlcat() */

#ifdef __DMC__
int strcasecmp(const char *s1, const char *s2)
{
	int	c1,c2,i;

	for( i=0; ; i++){
		if( !s1[i] && !s2[i] ) return 0; //equal
		if( !s1[i] || !s2[i] ) return 1;
		c1= ( isupper(s1[i]) ? tolower(s1[i]) : s1[i] );
		c2= ( isupper(s2[i]) ? tolower(s2[i]) : s2[i] );
		if( c1 != c2 )	return 1;
	}
	return 0; //equal
}
#endif

void strreplace (char *dst, char old, char replacement)
{
	while (*dst)
	{
		if (dst[0] == old)
		{
			dst[0] = replacement;
		}
		dst++;
	}
}

/* Detect MacOS / OS-X */
#if defined(__APPLE__) && defined(__MACH__)
# if !defined(MAC_OS_X_VERSION_10_12) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
#  include <mach/clock.h>
#  include <mach/mach_host.h>
int clock_gettime(clockid_t clk_id, struct timespec *tv)
{
	int retval;
	static clock_serv_t cclock;
	static int initialized = 0;
	mach_timespec_t mts;

	if (!initialized)
	{
		host_get_clock_service(mach_host_self(), clk_id, &cclock);
		initialized = 1;
	}
	retval = clock_get_time(cclock, &mts);
	if (retval)
	{
		tv->tv_sec = 0;
		tv->tv_nsec = 0;
		return retval;
	}
	//mach_port_deallocate(mach_task_self(), cclock);

	tv->tv_sec = mts.tv_sec;
	tv->tv_nsec = mts.tv_nsec;

	return 0;
}
# endif
#endif
