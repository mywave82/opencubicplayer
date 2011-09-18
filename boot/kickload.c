/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include "types.h"
#include "pmain.h"

static char *_cfConfigDir;
static const char *_cfDataDir;
static char *_cfProgramDir;

static char *argv0;
static char *dir;

static int AllowSymlinked;

/* This has todo with video-mode rescue */
static int *plScrMode, crashmode;
static void (**_plSetTextMode)(unsigned char size);
static void (**_plSetGraphMode)(unsigned char size);

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

	memset(&z, 0, sizeof(z));
	setitimer(ITIMER_REAL, &z, &i[0]);
	setitimer(ITIMER_VIRTUAL, &z, &i[1]);
	setitimer(ITIMER_PROF, &z, &i[2]);
	crashmode=*plScrMode;
	if (_plSetTextMode != NULL)
		if (*_plSetTextMode != NULL)
			(*_plSetTextMode)((unsigned char)255);
}
static void restartstuff(struct itimerval *i)
{
	setitimer(ITIMER_REAL, &i[0], 0);
	setitimer(ITIMER_VIRTUAL, &i[1], 0);
	setitimer(ITIMER_PROF, &i[2], 0);
	if (crashmode==101)
	{
		if (_plSetGraphMode != NULL)
			if (*_plSetGraphMode != NULL)
				(*_plSetGraphMode)((unsigned char)1);
	} else if (crashmode==100)
	{
		if (_plSetGraphMode != NULL)
			if (*_plSetGraphMode != NULL)
				(*_plSetGraphMode)((unsigned char)0);
	} else
		if (_plSetTextMode != NULL)
			if (*_plSetTextMode != NULL)
				(*_plSetTextMode)((unsigned char)crashmode);
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
	if ((retval=locate_ocp_ini_try(PREFIX "/share/ocp" DIR_SUFFIX "/etc/")))
		return retval;
	if ((retval=locate_ocp_ini_try(PREFIX "/share/ocp/" DIR_SUFFIX)))
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

static const char *locate_ocp_hlp_try(const char *base)
{
	static char buffer[256];
	struct stat st;
	snprintf(buffer, sizeof(buffer), "%s%s", base, "ocp.hlp");
	if (!stat(buffer, &st))
	{
		snprintf(buffer, sizeof(buffer), "%s", base);
		return buffer; /* since input data might come from getenv, it is not safe to keep them */
	}
	return NULL;
}

static const char *locate_ocp_hlp(void)
{
	const char *retval;
	const char *temp;
	if ((temp=getenv("OCPDIR")))
		if ((retval=locate_ocp_hlp_try(temp)))
			return retval;
	if ((retval=locate_ocp_hlp_try(PREFIX "/share/ocp" DIR_SUFFIX "/")))
		return retval;
	if ((retval=locate_ocp_hlp_try(PREFIX "/share/ocp" DIR_SUFFIX "/data/")))
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
	else
		fprintf(stderr, "Unknown fault\n");
#if defined(__linux) && ( defined(_X86) || defined(__i386__) )
	fprintf(stderr, "signal=%d\n", signal);
	fprintf(stderr, "eax=0x%08x ebx=0x%08x ecx=0x%08x edx=0x%08x\n", (int)r.eax, (int)r.ebx, (int)r.ecx, (int)r.edx);
	fprintf(stderr, "cs=0x%04x ds=0x%04x es=0x%04x fs=0x%04x gs=0x%04x ss=0x%04x\n", r.cs, r.ds, r.es, r.fs, r.gs, r.ss);
	fprintf(stderr, "edi=0x%08x esi=0x%08x ebp=0x%08x esp=0x%08x\n", (int)r.edi, (int)r.esi, (int)r.ebp, (int)r.esp_at_signal);
	fprintf(stderr, "eip=0x%08x\n", (int)r.eip);
	fprintf(stderr, "eflags=0x%08x\n", (int)r.eflags);
	fprintf(stderr, "err=%d trapno=%d cr2=0x%08x oldmask=0x%08x\n", (int)r.err, (int)r.trapno, (int)r.cr2, (int)r.oldmask);
	fprintf(stderr, "\n");
#endif /* we should have an else here for BSD */
}

#if defined(__linux)
void sigsegv(int signal, struct sigcontext r)
#else
void sigsegv(int signal)
#endif
{
	struct itimerval i[3];
#ifdef KICKSTART_GDB
	char *argv[5];
	char buffer[32];
	pid_t pid;
#endif

	stopstuff(i);

	/* loose setuid stuff before we launch any other external tools */
	if (getegid()!=getgid())
		setegid(getgid());

	if (geteuid()!=getuid())
		seteuid(getuid());

	if(_plSetTextMode)
		if (*_plSetTextMode) /* don't reset if we havn't used the screen yet */
			reset();
#if defined(__linux)
	dumpcontext(signal, r);
#else
	dumpcontext(signal);
#endif

#ifdef KICKSTART_GDB
	/* first hook up stderr if is not */
	/* don't start gdb if we weren't for instance x11 */
	if (isatty(0))
	{
		argv[0]="gdb";
		argv[1]=argv0;
		argv[2]="-p";
		snprintf(buffer, sizeof(buffer), "%d", getpid());
		argv[3]=buffer;
		argv[4]=NULL;

		if (!(pid=fork()))
		{
			if (!isatty(2))
			{
				eintr_close(2);
				eintr_dup(1);
			}
			setsid();

			if (dir)
				if (chdir(dir))
					perror(__FILE__ " chdir()");
			execvp("gdb", argv);

			perror("execvp(gdb)\n");
			exit(EXIT_FAILURE);
		} else if (pid>=0) /* not an error */
		{
			int status;
			eintr_waitpid(pid, &status, 0);
		}
	}
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
	_cfConfigDir=temp;

	if (stat(temp, &st)<0)
	{
		if (errno==ENOENT)
		{
			fprintf(stderr, "Creating $HOME/.ocp\n");
			if (mkdir(temp, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)<0)
			{
				perror("mkdir()");
				return -1;
			}
		} else {
			perror("stat($HOME)");
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
			perror("stat($HOME/.ocp/ocp.ini)");
			free(temp);
			return -1;
		}
		strcpy(temp, _cfConfigDir);
		strcat(temp, "ocp.ini");
		if (stat(temp, &st)<0)
		{
			if (errno!=ENOENT)
			{
				perror("stat($HOME/.ocp/ocp.ini)");
				free(temp);
				return -1;
			}
			if (!(temp2=locate_ocp_ini()))
			{
				fprintf(stderr, "Global ocp.ini not found\n");
				free(temp);
				return -1;
			}
			strcpy(temp, _cfConfigDir);
			strcat(temp, "ocp.ini");
			if (cp(temp2, temp))
			{
				perror("cp(global ocp.ini, $HOME/.ocp/ocp.ini)");
				free(temp);
				return -1;
			}
			fprintf(stderr, "$HOME/.ocp/ocp.ini created\n");
		}
	}
	free(temp);
	return 0;
}

static void *locate_libocp_try(const char *src, int verbose)
{
	char temp[PATH_MAX+1];
	void *retval;
	strcpy(temp, src);
	if (strlen(temp)>(PATH_MAX-20))
		return NULL;
	if (*temp)
	{
		if (temp[strlen(temp)-1]!='/')
			strcat(temp, "/");
	}
	strcat(temp, "libocp" LIB_SUFFIX);

	if (*src)
	{
		struct stat st;
		if (!AllowSymlinked)
		{
			if (lstat(temp, &st))
				return NULL;
			if (S_ISLNK(st.st_mode))
			{
				fprintf(stderr, "Symlinked libocp" LIB_SUFFIX " is not allowed when running setuid\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	if ((retval=dlopen(temp, RTLD_NOW|RTLD_GLOBAL)))
	{
		_cfProgramDir=malloc(strlen(src)+2);
		strcpy(_cfProgramDir, src);
		if (*_cfProgramDir)
			if (temp[strlen(_cfProgramDir)-1]!='/')
				strcat(_cfProgramDir, "/");

	} else if (verbose)
		fprintf(stderr, "%s: %s\n", temp, dlerror());
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

	return locate_libocp_try("", 1);
}

static int runocp(void *handle, int argc, char *argv[])
{
	struct mainstruct *bootup;
	char *cfConfigDir;
	char *cfDataDir;
	char *cfProgramDir;

	if (!(bootup=dlsym(handle, "bootup")))
	{
		fprintf(stderr, "Failed to locate symbol bootup in libocp" LIB_SUFFIX ": %s\n", dlerror());
		return -1;
	}
	if (!(cfConfigDir=(char *)dlsym(handle, "cfConfigDir")))
	{
		fprintf(stderr, "Failed to locate symbol cfConfigDir in libocp" LIB_SUFFIX ": %s\n", dlerror());
		return -1;
	}
	if (!(cfDataDir=(char *)dlsym(handle, "cfDataDir")))
	{
		fprintf(stderr, "Failed to locate symbol cfDataDir in libocp" LIB_SUFFIX ": %s\n", dlerror());
		return -1;
	}

	if (!(cfProgramDir=(char *)dlsym(handle, "cfProgramDir")))
	{
		fprintf(stderr, "Failed to locate symbol cfProgramDir in libocp " LIB_SUFFIX ": %s\n", dlerror());
		return -1;
	}

	if (!(plScrMode=(int *)(dlsym(handle, "plScrMode"))))
	{
		fprintf(stderr, "Failed to locate symbol plScrMode in libocp" LIB_SUFFIX ": %s\n", dlerror());
		return -1;
	}
	if (!(_plSetTextMode=(void (**)(unsigned char))(dlsym(handle, "_plSetTextMode"))))
	{
		fprintf(stderr, "Failed to locate symbol _plSetTextMode in libocp" LIB_SUFFIX ": %s\n", dlerror());
		return -1;
	}
	if (!(_plSetGraphMode=(void (**)(unsigned char))(dlsym(handle, "_plSetGraphMode"))))
	{
		fprintf(stderr, "Failed to locate symbol _plSetGraphMode in libocp" LIB_SUFFIX ": %s\n", dlerror());
		return -1;
	}

	fprintf(stderr, "Setting to cfConfigDir to %s\n", _cfConfigDir);
	fprintf(stderr, "Setting to cfDataDir to %s\n", _cfDataDir);
	fprintf(stderr, "Setting to cfProgramDir to %s\n", _cfProgramDir);
	strcpy(cfConfigDir, _cfConfigDir);
	strcpy(cfDataDir, _cfDataDir);
	strcpy(cfProgramDir, _cfProgramDir);

	return bootup->main(argc, argv);
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

	argv0=argv[0];

#ifdef __linux
	dir=get_current_dir_name();
#elif defined(HAVE_GETCWD)
	dir=getcwd(malloc(PATH_MAX), PATH_MAX);
#else /* BSD */
	dir=getwd(malloc(PATH_MAX));
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

	free(dir);

	dir=0;

	if (_cfConfigDir)
		free(_cfConfigDir);
	if (_cfProgramDir)
		free(_cfProgramDir);

#ifdef HAVE_DUMA
	DUMA_delFrame();
#endif
	return retval;
}
