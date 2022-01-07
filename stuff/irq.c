/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * IRQ handlers
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -POSIX signals looks so much cleaner, don't they?
 */

#include "config.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "irq.h"

#ifndef _NSIG
#define _NSIG NSIG
#endif

static struct sigaction orgstate[_NSIG] = {{{0}}};
static int gotmask = 0;
static sigset_t orgmask;

int irqInit(int signum, void (*rout)(int), int recursive)
{
	struct sigaction newstate;

	memset(&newstate, 0, sizeof(newstate));
	newstate.sa_handler=rout;
	if (recursive)
		newstate.sa_flags=SA_NODEFER;
	newstate.sa_flags|=SA_RESTART;

/*
	fprintf(stderr, "irqInit: setting irq %d to %p\n", signum, rout);
*/
	if (sigaction(signum, &newstate, &orgstate[signum]))
	{
		perror("sigaction()");
		exit(1);
	}

	if (!gotmask)
	{
		if (sigprocmask(SIG_SETMASK, NULL, &orgmask))
		{
			perror("sigprocmask(1)");
			exit(1);
		}
		gotmask=1;
	}

	if (sigismember(&orgmask, signum))
	{
		sigset_t mask;

		memset(&mask, 0, sizeof(mask));
		sigaddset(&mask, signum);
		if (sigprocmask(SIG_UNBLOCK, &mask, NULL))
		{
			perror("sigprocmask(2)");
			exit(1);
		}

	}
	return 1;
}

void irqDone(int signum)
{
	if (sigismember(&orgmask, signum))
	{
		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, signum);
		sigprocmask(SIG_BLOCK, &mask, NULL);
	}
	sigaction(signum, &orgstate[signum], NULL);
	return;
}
