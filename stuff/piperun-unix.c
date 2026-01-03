/* OpenCP Module Player
 * copyright (c) 2023-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Run a piped process under POSIX
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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "types.h"

#include "piperun.h"

struct ocpPipeProcess_t
{
	pid_t pid;
#if 0
	int in;
#endif
	int out, err;
};

void *ocpPipeProcess_create (const char * const commandLine[])
{
	struct ocpPipeProcess_t *process;

	int fdout[2];
	int fderr[2];

	if (pipe (fdout) < 0)
	{
		return 0;
	}
	if (pipe (fderr) < 0)
	{
		close (fdout[0]);
		close (fdout[1]);
		return 0;
	}

	process = calloc (1, sizeof (*process));
	process->out = fdout[0];
	process->err = fderr[0];

	fcntl (process->out, F_SETFL, O_NONBLOCK);
	fcntl (process->err, F_SETFL, O_NONBLOCK);
	fcntl (process->out, F_SETFD, FD_CLOEXEC);
	fcntl (process->err, F_SETFD, FD_CLOEXEC);

	process->pid = fork ();

	if (process->pid < 0)
	{
		close (fdout[0]);
		close (fdout[1]);
		close (fderr[0]);
		close (fderr[1]);
		free (process);
		return 0;
	}

	if (process->pid > 0)
	{
		close (fdout[1]);
		close (fderr[1]);
		return process;
	}

	close (0); /* stdin */
	open ("/dev/null", O_RDONLY);

	close (1); /* stdout */
	if (dup (fdout[1]) != 1)
	{
		perror ("dup() failed");
	}

	close (2); /* stderr */
	if (dup (fderr[1]) != 2)
	{
		perror ("dup() failed");
	}

	close (fdout[0]);
	close (fdout[1]);
	close (fderr[0]);
	close (fderr[1]);

	execvp (commandLine[0], (char * const *)commandLine);

	perror ("execvp()");

	_exit (1);
}

int ocpPipeProcess_destroy (void *_process)
{
	struct ocpPipeProcess_t *process = _process;
	int retval = 0;
	if (!process)
	{
		return -1;
	}
	close (process->out);
	close (process->err);

	while (process->pid >= 0)
	{
		pid_t retval = waitpid (process->pid, &retval, WNOHANG);
		if (retval == process->pid)
		{
			break;
		}
		if (retval < 0)
		{
			if ((errno != EAGAIN) &&
			    (errno != EINTR))
			{
				fprintf (stderr, "waitpid() failed: %s\n", strerror (errno));
				break;
			}
		}
		usleep(10000);
	}
	process->pid = -1;
	free (process);
	return retval;
}

int ocpPipeProcess_terminate (void *_process)
{
	struct ocpPipeProcess_t *process = _process;
	if (!process)
	{
		return -1;
	}
	if (process->pid < 0)
	{
		return -1;
	}
	return kill (process->pid, SIGQUIT);
}

int ocpPipeProcess_read_stdout (void *_process, char *const buffer, unsigned size)
{
	struct ocpPipeProcess_t *process = _process;
	ssize_t res;
	if (!process)
	{
		return -1;
	}
	res = read (process->out, buffer, size);
	if (res < 0)
	{
		if (errno == EAGAIN)
		{
			return 0;
		}
	}
	return res ? res : - 1;
}

int ocpPipeProcess_read_stderr (void *_process, char *const buffer, unsigned size)
{
	struct ocpPipeProcess_t *process = _process;
	ssize_t res;
	if (!process)
	{
		return -1;
	}
	res = read (process->err, buffer, size);
	if (res < 0)
	{
		if (errno == EAGAIN)
		{
			return 0;
		}
	}
	return res ? res : -1;
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
	const char *const commandLine[] = {"ping", "-c", "4", "8.8.8.8", 0};
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
			usleep (50000);
		}
	} while ((bytes_read1 >= 0) || (bytes_read2 >= 0));

	ocpPipeProcess_destroy (process);

	return 0;
}
#endif
