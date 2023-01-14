/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Systemtimer handlers
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
 *  -doj990328  Dirk Jagdmann  <doj@cubic.org>
 *    -changed interrupt access calls to calls from irq.h
 *  -fd990518   Felix Domke  <tmbinc@gmx.net>
 *    -added CLD after the tmOldTimer-call.
 *     this removed the devwmix*-STRANGEBUG. (finally, i hope)
 *  -fd990817   Felix Domke  <tmbinc@gmx.net>
 *    -added tmSetSecure/tmReleaseSecure to ensure that timer is only
 *     called when not "indos". needed for devpVXD (and some other maybe).
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -Rewritten to use the posix itimer, and posix signals to fetch them
 *  --ss040907  Stian Skjelstad <stian@nixia.no>
 *    -Use gettimeofday() to calculate cpu-usage, since itimer() uses rounded off values
 */

#include "config.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include "types.h"
#include "imsrtns.h"
#include "poll.h"

static void (*tmTimerRoutineSlave)()=NULL;

void tmTimerHandler(void)
{
	if (tmTimerRoutineSlave)
		tmTimerRoutineSlave();
}

int pollInit(void (*f)(void))
{
	tmTimerRoutineSlave=f;
	return 1;
}

void pollClose(void)
{
	tmTimerRoutineSlave=0;
}
