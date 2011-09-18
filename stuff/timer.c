/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include "types.h"
#include "irq.h"
#include "imsrtns.h"
#include "timer.h"
#include "poll.h"

#define CLOCK_TICK_RATE 1193180 /* Base hertz of the i8253 or what ever that chip was namned - Stian */

static void (*tmTimerRoutine)()=NULL;
static void (*tmTimerRoutineSlave)()=NULL;
static int secure=0;
#ifdef TIMER_DEBUG
static int tmInited = 0;
#endif

#ifdef DISABLE_SIGALRM
#include "compat.h"
void tmTimerHandler(void)
{
	if (!secure)
		if (tmTimerRoutine)
			tmTimerRoutine();
	if (tmTimerRoutineSlave)
		tmTimerRoutineSlave();
}

int tmInit(void (*rout)(), int timerval)
{
	tmTimerRoutine=rout;
#ifdef TIMER_DEBUG
	tmInited=1;
#endif
	return 1;
}

void tmClose(void)
{
#ifdef TIMER_DEBUG
	tmInited=0;
#endif
	tmTimerRoutine=NULL;
}

void tmSetNewRate(int rate)
{
}

time_t tmGetTimer(void)
{
	return dos_clock(); /* this is somewhat off, but we don't care */
}

int  tmGetCpuUsage(void)
{
	return 0;
}

int pollInit(void (*f)(void))
{
	tmTimerRoutineSlave=f;
	return 1;
}
void pollClose(void)
{
	tmTimerRoutineSlave=NULL;
}

#else

static unsigned long count_to_time(unsigned long ticks)
{
	unsigned long retval;

	if (ticks&0xfffff000)
	{
		ticks*=62500; /* this is safe, since counter on the chip is 16 bit */
		retval = ticks / CLOCK_TICK_RATE;
		retval*=16;
	} else {
		ticks*=1000000;
		retval = ticks / CLOCK_TICK_RATE;
	}
	return retval;
}

static unsigned long time_to_count(unsigned long time)
{
	unsigned long retval;

	if (time<=4000)
	{
		retval = time * CLOCK_TICK_RATE / 1000000;
	} else {
		time/=4000;
		retval = time * CLOCK_TICK_RATE / 2500;
	}
	return retval;
}

static unsigned long tmTimerRate;
static unsigned long tmIntCount;
static unsigned long tmTicker;

static volatile char overload;
static volatile float cpuusage;

static int stackused; /* we don't provide a stack, but is for locking */

static void tmTimerHandler(int ignore)
{
	struct timeval pre, pro;

	gettimeofday(&pre, NULL);
	tmTicker+=tmTimerRate;

	tmIntCount+=tmTimerRate;
	if (tmIntCount&0xFFFFc000)
	{
		tmIntCount&=0x3FFF;
		if (tmTimerRoutineSlave)
			tmTimerRoutineSlave();
	}

	if (stackused++)
	{
		stackused--;
		cpuusage=100;
		overload=1;
		return;
	}
	if (!secure)
	{
		if (tmTimerRoutine)
			tmTimerRoutine();
	}
	stackused--;

	if (!overload)
	{
		struct itimerval spec;
		unsigned long spent;

		getitimer(ITIMER_REAL, &spec);
		gettimeofday(&pro, NULL);
		spent=(pro.tv_sec-pre.tv_sec)*1000000+pro.tv_usec-pre.tv_usec;
		cpuusage=0.1*(100.0*spent/spec.it_interval.tv_usec)+0.9*cpuusage;
	} else
		cpuusage=100;
	overload=0;
}

int tmInit(void (*rout)(), int timerval)
{
	struct itimerval spec;

#ifdef TIMER_DEBUG
	if (tmInited)
	{
		fprintf(stderr, "tmInit: we are already inited\n");
	} else {
		tmInited=1;
	}
#endif

	tmTimerRate = timerval;
	tmTicker = -timerval;
#ifdef TIMER_DEBUG
	fprintf(stderr, "tmInit (ticks=%d, time=%ld)\n", timerval, count_to_time(timerval));
#endif
	timerval = count_to_time(timerval);

	tmTimerRoutine=rout;
	tmIntCount=0;

	irqInit(SIGALRM, tmTimerHandler, 1);
	spec.it_interval.tv_sec=0;
	spec.it_interval.tv_usec=timerval;
	spec.it_value.tv_sec=0;
	spec.it_value.tv_usec=timerval;
	setitimer(ITIMER_REAL, &spec, NULL);

	cpuusage=0;

	return 1;
}

static void tmResetTimer(void)
{
#ifdef TIMER_DEBUG
	if (!tmInited)
		fprintf(stderr, "tmResetTimer: we are not inited\n");
#endif
	if (!(tmTimerRoutine&&tmTimerRoutineSlave))
	{
		/* shut down irq */
		struct itimerval spec;

		spec.it_interval.tv_sec=0;
		spec.it_interval.tv_usec=0;
		spec.it_value.tv_sec=0;
		spec.it_value.tv_usec=0;
		setitimer(ITIMER_REAL, &spec, NULL);

		irqDone(SIGALRM);
	} else if (!tmTimerRoutine)
		/* recalc to a less/more aggressive clock */
		tmSetNewRate(17100);

}

void tmClose(void)
{
#ifdef TIMER_DEBUG
	if (!tmInited)
		fprintf(stderr, "tmClose: we are not inited\n");
#endif
	tmTimerRoutine=NULL;
	tmResetTimer();
#ifdef TIMER_DEBUG
	tmInited=0;
#endif
}

void tmSetNewRate(int timerval)
{
	struct itimerval spec;

#ifdef TIMER_DEBUG
	if (!tmInited)
		fprintf(stderr, "tmSetNewRate: we are not inited\n");
#endif

	tmTimerRate = timerval;
#ifdef TIMER_DEBUG
	fprintf(stderr, "tmSetNewRate (ticks=%d, time=%ld)\n", timerval, count_to_time(timerval));
#endif

	timerval = count_to_time(timerval);

	spec.it_interval.tv_sec=0;
	spec.it_interval.tv_usec=timerval;
	spec.it_value.tv_sec=0;
	spec.it_value.tv_usec=timerval;
	setitimer(ITIMER_REAL, &spec, NULL);
}

time_t tmGetTimer(void)
{
	unsigned long tm = tmTimerRate+tmTicker;

	unsigned long tv2;
	struct itimerval spec;

#ifdef TIMER_DEBUG
	if (!tmInited)
		fprintf(stderr, "tmGetTimer: we are not inited\n");
#endif

	getitimer(ITIMER_REAL, &spec);
	tv2 = time_to_count(spec.it_value.tv_usec);

	tm -= tv2;
	return umulshr16(tm, 3600);
}

int tmGetCpuUsage()
{
#ifdef TIMER_DEBUG
	if (!tmInited)
		fprintf(stderr, "tmGetCpuUsage: we are not inited\n");
#endif
	return (char)cpuusage;
}

int pollInit(void (*proc)(void))
{
	tmTimerRoutineSlave=proc;
	if (!tmTimerRoutine) /* If no timer active, just start to count */
		tmInit(NULL, 17100);
	return 1;
}

void pollClose(void)
{
	tmTimerRoutineSlave=NULL;
	tmResetTimer();
}

#endif

void tmSetSecure()
{
#ifdef TIMER_DEBUG
	if (!tmInited)
		fprintf(stderr, "tmSecure: we are not inited\n");
#endif
	secure++;
}

void tmReleaseSecure(void)
{
#ifdef TIMER_DEBUG
	if (!tmInited)
		fprintf(stderr, "tmReleaseSecure: we are not inited\n");
#endif
	if (secure)
		secure--;
}
