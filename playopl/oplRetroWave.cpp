/* OpenCP Module Player
 * copyright (c) 2005-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/file.h>
#ifndef _WIN32
#include <termios.h>
#else
#include <fileapi.h>
#include <handleapi.h>
#include <windows.h>
#endif
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>
#endif
extern "C"
{
#include "types.h"
#include "cpiface/cpiface.h"
}
#include "oplRetroWave.h"

#ifdef _WIN32
static HANDLE fd = INVALID_HANDLE_VALUE;
#else
static int fd = -1;
#endif

#include "oplRetroWave-serialization.cpp"

#include "oplRetroWave-helperthread.cpp"

oplRetroWave::oplRetroWave (void(*cpiDebug)(struct cpifaceSessionAPI_t *cpifaceSession, const char *fmt, ...), struct cpifaceSessionAPI_t *cpifaceSession, const char *device, int rate)

{
	FailedToOpen = oplRetroWave_Open (cpiDebug, cpifaceSession, device);
	currType = TYPE_OPL3;
	this->rate = rate;
}

oplRetroWave::~oplRetroWave()
{
	oplRetroWave_Close();
}

void oplRetroWave::update(short *buf, int samples)
{
	memset (buf, 0, samples * 2);
	unsigned int request = ((uint64_t)1000000ll * 65536 * (unsigned)samples + samples_rounded) / rate;
	samples_rounded = request % ratescale;
	request /= ratescale;
	oplRetroWave_Sleep (request);
}

void oplRetroWave::write(int reg, int val)
{
	oplRetroWave_Write (currChip, reg, val);
}

void oplRetroWave::init()
{
	oplRetroWave_Reset();
}
