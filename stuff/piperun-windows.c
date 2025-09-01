/* OpenCP Module Player
 * copyright (c) 2023-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Run a piped process under Windows
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
#include <handleapi.h>
#include <inttypes.h>
#include <processthreadsapi.h>
#include <stdio.h>
#include <stdint.h>
#include <synchapi.h>
#include <windows.h>
#include "types.h"
#include "piperun.h"
#include "stuff/utf-16.h"

struct ocpPipeProcess_t
{
	void *hProcess;
#if 0
	void *hStdInput;
#endif
	void *hStdOutput;
	void *hStdError;
	int ReadingOutput; // outstanding request
	int ReadingError; // outstanding request
	OVERLAPPED ov_Output;
	OVERLAPPED ov_Error;
};

#if defined(_MSC_VER)
#define ocp_tls __declspec(thread)
#elif defined(__MINGW32__)
#define ocp_tls __thread
#elif defined(__clang__) || defined(__GNUC__)
#define ocp_tls __thread
#else
#error Non clang, non gcc, non MSVC compiler found!
#endif

int ocpPipeProcess_destroy (void *_process)
{
	struct ocpPipeProcess_t *process = _process;
	int retval = -1;

	if (!process)
	{
		return retval;
	}

#if 0
	if (process->hStdInput)
	{
		CloseHandle (process->hStdInput);
		process->hStdInput = NULL;
	}
#endif

	if (process->hStdOutput)
	{
		CloseHandle (process->hStdOutput);
		process->hStdOutput = NULL;
	}

	if (process->hStdError)
	{
		CloseHandle (process->hStdError);
		process->hStdError = NULL;
	}

	if (process->ov_Output.hEvent)
	{
		CloseHandle (process->ov_Output.hEvent);
		process->ov_Output.hEvent = NULL;
	}

	if (process->ov_Error.hEvent)
	{
		CloseHandle (process->ov_Error.hEvent);
		process->ov_Error.hEvent = NULL;
	}

	if (process->hProcess)
	{
		DWORD ExitCode = 0;
		int retries = 50;
		do
		{
			if (GetExitCodeProcess (process->hProcess, &ExitCode))
			{
				if (ExitCode == STILL_ACTIVE)
				{
					if (retries)
					{
						retries--;
						Sleep (1);
						continue;
					}
					ExitCode = (DWORD)-2;
				}
				break;
			} else {
				ExitCode = (DWORD)-1;
				break;
			}
		} while (1);
		retval = (int32_t) ExitCode;

		CloseHandle (process->hProcess);
		process->hProcess = NULL;
	}

	free (process);

	return retval;
}

static int ocpPipeProcess_create_helper (HANDLE *rd, HANDLE *wr)
{
	SECURITY_ATTRIBUTES saAttr = {sizeof(saAttr), NULL, 1};
	char name[256] = {0};
	static ocp_tls long index = 0;
	const long unique = index++;

	snprintf (name, sizeof(name) - 1, "\\\\.\\pipe\\opencubicplayer.%08lx.%08lx.%ld", GetCurrentProcessId(), GetCurrentThreadId(), unique);

	*rd = CreateNamedPipeA (name,
	                        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
	                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
	                        1,    /* max-instances */
	                        4096, /* nOutBufferSize */
	                        4096, /* nInBufferSize */
	                        0,    /* nDefaultTimeOut */
	                        &saAttr);

	if (*rd == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	*wr = CreateFileA (name,
	                   GENERIC_WRITE,
	                   0,
	                   &saAttr,
	                   OPEN_EXISTING,
	                   FILE_ATTRIBUTE_NORMAL,
	                   0);

	if (*wr == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	return 0;
}

void *ocpPipeProcess_create (const char * const commandLine[])
{
	struct ocpPipeProcess_t *process;
	uint16_t *commandLineCombined;
	size_t len;
	int i, j;
	int need_quoting;
	PROCESS_INFORMATION processInfo;

	SECURITY_ATTRIBUTES saAttr =
	{
		sizeof(saAttr),
		NULL,
		1
	};

	STARTUPINFOW startInfo =
	{
		sizeof (startInfo),   /* cb */
		NULL,                 /* lpReserved */
		NULL,                 /* lpDesktop */
		NULL,                 /* lpDesktop */
		0,                    /* dwX */
		0,                    /* dwY */
		0,                    /* dwXSize */
		0,                    /* dwYSize */
		0,                    /* dwXCountChars */
		0,                    /* dwYCountChars */
		0,                    /* dwFillAttribute */
		STARTF_USESTDHANDLES, /* dwFlags */
		0,                    /* wShowWindow */
		0,                    /* cbReserved2 */
		NULL,                 /* lpReserved2 */
		NULL,                 /* hStdInput */
		NULL,                 /* hStdOutput */
		NULL                  /* hStdError */
	};

	process = calloc (1, sizeof (*process));

	/* spawn stdin, using regular Pipe */
#if 0
	if (!CreatePipe (&rd, &wr, &saAttr, 0)) return -1;
	if (!SetHandleInformation (wr, HANDLE_FLAG_INHERIT, 0)) return -1;
	fd = _open_osfhandle((subprocess_intptr_t)wr), 0);
	if (-1 != fd)
	{
		process->stdin_file = _fdopen(fd, "wb");
		if (!process->stdin_file) return -1;
	}
	startInfo.hStdInput = rd;
#endif

	/* spawn stdout, using named pipe (allows to read partial results before peer closes it) */
	if (ocpPipeProcess_create_helper (&process->hStdOutput, &startInfo.hStdOutput)) goto error;
	if (!SetHandleInformation (process->hStdOutput, HANDLE_FLAG_INHERIT, 0)) goto error;

	/* spawn stderr, using named pipe (allows to read partial results before peer closes it) */
	if (ocpPipeProcess_create_helper (&process->hStdError, &startInfo.hStdError)) goto error;
	if (!SetHandleInformation (process->hStdError, HANDLE_FLAG_INHERIT, 0)) goto error;

	/* create event handlers */
	if (!(process->ov_Output.hEvent = CreateEventA (&saAttr, 1, 1, NULL))) goto error;
	if (!(process->ov_Error.hEvent  = CreateEventA (&saAttr, 1, 1, NULL))) goto error;

	// Combine commandLine together into a single string
	len = 0;
	for (i = 0; commandLine[i]; i++)
	{ // for the trailing \0
		len++;

		// Quote the argument if it has a space in it
		if (strpbrk (commandLine[i], "\t\v ") != NULL)
		{
			len += 2;
		}

		for (j = 0; '\0' != commandLine[i][j]; j++)
		{
			switch (commandLine[i][j])
			{
				default:
					break;

				case '\\':
					if (commandLine[i][j + 1] == '"')
					{
						len++;
					}
					break;

				case '"':
					len++;
					break;
			}
			len++;
		}
	}

	commandLineCombined = (uint16_t *) _alloca(len * 4); /* utf-16, worst case length */
	if (!commandLineCombined) goto error;

	// Gonna re-use len to store the write index into commandLineCombined
	len = 0;
	for (i = 0; commandLine[i]; i++)
	{
		uint16_t *w = utf8_to_utf16 (commandLine[i]);
		if (0 != i)
		{
			commandLineCombined[len++] = ' ';
		}

		need_quoting = strpbrk(commandLine[i], "\t\v ") != NULL;
		if (need_quoting)
		{
			commandLineCombined[len++] = '"';
		}

		for (j = 0; w[j]; j++)
		{
			switch (w[j])
			{
				default:
					break;

				case '\\':
					if (w[j + 1] == '"')
					{
						commandLineCombined[len++] = '\\';
					}
					break;

				case '"':
					commandLineCombined[len++] = '\\';
					break;
			}

			commandLineCombined[len++] = w[j];
		}
		if (need_quoting)
		{
			commandLineCombined[len++] = '"';
		}
		free (w);
	}

	commandLineCombined[len] = '\0';

	if (!CreateProcessW (NULL,
	                     commandLineCombined, // command line
	                     NULL,                // process security attributes
	                     NULL,                // primary thread security attributes
	                     1,                   // handles are inherited
	                     CREATE_NO_WINDOW,    // creation flags
	                     NULL,                // use parent's environment
	                     NULL,                // use parent's current directory
	                     &startInfo,          // STARTUPINFO pointer
	                     &processInfo))
	{
		goto error;
	}

	process->hProcess = processInfo.hProcess;

	// We don't need the handle of the primary thread in the called process.
	CloseHandle (processInfo.hThread);
	CloseHandle (startInfo.hStdOutput);
	CloseHandle (startInfo.hStdError);

	return process;

error:
	if (startInfo.hStdError)
	{
		CloseHandle (startInfo.hStdError);
		startInfo.hStdError = 0;
	}

	if (processInfo.hThread)
	{
		CloseHandle (processInfo.hThread);
		processInfo.hThread = 0;
	}

	if (startInfo.hStdOutput)
	{
		CloseHandle (startInfo.hStdOutput);
		startInfo.hStdOutput = 0;
	}

	if (startInfo.hStdError)
	{
		CloseHandle (startInfo.hStdError);
		startInfo.hStdError = 0;
	}

	ocpPipeProcess_destroy (process);

	return 0;
}

int ocpPipeProcess_terminate (void *_process)
{
	struct ocpPipeProcess_t *const process = _process;
	unsigned int killed_process_exit_code;

	killed_process_exit_code = 99;
	return TerminateProcess (process->hProcess, killed_process_exit_code);
}

static int ocpPipeProcess_read_common (char *const buffer, unsigned size, HANDLE *h, int *Reading, LPOVERLAPPED ov)
{
	DWORD bytes_read = 0;

	if (!h)
	{
		return -1;
	}
	if (*Reading) // async in progress
	{
		if (WaitForSingleObject (ov->hEvent, 0) != WAIT_OBJECT_0)
		{
			return 0;
		}
		*Reading = 0;
		if (!GetOverlappedResult (h, ov, &bytes_read, 1))
		{
			return -1;
		}
		return (int32_t)bytes_read;
	}

	if (!ReadFile (h, buffer, size, &bytes_read, ov))
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			*Reading = 1;
			return 0;
		}
		return -1;
	}
	return (int32_t)bytes_read;
}

int ocpPipeProcess_read_stdout (void *_process, char *const buffer, unsigned size)
{
	struct ocpPipeProcess_t *const process = _process;
	return ocpPipeProcess_read_common (buffer, size, process->hStdOutput, &process->ReadingOutput, &process->ov_Output);
}

int ocpPipeProcess_read_stderr (void *_process, char *const buffer, unsigned size)
{
	struct ocpPipeProcess_t *const process = _process;
	return ocpPipeProcess_read_common (buffer, size, process->hStdError, &process->ReadingError, &process->ov_Error);
}

const struct PipeProcessAPI_t PipeProcess =
{
	ocpPipeProcess_create,
	ocpPipeProcess_destroy,
	ocpPipeProcess_terminate,
	ocpPipeProcess_read_stdout,
	ocpPipeProcess_read_stderr
};

#if 0
int main (int argc, char *argv[])
{
	const char *const commandLine[] = {"ping", "8.8.8.8", 0};
	struct ocpPipeProcess_t *process;
	int32_t bytes_read1, bytes_read2;

	if ( !(process = ocpPipeProcess_create (commandLine)) )
	{
		fprintf(stderr, "ocpPipeProcess_create failed!");
		return -1;
	}

	do
	{
		static char buffer1[1024] = {0};
		static char buffer2[1024] = {0};
		printf ("."); fflush(stdout);
		bytes_read1 = ocpPipeProcess_read_stdout(process, buffer1, sizeof(buffer1));
		if (bytes_read1 > 0)
		{
			printf ("%.*s", bytes_read1, buffer1);
		}
		bytes_read2 = ocpPipeProcess_read_stdout(process, buffer2, sizeof(buffer2));
		if (bytes_read2 > 0)
		{
			printf ("%.*s", bytes_read2, buffer2);
		}
		fflush(stdout);
		if ((bytes_read1 == 0) && (bytes_read2 == 0))
		{
			Sleep (50);
		}
		DWORD ExitCode;
		SetLastError (0);
	} while ((bytes_read1 >= 0) || (bytes_read2 >= 0));

	fprintf (stderr, "ExitCode = %d\n", ocpPipeProcess_destroy (process));

	return 0;
}
#endif
