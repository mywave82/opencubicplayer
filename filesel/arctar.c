/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Archive handler for TAR-balls archives
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
 *  -ss040816   Stian Skjelstad <stian@nixia.no
 *    -first release
 */

#include "config.h"
#include "fcntl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "types.h"
#include "adb.h"
#include "mdb.h"
#include "mif.h"
#include "modlist.h"
#include "pfilesel.h"
#include "boot/plinkman.h"
#include "stuff/compat.h"

struct __attribute__((packed)) posix_header
{                               /* byte offset */
	char name[100];               /*   0 */
	char mode[8];                 /* 100 */
	char uid[8];                  /* 108 */
	char gid[8];                  /* 116 */
	char size[12];                /* 124 */
	char mtime[12];               /* 136 */
	char chksum[8];               /* 148 */
	char typeflag;                /* 156 */
	char linkname[100];           /* 157 */
	char magic[6];                /* 257 */
	char version[2];              /* 263 */
	char uname[32];               /* 265 */
	char gname[32];               /* 297 */
	char devmajor[8];             /* 329 */
	char devminor[8];             /* 337 */
	char prefix[155];             /* 345 */
	/* 500 */
	char padding[12];
};

#define TMAGIC   "ustar"        /* ustar and a null */

#define REGTYPE  '0'            /* regular file */
#define AREGTYPE '\0'           /* regular file */
#define DIRTYPE  '5'            /* directory */

#define BLOCKSIZE 512

static int format=0;
static char ext[NAME_MAX+1];
static char name[NAME_MAX+1];
static char arcname[ARC_PATH_MAX+1];

static int setupformat(const char *path)
{
	_splitpath(path, 0, 0, name, ext);

	if ((strlen(name)+strlen(ext)+1)>ARC_PATH_MAX)
		return 0;
	strcpy(arcname, name);
	strcat(arcname, ext);

	if (!strcasecmp(ext, ".tgz"))
		format=1;
	else if (!strcasecmp(ext, ".tar.gz"))
		format=1;
	else if (!strcasecmp(ext, ".tar.Z"))
		format=3;
	else if (!strcasecmp(ext, ".tZ"))
		format=3;
	else if (!strcasecmp(ext, ".tar.bz2"))
		format=2;
	else if (!strcasecmp(ext, ".tbz"))
		format=2;
	else
		format=0;
	return 1;
}

size_t char12tosize_t(char src[12])
{
	size_t retval;
	char tmp[13];
	strncpy(tmp, src, 12);
	tmp[12]=0;
	retval=strtol(tmp, 0, 8);
	return retval;
}

static int pipe_pid;
static int pipe_fd;

static int pipe_done(void)
{
	int result=0;
	if (pipe_fd>0)
	{
		close(pipe_fd);
		pipe_fd=-1;
	}

	if (pipe_pid>0)
	{
		kill(SIGKILL, pipe_pid); /* make sure it is dead */
		if (waitpid(pipe_pid, &result, WUNTRACED))
			result=-1;
	}
	pipe_pid=-1;
	return result;
}

static int pipe_uncompress(const char *program, char *const argv[], const char *source)
{
	int fds[2];
	int fd;

	pipe_fd=-1;
	if ((fd=open(source, O_RDONLY))<0)
	{
		perror("arctar: open(source, O_RDONLY)");
		return -1;
	}

	if (pipe(fds))
	{
		perror("arctar: pipe()");
		return -1;
	}

	if ((pipe_pid=fork()))
	{
		if (pipe_pid<0)
		{
			perror("arctar: fork()");
			close(fds[1]);
			close(fds[0]);
			close(fd);
			return -1;
		}
		close(fds[1]);
		close(fd);
		return pipe_fd=fds[0];
	}
	close(fds[0]);

	close(1);
	if (dup(fds[1])!=1)
	{
		perror(__FILE__ ": dup() failed #1");
		exit(1);
	}
	close(fds[1]);

	close(0);
	if (dup(fd)!=0)
	{
		perror(__FILE__ ": dup() failed #2");
		exit(1);
	}
	close(fd);

	execvp(program, argv);
	perror("arctar: execlp(program, argv, NULL)");
	exit(-1);
}

#define BUFFER_SIZE 128*1024
static int adbTARScan(const char *path)
{
	uint32_t arcref;
	struct arcentry a;

	int extfd;
	char buffer[BUFFER_SIZE];
	size_t bufferfill=0;
	int retval;
	size_t skip=0;
	size_t requiredata=0;

	/* fprintf(stderr, "adbTARScan, %s\n", path);*/

	if (!setupformat(path))
		return 0;

	switch (format)
	{
/*
		case 0:
*/
		default: /* avoids warning -ss040902 */
			extfd=open(path, O_RDONLY);
			break;
		case 1:
			{
				char *argv[5];
				argv[0]="gunzip";
				argv[1]="-c";
				argv[2]="-d";
				argv[3]="-f";
				argv[4]=NULL;
				extfd=pipe_uncompress("gunzip", argv, path);
				break;
			}
		case 2:
			{
				char *argv[4];
				argv[0]="bzcat";
				argv[1]="-d";
				argv[2]="-c";
				argv[3]=NULL;
				extfd=pipe_uncompress("bzcat", argv, path);
				break;
			}
		case 3:
			{
				char *argv[2];
				argv[0]="zcat";
				argv[1]=NULL;
				extfd=pipe_uncompress("zcat", argv, path);
				break;
			}
	}

	if (extfd<0)
		return 0;
	if ((retval=read(extfd, buffer, BUFFER_SIZE))<=0)
	{
		pipe_done();
		return 0;
	}
	bufferfill=retval;

	memset(a.name, 0, sizeof(a.name));
	strncpy(a.name, arcname, sizeof(a.name)-1);
	a.size=_filelength(path);
	a.flags=ADB_ARC;
	if (!adbAdd(&a))
	{
		pipe_done();
		return 0;
	}
	arcref=adbFind(arcname);

	while (1)
	{
		while ((bufferfill>(sizeof(struct posix_header)+requiredata))&&(!skip))
		{
			struct posix_header *entry=(struct posix_header *)buffer;
			/* do we need this entry? */
			size_t size;

			if (strncmp(entry->magic, "ustar", 5))
			{
				if (memcmp(entry->magic, "\0\0\0\0\0\0", 6))
				{
					fprintf(stderr, "arctar: Error in TAR-stream: %s\n", path);
					pipe_done();
					return 0;
				}
			}
			if (!*entry->name)
			{
				pipe_done();
				return 1;
			}
/*
			fprintf(stderr, "arctar: Entry: %s\n", entry->name);
*/
			size=char12tosize_t(entry->size);

			_splitpath(entry->name, 0, 0, name, ext);
			if(fsIsModule(ext))
			{
				if
				(
					((strlen(entry->name)+1)<ARC_PATH_MAX)
					&&
					(
						(entry->typeflag==REGTYPE)
						||
						(entry->typeflag==AREGTYPE)
					)
				)
				{
				/* TODO if ((!strcasecmp(ext, MIF_EXT))&&size<65536)
						requiredata=size;
					else*/ {
						requiredata=1084;
						if (size<requiredata)
							requiredata=size;
					}
					if (bufferfill<(sizeof(struct posix_header)+requiredata))
						break; /* we need more data */

					strcpy(a.name, entry->name);
					a.size=size;
					a.flags=0;
					a.parent=arcref;
					if(!adbAdd(&a))
					{
						pipe_done();
						return 0;
					}

					strcpy(a.name, name);
					strcat(a.name, ext);

				        if (fsScanInArc)
					{
						char shortname[12];
						uint32_t mdb_ref;
						struct moduleinfostruct mi;
						fs12name(shortname, a.name);
						mdb_ref=mdbGetModuleReference(shortname, a.size);
						if (mdb_ref==0xffffffff)
						{
							pipe_done();
							return 0;
						}
						if (!mdbInfoRead(mdb_ref))
						{
							if (mdbGetModuleInfo(&mi, mdb_ref))
							{
								mdbReadMemInfo(&mi, buffer+sizeof(struct posix_header), 1084);
								mdbWriteModuleInfo(mdb_ref, &mi);
							}
						}
						/* TODO MIF_EXT a.name....
						if ((!stricmp(ext, MIF_EXT)) && (size<65536))
							mifMemRead(a.name, size, buffer+sizeof(struct posix_header));
						*/
					}
					requiredata=0;
				}
			}
			skip=(sizeof(struct posix_header)+size+BLOCKSIZE-1)&~(BLOCKSIZE-1);
		}
		if (skip)
		{
			if (skip>bufferfill)
			{
				skip-=bufferfill;
				bufferfill=0;
			} else {
				memmove(buffer, buffer+skip, bufferfill-skip);
				bufferfill-=skip;
				skip=0;
			}
		}

		retval=read(extfd, buffer+bufferfill, BUFFER_SIZE-bufferfill);
		if (retval<=0)
			break;
		bufferfill+=retval;
	}
	pipe_done();
	return 1;
}

static int adbTARCall(const int act, const char *apath, const char *fullname, const int fd)
{
/*
	fprintf(stderr, "adbTARCall, %d %s %s %s\n", act, apath, file, dpath);
*/
	if (!setupformat(apath))
			return 0;
	switch (act)
	{
		case adbCallGet:
			{
				char *argv[6];
				pid_t child;
				int status;

				argv[0]="tar";
				switch(format)
				{
					case 0:
						argv[1]="xf";
						break;
					case 1:
						argv[1]="xfz";
						break;
					case 2:
						argv[1]="xfj";
						break;
					case 3:
						argv[1]="xfZ";
						break;
				}
				argv[2]=(char *)apath; /* dirty, but should be safe */
				argv[3]="-O";
				argv[4]=(char *)fullname; /* dirty, but should be safe */
				argv[5]=NULL;

				if (!(child=fork()))
				{
					close(1);
					if (dup(fd)!=1)
					{
						perror(__FILE__ ": dup() failed #3: ");
						exit(1);
					}
					execvp("tar", argv);
					perror(__FILE__": execvp(tar, argv): ");
					exit(1);
				}
				if (child<0)
				{
					perror(__FILE__ ": fork(): ");
					return 0;
				}
				if (waitpid(child, &status, WUNTRACED)<0)
				{
					perror(__FILE__ ": waitpid(): ");
					return 0;
				}
				if (status)
				{
					fprintf(stderr, __FILE__ ": Child exited with error on archive %s\n", apath);
					return 0;
				}
				return 1;
			}
		case adbCallPut:
			return 0;
		case adbCallDelete:
			return 0;
		case adbCallMoveTo:
			return 0;
		case adbCallMoveFrom:
			return 0;
	}
	return 0;
}


static struct adbregstruct adbTARReg1 = {".TGZ", adbTARScan, adbTARCall ADBREGSTRUCT_TAIL};
static struct adbregstruct adbTARReg2 = {".TBZ", adbTARScan, adbTARCall ADBREGSTRUCT_TAIL};
static struct adbregstruct adbTARReg3 = {".TAR", adbTARScan, adbTARCall ADBREGSTRUCT_TAIL};
static struct adbregstruct adbTARReg4 = {".TZ", adbTARScan, adbTARCall ADBREGSTRUCT_TAIL};
static struct adbregstruct adbTARReg5 = {".TAR.GZ", adbTARScan, adbTARCall ADBREGSTRUCT_TAIL};
static struct adbregstruct adbTARReg6 = {".TAR.BZ2", adbTARScan, adbTARCall ADBREGSTRUCT_TAIL};
static struct adbregstruct adbTARReg7 = {".TAR.Z", adbTARScan, adbTARCall ADBREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	adbRegister(&adbTARReg1);
	adbRegister(&adbTARReg2);
	adbRegister(&adbTARReg3);
	adbRegister(&adbTARReg4);
	adbRegister(&adbTARReg5);
	adbRegister(&adbTARReg6);
	adbRegister(&adbTARReg7);
}

static void __attribute__((destructor))done(void)
{
	adbUnregister(&adbTARReg1);
	adbUnregister(&adbTARReg2);
	adbUnregister(&adbTARReg3);
	adbUnregister(&adbTARReg4);
	adbUnregister(&adbTARReg5);
	adbUnregister(&adbTARReg6);
	adbUnregister(&adbTARReg7);
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "arctar", .desc = "OpenCP Archive Reader: .TAR (c) 2004-09 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
