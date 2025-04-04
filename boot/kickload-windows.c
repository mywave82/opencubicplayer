/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <assert.h>
#include <errhandlingapi.h>
#include <fileapi.h>
#include <handleapi.h>
#include <libloaderapi.h>
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "types.h"
#include "boot/pmain.h"
#include "boot/console.h"
#include "stuff/poutput.h"

static char *_cfConfigHomePath;
static char *_cfDataHomePath;
static char *_cfHomePath;
static char *_cfDataPath;
static char *_cfProgramPath;

static const char *verify_file (const char *base)
{
	DWORD st;

	st = GetFileAttributes (base);
#ifdef KICKSTART_DEBUG
	fprintf (stderr, "verify_file(%s) = %u\n", base, (unsigned)st);
#endif
	if (st == INVALID_FILE_ATTRIBUTES)
	{
		return 0;
	}
	if (st & FILE_ATTRIBUTE_DIRECTORY)
	{
		return 0;
	}
	return base;
}

static char *locate_ocp_ini(void)
{
	char *temp;

	temp = malloc (strlen (_cfProgramPath) + 4 + strlen ("ocp.ini") + 1);
	sprintf (temp, "%setc\\ocp.ini", _cfProgramPath);
	if (verify_file(temp))
	{
		return temp;
	}
	free (temp);

	temp = malloc (strlen (_cfDataPath) + 4 + strlen ("ocp.ini") + 1);
	sprintf (temp, "%setc\\ocp.ini", _cfDataPath);
	if (verify_file(temp))
	{
		return temp;
	}
	free (temp);

	temp = malloc (strlen (_cfProgramPath) + strlen ("ocp.ini") + 1);
	sprintf (temp, "%socp.ini", _cfProgramPath);
	if (verify_file(temp))
	{
		return temp;
	}
	free (temp);

	temp = malloc (strlen (_cfDataPath) + strlen ("ocp.ini") + 1);
	sprintf (temp, "%socp.ini", _cfDataPath);
	if (verify_file(temp))
	{
		return temp;
	}
	free (temp);

	return NULL;
}

static char *locate_ocp_hlp_try(const char *base, const char *suffix)
{
	char *buffer;
	int size = strlen (base) + strlen(suffix) + 8;

	buffer = malloc (size);
	snprintf(buffer, size, "%s%s%s", base, suffix, "ocp.hlp");

	if (verify_file (buffer))
	{
		snprintf(buffer, size, "%s%s", base, suffix);
		return buffer;
	}

	free (buffer);

	return NULL;
}

static char *locate_ocp_hlp(void)
{
	char *retval;
	const char *temp;

	if ((temp=getenv("OCPDIR")))
		if ((retval=locate_ocp_hlp_try(temp, "")))
			return retval;

	if ((retval=locate_ocp_hlp_try(_cfProgramPath, "")))
		return retval;

	if ((retval=locate_ocp_hlp_try(_cfProgramPath, "data\\")))
		return retval;

	return NULL;
}

static void sigsegv(int signal)
{
	if (signal==SIGSEGV)
		fprintf(stderr, "Segmentation Fault\n");
	else if (signal==SIGILL)
		fprintf(stderr, "Illegal Instruction\n");
	else if (signal==SIGFPE)
		fprintf(stderr, "Division by zero / Floating Point Error\n");
	else if (signal==SIGINT)
		fprintf(stderr, "User pressed ctrl-C\n");
	else {
		fprintf(stderr, "Unknown fault\n");
		fprintf(stderr, "signal=%d\n", signal);
	}

	exit (0);
}

static int cp(const char *src, const char *dst)
{
	int storeError;
	HANDLE srcfd, dstfd;
	char buffer[65536];

#ifdef KICKSTART_DEBUG
	fprintf (stderr, "cp %s %s\n", src, dst);
#endif

	srcfd = CreateFile (src,                   /* lpFileName */
	                    GENERIC_READ,          /* dwDesiredAccess */
	                    FILE_SHARE_READ,       /* dwShareMode */
	                    0,                     /* lpSecurityAttributes */
	                    OPEN_EXISTING,         /* dwCreationDisposition */
	                    FILE_ATTRIBUTE_NORMAL, /* dwFlagsAndAttributes */
	                    0);                    /* hTemplateFile */
	if (srcfd == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	dstfd = CreateFile (dst,                   /* lpFileName */
	                    GENERIC_WRITE,         /* dwDesiredAccess */
	                    FILE_SHARE_READ,       /* dwShareMode */
	                    0,                     /* lpSecurityAttributes */
	                    CREATE_ALWAYS,         /* dwCreationDisposition */
	                    FILE_ATTRIBUTE_NORMAL, /* dwFlagsAndAttributes */
	                    0);                    /* hTemplateFile */
	if (dstfd == INVALID_HANDLE_VALUE)
	{
		storeError=GetLastError();
		CloseHandle (srcfd);
		SetLastError(storeError);
		return -1;
	}
	while (1)
	{
		DWORD bytesread = 0;
		DWORD byteswrote = 0;

		if (!ReadFile (srcfd, buffer, sizeof (buffer), &bytesread, 0))
		{
			storeError=GetLastError();
			CloseHandle (srcfd);
			CloseHandle (dstfd);
			SetLastError(storeError);
			return -1;
		}
		if (!bytesread)
		{
			break;
		}
		if (!WriteFile (dstfd, buffer, bytesread, &byteswrote, 0))
		{
			storeError=GetLastError();
			CloseHandle (srcfd);
			CloseHandle (dstfd);
			SetLastError(storeError);
			return -1;
		}
	}

	SetEndOfFile (dstfd);
	CloseHandle (srcfd);
	CloseHandle (dstfd);

	return 0;
}

static int mkdir_r (char *path)
{
	char *next;
	next = strchr (path + 1, '\\');
	while (1)
	{
		DWORD st;
		if (next)
		{
			*next = 0;
		}

		st = GetFileAttributes (path);
		if (st == INVALID_FILE_ATTRIBUTES)
		{
#ifdef KICKSTART_DEBUG
			fprintf (stderr, "CreateDirectory %s\n", path);
#endif
			if (!CreateDirectory (path, 0))
			{
				fprintf (stderr, "Failed to create %s\n", path);
				goto failout;
			}
		} if (!(st & FILE_ATTRIBUTE_DIRECTORY))
		{
			fprintf (stderr, "%s is not a directory\n", path);
			goto failout;
		}
		if (next)
		{
			*next = '\\';
			next = strchr (next + 1, '\\');
		} else {
			break;
		}
	}
	return 0;
failout:
	if (next)
	{
		*next = '\\';
	}
	return -1;
}

int validate_home(void)
{
	/* validate ocp.ini */
	char *temp = malloc (strlen (_cfConfigHomePath) + 7 + 1);
	DWORD st;

	if (!temp)
	{
		fprintf (stderr, "malloc() failed\n");
		return -1;
	}
	sprintf (temp, "%socp.ini", _cfConfigHomePath);

	st = GetFileAttributes (temp);
	if (st == INVALID_FILE_ATTRIBUTES)
	{ /* failed to stat ocp.ini */
		char *temp2;
		/* Try to locate system default ocp.ini, if not found, fail hard */
		if (!(temp2=locate_ocp_ini()))
		{
			fprintf(stderr, "Global ocp.ini not found\n");
			free(temp);
			return -1;
		} /* copy the system default ocp.ini */
		if (cp(temp2, temp))
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
				fprintf(stderr, "cp(%s, %s): %s\n", temp2, temp, lpMsgBuf);
				LocalFree (lpMsgBuf);
			}
			free(temp);
			free (temp2);
			return -1;
		}
		free (temp2);
		fprintf(stderr, "%s created\n", temp);
	} else if (st & FILE_ATTRIBUTE_DIRECTORY)
	{
		fprintf (stderr, "%s is not attributed as a file\n", temp);
		free (temp);
		return -1;
	}
	free(temp);

	/* we are good to go */
	return 0;
}

static int runocp (int argc, char *argv[])
{
	HMODULE handle;
	struct bootupstruct *bootup;

	handle = LoadLibrary ("libocp" LIB_SUFFIX);
	if (!handle)
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
			fprintf(stderr, "Failed to open libocp" LIB_SUFFIX ": %s\n", lpMsgBuf);
			LocalFree (lpMsgBuf);
		}
		return -1;
	}

	if (!(bootup = (struct bootupstruct *)GetProcAddress(handle, "bootup")))
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
			fprintf(stderr, "Failed to locate symbol bootup in libocp" LIB_SUFFIX ": %s\n", lpMsgBuf);
			LocalFree (lpMsgBuf);
		}
		return -1;
	}

	fprintf(stderr, "Setting cfHome to %s\n", _cfHomePath);
	fprintf(stderr, "Setting cfConfigHomePath to %s\n", _cfConfigHomePath);
	fprintf(stderr, "Setting cfDataHomePath to %s\n", _cfDataHomePath);
	fprintf(stderr, "Setting cfDataPath to %s\n", _cfDataPath);
	fprintf(stderr, "Setting cfProgramPath to %s\n", _cfProgramPath);

	return bootup->main(argc, argv, _cfHomePath, _cfConfigHomePath, _cfDataHomePath, _cfDataPath, _cfProgramPath);
}

int main(int argc, char *argv[])
{
	char *a, *b;
	int retval;
	char path[MAX_PATH+1024] = {0};

#ifdef HAVE_DUMA
	DUMA_newFrame();
#endif

	signal(SIGSEGV, sigsegv);
	signal(SIGFPE, sigsegv);
	signal(SIGILL, sigsegv);
	signal(SIGINT, sigsegv);

	if (SHGetFolderPath (NULL, CSIDL_APPDATA, NULL, 0, path) != S_OK)
	{
		fprintf (stderr, "Failed to retrieve %%APPDATA%%\n");
		return -1;
	}
	assert (path[0]);
	_cfConfigHomePath = malloc (strlen (path) + 1 +  strlen("OpenCubicPlayer\\Config\\") + 1);
	_cfDataHomePath = malloc (strlen (path) + 1 + strlen("OpenCubicPlayer\\Data\\") + 1);
	sprintf (
		_cfConfigHomePath,
		"%s%s%s",
		path,
		path[strlen(path)-1] != '\\' ? "\\" : "",
		"OpenCubicPlayer\\Config\\"
	);
	sprintf (
		_cfDataHomePath,
		"%s%s%s",
		path,
		path[strlen(path)-1] != '\\' ? "\\" : "",
		"OpenCubicPlayer\\Data\\"
	);

#ifdef KICKSTART_DEBUG
	fprintf (stderr, "cfConfigHomePath set to %s\n", _cfConfigHomePath);
	fprintf (stderr, "cfDataHomePath set to %s\n", _cfDataHomePath);
#endif

	if (mkdir_r (_cfConfigHomePath) ||
	    mkdir_r (_cfDataHomePath))
	{
		return -1;
	}

	a = getenv("HOME");
	if (a && a[0])
	{
#ifdef KICKSTART_DEBUG
		if (a) fprintf (stderr, "$HOME = %s\n", a);
#endif
		_cfHomePath = strdup (a);
	}
	if (!_cfHomePath)
	{
		a = getenv("HOMEPATH");
		b = getenv("HOMEDRIVE");
		if (a && a[0] && b && b[0])
		{
#ifdef KICKSTART_DEBUG
			if (a) fprintf (stderr, "$HOMEDRIVE = %s  $HOMEPATH = %s\n", b, a);
#endif
			_cfHomePath = malloc (strlen (a) + strlen (b) + 1);
			if (_cfHomePath)
			{
				sprintf (_cfHomePath, "%s%s", b, a);
			}
		}
	}
	if (!_cfHomePath)
	{
		a = getenv("home");
#ifdef KICKSTART_DEBUG
		if (a) fprintf (stderr, "$home = %s\n", a);
#endif
		_cfHomePath = strdup (a);
	}
	if (!_cfHomePath)
	{
		if (SHGetFolderPath (NULL, CSIDL_APPDATA, NULL, 0, path) != S_OK)
		{
			fprintf (stderr, "Failed to retrieve %%PROFILE%%\n");
			return -1;
		}
#ifdef KICKSTART_DEBUG
		if (a) fprintf (stderr, "%%PROFILE%% = %s\n", a);
#endif
		_cfHomePath = strdup (path);
	}
	/* ensure that _cfHomePath ends with \\ */
	if (_cfHomePath && _cfHomePath[strlen(_cfHomePath)-1] != '\\')
	{
		char *t = malloc (strlen(_cfHomePath) + 2);
		if (!t)
		{
			fprintf (stderr, "malloc() failed\n");
			return -1;
		}
		sprintf (t, "%s\\", _cfHomePath);
		free (_cfHomePath);
		_cfHomePath = t;
	}
	if ((!_cfHomePath) ||
	    (!_cfHomePath[0]) ||
	    (_cfHomePath[1] != ':') ||
	    ( ((_cfHomePath[1] <= 'A') && (_cfHomePath[1] >= 'Z')) && ((_cfHomePath[1] <= 'a') && (_cfHomePath[1] >= 'z')) ) ||
	    strstr (_cfHomePath, "\\..\\") )
	{
		fprintf (stderr, "Error, $HOME is not an absolute path, ignoring value\n");
		return -1;
	}

	if (!GetModuleFileName (NULL, path, sizeof (path)))
	{
		fprintf (stderr, "Failed to retrieve path of OCP.EXE\n");
		return -1;
	}
	_cfProgramPath = strdup (path);
	a = strrchr (_cfProgramPath, '\\'); /* remove ocp.exe from string */
	if (a)
	{
		a[1] = 0;
	}

#ifdef KICKSTART_DEBUG
	fprintf (stderr, "cfProgramPath set to %s\n", _cfProgramPath);
#endif

	if (!(_cfDataPath=locate_ocp_hlp()))
	{
		fprintf(stderr, "Failed to locate ocp.hlp..\n");
		return -1;
	}

#ifdef KICKSTART_DEBUG
	fprintf (stderr, "cfDataPath set to %s\n", _cfDataPath);
#endif

	if (validate_home())
		return -1;

	retval = runocp (argc, argv);
	free (_cfConfigHomePath);
	free (_cfDataHomePath);
	free (_cfHomePath);
	free (_cfDataPath);
	free (_cfProgramPath);

#ifdef HAVE_DUMA
	DUMA_delFrame();
#endif
	return retval;
}
