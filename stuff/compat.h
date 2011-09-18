#ifndef _COMPAT_H
#define _COMPAT_H

#include <sys/time.h>

#define DOS_CLK_TCK 0x10000

extern time_t dos_clock(void);
extern void _splitpath(const char *src, char *drive, char *path, char *file, char *ext);
extern void _makepath(char *dst /* PATH_MAX+1 */, const char *drive, const char *path, const char *file, const char *ext);

#ifndef HAVE_STRUPR
extern char *strupr(char *src);
#else
#include <string.h>
#endif

extern size_t filelength(int fd);
extern size_t _filelength(const char *path);
extern int memicmp(const void *s1, const void *s2, size_t n);

#endif
