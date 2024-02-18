/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Frames per second lock
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
 *  -ss050118   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#include "config.h"
#include "types.h"
#include <sys/time.h>
#include <unistd.h>
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "filesel/pfilesel.h"
#include "stuff/err.h"
#include "stuff/framelock.h"
#include "stuff/poll.h"

static int Current = 0;
static int PendingPoll = 0;
int fsFPS=25;
int fsFPSCurrent=0;

static struct timeval targetFPS = {0, 0};
static struct timeval targetAudioPoll = {0, 0};

OCP_INTERNAL void framelock_init (void)
{
	fsFPS=cfGetProfileInt("screen", "fps", 20, 0);
	if (fsFPS<=0)
		fsFPS=20;
}

/* If fsFPS < 50, we run to parallel timers. One for framelock and one for AudioPoll. If fsFPS > 50, we run them on the same timer */

static void AudioPoll(struct timeval *curr)
{
	if (curr->tv_sec != targetAudioPoll.tv_sec)
	{
		targetAudioPoll.tv_sec = curr->tv_sec;
		targetAudioPoll.tv_usec = 1000000/50;
		tmTimerHandler ();
	} else if (curr->tv_usec >= targetAudioPoll.tv_usec)
	{
		targetAudioPoll.tv_usec += 1000000/50;
		tmTimerHandler ();
	}
}

void framelock(void)
{
	struct timeval curr;

	PendingPoll = 0;
rerun:
	gettimeofday(&curr, 0);

	if (fsFPS < 50)
	{
		AudioPoll (&curr);
	}

	if (curr.tv_sec!=targetFPS.tv_sec)
	{
		fsFPSCurrent=Current;
		Current=1;
		targetFPS.tv_sec=curr.tv_sec;
		targetFPS.tv_usec=1000000/fsFPS;
		return;
	} else if ((fsFPS >= 50) || (targetFPS.tv_usec < targetAudioPoll.tv_usec))
	{
		if (curr.tv_usec<targetFPS.tv_usec)
		{
			usleep(targetFPS.tv_usec-curr.tv_usec);
		}
		targetFPS.tv_usec+=1000000/fsFPS;
	} else {
		if (curr.tv_usec < targetAudioPoll.tv_usec)
		{
			usleep(targetAudioPoll.tv_usec-curr.tv_usec);
		}
		goto rerun;
	}
	//if (fsFPS >= 50)
	{
		tmTimerHandler();
	}
	Current++;
}

void preemptive_framelock (void)
{
	struct timeval curr;

	gettimeofday(&curr, 0);

	if (fsFPS < 50)
	{
		AudioPoll (&curr);
	}

	if (curr.tv_sec!=targetFPS.tv_sec)
	{
		fsFPSCurrent=Current;
		Current=1;
		targetFPS.tv_sec=curr.tv_sec;
		targetFPS.tv_usec=1000000/fsFPS;
		PendingPoll = 1;
		return;
	} else if (curr.tv_usec<targetFPS.tv_usec)
	{
		return; /* we were suppose to sleep */
	}
	targetFPS.tv_usec+=1000000/fsFPS;
	//if (fsFPS >= 50)
	{
		tmTimerHandler();
	}
	Current++;
	PendingPoll = 1;
}

int poll_framelock(void)
{
	struct timeval curr;

	gettimeofday(&curr, 0);

	if (fsFPS < 50)
	{
		AudioPoll (&curr);
	}

	if (curr.tv_sec!=targetFPS.tv_sec)
	{
		fsFPSCurrent=Current;
		Current=1;
		targetFPS.tv_sec=curr.tv_sec;
		targetFPS.tv_usec=1000000/fsFPS;
		PendingPoll = 0;
		return 1;
	} else if (curr.tv_usec<targetFPS.tv_usec)
	{
		if (PendingPoll)
		{
			PendingPoll = 0;
			return 1;
		}
		return 0; /* we were suppose to sleep */
	}
	targetFPS.tv_usec+=1000000/fsFPS;

	//if (fsFPS >= 50)
	{
		tmTimerHandler();
	}

	Current++;
	PendingPoll = 0;
	return 1;
}
