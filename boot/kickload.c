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
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
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

static char *_cfConfigDir;
static char *_cfDataDir;
static char *_cfProgramDir;

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

	bzero (&z, sizeof(z));
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
	static char buffer[256];
	struct stat st;
	snprintf(buffer, sizeof(buffer), "%s%s", base, "ocp.ini");
	if (!stat(buffer, &st))
		return buffer;
	return NULL;
}

static const char *locate_ocp_ini(void)
{
	const char *retval;
	const char *temp;
	if ((temp=getenv("CPDIR")))
	{
/*
		if (!AllowSymlinked)
		{
			fprintf(stderr, "Using $CPDIR when running setuid is not allowed\n");
		} else*/if ((retval=locate_ocp_ini_try(temp)))
				return retval;
	}
	if ((retval=locate_ocp_ini_try(DATADIR "/ocp" DIR_SUFFIX "/etc/")))
		return retval;
	if ((retval=locate_ocp_ini_try(DATADIR "/share/ocp/" DIR_SUFFIX)))
		return retval;
	if ((retval=locate_ocp_ini_try(PREFIX "/etc/ocp/" DIR_SUFFIX)))
		return retval;
	if ((retval=locate_ocp_ini_try(PREFIX "/etc/ocp/" )))
		return retval;
	if ((retval=locate_ocp_ini_try(PREFIX "/etc/" )))
		return retval;
	if ((retval=locate_ocp_ini_try("/etc/ocp" DIR_SUFFIX "/")))
		return retval;
	if ((retval=locate_ocp_ini_try("/etc/ocp/")))
		return retval;
	if ((retval=locate_ocp_ini_try("/etc/")))
		return retval;
	return NULL;
}

static char *locate_ocp_hlp_try(const char *base)
{
	char *buffer;
	int size = strlen (base) + 8;
	struct stat st;
	buffer = malloc (size);
	snprintf(buffer, size, "%s%s", base, "ocp.hlp");
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
	if ((retval=locate_ocp_hlp_try(DATADIR "/ocp" DIR_SUFFIX "/")))
		return retval;
	if ((retval=locate_ocp_hlp_try(DATADIR "/ocp" DIR_SUFFIX "/data/")))
		return retval;
	if ((retval=locate_ocp_hlp_try(LIBDIR)))
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
void sigsegv(int signal, struct sigcontext r)
#else
void sigsegv(int signal)
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

int validate_home(void)
{
	const char *home=getenv("HOME");
	char *temp;
	const char *temp2;
	struct stat st;

	if (!home)
		home="/root";
	else if (!strlen(home))
		home="/root";
	else if ((*home)!='/')
	{
		fprintf(stderr, "$HOME does not start with a /\n");
		return -1;
	}
	temp=malloc(strlen(home)+1+5+1);
	strcpy(temp, home);
	if (temp[strlen(temp)-1]!='/')
		strcat(temp, "/");
	strcat(temp, ".ocp/");

#ifdef __HAIKU__
	{
		char settingsPath[PATH_MAX];
		if (find_directory(B_USER_SETTINGS_DIRECTORY, -1, false, settingsPath, sizeof(settingsPath)) == B_OK)
		{
			free(temp);
			temp=malloc(strlen(settingsPath)+1+4+1);
			strcpy(temp, settingsPath);
			strcat(temp, "/ocp/");
		}
	}
#endif

	_cfConfigDir=temp;

	if (stat(temp, &st)<0)
	{
		if (errno==ENOENT)
		{
			fprintf(stderr, "Creating %s\n", temp);
			if (mkdir(temp, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)<0)
			{
				perror("mkdir()");
				return -1;
			}
		} else {
			fprintf (stderr, "stat(%s): %s\n", temp, strerror(errno));
			return -1;
		}
	}

	temp=malloc(strlen(_cfConfigDir)+12);
	strcpy(temp, _cfConfigDir);
	strcat(temp, "ocp.ini");
	if (stat(temp, &st)<0)
	{
		if (errno!=ENOENT)
		{
			fprintf (stderr, "stat(%s): %s\n", temp, strerror(errno));
			free(temp);
			return -1;
		}
		if (!(temp2=locate_ocp_ini()))
		{
			fprintf(stderr, "Global ocp.ini not found\n");
			free(temp);
			return -1;
		}
		if (cp(temp2, temp))
		{
			fprintf(stderr, "cp(%s, %s): %s\n", temp2, temp, strerror(errno));
			free(temp);
			return -1;
		}
		fprintf(stderr, "%s created\n", temp);
	}
	free(temp);
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
	if ((retval=locate_libocp_try(LIBDIR, 1)))
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

	fprintf(stderr, "Setting to cfConfigDir to %s\n", _cfConfigDir);
	fprintf(stderr, "Setting to cfDataDir to %s\n", _cfDataDir);
	fprintf(stderr, "Setting to cfProgramDir to %s\n", _cfProgramDir);

	return bootup->main(argc, argv, _cfConfigDir, _cfDataDir, _cfProgramDir);
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

	if (_cfConfigDir)
		free(_cfConfigDir);
	if (_cfDataDir)
		free (_cfDataDir);
	if (_cfProgramDir)
		free(_cfProgramDir);

#ifdef HAVE_DUMA
	DUMA_delFrame();
#endif
	return retval;
}
