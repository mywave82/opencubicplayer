/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "compat.h"

int memicmp(const void *_s1, const void *_s2, size_t n)
{
	const uint8_t *s1=_s1;
	const uint8_t *s2=_s2;
	size_t i;
	for (i=0;i<n;i++,s1++,s2++)
	{
		unsigned char u1=toupper(*s1);
		unsigned char u2=toupper(*s2);
		if (u1<u2) return -1;
		if (u2<u1) return 1;
	}
	return 0;
}

time_t dos_clock(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec*0x10000+t.tv_usec*1024/15625;
}

void _splitpath(const char *src, char *drive, char *path, char *file, char *ext)
/*                             , NAME_MAX+1,  PATH_MAX+1, NAME_MAX+1, NAME_MAX+1
                                 drive    :  /path/       name.egon   .tar.gz    */
{
	int len=0;
	const char *ref1;
	if (*src!='/')
	if ((ref1=strchr(src, ':')))
	{
		if (((ref1+1)==strchr(src, '/'))||(!ref1[1]))
		while (src<=ref1)
		{
			if (drive)
			{
				len++;
				if (len<=NAME_MAX)
				{
					*drive=*src;
					drive++;
				}
			}
			if (*src==':')
			{
				src++;
				break;
			}
			src++;
		}
	}
	len=0;
	if ((ref1=rindex(src, '/')))
	while (src<=ref1)
	{
		if (path)
		{
			len++;
			if (len<=PATH_MAX)
			{
				*path=*src;
				path++;
			}
		}
		src++;
	}
	len=0;
	if (!(ref1=rindex(src, '.')))
		ref1=src+strlen(src);
	while (/*(*src)*/(src<ref1))
	{
/*
		if (!strchr(src+1, '.'))
			break;
*/
		if (!strcasecmp(src, ".tar.gz")) /* I am a bad boy */
			break;
		if (!strcasecmp(src, ".tar.bz2")) /* very bad */
			break;
		if (!strcasecmp(src, ".tar.Z")) /* and this is creepy */
			break;
		if (file)
		{
			len++;
			if (len<=NAME_MAX)
			{
				*file=*src;
				file++;
			}
		}
		src++;
	}
	len=0;
	while (*src)
	{
		if (ext)
		{
			len++;
			if (len<=NAME_MAX)
			{
				*ext=*src;
				ext++;
			}
		}
		src++;
	}
	if (drive)
		*drive=0;
	if (path)
		*path=0;
	if (file)
		*file=0;
	if (ext)
		*ext=0;
}

void _makepath(char *dst, const char *drive, const char *path, const char *file, const char *ext)
{
	unsigned int left=PATH_MAX;
	unsigned int cache;
	*dst=0;
	if (drive)
	{
		cache=strlen(drive);
		if (cache<=left)
		{
			strcat(dst, drive);
			left-=cache;
		}
	}
	if (path)
	{
		cache=strlen(path);
		if (cache<=left)
		{
			strcat(dst, path);
			left-=cache;
			if (left)
				if (dst[PATH_MAX-left-1]!='/')
				{
					strcat(dst, "/");
					left--;
				}
		}
	}
	if (file)
	{
		cache=strlen(file);
		if (cache<=left)
		{
			strcat(dst, file);
			left-=cache;
		}
	}
	if (ext)
		if (strlen(ext)<=left)
			strcat(dst, ext);
}

#ifndef HAVE_STRUPR

char *strupr(char *src)
{
	char *retval = src;
	while (*src)
	{
		*src=toupper(*src);
		src++;
	}
	return retval;
}

#endif

size_t filelength(int fd)
{
	off_t cur=lseek(fd, 0, SEEK_CUR);
	size_t retval;
	lseek(fd, 0, SEEK_END);
	retval=lseek(fd, 0, SEEK_CUR);
	lseek(fd, cur, SEEK_SET);
	return retval;
}

size_t _filelength(const char *path)
{
	struct stat st;
	if (stat(path, &st))
		return 0;
	return st.st_size;
}

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

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "compat", .desc = "OpenCP DOS compatible function-wrappers (c) 2004-09 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
