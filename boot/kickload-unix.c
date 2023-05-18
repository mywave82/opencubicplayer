/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * The little program that loads up the basic libs and starts main. It can also
 * start up the gdb debugger on fatal signals.
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
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss042408   Stian Skjelstad <stian@nixia.no>
 *    -added some setuid limits
 *    -create $HOME/.ocp/ocp.ini
 *    -setup cfProgramDir and cfConfigDir into libocp.so
 *  -ss040831   Stian Skjelstad <stian@nixia.no>
 *    -safety added when ran setuid
 *  -ss040914   Stian Skjelstad <stian@nixia.no>
 *    -use execvp on "gdb" aswell and honor $PATH
 *  -ss040924   Stian Skjelstad <stian@nixia.no>
 *    -get out of graphicmode on signals
 *  -ryb050118  Roman Y. Bogdanov <sam@brj.pp.ru>
 *    -bsd updates
 */

#include "config.h"
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef __HAIKU__
#include <FindDirectory.h>
#endif
#include "types.h"
#include "boot/pmain.h"
#include "boot/console.h"
#include "stuff/poutput.h"

static char *_cfConfigHomeDir;
static char *_cfDataHomeDir;
static char *_cfDataDir;
static char *_cfProgramDir;
static char *_cfHomeDir;

static int AllowSymlinked;
static struct console_t *_Console;

/* This has todo with video-mode rescue */
static int crashmode;

static inline void eintr_close(int fd)
{
	while (close(fd))
	{
		if (errno!=EINTR)
		{
			perror(__FILE__ " close()");
			break;
		}
	}
}

static inline void eintr_waitpid(pid_t pid, int *status, int options)
{
	while (waitpid(pid, status, options)<0)
	{
		if (errno!=EINTR)
		{
			perror(__FILE__ " waitpid()");
			break;
		}
	}
}

static inline void eintr_dup(int fd)
{
	while (dup(fd)<0)
	{
		if (errno!=EINTR)
		{
			perror(__FILE__ " dup()");
			break;
		}
	}
}

static void stopstuff(struct itimerval *i)
{
	struct itimerval z;

	memset (&z, 0, sizeof(z));
	setitimer(ITIMER_REAL, &z, &i[0]);
	setitimer(ITIMER_VIRTUAL, &z, &i[1]);
	setitimer(ITIMER_PROF, &z, &i[2]);
	crashmode = _Console->CurrentMode;
	_Console->Driver->SetTextMode ((unsigned char)255);
}
static void restartstuff(struct itimerval *i)
{
	setitimer(ITIMER_REAL, &i[0], 0);
	setitimer(ITIMER_VIRTUAL, &i[1], 0);
	setitimer(ITIMER_PROF, &i[2], 0);
	if (crashmode==101)
	{
		_Console->Driver->SetGraphMode ((unsigned char)1);
	} else if (crashmode==100)
	{
		_Console->Driver->SetGraphMode ((unsigned char)0);
	} else {
		_Console->Driver->SetTextMode ((unsigned char)crashmode);
	}
}

static const char *locate_ocp_ini_try(const char *base)
{
	struct stat st;
	if (!stat(base, &st))
		return base;
	return NULL;
}

static const char *locate_ocp_ini(void)
{
	const char *retval;
	const char *temp;
#warning need to search XDG_DATA_DIRS too ?
	if ((temp=getenv("CPDIR")))
	{
/*
		if (!AllowSymlinked)
		{
			fprintf(stderr, "Using $CPDIR when running setuid is not allowed\n");
		} else*/if ((retval=locate_ocp_ini_try(temp)))
				return retval;
	}
	if ((retval=locate_ocp_ini_try(DATADIROCP "/etc/"     "ocp.ini")))
		return retval;
	if ((retval=locate_ocp_ini_try(DATADIR "/share/ocp/"  "ocp.ini")))
		return retval;
	if ((retval=locate_ocp_ini_try(PREFIX "/etc/ocp/"     "ocp.ini")))
		return retval;
	if ((retval=locate_ocp_ini_try(PREFIX "/etc/"         "ocp.ini")))
		return retval;
	if ((retval=locate_ocp_ini_try("/etc/ocp/"            "ocp.ini")))
		return retval;
	if ((retval=locate_ocp_ini_try("/etc/"                "ocp.ini")))
		return retval;
	return NULL;
}

static char *locate_ocp_hlp_try(const char *base)
{
	char *buffer;
	int size = strlen (base) + 1 + 8;
	struct stat st;
	buffer = malloc (size);
	snprintf(buffer, size, "%s%s%s", base, (base[strlen(base)-1] == '/') ? "" : "/", "ocp.hlp");
	if (!stat(buffer, &st))
	{
		free (buffer);
		return strdup(base);
	}
	free (buffer);
	return NULL;
}

static char *locate_ocp_hlp(void)
{
	char *retval;
	const char *temp;
	if ((temp=getenv("OCPDIR")))
		if ((retval=locate_ocp_hlp_try(temp)))
			return retval;
	if ((retval=locate_ocp_hlp_try(DATADIROCP "/")))
		return retval;
	if ((retval=locate_ocp_hlp_try(DATADIROCP "/data/")))
		return retval;
	if ((retval=locate_ocp_hlp_try(LIBDIROCP)))
		return retval;
	return NULL;
}

static void reset(void)
{
	pid_t pid;
	const char *argv[5];

	if (!(pid=fork()))
	{
		if (!isatty(2))
		{
			eintr_close(2);
			eintr_dup(1);
		}
		argv[1]=NULL;
		argv[0]="reset";
		execvp("reset", (char **)argv);
		fprintf(stderr, "Failed to exec reset\n");
		exit(EXIT_FAILURE);
	} else if (pid>0) /* not an error */
	{
		int status;
		eintr_waitpid(pid, &status, 0);
	}

	if (!(pid=fork()))
	{
		argv[1]=NULL;
		argv[0]="clear";
		(void)execvp("clear", (char * const *)argv);
		exit(EXIT_FAILURE);
	} else if (pid>0) /* not an error */
	{
		int status;
		eintr_waitpid(pid, &status, 0);
	}
}

#if defined(__linux)
static void dumpcontext(int signal, struct sigcontext r)
#else
static void dumpcontext(int signal)
#endif
{
	if (signal==SIGSEGV)
		fprintf(stderr, "Segmentation Fault\n");
	else if (signal==SIGILL)
		fprintf(stderr, "Illegal Instruction\n");
	else if (signal==SIGBUS)
		fprintf(stderr, "Bus Error\n");
	else if (signal==SIGFPE)
		fprintf(stderr, "Division by zero / Floating Point Error\n");
	else if (signal==SIGINT)
		fprintf(stderr, "User pressed ctrl-C\n");
	else {
		fprintf(stderr, "Unknown fault\n");
		fprintf(stderr, "signal=%d\n", signal);
	}

#if defined(__linux) && ( defined(_X86) || defined(__i386__) )
	fprintf(stderr, "eax=0x%08lx ebx=0x%08lx ecx=0x%08lx edx=0x%08lx\n", r.eax, r.ebx, r.ecx, r.edx);
	fprintf(stderr, "edi=0x%08lx esi=0x%08lx ebp=0x%08lx esp=0x%08lx\n", r.edi, r.esi, r.ebp, r.esp_at_signal);
	fprintf(stderr, "cs=0x%04x ds=0x%04x es=0x%04x fs=0x%04x gs=0x%04x ss=0x%04x\n", r.cs, r.ds, r.es, r.fs, r.gs, r.ss);
	fprintf(stderr, "eip=0x%08lx\n", r.eip);
	fprintf(stderr, "eflags=0x%08lx\n", r.eflags);
	fprintf(stderr, "err=%ld trapno=0x%08lx cr2=0x%08lx oldmask=0x%08lx\n", r.err, r.trapno, r.cr2, r.oldmask);
	fprintf(stderr, "\n");
#elif defined (__linux) &&  defined(__x86_64__)
	fprintf(stderr, "rax=0x%016"PRIx64" rbx=0x%016"PRIx64" rcx=0x%016"PRIx64" rdx=0x%016"PRIx64"\n", r.rax, r.rbx, r.rcx, r.rdx);
	fprintf(stderr, "rdi=0x%016"PRIx64" rsi=0x%016"PRIx64" rbp=0x%016"PRIx64" rsp=0x%016"PRIx64"\n", r.rdi, r.rsi, r.rbp, r.rsp);
	fprintf(stderr, " r8=0x%016"PRIx64"  r9=0x%016"PRIx64" r10=0x%016"PRIx64" r11=0x%016"PRIx64"\n", r.r8, r.r9, r.r10, r.r11);
	fprintf(stderr, "r12=0x%016"PRIx64" r13=0x%016"PRIx64" r14=0x%016"PRIx64" r15=0x%016"PRIx64"\n", r.r12, r.r13, r.r14, r.r15);
	fprintf(stderr, "cs=0x%04x fs=0x%04x gs=0x%04x\n", r.cs, r.fs, r.gs);
	fprintf(stderr, "eip=0x%016"PRIx64"\n", r.rip);
	fprintf(stderr, "eflags=0x%016"PRIx64"\n", r.eflags);
	fprintf(stderr, "err=%"PRIu64" trapno=0x016%"PRIx64" cr2=0x%016"PRIx64" oldmask=0x%016"PRIx64"\n", r.err, r.trapno, r.cr2, r.oldmask);
#endif

	exit (0);
}

#if defined(__linux)
static void sigsegv(int signal, struct sigcontext r)
#else
static void sigsegv(int signal)
#endif
{
	struct itimerval i[3];

	stopstuff(i);

	/* loose setuid stuff before we launch any other external tools */
	if (getegid()!=getgid())
	{
		if (setegid(getgid()))
		{
			perror ("warning: setegid(getgid())");
		}
	}

	if (geteuid()!=getuid())
	{
		if (seteuid(getuid()))
		{
			perror ("warning: seteuid(getuid())");
		}
	}

	reset();
#if defined(__linux)
	dumpcontext(signal, r);
#else
	dumpcontext(signal);
#endif

	if (signal!=SIGINT)
		exit(signal);
	else
		restartstuff(i);
}

static int cp(const char *src, const char *dst)
{
	int _errno;
	int srcfd, dstfd;
	char buffer[65536];
	ssize_t retval;
	if ((srcfd=open(src, O_RDONLY))<0)
		return -1;
	if ((dstfd=open(dst, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))<0)
	{
		_errno=errno;
		eintr_close(srcfd);
		errno=_errno;
		return -1;
	}
	while ((retval=read(srcfd, buffer, (size_t)65536))>0)
	{
		if (write(dstfd, buffer, (size_t)retval)!=retval)
		{
			_errno=errno;
			eintr_close(srcfd);
			eintr_close(dstfd);
			errno=_errno;
			return -1;
		}
	}
	if (retval<0)
	{
		_errno=errno;
		eintr_close(srcfd);
		eintr_close(dstfd);
		errno=_errno;
		return -1;
	}
	eintr_close(srcfd);
	eintr_close(dstfd);
	return 0;
}

static char *validate_xdg_dir_absolute (const char *name, const char *def)
{
	char *xdg = getenv (name);
	if (xdg)
	{
		if (xdg[0] == 0) /* it is an empty string */
		{
			xdg = 0; /* silent ignore */
		} else if ((xdg[0] != '/') ||      /* not absolute, must start with '/' */
		           strstr(xdg, "/../") ||  /* not absolute, contains "/../" */
		           ((strlen (xdg) >= 3) && (!strcmp (xdg + strlen(xdg) - 3, "/.."))) ) /* not absolute, ends with "/.." */
		{
			fprintf (stderr, "Warning, $%s is not an absolute path, ignoring value\n", name);
			xdg = 0;
		}
	}
	if (xdg)
	{
		char *retval = malloc (strlen (xdg) + 5);
		if (retval)
		{
			sprintf (retval, "%s%socp/",
				xdg,
				( xdg [ strlen(xdg) - 1 ] != '/' ) ? "/" : "" /* ensure that path ends with '/', but at the same that we only have one */
			);
		}
		return retval;
	}
	xdg = malloc (strlen(_cfHomeDir) + strlen (def) + 5 + 1);
	sprintf (xdg, "%s%s/ocp/",
		_cfHomeDir,
		def
	);
	return xdg;
}

static int mkdir_r (char *path)
{
	char *next;
	next = strchr (path + 1, '/');
	while (1)
	{
		struct stat st;
		if (next)
		{
			*next = 0;
		}

		if (stat (path, &st))
		{
			if (errno == ENOENT)
			{
				if (mkdir (path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) /* 755 */
				{
					fprintf (stderr, "Failed to create %s\n", path);
					goto failout;
				}
			} else {
				fprintf (stderr, "Failed to stat %s: %s\n", path, strerror (errno));
				goto failout;
			}
		} else {
			if ((st.st_mode & S_IFMT) != S_IFDIR)
			{
				fprintf (stderr, "%s is not a directory\n", path);
				goto failout;
			}
		}
		if (next)
		{
			*next = '/';
			next = strchr (next + 1, '/');
		} else {
			break;
		}
	}
	return 0;
failout:
	if (next)
	{
		*next = '/';
	}
	return -1;
}

static int rename_exdev (const char *oldpath, const char *newpath)
{
	struct stat st;
	if (lstat (oldpath, &st))
	{
		fprintf (stderr, "stat(%s) failed: %s\n", oldpath, strerror (errno));
		return -1;
	}
	switch (st.st_mode & S_IFMT)
	{
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
		case S_IFSOCK:
		default:
			break; /* silenty this special file will be deleted */
		case S_IFDIR:
		{
			DIR *d;
			mkdir (newpath, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
			fprintf (stderr, " mkdir %s\n", newpath);
			if ((d = opendir (oldpath)))
			{
				struct dirent *de;
				while ((de = readdir (d)))
				{
					char *tempold;
					char *tempnew;

					if (!strcmp (de->d_name, ".")) continue;
					if (!strcmp (de->d_name, "..")) continue;

					tempold = malloc (strlen (oldpath) + 1 + strlen (de->d_name) + 1);
					tempnew = malloc (strlen (newpath) + 1 + strlen (de->d_name) + 1);
					if ((!tempold) || (!tempnew))
					{
						fprintf (stderr, "malloc() failed\n");
						free (tempold);
						free (tempnew);
						closedir (d);
						return -1;
					}
					sprintf (tempold, "%s/%s", oldpath, de->d_name);
					sprintf (tempnew, "%s/%s", newpath, de->d_name);
					if (rename_exdev (tempold, tempnew))
					{
						free (tempold);
						free (tempnew);
						closedir (d);
						return -1;
					}
					free (tempold);
					free (tempnew);
				}
				closedir (d);
			}
			rmdir (oldpath);
			return 0; /* do not use the common unlink path */
		}
		case S_IFLNK:
		{
			char linkdata[4096];
			memset (linkdata, 0, sizeof (linkdata));
			if (readlink (oldpath, linkdata, sizeof (linkdata) - 1) >= 0)
			{
				symlink (newpath, linkdata);
				fprintf (stderr, " symlink %s %s", newpath, linkdata);
				if (strstr (linkdata, ".."))
				{
					fprintf(stderr, "%s (warning, relative symlinks will likely break)%s", isatty (2) ? "\033[1m\033[31m" : "", isatty (2) ? "\033[0m" : "");
				}
				fprintf (stderr, "\n");
			}
			unlink (linkdata);
			break;
		}
		case S_IFREG:
		{
			int oldfd = -1;
			int newfd = -1;
			char data[4096];
			int fill;
			if ((oldfd = open (oldpath, O_RDONLY)) < 0)
			{
				fprintf (stderr, "Failed to open %s: %s\n", oldpath, strerror (errno));
				return -1;
			}
			if ((newfd = creat (newpath, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO))) < 0)
			{
				fprintf (stderr, "Failed to create %s: %s\n", newpath, strerror (errno));
				close (oldfd);
				return -1;
			}
			while (1)
			{
				fill = read (oldfd, data, sizeof (data));
				if (fill == 0)
				{
					break;
				}
				if (fill < 0)
				{
					if (errno == EINTR) continue;
					if (errno == EAGAIN) continue;
					fprintf (stderr, "Failed to read %s: %s\n", oldpath, strerror (errno));
					close (oldfd);
					close (newfd);
					return -1;
				}
again:
				if (write (newfd, data, fill) < 0)
				{
					if (errno == EINTR) goto again;
					if (errno == EAGAIN) goto again;
					fprintf (stderr, "Failed to write %s: %s\n", newpath, strerror (errno));
					close (oldfd);
					close (newfd);
					return -1;
				}
			}
			fprintf (stderr, " copy %s %s\n", oldpath, newpath);
			close (oldfd);
			close (newfd);
			unlink (oldpath);
			break;
		}
	}
	unlink (oldpath);
	return 0;
}

static int move_exdev (const char *olddir, const char *filename, const char *newdir)
{
	char *tempold;
	char *tempnew;

	tempold = malloc (strlen (olddir) + 1 + strlen (filename) + 1);
	tempnew = malloc (strlen (newdir) + 1 + strlen (filename) + 1);
	if ((!tempold) || (!tempnew))
	{
		fprintf (stderr, "malloc() failed\n");
		free (tempold);
		free (tempnew);
		return -1;
	}
	sprintf (tempold, "%s%s%s",
		olddir,
		(olddir[strlen(olddir)-1]!='/') ? "/" : "",
		filename
	);
	sprintf (tempnew, "%s%s%s",
		newdir,
		(newdir[strlen(newdir)-1]!='/') ? "/" : "",
		filename
	);

	if (rename (tempold, tempnew))
	{
		if (errno == EXDEV)
		{ /* crossing file-systems requires manual work */
			if (rename_exdev (tempold, tempnew))
			{
				return -1;
			}
		} else {
			fprintf (stderr, "rename %s %s failed: %s\n", tempold, tempnew, strerror (errno));
			free (tempold);
			free (tempnew);
			return -1;
		}
	} else {
		fprintf (stderr, " renamed %s, %s\n", tempold, tempnew);
	}

	free (tempold);
	free (tempnew);
	return 0;
}

/* returns true if ocp.ini has been located and moved */
static int migrate_old_ocp_ini (void)
{
	char *oldpath;
	DIR *d;
	int retval = 0;

	oldpath = malloc (strlen (_cfHomeDir) + 5 + 1);
	if (!oldpath)
	{
		fprintf (stderr, "malloc() failed\n");
		return -1;
	}
	sprintf (oldpath, "%s.ocp/", _cfHomeDir);
	if ((d = opendir (oldpath)))
	{
		struct dirent *de;
		fprintf (stderr, "Going to migrate %s into %s and %s in order to comply with XDG Base Directory Specification\n", oldpath, _cfConfigHomeDir, _cfDataHomeDir);
		while ((de = readdir (d)))
		{
			if (!strcmp (de->d_name, ".")) continue;
			if (!strcmp (de->d_name, "..")) continue;

			if (!strcmp (de->d_name, "ocp.ini"))
			{
				if (!move_exdev (oldpath, de->d_name, _cfConfigHomeDir))
				{
					retval = 1;
				}
			} else {
				move_exdev (oldpath, de->d_name, _cfDataHomeDir);
			}
		}
		closedir (d);
	}
	if (rmdir (oldpath))
	{
		fprintf (stderr, "Warning, failed to rmdir %s: %s\n", oldpath, strerror (errno));
	}
	free (oldpath);
	return retval;
}

#if 0
#ifdef __HAIKU__
static int haiku_migrate_old_ocp_init(void)
{
	DIR *d;
	if (mkdir_r (_cfConfigHomeDir))
	{
		return -1;
	}
	if (mkdir_r (_cfDataHomeDir))
	{
		return -1;
	}
	if ((d = opendir (_cfConfigHomeDir)))
	{
		struct dirent *de;
		while ((de = readdir (d)))
		{
			if (!strcmp (de->d_name, ".")) continue;
			if (!strcmp (de->d_name, "..")) continue;

			if (!strcmp (de->d_name, "ocp.ini"))
			{
				/* leave this file where it is */
			} else {
				/* move all others */
				if (move_exdev (_cfConfigHomeDir, de->d_name, _cfDataHomeDir))
				{
					closedir (d);
					return -1;
				}
			}
		}
		closedir (d);
	}
	return 0;
}
#endif
#endif

int validate_home(void)
{
/* configure _cfHomeDir */
	_cfHomeDir = getenv("HOME");
	if (_cfHomeDir)
	{
		_cfHomeDir = strdup (_cfHomeDir);
	}

#ifdef __HAIKU__
	{
		char homePath[PATH_MAX];
		if (find_directory(B_USER_DIRECTORY, -1, false, homePath, sizeof(homePath)) == B_OK)
		{
			_cfHomeDir = strdup (homePath);
		}
	}
#endif

#ifdef HAVE_GETPWUID
	if (!_cfHomeDir)
	{
		struct passwd *p = getpwuid (getuid());
		if (p && p->pw_dir)
		{
			_cfHomeDir = strdup (p->pw_dir);
		}
		endpwent();
	}
#endif
	if (!_cfHomeDir)
	{
		fprintf (stderr, "Unable to locate $HOME\n");
		return -1;
	}

	if (_cfHomeDir[0] == 0) /* it is an empty string */
	{
		fprintf (stderr, "Error, $HOME is empty\n");
		return -1;
	}

	if ((_cfHomeDir[0] != '/') ||      /* not absolute, must start with '/' */
	    strstr(_cfHomeDir, "/../") ||  /* not absolute, contains "/../" */
	    ((strlen (_cfHomeDir) >= 3) && (!strcmp (_cfHomeDir + strlen(_cfHomeDir) - 3, "/.."))) ) /* not absolute, ends with "/.." */
	{
		fprintf (stderr, "Error, $HOME is not an absolute path, ignoring value\n");
		return -1;
	}

	/* ensure that _cfHomeDir ends with / */
	if (_cfHomeDir[strlen(_cfHomeDir)-1] != '/')
	{
		char *tmp = malloc (strlen (_cfHomeDir) + 2);
		if (!tmp)
		{
			fprintf (stderr, "validate_home: malloc() failed\n");
			return -1;
		}
		sprintf (tmp, "%s/", _cfHomeDir);
		free (_cfHomeDir);
		_cfHomeDir = tmp;
	}

/* configure _cfConfigHomeDir and _cfDataHomeDir */
	_cfConfigHomeDir = 0;
	_cfDataHomeDir   = 0;
#ifdef __HAIKU__
	{
		char settingsPath[PATH_MAX];
		if (find_directory(B_USER_SETTINGS_DIRECTORY, -1, false, settingsPath, sizeof(settingsPath)) == B_OK)
		{
			_cfConfigHomeDir = malloc (strlen (settingsPath) + 5);
			if (!_cfConfigHomeDir)
			{
				fprintf (stderr, "malloc() failed\n");
				return -1;
			}
			sprintf (_cfConfigHomeDir, "%s/ocp/", settingsPath);
		}
#if 1
#warning Atleast until HAIKU R1Beta4, B_USER_DATA_DIRECTORY refers to a non-existing directory with a read-only parent, maintaing configuration and data in the same directory for now
		_cfDataHomeDir = strdup (_cfConfigHomeDir);
		if (!_cfDataHomeDir)
		{
			fprintf (stderr, "malloc() failed\n");
			free (_cfConfigHomeDir);
			_cfConfigHomeDir = 0;
			return -1;
		}
#else
		if (find_directory(B_USER_DATA_DIRECTORY, -1, false, settingsPath, sizeof(settingsPath)) == B_OK)
		{
			_cfDataHomeDir = malloc (strlen (settingsPath) + 5);
			if (!_cfDataHomeDir)
			{
				fprintf (stderr, "malloc() failed\n");
				free (_cfConfigHomeDir);
				_cfConfigHomeDir = 0;
				return -1;
			}
			sprintf (_cfDataHomeDir, "%s/ocp/", settingsPath);
		}

		do {
			struct stat st;
			if (_cfConfigHomeDir && _cfDataHomeDir &&
			    !stat (_cfConfigHomeDir, &st) && stat (_cfDataHomeDir, &st))
			{
				if (haiku_migrate_old_ocp_init())
				{
					return -1;
				}
			}
		} while (0);
#endif
	}
#endif

	/* Ensure that we have _cfConfigHomeDir and _cfDataHomeDir */
	if (!_cfConfigHomeDir)
	{
		_cfConfigHomeDir = validate_xdg_dir_absolute("XDG_CONFIG_HOME", ".config");
		if (!_cfConfigHomeDir)
		{
			return -1;
		}
	}
	if (!_cfDataHomeDir)
	{
		_cfDataHomeDir = validate_xdg_dir_absolute("XDG_DATA_HOME", ".local/share");
		if (!_cfDataHomeDir)
		{
			return -1;
		}
	}
	if (mkdir_r (_cfConfigHomeDir) ||
	    mkdir_r (_cfDataHomeDir))
	{
		return -1;
	}

	/* validate ocp.ini */
	{
		char *temp = malloc (strlen (_cfConfigHomeDir) + 7 + 1);
		struct stat st;
		if (!temp)
		{
			fprintf (stderr, "malloc() failed\n");
			return -1;
		}
		sprintf (temp, "%socp.ini", _cfConfigHomeDir);
		if (stat(temp, &st)<0)
		{ /* failed to stat ocp.ini */
			const char *temp2;
			if (errno != ENOENT)
			{ /* error is fatal */
				fprintf (stderr, "stat(%s): %s\n", temp, strerror(errno));
				free(temp);
				return -1;
			}
			/* to to migrate old ocp.ini, if successfull stop further actions */
			if (migrate_old_ocp_ini())
			{
				free (temp);
				return 0;
			}
			/* Try to locate system default ocp.ini, if not found, fail hard */
			if (!(temp2=locate_ocp_ini()))
			{
				fprintf(stderr, "Global ocp.ini not found\n");
				free(temp);
				return -1;
			} /* copy the system default ocp.ini */
			if (cp(temp2, temp))
			{
				fprintf(stderr, "cp(%s, %s): %s\n", temp2, temp, strerror(errno));
				free(temp);
				return -1;
			}
			fprintf(stderr, "%s created\n", temp);
		}
		free(temp);
	}
	/* we are good to go */
	return 0;
}

static void *locate_libocp_try(const char *src, int verbose)
{
	char *temp;
	void *retval;
	int srclen = strlen (src);
	int req = srclen + 32;

	temp = malloc (req);

	snprintf (temp, req, "%s%s" "libocp" LIB_SUFFIX, src, (srclen && (src[srclen-1] != '/')) ? "/" : "");

	if (*src)
	{
		struct stat st;
		if (!AllowSymlinked)
		{
			if (lstat(temp, &st))
			{
				free (temp);
				return NULL;
			}
			if (S_ISLNK(st.st_mode))
			{
				fprintf(stderr, "Symlinked libocp" LIB_SUFFIX " is not allowed when running setuid\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	if ((retval=dlopen(temp, RTLD_NOW|RTLD_GLOBAL)))
	{
		_cfProgramDir=malloc(srclen+2);

		snprintf (_cfProgramDir, srclen+2, "%s%s", src, (srclen && (src[srclen-1] != '/')) ? "/" : "");
	} else if (verbose)
	{
		fprintf(stderr, "%s: %s\n", temp, dlerror());
	}

	free (temp);
	return retval;
}

static char *locate_libocp(void)
{
	char *retval;
/*
	if (AllowSymlinked)
		if ((retval=locate_libocp_try(dir, 0)))
			return retval;
*/
	if ((retval=locate_libocp_try(LIBDIROCP, 1)))
		return retval;
	if ((retval=locate_libocp_try(PREFIX "/lib", 1)))
		return retval;

	return locate_libocp_try("", 1); /* current working directory, for running OCP from build-directory AND it is not installed */
}

static int runocp(void *handle, int argc, char *argv[])
{
	struct bootupstruct *bootup;

	if (!(bootup=dlsym(handle, "bootup")))
	{
		fprintf(stderr, "Failed to locate symbol bootup in libocp" LIB_SUFFIX ": %s\n", dlerror());
		return -1;
	}
	if (!(_Console = dlsym(handle, "Console")))
	{
		fprintf(stderr, "Failed to locate symbol Console in libocp" LIB_SUFFIX ": %s\n", dlerror());
		return -1;
	}

	fprintf(stderr, "Setting to cfHomeDir to %s\n", _cfHomeDir);
	fprintf(stderr, "Setting to cfConfigHomeDir to %s\n", _cfConfigHomeDir);
	fprintf(stderr, "Setting to cfDataHomeDir to %s\n", _cfDataHomeDir);
	fprintf(stderr, "Setting to cfDataDir to %s\n", _cfDataDir);
	fprintf(stderr, "Setting to cfProgramDir to %s\n", _cfProgramDir);

	return bootup->main(argc, argv, _cfHomeDir, _cfConfigHomeDir, _cfDataHomeDir, _cfDataDir, _cfProgramDir);
}

int main(int argc, char *argv[])
{
	void *handle;
	int retval;

#ifdef HAVE_DUMA
	DUMA_newFrame();
#endif

#ifdef __linux
	signal(SIGSEGV, (sighandler_t)sigsegv);
	signal(SIGFPE, (sighandler_t)sigsegv);
	signal(SIGILL, (sighandler_t)sigsegv);
	signal(SIGBUS, (sighandler_t)sigsegv);
	signal(SIGINT, (sighandler_t)sigsegv);
#else
	signal(SIGSEGV, sigsegv);
	signal(SIGFPE, sigsegv);
	signal(SIGILL, sigsegv);
	signal(SIGBUS, sigsegv);
	signal(SIGINT, sigsegv);
#endif

	AllowSymlinked=(getuid()==geteuid());

	if (validate_home())
		return -1;

	if (!(handle=locate_libocp()))
	{
		fprintf(stderr, "Failed to locate libocp" LIB_SUFFIX ".. Try to set LD_LIBRARY_PATH\n");
		return -1;
	}

	if (!(_cfDataDir=locate_ocp_hlp()))
	{
		fprintf(stderr, "Failed to locate ocp.hlp..\n");
		return -1;
	}

	retval = runocp(handle, argc, argv);
	free (_cfConfigHomeDir);
	free (_cfDataHomeDir);
	free (_cfDataDir);
	free (_cfProgramDir);

#ifdef HAVE_DUMA
	DUMA_delFrame();
#endif
	return retval;
}
