/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include "framelock.h"
#ifdef DISABLE_SIGALRM
#include "timer.h"
#endif

static int Current = 0;
int fsFPS=25;
int fsFPSCurrent=0;

static int fpsInit(void)
{
	fsFPS=cfGetProfileInt("screen", "fps", 20, 0);
	if (fsFPS<=0)
		fsFPS=20;
	return errOk;
}

void framelock(void)
{
	static struct timeval target = {0, 0};
	static struct timeval curr;
rerun:
	gettimeofday(&curr, 0);
	if (curr.tv_sec!=target.tv_sec)
	{
		fsFPSCurrent=Current;
		Current=1;
		target.tv_sec=curr.tv_sec;
		target.tv_usec=1000000/fsFPS;
		return;
	} else if (curr.tv_usec<target.tv_usec)
	{
		usleep(target.tv_usec-curr.tv_usec);
		goto rerun;
	}
	target.tv_usec+=1000000/fsFPS;

#ifdef DISABLE_SIGALRM
	tmTimerHandler();
#endif
	Current++;
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {"fps", "OpenCP Frames Per Second lock (c) 2005-09 Stian Skjelstad", DLLVERSION, 0, Init: fpsInit};
